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

#define USE_STATIC_IP 1

// имя точки в режиме AP
#define GT_AP_SSID "GyverTwink"
#define GT_AP_PASS "12345678"
// имя и пароль Wi-Fi сети для режима клиента (оставь пустыми, если не нужно)
#define GT_STA_SSID ""
#define GT_STA_PASS ""
#define DEBUG_SERIAL_GT   // раскомментируй, чтобы включить отладку

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

IPAddress STATIC_IP (192,168,178,88);
IPAddress STATIC_GW (192,168,178,1);
IPAddress STATIC_SN (255,255,255,0);
IPAddress STATIC_DNS(192,168,178,1);  // можно 8,8,8,8

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
  cfg.parisMoments = state;
  if (cfg.parisMoments) {
    if (!parisOverlayActive) scheduleParisEvent();
  } else {
    stopParisOverlay();
    parisEventTimer.stop();
  }
}

void applySnowflakeState(bool state) {
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

  // EEPROM сначала
  EEPROM.begin(2048);

  // 1) Загружаем portalCfg из EEPROM
  bool firstLaunch = EEwifi.begin(0, 'a');

  // 2) Если в EEPROM пусто и есть зашитые дефолтные STA-учётки — применяем их и сохраняем
  if ((portalCfg.SSID[0] == '\0') && hasDefaultStaCredentials()) {
    applyDefaultWifiConfig();   // кладёт GT_STA_SSID/GT_STA_PASS в portalCfg и ставит WIFI_STA
    EEwifi.updateNow();         // сразу сохраняем в EEPROM
    DEBUGLN(F("Applied default STA creds to EEPROM"));
  }

  // 3) Кнопка для входа в портал (если не самый первый старт, либо дефолт есть)
  bool buttonPressed = false;
  if (!firstLaunch || hasDefaultStaCredentials()) {
    buttonPressed = checkButton();
  }

  // 4) Портал настройки при первом старте без дефолтов или по кнопке
  if ((firstLaunch && !hasDefaultStaCredentials()) || buttonPressed) {
    portalRoutine();            // после выхода portalCfg уже обновлён
    DEBUGLN(F("Portal finished"));
  }

  // 5) Запускаем Wi-Fi в нужном режиме
  if (portalCfg.mode == WIFI_STA && portalCfg.SSID[0] != '\0') {
    setupSTA();                 // подключение к роутеру
  } else {
    setupAP();                  // точка доступа
  }

  DEBUG(F("My IP: "));
  DEBUGLN(myIP);

  // === остальные менеджеры/инициализация ===
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

  udp.begin(8888);              // порт UDP для приложения
}

// ================== LOOP ==================
void loop() {
  button();   // опрос кнопки

  // менеджер епром
  EEcfg.tick();
  EEeff.tick();

  parsing();  // парсим udp

  protoTick(); 

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


void udpService() {
  static char buf[64];
  int len = udp.parsePacket();
  if (len > 0) {
    len = udp.read(buf, min(len, (int)sizeof(buf)));
    // Ответ приложению на его порт
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write("GT");                 // короткий «я тут»
    // можно добавить полезное: версия/LED_MAX/текущий IP
    // udp.printf("GT,%u,%u,%u.%u.%u.%u", LED_MAX, cfg.ledAm, myIP[0], myIP[1], myIP[2], myIP[3]);
    udp.endPacket();
  }
}

IPAddress calcBroadcast(IPAddress ip, IPAddress mask) {
  uint32_t ip32 = (uint32_t)ip;
  uint32_t m32  = (uint32_t)mask;
  return IPAddress( (ip32 & m32) | (~m32) );
}

// Раз в секунду отправляем «маяк» по broadcast на 8888
void udpAnnounceTick() {
  static uint32_t tmr = 0;
  if (millis() - tmr < 1000) return;
  tmr = millis();

  IPAddress bcast = calcBroadcast(WiFi.localIP(), WiFi.subnetMask()); // для Fritz: 192.168.178.255
  udp.beginPacket(bcast, 8888);
  udp.write("GYVERTWINK");   // произвольная метка
  udp.endPacket();
}

static uint8_t rx[64];

void sendCfgPacket() {
  // формат, который ждёт приложение в case 1:
  // [ 'G','T', 1, ledAm/100, ledAm%100, power, bright, auto, rnd, prd, offT, offSec, paris, snow ]
  uint16_t am = cfg.ledAm;
  uint8_t pkt[14] = {
    'G','T', 1,
    (uint8_t)(am/100), (uint8_t)(am%100),
    (uint8_t)cfg.power,
    (uint8_t)cfg.bright,
    (uint8_t)cfg.autoCh,
    (uint8_t)cfg.rndCh,
    (uint8_t)cfg.prdCh,
    (uint8_t)cfg.turnOff,
    (uint8_t)cfg.offTmr,
    (uint8_t)cfg.parisMoments,
    (uint8_t)cfg.snowflakes
  };
  udp.beginPacket(udp.remoteIP(), udp.remotePort());
  udp.write(pkt, sizeof(pkt));
  udp.endPacket();
}

void sendSearchReply() {
  // формат, который ждёт приложение в case 0:
  // [ 'G','T', 0, lastOctet ]
  uint8_t last = WiFi.localIP()[3];
  uint8_t pkt[4] = { 'G','T', 0, last };
  udp.beginPacket(udp.remoteIP(), udp.remotePort());
  udp.write(pkt, sizeof(pkt));
  udp.endPacket();
}

void sendEffectState() {
  // для вкладки эффектов (case 4 в приложении):
  // [ 'G','T', 4, fav, scale, speed ]
  uint8_t pkt[6] = { 'G','T', 4, (uint8_t)effs[curEff].fav,
                              (uint8_t)effs[curEff].scale,
                              (uint8_t)effs[curEff].speed };
  udp.beginPacket(udp.remoteIP(), udp.remotePort());
  udp.write(pkt, sizeof(pkt));
  udp.endPacket();
}

void protoTick() {
  int len = udp.parsePacket();
  if (len <= 0) return;

  len = udp.read(rx, min(len, (int)sizeof(rx)));
  if (len < 3) return;
  if (rx[0] != 'G' || rx[1] != 'T') return;

  uint8_t cmd = rx[2];

  switch (cmd) {
    case 0:   // поиск
      sendSearchReply();
      break;

    case 1:   // запрос конфигурации
      sendCfgPacket();
      break;

    case 2:   // управление (в приложении sendData({2,...}))
      // тут уже у тебя есть обработка (яркость, power, и т.д.)
      // после применения настроек можно при желании вернуть актуальную конфигурацию:
      // sendCfgPacket();
      break;

    case 3:   // калибровка — у тебя уже реализовано
      break;

    case 4:   // эффекты
      // у тебя уже есть парсинг. Для обновления UI приложения можно вернуть состояние:
      sendEffectState();
      break;
  }
}