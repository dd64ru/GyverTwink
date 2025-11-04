/*
  Скетч к проекту "GyverTwink"
  - Страница проекта (схемы, описания): https://alexgyver.ru/gyvertwink/
  - Исходники на GitHub: https://github.com/AlexGyver/GyverTwink
  Проблемы с загрузкой? Читай гайд для новичков: https://alexgyver.ru/arduino-first/
  AlexGyver, AlexGyver Technologies, 2021
*/

/*
  1.1 - исправлена калибровка больше 255 светодиодов
  1.2 - исправлена ошибка с калибровкой
*/

/*
  Мигает синим - открыт портал
  Мигает жёлтым - подключаемся к точке
  Мигнул зелёным - подключился к точке
  Мигнул красным - ошибка подключения к точке
  Мигнул розовым - создал точку
*/

// ================ НАСТРОЙКИ ================
#define BTN_PIN D3      // пин кнопки
#define BTN_TOUCH 0     // 1 - сенсорная кнопка, 0 - нет

#define LED_PIN D1      // пин ленты
#define LED_TYPE WS2812 // чип ленты
#define LED_ORDER GRB   // порядок цветов ленты
#define LED_MAX 500     // макс. светодиодов

// имя точки в режиме AP
#define GT_AP_SSID "GyverTwink"
#define GT_AP_PASS "12345678"
// имя и пароль Wi-Fi сети для режима клиента (оставь пустыми, если не нужно)
#define GT_STA_SSID ""
#define GT_STA_PASS ""
//#define DEBUG_SERIAL_GT   // раскомментируй, чтобы включить отладку

// ================== LIBS ==================
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <SimplePortal.h>
#include <FastLED.h>
#include <EEManager.h>
#include <EncButton.h>
#include <string.h>
#include "palettes.h"
#include "Timer.h"

// ================== OBJECTS ==================
WiFiServer server(80);
WiFiUDP udp;
EEManager EEwifi(portalCfg);
CRGB leds[LED_MAX];
CLEDController *strip;
EncButton<EB_TICK, BTN_PIN> btn;
IPAddress myIP;

// ================== EEPROM BLOCKS ==================
struct Cfg {
  uint16_t ledAm = 50;
  bool power = 1;
  byte bright = 100;
  bool autoCh = 0;
  bool rndCh = 0;
  byte prdCh = 1;
  bool turnOff = 0;
  byte offTmr = 60;
  bool parisMoments = false;
  bool snowflakes = false;
};
Cfg cfg;
EEManager EEcfg(cfg);

byte xy[LED_MAX][2];
EEManager EExy(xy);

struct MM {
  byte minY = 0;
  byte maxY = 255;
  byte minX = 0;
  byte maxX = 255;
  byte w = 255;
  byte h = 255;
};
MM mm;
EEManager EEmm(mm);

#define ACTIVE_PALETTES 11
#define TOTAL_EFFECTS (ACTIVE_PALETTES * 2 + 1)
struct Effects {
  bool fav = true;
  byte scale = 50;
  byte speed = 150;
};
Effects effs[TOTAL_EFFECTS];
EEManager EEeff(effs);

const byte PARIS_EFFECT_INDEX = TOTAL_EFFECTS - 1;

// ================== MISC DATA ==================
Timer forceTmr(30000, false);
Timer switchTmr(0, false);
Timer offTmr(60000, false);
Timer parisEventTimer(0, false);
Timer parisDurationTimer(0, false);
bool calibF = false;
byte curEff = 0;
byte forceEff = 0;
bool parisOverlayActive = false;

void resetSnowflakes();
void applySnowflakeState(bool state);

bool hasDefaultStaCredentials() {
  return GT_STA_SSID[0] != '\0';
}

void applyDefaultWifiConfig() {
  if (!hasDefaultStaCredentials()) return;

  strncpy(portalCfg.SSID, GT_STA_SSID, sizeof(portalCfg.SSID) - 1);
  portalCfg.SSID[sizeof(portalCfg.SSID) - 1] = '\0';

  strncpy(portalCfg.pass, GT_STA_PASS, sizeof(portalCfg.pass) - 1);
  portalCfg.pass[sizeof(portalCfg.pass) - 1] = '\0';

  portalCfg.mode = WIFI_STA;
}

void scheduleParisEvent() {
  uint32_t delayMs = random(3, 11) * 60000ul;
  parisEventTimer.setPrd(delayMs);
  parisEventTimer.restart();
}

void startParisOverlay() {
  parisOverlayActive = true;
  uint32_t duration = random(15000, 30001);
  parisDurationTimer.setPrd(duration);
  parisDurationTimer.restart();
}

void stopParisOverlay() {
  parisOverlayActive = false;
  parisDurationTimer.stop();
}

void applyParisMomentState(bool state) {
  if (state && cfg.snowflakes) {
    applySnowflakeState(false);
  }
  cfg.parisMoments = state;
  if (cfg.parisMoments) {
    if (!parisOverlayActive) scheduleParisEvent();
  } else {
    stopParisOverlay();
    parisEventTimer.stop();
  }
}

void applySnowflakeState(bool state) {
  if (state && cfg.parisMoments) {
    applyParisMomentState(false);
  }
  cfg.snowflakes = state;
  if (!cfg.snowflakes) resetSnowflakes();
}

void handleParisMoments() {
  if (!cfg.parisMoments || calibF || !cfg.power) {
    if (parisOverlayActive) stopParisOverlay();
    parisEventTimer.stop();
    return;
  }

  if (!parisOverlayActive && !parisEventTimer.state()) {
    scheduleParisEvent();
  }

  if (!parisOverlayActive && parisEventTimer.state() && parisEventTimer.ready()) {
    parisEventTimer.stop();
    startParisOverlay();
  }

  if (parisOverlayActive && parisDurationTimer.ready()) {
    stopParisOverlay();
    scheduleParisEvent();
  }
}

#ifdef DEBUG_SERIAL_GT
#define DEBUGLN(x) Serial.println(x)
#define DEBUG(x) Serial.print(x)
#else
#define DEBUGLN(x)
#define DEBUG(x)
#endif

// ================== SETUP ==================
void setup() {
#ifdef DEBUG_SERIAL_GT
  Serial.begin(115200);
  DEBUGLN();
#endif
  delay(200);
  if (BTN_TOUCH) btn.setButtonLevel(HIGH);
  startStrip();
  EEPROM.begin(2048); // с запасом!

  applyDefaultWifiConfig();
  bool firstLaunch = EEwifi.begin(0, 'a');
  bool buttonPressed = false;
  if (!firstLaunch || hasDefaultStaCredentials()) {
    buttonPressed = checkButton();
  }

  // если нет дефолтных данных или была нажата кнопка, открываем портал
  if ((firstLaunch && !hasDefaultStaCredentials()) || buttonPressed) portalRoutine();

  // создаём точку или подключаемся к AP
  if (portalCfg.mode == WIFI_AP || (portalCfg.mode == WIFI_STA && portalCfg.SSID[0] == '\0')) setupAP();
  else setupSTA();
  DEBUGLN(myIP);

  EEcfg.begin(EEwifi.nextAddr(), 'a');
  EEeff.begin(EEcfg.nextAddr(), 'a');
  EEmm.begin(EEeff.nextAddr(), (uint8_t)LED_MAX);
  EExy.begin(EEmm.nextAddr(), (uint8_t)LED_MAX);

  switchTmr.setPrd(cfg.prdCh * 60000ul);
  if (cfg.autoCh) switchTmr.restart();
  switchEff();
  applyParisMomentState(cfg.parisMoments);
  applySnowflakeState(cfg.snowflakes);
  cfg.turnOff = false;
  strip->setLeds(leds, cfg.ledAm);
  udp.begin(8888);
}

// ================== LOOP ==================
void loop() {
  button();   // опрос кнопки

  // менеджер епром
  EEcfg.tick();
  EEeff.tick();

  parsing();  // парсим udp

  // таймер принудительного показа эффектов
  if (forceTmr.ready()) {
    forceTmr.stop();
    switchEff();
  }

  // форс выключен и настало время менять эффект
  if (!forceTmr.state() && !parisOverlayActive && switchTmr.ready()) switchEff();

  // таймер выключения
  if (offTmr.ready()) {
    offTmr.stop();
    cfg.turnOff = false;
    cfg.power = false;
    strip->showLeds(0);
    EEcfg.update();
    DEBUGLN("Off tmr");
  }

  // показываем эффект, если включены
  handleParisMoments();

  if (!calibF && cfg.power) effects();
}
