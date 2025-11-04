static Timer effTmr(30);
static uint16_t countP = 0;
static byte prevShownEffect = 255;
static byte fadeCount = 0;
static uint8_t parisSpark[LED_MAX];
static uint8_t snowflakeState[LED_MAX];
static uint8_t snowflakeLevel[LED_MAX];
static uint8_t snowflakeHold[LED_MAX];

void resetSnowflakes() {
  for (int i = 0; i < LED_MAX; i++) {
    snowflakeState[i] = 0;
    snowflakeLevel[i] = 0;
    snowflakeHold[i] = 0;
  }
}

static void renderStandardEffect(byte thisEffect, byte scale, byte speed, byte fade) {
  byte curPal = thisEffect;
  if (curPal >= ACTIVE_PALETTES) curPal -= ACTIVE_PALETTES;

  for (int i = 0; i < cfg.ledAm; i++) {
    byte idx;

    if (thisEffect < ACTIVE_PALETTES) {
      idx = countP + ((mm.w * xy[i][0] / mm.h) + xy[i][1]) * scale / 100;
    } else {
      idx = inoise8(xy[i][0] * scale / 10, xy[i][1] * scale / 10, countP);
    }

    CRGB color = ColorFromPalette(paletteArr[curPal], idx, 255, LINEARBLEND);
    if (fade) leds[i] = blend(leds[i], color, 40);
    else leds[i] = color;
  }

  countP += (speed - 128) / 10;
}

static void renderParisEffect(byte scale, byte speed, bool intense, byte fade) {
  if (cfg.ledAm == 0) return;

  uint8_t fadeStep = constrain(map(speed, 0, 255, 1, 12), 1, 20);
  if (intense && fadeStep < 6) fadeStep = 6;

  for (int i = 0; i < cfg.ledAm; i++) {
    if (parisSpark[i] > fadeStep) parisSpark[i] -= fadeStep;
    else parisSpark[i] = 0;
  }

  uint8_t base = map(scale, 0, 255, 10, 80);
  uint8_t densityMax = cfg.ledAm / 12;
  if (densityMax < 1) densityMax = 1;
  uint16_t spawn = map(scale, 0, 255, 1, densityMax);
  if (spawn < 1) spawn = 1;
  if (intense) {
    uint16_t tempBase = base + 50;
    if (tempBase > 180) tempBase = 180;
    base = tempBase;
    uint16_t intenseLimit = cfg.ledAm / 2;
    if (intenseLimit < 1) intenseLimit = 1;
    uint16_t newSpawn = spawn * 2 + 3;
    if (newSpawn > intenseLimit) newSpawn = intenseLimit;
    spawn = newSpawn;
  }

  if (spawn > cfg.ledAm) spawn = cfg.ledAm;

  for (uint16_t n = 0; n < spawn; n++) {
    int idx = random16(cfg.ledAm);
    parisSpark[idx] = qadd8(parisSpark[idx], random8(160, 255));
  }

  for (int i = 0; i < cfg.ledAm; i++) {
    uint8_t sparkle = parisSpark[i];
    uint8_t brightness = qadd8(base, sparkle);
    CRGB color = ColorFromPalette(ParisLights_p, 255 - sparkle, brightness, LINEARBLEND);
    if (intense && sparkle > 200 && random8() < 96) {
      color = CRGB::White;
    }
    if (fade) leds[i] = blend(leds[i], color, 40);
    else leds[i] = color;
  }
}

static void updateSnowflakesOverlay() {
  if (cfg.ledAm == 0) return;

  uint8_t active = 0;

  for (int i = 0; i < cfg.ledAm; i++) {
    switch (snowflakeState[i]) {
      case 0:
        break;
      case 1:
        snowflakeLevel[i] = qadd8(snowflakeLevel[i], 40);
        if (snowflakeLevel[i] >= 240) {
          snowflakeLevel[i] = 255;
          snowflakeState[i] = 2;
        }
        break;
      case 2:
        if (snowflakeHold[i]) snowflakeHold[i]--;
        else snowflakeState[i] = 3;
        break;
      case 3:
        if (snowflakeLevel[i] > 25) snowflakeLevel[i] = qsub8(snowflakeLevel[i], 35);
        else {
          snowflakeLevel[i] = 0;
          snowflakeState[i] = 0;
          snowflakeHold[i] = 0;
        }
        break;
    }

    if (snowflakeState[i]) active++;
  }

  if (cfg.snowflakes) {
    uint8_t target = cfg.ledAm < 25 ? cfg.ledAm : 25;
    if (target > active) {
      uint8_t deficit = target - active;
      uint8_t spawnCount = deficit < 4 ? deficit : 4;
      uint16_t attempts = 0;
      while (spawnCount && attempts < cfg.ledAm * 4) {
        int idx = random16(cfg.ledAm);
        if (snowflakeState[idx] == 0 && snowflakeLevel[idx] == 0) {
          snowflakeState[idx] = 1;
          snowflakeLevel[idx] = 0;
          snowflakeHold[idx] = random8(30, 50);
          spawnCount--;
        }
        attempts++;
      }
    }
  } else {
    for (int i = 0; i < cfg.ledAm; i++) {
      if (snowflakeState[i] == 0 && snowflakeLevel[i] > 0) {
        if (snowflakeLevel[i] > 20) snowflakeLevel[i] = qsub8(snowflakeLevel[i], 20);
        else snowflakeLevel[i] = 0;
      }
    }
  }

  for (int i = 0; i < cfg.ledAm; i++) {
    if (snowflakeLevel[i]) {
      byte level = snowflakeLevel[i];
      leds[i].r = level;
      leds[i].g = level;
      leds[i].b = level;
    }
  }
}

void effects() {
  if (!effTmr.ready()) return;

  byte thisEffect;
  if (parisOverlayActive) thisEffect = PARIS_EFFECT_INDEX;
  else if (forceTmr.state()) thisEffect = forceEff;
  else thisEffect = curEff;

  if (prevShownEffect != thisEffect) {
    prevShownEffect = thisEffect;
    fadeCount = 25;
  }

  byte scale = effs[thisEffect].scale;
  byte speed = effs[thisEffect].speed;

  if (thisEffect == PARIS_EFFECT_INDEX) renderParisEffect(scale, speed, parisOverlayActive, fadeCount);
  else renderStandardEffect(thisEffect, scale, speed, fadeCount);

  updateSnowflakesOverlay();

  if (fadeCount) fadeCount--;

  strip->showLeds(cfg.bright);
}
