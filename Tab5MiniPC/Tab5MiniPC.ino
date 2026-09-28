// Tab5 Mini PC - a touchscreen launcher with small hardware demo apps for the
// M5Stack Tab5 (ESP32-P4).
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
//
// Board: "M5Tab5" from the "esp32 by Espressif Systems" package (3.3.11+).
// See README.md for the full board settings and required libraries.

#include <M5Unified.h>
#include "ui.h"
#include "audio_app.h"
#include "clock.h"
#include "keyboard.h"
#include "app_home.h"
#include "app_camera.h"
#include "app_recorder.h"
#include "app_micmeter.h"
#include "app_melody.h"
#include "app_radio.h"
#include "app_addstream.h"
#include "app_wifi.h"
#include "app_datetime.h"

// Exactly one screen is active at a time. Enter() draws it and starts its
// hardware, Loop() runs every iteration, Exit() releases the hardware.
Screen currentScreen = HOME;
Screen requestedScreen = HOME;
bool switchRequested = false;

void goTo(Screen next) {
  requestedScreen = next;
  switchRequested = true;
}

void screenEnter(Screen s) {
  switch (s) {
    case HOME:      homeEnter();      break;
    case CAMERA:    cameraEnter();    break;
    case RECORDER:  recorderEnter();  break;
    case MICMETER:  micMeterEnter();  break;
    case MELODY:    melodyEnter();    break;
    case RADIO:     radioEnter();     break;
    case ADDSTREAM: addStreamEnter(); break;
    case WIFI:      wifiEnter();      break;
    case DATETIME:  dateTimeEnter();  break;
  }
}

void screenLoop(Screen s) {
  switch (s) {
    case HOME:      homeLoop();      break;
    case CAMERA:    cameraLoop();    break;
    case RECORDER:  recorderLoop();  break;
    case MICMETER:  micMeterLoop();  break;
    case MELODY:    melodyLoop();    break;
    case RADIO:     radioLoop();     break;
    case ADDSTREAM: addStreamLoop(); break;
    case WIFI:      wifiLoop();      break;
    case DATETIME:  dateTimeLoop();  break;
  }
}

void screenExit(Screen s) {
  switch (s) {
    case HOME:      homeExit();      break;
    case CAMERA:    cameraExit();    break;
    case RECORDER:  recorderExit();  break;
    case MICMETER:  micMeterExit();  break;
    case MELODY:    melodyExit();    break;
    case RADIO:     radioExit();     break;
    case ADDSTREAM: addStreamExit(); break;
    case WIFI:      wifiExit();      break;
    case DATETIME:  dateTimeExit();  break;
  }
}

// ---------------------------------------------------------------------------
// Screen transitions
// ---------------------------------------------------------------------------
void rampBacklight(uint8_t from, uint8_t to, int totalMs) {
  const int steps = 6;
  for (int i = 1; i <= steps; i++) {
    M5.Display.setBrightness(from + (to - from) * i / steps);
    delay(totalMs / steps);
  }
}

void transitionOut() {
  uint8_t level = brightnessValue();
  switch (transitionMode) {
    case TR_DIM:  rampBacklight(level, level * 2 / 5, 60); break;
    case TR_FADE: rampBacklight(level, 0, 90); break;
    case TR_WIPE:
      for (int i = 0; i < 16; i++) {
        M5.Display.fillRect(i * SCREEN_W / 16, 0, SCREEN_W / 16, SCREEN_H, C_DARK);
        delay(9);
      }
      break;
    default: break;
  }
}

void transitionIn() {
  uint8_t level = brightnessValue();
  switch (transitionMode) {
    case TR_DIM:  rampBacklight(level * 2 / 5, level, 90); break;
    case TR_FADE: rampBacklight(0, level, 120); break;
    default: break;
  }
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
  // The camera needs a 24 MHz XCLK from LEDC. The default 40 MHz XTAL source
  // tops out at 20 MHz, so switch to the 80 MHz PLL. This must happen before
  // M5.begin() because the backlight also uses LEDC and the source is locked
  // once any channel is attached.
  ledcSetClockSource(LEDC_USE_PLL_DIV_CLK);

  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  M5.Display.setRotation(SCREEN_ROTATION);
  brightnessPercent = max(BRIGHTNESS_MIN, M5.Display.getBrightness() * 100 / 255);
  if (M5.Display.getBrightness() == 0) brightnessPercent = 60;
  applyBrightness();
  Serial.printf("Tab5 Mini PC: display %d x %d\n", (int)M5.Display.width(), (int)M5.Display.height());

  M5.Speaker.end();
  M5.Mic.end();
  audioBegin();
  applyVolume();

  clockBegin();

  screenEnter(currentScreen);

  // Started after Home is drawn: bringing up the C6 link takes a moment.
  wifiAutoConnect();
}

void loop() {
  M5.update();

  // Switch between two Loop() calls so an app is never torn down mid-loop.
  if (switchRequested) {
    switchRequested = false;
    transitionOut();
    screenExit(currentScreen);
    currentScreen = requestedScreen;
    screenEnter(currentScreen);
    transitionIn();
  }

  screenLoop(currentScreen);
  clockLoop();

  delay(1);
}
