// Time zone, NTP sync and the battery-backed RTC.
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
//
// The system clock and the RX8130 RTC both hold UTC; the TZ variable converts
// to local time on display, so changing the offset never touches the clock.
// Time is set from the RTC at boot, from NTP when Wi-Fi connects, or by hand.
#pragma once
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>
#include <WiFi.h>
#include "ui.h"

// Default UTC offset in minutes (+05:30, IST). RAM only.
constexpr int DEFAULT_UTC_OFFSET_MIN = 5 * 60 + 30;
int utcOffsetMin = DEFAULT_UTC_OFFSET_MIN;

enum TimeSource { TIME_NOT_SET, TIME_FROM_RTC, TIME_FROM_INTERNET, TIME_BY_HAND };
TimeSource timeSource = TIME_NOT_SET;

// Set from the SNTP task; the RTC write happens later in clockLoop().
volatile bool ntpJustSynced = false;
bool ntpWaiting = false;
uint32_t ntpStartMs = 0;
time_t lastSyncTime = 0;

// POSIX TZ offsets are inverted: UTC+5:30 is written "LOC-5:30".
String tzString() {
  int m = -utcOffsetMin;
  char buf[24];
  snprintf(buf, sizeof(buf), "LOC%c%d:%02d", m < 0 ? '-' : '+', abs(m) / 60, abs(m) % 60);
  return String(buf);
}

void applyTimeZone() {
  setenv("TZ", tzString().c_str(), 1);
  tzset();
}

String offsetText() {
  char buf[24];
  snprintf(buf, sizeof(buf), "%c%02d:%02d", utcOffsetMin < 0 ? '-' : '+', abs(utcOffsetMin) / 60,
           abs(utcOffsetMin) % 60);
  return String(buf);
}

void changeUtcOffset(int deltaMin) {
  utcOffsetMin = constrain(utcOffsetMin + deltaMin, -12 * 60, 14 * 60);
  applyTimeZone();
}

bool timeIsValid() {
  return time(nullptr) > 1704067200;  // 2024-01-01 00:00 UTC
}

void localNow(struct tm& t) {
  time_t now = time(nullptr);
  localtime_r(&now, &t);
}

void formatClock(char* buf, size_t len) {
  if (!timeIsValid()) {
    snprintf(buf, len, "--:--");
    return;
  }
  struct tm t;
  localNow(t);
  snprintf(buf, len, "%02d:%02d", t.tm_hour, t.tm_min);
}

void saveTimeToRtc() {
  if (!M5.Rtc.isEnabled()) return;
  time_t now = time(nullptr);
  struct tm utc;
  gmtime_r(&now, &utc);
  M5.Rtc.setDateTime(&utc);
}

void loadTimeFromRtc() {
  if (!M5.Rtc.isEnabled()) return;
  M5.Rtc.setSystemTimeFromRtc();
  applyTimeZone();  // setSystemTimeFromRtc() modifies TZ internally
  if (timeIsValid()) timeSource = TIME_FROM_RTC;
}

void ntpCallback(struct timeval* tv) {
  (void)tv;
  ntpJustSynced = true;
}

void startClockSync() {
  sntp_set_time_sync_notification_cb(ntpCallback);
  configTzTime(tzString().c_str(), "pool.ntp.org", "time.google.com");
  ntpWaiting = true;
  ntpStartMs = millis();
}

// Takes local time in the current offset.
void setTimeByHand(int year, int month, int day, int hour, int minute) {
  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_isdst = 0;
  struct timeval tv = {mktime(&t), 0};
  settimeofday(&tv, nullptr);

  // Otherwise the next periodic NTP sync would overwrite the manual time.
  esp_sntp_stop();
  ntpWaiting = false;

  timeSource = TIME_BY_HAND;
  saveTimeToRtc();
}

void clockBegin() {
  applyTimeZone();
  loadTimeFromRtc();
}

void clockLoop() {
  // Sync whenever Wi-Fi comes up (first connect or reconnect).
  static bool wasOnline = false;
  static uint32_t lastCheckMs = 0;
  if (millis() - lastCheckMs >= 500) {
    lastCheckMs = millis();
    bool online = (WiFi.status() == WL_CONNECTED);
    if (online && !wasOnline) startClockSync();
    wasOnline = online;
  }

  if (ntpJustSynced) {
    ntpJustSynced = false;
    ntpWaiting = false;
    timeSource = TIME_FROM_INTERNET;
    lastSyncTime = time(nullptr);
    saveTimeToRtc();
    Serial.println("Clock: synced from the internet");
  }
  // SNTP keeps retrying in the background; we just stop reporting "waiting".
  if (ntpWaiting && millis() - ntpStartMs > 15000) ntpWaiting = false;
}
