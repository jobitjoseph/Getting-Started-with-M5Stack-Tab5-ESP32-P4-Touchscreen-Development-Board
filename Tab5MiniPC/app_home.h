// Home screen: status bar, app tiles, brightness and transition panels.
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
#pragma once
#include <WiFi.h>
#include "ui.h"
#include "clock.h"

// ---------------------------------------------------------------------------
// Status bar (drawn into a PSRAM canvas and pushed once per second)
// ---------------------------------------------------------------------------
constexpr int STATUS_H = 64;

M5Canvas statusBar(&M5.Display);
Button wifiArea;
Button clockArea;
Button sunArea;
Button fxArea;
uint32_t lastStatusDraw = 0;

void drawStatusBar() {
  auto& s = statusBar;
  s.fillSprite(C_DARK);
  s.drawFastHLine(0, STATUS_H - 1, SCREEN_W, C_BORDER);

  s.setFont(FONT_LABEL);
  s.setTextColor(C_TEXT);
  s.setTextDatum(middle_left);
  s.drawString("TAB5 MINI PC", 32, STATUS_H / 2);

  // Laid out right to left: [wifi + name] [layers] [sun] [battery + %] [clock]
  char clock[16];
  formatClock(clock, sizeof(clock));

  int level = M5.Power.getBatteryLevel();  // negative if unknown
  char batt[16];
  if (level >= 0) snprintf(batt, sizeof(batt), "%d%%", level);
  else snprintf(batt, sizeof(batt), "--%%");

  bool online = (WiFi.status() == WL_CONNECTED);
  String name = online ? WiFi.SSID() : String("No Wi-Fi");
  if (name.length() > 16) name = name.substring(0, 15) + "~";

  s.setFont(FONT_MONO);
  int cy = STATUS_H / 2;
  int x = SCREEN_W - 32;

  s.setTextDatum(middle_right);
  s.drawString(clock, x, cy);
  int clockLeft = x - s.textWidth(clock);
  clockArea = {clockLeft - 16, 0, SCREEN_W - clockLeft + 16, STATUS_H, "", C_DARK, C_TEXT};
  x -= s.textWidth(clock) + 32;

  s.drawString(batt, x, cy);
  x -= s.textWidth(batt) + 10 + 28;

  iconDrawOn(s);
  iconBattery(x, cy - 14, 28, max(level, 0), C_TEXT, C_DARK);
  x -= 32;

  iconSun(x - 26, cy - 13, 26, C_TEXT, C_DARK);
  sunArea = {x - 26 - 16, 0, 26 + 32, STATUS_H, "", C_DARK, C_TEXT};
  x -= 26 + 32;

  iconLayers(x - 26, cy - 13, 26, C_TEXT, C_DARK);
  fxArea = {x - 26 - 16, 0, 26 + 32, STATUS_H, "", C_DARK, C_TEXT};
  x -= 26 + 32;

  s.setTextColor(online ? C_TEXT : C_TEXT2);
  s.drawString(name, x, cy);
  int iconX = x - s.textWidth(name) - 10 - 26;
  iconWifi(iconX, cy - 13, 26, 2.2f, online ? C_BLUE : C_TEXT2, C_DARK);
  iconDrawOn(M5.Display);

  wifiArea = {iconX - 8, 0, (x - iconX) + 16, STATUS_H, "", C_DARK, C_TEXT};

  s.pushSprite(0, 0);
  lastStatusDraw = millis();
}

// ---------------------------------------------------------------------------
// Tiles
// ---------------------------------------------------------------------------
struct Tile {
  Screen target;
  const char* title;
  void (*icon)(int x, int y, int size, uint16_t fg, uint16_t bg);
  uint16_t iconColor;
};

void iconWifiTile(int x, int y, int size, uint16_t fg, uint16_t bg) { iconWifi(x, y, size, 1.6f, fg, bg); }

const Tile tiles[6] = {
  {CAMERA,   "Camera",         iconCamera,   C_AMBER},
  {RECORDER, "Recorder",       iconMic,      C_AMBER},
  {MICMETER, "Mic Meter",      iconBars,     C_AMBER},
  {MELODY,   "Melody",         iconMusicNote, C_AMBER},
  {RADIO,    "Internet Radio", iconRadio,    C_AMBER},
  {WIFI,     "Wi-Fi",          iconWifiTile, C_BLUE},
};

constexpr int GRID_X = 40, GRID_TOP = STATUS_H + 36, GRID_GAP = 28;
constexpr int TILE_W = (SCREEN_W - 2 * 40 - 2 * GRID_GAP) / 3;
constexpr int TILE_H = (SCREEN_H - GRID_TOP - 40 - GRID_GAP) / 2;

Button tileRect(int i) {
  int col = i % 3, row = i / 3;
  return {GRID_X + col * (TILE_W + GRID_GAP), GRID_TOP + row * (TILE_H + GRID_GAP), TILE_W, TILE_H,
          tiles[i].title, C_PANEL, C_TEXT};
}

void drawTile(int i) {
  auto& d = M5.Display;
  Button r = tileRect(i);
  d.fillSmoothRoundRect(r.x, r.y, r.w, r.h, R_TILE, C_BORDER);
  d.fillSmoothRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, R_TILE - 1, C_PANEL);

  int cx = r.x + r.w / 2;
  int top = r.y + (r.h - 76 - 16 - 30) / 2;
  tiles[i].icon(cx - 38, top, 76, tiles[i].iconColor, C_PANEL);

  d.setTextDatum(top_center);
  d.setFont(FONT_TITLE);
  d.setTextColor(C_TEXT);
  d.drawString(tiles[i].title, cx, top + 76 + 16);
}

// ---------------------------------------------------------------------------
// Drop-down panels (sun = brightness, layers = transition effect)
// ---------------------------------------------------------------------------
enum HomePanel { PANEL_NONE, PANEL_BRIGHTNESS, PANEL_TRANSITION };
HomePanel openPanel = PANEL_NONE;

constexpr int PANEL_X = 608, PANEL_Y = STATUS_H + 12, PANEL_W = 640, PANEL_H = 124;
const SliderRow brightnessRow = {PANEL_X + 24, PANEL_Y + 26, PANEL_W - 48, 72, C_PANEL,
                                 "Screen brightness", &brightnessPercent, BRIGHTNESS_MIN};

Button transitionButton(int i) {
  const int w = 138, gap = 12;
  return {PANEL_X + 26 + i * (w + gap), PANEL_Y + 42, w, 64, TRANSITION_NAMES[i], C_BG, C_TEXT};
}

void drawTransitionChoices() {
  auto& d = M5.Display;
  for (int i = 0; i < TR_COUNT; i++) {
    Button b = transitionButton(i);
    bool sel = (i == transitionMode);
    d.fillSmoothRoundRect(b.x, b.y, b.w, b.h, R_SMALL, sel ? C_AMBER : C_BORDER2);
    int t = sel ? 3 : 1;
    d.fillSmoothRoundRect(b.x + t, b.y + t, b.w - 2 * t, b.h - 2 * t, R_SMALL - t, C_BG);
    d.setFont(FONT_LABEL);
    d.setTextColor(C_TEXT);
    d.setTextDatum(middle_center);
    d.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2);
  }
}

void drawPanel(HomePanel which) {
  auto& d = M5.Display;
  d.fillSmoothRoundRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, R_BTN, C_BORDER2);
  d.fillSmoothRoundRect(PANEL_X + 1, PANEL_Y + 1, PANEL_W - 2, PANEL_H - 2, R_BTN - 1, C_PANEL);
  if (which == PANEL_BRIGHTNESS) {
    sliderRowDraw(brightnessRow);
  } else {
    d.setFont(FONT_SMALL);
    d.setTextColor(C_TEXT2);
    d.setTextDatum(top_left);
    d.drawString("Screen transition", PANEL_X + 26, PANEL_Y + 12);
    drawTransitionChoices();
  }
  openPanel = which;
}

void closePanel() {
  openPanel = PANEL_NONE;
  M5.Display.fillRect(0, STATUS_H, SCREEN_W, SCREEN_H - STATUS_H, C_BG);
  for (int i = 0; i < 6; i++) drawTile(i);
}

// A tap outside the panel (or on its own icon) closes it.
void panelLoop() {
  if (openPanel == PANEL_BRIGHTNESS && sliderRowUpdate(brightnessRow)) applyBrightness();

  int x, y;
  if (!touchPressed(x, y)) return;
  Button panel = {PANEL_X, PANEL_Y, PANEL_W, PANEL_H, "", C_PANEL, C_TEXT};
  bool onOwnIcon = (openPanel == PANEL_BRIGHTNESS) ? hit(sunArea, x, y) : hit(fxArea, x, y);
  if (onOwnIcon || !hit(panel, x, y)) {
    closePanel();
    return;
  }
  if (openPanel == PANEL_TRANSITION) {
    for (int i = 0; i < TR_COUNT; i++) {
      if (hit(transitionButton(i), x, y)) {
        transitionMode = (Transition)i;
        drawTransitionChoices();
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Screen functions
// ---------------------------------------------------------------------------
void homeEnter() {
  M5.Display.fillScreen(C_BG);
  if (statusBar.width() == 0) {
    statusBar.setPsram(true);
    statusBar.createSprite(SCREEN_W, STATUS_H);
  }
  openPanel = PANEL_NONE;
  drawStatusBar();
  for (int i = 0; i < 6; i++) drawTile(i);
}

void homeLoop() {
  if (millis() - lastStatusDraw >= 1000) drawStatusBar();

  if (openPanel != PANEL_NONE) {
    panelLoop();
    return;
  }

  int x, y;
  if (!touchPressed(x, y)) return;

  if (hit(sunArea, x, y)) {
    drawPanel(PANEL_BRIGHTNESS);
    return;
  }
  if (hit(fxArea, x, y)) {
    drawPanel(PANEL_TRANSITION);
    return;
  }
  if (hit(wifiArea, x, y)) {
    goTo(WIFI);
    return;
  }
  if (hit(clockArea, x, y)) {
    goTo(DATETIME);
    return;
  }
  for (int i = 0; i < 6; i++) {
    if (hit(tileRect(i), x, y)) {
      goTo(tiles[i].target);
      return;
    }
  }
}

void homeExit() {}
