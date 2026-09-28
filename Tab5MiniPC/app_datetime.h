// Date & Time: live clock, UTC offset, NTP sync and manual date/time entry.
// Opened by tapping the clock in the Home status bar.
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
#pragma once
#include <WiFi.h>
#include "ui.h"
#include "clock.h"

constexpr int DL_X = 32, DL_W = 520, DR_X = 584, DR_W = 664, DCARD_Y = 116, DCARD_H = 576;
constexpr int DL_IN = DL_X + 32, DR_IN = DR_X + 32;
Button dtOffMinusBtn = {DL_IN, 416, 72, 72, "-", C_BG, C_TEXT};
Button dtOffPlusBtn = {DL_IN + 384, 416, 72, 72, "+", C_BG, C_TEXT};
Button dtSyncBtn = {DL_IN, 540, 456, 72, "Sync from internet", C_BLUE, C_BG};
Button dtSetBtn = {DR_IN, 472, 600, 80, "Set date & time", C_AMBER, C_BG};

enum { SP_DAY, SP_MONTH, SP_YEAR, SP_HOUR, SP_MIN, SP_COUNT };
const char* const SP_LABELS[SP_COUNT] = {"Day", "Month", "Year", "Hour", "Min"};
const int SP_X[SP_COUNT] = {DR_IN, DR_IN + 115, DR_IN + 230, DR_IN + 385, DR_IN + 500};
const int SP_W[SP_COUNT] = {100, 100, 140, 100, 100};
constexpr int SP_UP_Y = 216, SP_VAL_Y = 288, SP_DOWN_Y = 368, SP_BTN_H = 64, SP_VAL_H = 72;
int spValue[SP_COUNT];

const char* const MONTHS[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char* const WEEKDAYS[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

bool dtSyncing = false;
time_t dtSyncSeenTime = 0;  // lastSyncTime when Sync was pressed
int dtLastSecond = -1;
TimeSource dtShownSource = TIME_NOT_SET;

int daysInMonth(int year, int month) {
  static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return (month == 2 && leap) ? 29 : days[month - 1];
}

// ---------------------------------------------------------------------------
// Left card
// ---------------------------------------------------------------------------
const char* sourceText() {
  switch (timeSource) {
    case TIME_FROM_INTERNET: return "Set from the internet";
    case TIME_BY_HAND:       return "Set by hand";
    case TIME_FROM_RTC:      return "Kept by the RTC while off";
    default:                 return "Not set yet";
  }
}

void drawLiveTime() {
  char timeBuf[12], dateBuf[24];
  if (timeIsValid()) {
    struct tm t;
    localNow(t);
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    snprintf(dateBuf, sizeof(dateBuf), "%s %d %s %d", WEEKDAYS[t.tm_wday], t.tm_mday, MONTHS[t.tm_mon],
             t.tm_year + 1900);
  } else {
    snprintf(timeBuf, sizeof(timeBuf), "--:--:--");
    snprintf(dateBuf, sizeof(dateBuf), "Date not set");
  }
  drawTextBox(DL_IN, 180, 456, 84, timeBuf, &fonts::Font8, C_TEXT, C_PANEL);
  drawTextBox(DL_IN, 272, 456, 40, dateBuf, FONT_TITLE, C_TEXT, C_PANEL);
  drawTextBox(DL_IN, 314, 456, 32, sourceText(), FONT_SMALL, C_TEXT2, C_PANEL);
  dtShownSource = timeSource;
}

void drawOffset() {
  String text = "UTC" + offsetText();
  drawTextBox(DL_IN + 84, 416, 288, 72, text.c_str(), FONT_TITLE, C_TEXT, C_PANEL, middle_center);
}

void drawSyncStatus(const char* text, uint16_t color = C_TEXT2) {
  drawTextBox(DL_IN, 620, 456, 40, text, FONT_SMALL, color, C_PANEL);
}

// ---------------------------------------------------------------------------
// Right card
// ---------------------------------------------------------------------------
void drawSpinnerValue(int i) {
  char buf[8];
  if (i == SP_MONTH) snprintf(buf, sizeof(buf), "%s", MONTHS[spValue[i] - 1]);
  else if (i == SP_YEAR) snprintf(buf, sizeof(buf), "%d", spValue[i]);
  else snprintf(buf, sizeof(buf), "%02d", spValue[i]);
  auto& d = M5.Display;
  d.fillSmoothRoundRect(SP_X[i], SP_VAL_Y, SP_W[i], SP_VAL_H, 12, C_BORDER2);
  d.fillSmoothRoundRect(SP_X[i] + 1, SP_VAL_Y + 1, SP_W[i] - 2, SP_VAL_H - 2, 11, C_BG);
  d.setFont(FONT_TITLE);
  d.setTextColor(C_TEXT);
  d.setTextDatum(middle_center);
  d.drawString(buf, SP_X[i] + SP_W[i] / 2, SP_VAL_Y + SP_VAL_H / 2);
}

Button spinnerUp(int i) { return {SP_X[i], SP_UP_Y, SP_W[i], SP_BTN_H, "", C_BG, C_TEXT}; }
Button spinnerDown(int i) { return {SP_X[i], SP_DOWN_Y, SP_W[i], SP_BTN_H, "", C_BG, C_TEXT}; }

void drawSpinner(int i) {
  auto& d = M5.Display;
  d.setFont(FONT_SMALL);
  d.setTextColor(C_TEXT2);
  d.setTextDatum(middle_center);
  d.drawString(SP_LABELS[i], SP_X[i] + SP_W[i] / 2, 192);
  for (int k = 0; k < 2; k++) {
    Button b = k ? spinnerDown(i) : spinnerUp(i);
    drawButton(b, FONT_LABEL, R_SMALL);
    int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    if (k == 0) d.fillTriangle(cx - 14, cy + 8, cx + 14, cy + 8, cx, cy - 10, C_TEXT);
    else d.fillTriangle(cx - 14, cy - 8, cx + 14, cy - 8, cx, cy + 10, C_TEXT);
  }
  drawSpinnerValue(i);
}

// Wraps around (e.g. minute 59 -> 00).
void stepSpinner(int i, int dir) {
  int lo = 0, hi = 0;
  switch (i) {
    case SP_DAY:   lo = 1; hi = daysInMonth(spValue[SP_YEAR], spValue[SP_MONTH]); break;
    case SP_MONTH: lo = 1; hi = 12; break;
    case SP_YEAR:  lo = 2024; hi = 2099; break;
    case SP_HOUR:  lo = 0; hi = 23; break;
    case SP_MIN:   lo = 0; hi = 59; break;
  }
  int v = spValue[i] + dir;
  if (v > hi) v = lo;
  if (v < lo) v = hi;
  spValue[i] = v;
  drawSpinnerValue(i);

  // Clamp the day after a month/year change.
  int maxDay = daysInMonth(spValue[SP_YEAR], spValue[SP_MONTH]);
  if (spValue[SP_DAY] > maxDay) {
    spValue[SP_DAY] = maxDay;
    drawSpinnerValue(SP_DAY);
  }
}

void drawSetMessage(const char* text, uint16_t color = C_TEXT2) {
  drawTextBox(DR_IN, 572, 600, 40, text, FONT_SMALL, color, C_PANEL);
}

// ---------------------------------------------------------------------------
// Screen functions
// ---------------------------------------------------------------------------
void dateTimeEnter() {
  drawHeader("Date & Time");
  auto& d = M5.Display;
  d.fillSmoothRoundRect(DL_X, DCARD_Y, DL_W, DCARD_H, R_BTN, C_PANEL);
  d.fillSmoothRoundRect(DR_X, DCARD_Y, DR_W, DCARD_H, R_BTN, C_PANEL);

  d.setFont(FONT_LABEL);
  d.setTextColor(C_TEXT2);
  d.setTextDatum(middle_left);
  d.drawString("NOW", DL_IN, 148);
  d.drawString("Time offset", DL_IN, 392);
  drawLiveTime();
  drawButton(dtOffMinusBtn, FONT_TITLE, 16);
  drawButton(dtOffPlusBtn, FONT_TITLE, 16);
  drawOffset();
  drawTextBox(DL_IN, 494, 456, 32, "Default UTC+05:30 (India)", FONT_SMALL, C_TEXT2, C_PANEL);
  drawButton(dtSyncBtn, FONT_TITLE, 16);
  dtSyncing = false;
  drawSyncStatus(WiFi.status() == WL_CONNECTED ? "Uses pool.ntp.org" : "Needs Wi-Fi");

  d.setFont(FONT_LABEL);
  d.setTextColor(C_TEXT2);
  d.setTextDatum(middle_left);
  d.drawString("SET BY HAND", DR_IN, 148);
  if (timeIsValid()) {
    struct tm t;
    localNow(t);
    spValue[SP_DAY] = t.tm_mday;
    spValue[SP_MONTH] = t.tm_mon + 1;
    spValue[SP_YEAR] = t.tm_year + 1900;
    spValue[SP_HOUR] = t.tm_hour;
    spValue[SP_MIN] = t.tm_min;
  } else {
    spValue[SP_DAY] = 1;
    spValue[SP_MONTH] = 1;
    spValue[SP_YEAR] = 2026;
    spValue[SP_HOUR] = 12;
    spValue[SP_MIN] = 0;
  }
  for (int i = 0; i < SP_COUNT; i++) drawSpinner(i);
  drawButton(dtSetBtn, FONT_TITLE, 16);
  drawSetMessage("Uses the time offset on the left.");
  drawTextBox(DR_IN, 620, 600, 40, "The Tab5's RTC keeps the time while it is off.", FONT_SMALL, C_TEXT2,
              C_PANEL);
  dtLastSecond = -1;
}

void dateTimeLoop() {
  time_t now = time(nullptr);
  if ((int)(now % 60) != dtLastSecond || timeSource != dtShownSource) {
    dtLastSecond = now % 60;
    drawLiveTime();
  }

  if (dtSyncing && lastSyncTime != dtSyncSeenTime) {
    dtSyncing = false;
    struct tm t;
    localNow(t);
    char buf[40];
    snprintf(buf, sizeof(buf), "Synced at %02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    drawSyncStatus(buf, C_BLUE);
  } else if (dtSyncing && !ntpWaiting) {
    dtSyncing = false;
    drawSyncStatus("No answer from the time server", C_AMBER);
  }

  int x, y;
  if (!touchPressed(x, y)) return;
  if (hit(backBtn, x, y)) { goTo(HOME); return; }

  if (hit(dtOffMinusBtn, x, y) || hit(dtOffPlusBtn, x, y)) {
    changeUtcOffset(hit(dtOffPlusBtn, x, y) ? 15 : -15);
    drawOffset();
    drawLiveTime();
    return;
  }
  if (hit(dtSyncBtn, x, y)) {
    if (WiFi.status() != WL_CONNECTED) {
      drawSyncStatus("Connect to Wi-Fi first", C_AMBER);
    } else {
      dtSyncSeenTime = lastSyncTime;
      dtSyncing = true;
      startClockSync();
      drawSyncStatus("Syncing...");
    }
    return;
  }
  for (int i = 0; i < SP_COUNT; i++) {
    if (hit(spinnerUp(i), x, y)) { stepSpinner(i, +1); return; }
    if (hit(spinnerDown(i), x, y)) { stepSpinner(i, -1); return; }
  }
  if (hit(dtSetBtn, x, y)) {
    setTimeByHand(spValue[SP_YEAR], spValue[SP_MONTH], spValue[SP_DAY], spValue[SP_HOUR], spValue[SP_MIN]);
    dtSyncing = false;
    drawLiveTime();
    drawSetMessage("Clock set.", C_BLUE);
    drawSyncStatus("Internet sync paused (set by hand)");
  }
}

void dateTimeExit() {}
