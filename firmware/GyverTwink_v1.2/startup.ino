void portalRoutine() {
  // запускаем portal
  portalStart();
  DEBUGLN("Portal start");

  // ждём действий пользователя, мигаем
  while (!portalTick()) fader(CRGB::Blue);

  // если это 1 connect, 2 ap, 3 local, обновляем данные в епр
  if (portalStatus() <= 3) EEwifi.updateNow();

  DEBUG("Portal status: ");
  DEBUGLN(portalStatus());
}

void startStrip() {
  strip = &FastLED.addLeds<LED_TYPE, LED_PIN, LED_ORDER>(leds, LED_MAX).setCorrection(TypicalLEDStrip);
  strip->setLeds(leds, LED_MAX);
  strip->clearLedData();
  // выводим ргб
  leds[0] = CRGB::Red;
  leds[1] = CRGB::Green;
  leds[2] = CRGB::Blue;
  strip->showLeds(50);
}

bool checkButton() {
  uint32_t tmr = millis();
  while (millis() - tmr < 2000) {
    btn.tick();
    if (btn.state()) return true;
  }
  return false;
}

void setupAP() {
  DEBUG("AP Mode");
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(GT_AP_SSID, GT_AP_PASS);
  myIP = WiFi.softAPIP();
  server.begin();
  fadeBlink(CRGB::Magenta);
}

void setupSTA() {
  DEBUG("Connecting to AP... ");
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

#if USE_STATIC_IP
  if (WiFi.config(STATIC_IP, STATIC_GW, STATIC_SN, STATIC_DNS)) {
    DEBUG("Static IP set: "); DEBUGLN(STATIC_IP);
  } else {
    DEBUGLN("Static IP set FAILED, fallback to DHCP");
  }
#else
  // Явно включаем DHCP (на всякий случай)
  WiFi.config(0U, 0U, 0U);
#endif

  WiFi.begin(portalCfg.SSID, portalCfg.pass);

  uint32_t tmr = millis();
  while (millis() - tmr < 15000) {
    if (WiFi.status() == WL_CONNECTED) {
      fadeBlink(CRGB::Green);
      DEBUGLN("ok");
      myIP = WiFi.localIP();
      WiFiUDP kick;
      kick.begin(0);
      kick.beginPacket(STATIC_GW, 9); // любой порт
      kick.write(0);
      kick.endPacket();
      return;
    }
    fader(CRGB::Yellow);
    yield();
  }
  fadeBlink(CRGB::Red);
  DEBUGLN("fail");
  setupAP();
}
