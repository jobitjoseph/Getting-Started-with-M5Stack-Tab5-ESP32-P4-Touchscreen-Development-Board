// Mic Meter: live RMS level (dBFS) for both microphones.
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
//
// The ES7210 runs in stereo (left = MIC 1, right = MIC 2). Two 32 ms blocks
// are double-buffered: M5Unified fills one while the other is measured.
#pragma once
#include "ui.h"
#include "audio_app.h"

constexpr uint32_t MM_RATE = 16000;
constexpr size_t MM_FRAMES = 512;
int16_t mmBuf[2][MM_FRAMES * 2];  // interleaved L,R
int mmQueued = 0;
int mmNextToQueue = 0, mmNextToRead = 0;

float mmLevel[2] = {-60, -60};
float mmPeak[2] = {-60, -60};
uint32_t mmPeakMs[2] = {0, 0};
int mmShownDb[2] = {999, 999};

constexpr int MM_LABEL_X = 72, MM_BAR_X = 220, MM_BAR_W = 810, MM_BAR_H = 72;
constexpr int MM_DB_X = 1058, MM_DB_W = 150;
const int MM_ROW_Y[2] = {224, 352};
M5Canvas mmCanvas(&M5.Display);  // shared by both rows

float dbToX(float db) { return (db + 60.0f) / 60.0f * MM_BAR_W; }

void drawMeterBar(int ch) {
  auto& c = mmCanvas;
  c.fillSprite(C_BG);
  c.fillSmoothRoundRect(0, 0, MM_BAR_W, MM_BAR_H, 14, C_DARK);
  int fillW = (int)dbToX(mmLevel[ch]);
  if (fillW > 0) {
    // Rounded left end, square right end.
    c.fillSmoothRoundRect(0, 0, max(fillW, 28), MM_BAR_H, 14, C_BLUE);
    if (fillW > 28) c.fillRect(14, 0, fillW - 14, MM_BAR_H, C_BLUE);
  }
  int px = constrain((int)dbToX(mmPeak[ch]), 0, MM_BAR_W - 6);
  if (mmPeak[ch] > -59.5f) c.fillRect(px, 0, 6, MM_BAR_H, C_AMBER);
  c.pushSprite(MM_BAR_X, MM_ROW_Y[ch]);

  int db = (int)lroundf(mmLevel[ch]);
  if (db != mmShownDb[ch]) {
    mmShownDb[ch] = db;
    char buf[12];
    snprintf(buf, sizeof(buf), "%d dB", db);
    drawTextBox(MM_DB_X, MM_ROW_Y[ch], MM_DB_W, MM_BAR_H, buf, &fonts::FreeMonoBold18pt7b, C_TEXT, C_BG,
                middle_right);
  }
}

// RMS of one channel in dBFS, clamped to -60..0.
float channelDb(const int16_t* block, int ch) {
  double sum = 0;
  for (size_t i = 0; i < MM_FRAMES; i++) {
    float s = block[i * 2 + ch];
    sum += s * s;
  }
  float rms = sqrt(sum / MM_FRAMES) / 32768.0f;
  return constrain(20.0f * log10f(rms + 1e-9f), -60.0f, 0.0f);
}

void micMeterUpdate(const int16_t* block) {
  uint32_t now = millis();
  for (int ch = 0; ch < 2; ch++) {
    float db = channelDb(block, ch);
    // Fast attack, slower release.
    mmLevel[ch] = (db > mmLevel[ch]) ? db : max(db, mmLevel[ch] - 2.0f);
    // Peak hold for 1 s, then decays about 15 dB/s.
    if (db >= mmPeak[ch]) {
      mmPeak[ch] = db;
      mmPeakMs[ch] = now;
    } else if (now - mmPeakMs[ch] > 1000) {
      mmPeak[ch] = max(-60.0f, mmPeak[ch] - 0.5f);
    }
    drawMeterBar(ch);
  }
}

void micMeterEnter() {
  drawHeader("Mic Meter");
  if (mmCanvas.width() == 0) {
    mmCanvas.setPsram(true);
    mmCanvas.createSprite(MM_BAR_W, MM_BAR_H);
  }
  auto& d = M5.Display;
  const lgfx::IFont* big = &fonts::FreeMonoBold18pt7b;

  for (int ch = 0; ch < 2; ch++) {
    mmLevel[ch] = mmPeak[ch] = -60;
    mmShownDb[ch] = 999;
    d.setFont(big);
    d.setTextColor(C_TEXT);
    d.setTextDatum(middle_left);
    d.drawString(ch == 0 ? "MIC 1" : "MIC 2", MM_LABEL_X, MM_ROW_Y[ch] + MM_BAR_H / 2);
    drawMeterBar(ch);
  }

  d.setFont(FONT_MONO);
  d.setTextColor(C_TEXT2);
  const char* marks[] = {"-60", "-45", "-30", "-15", "0 dB"};
  for (int i = 0; i < 5; i++) {
    d.setTextDatum(i == 0 ? middle_left : i == 4 ? middle_right : middle_center);
    d.drawString(marks[i], MM_BAR_X + MM_BAR_W * i / 4, 492);
  }

  d.setFont(FONT_SMALL);
  d.setTextDatum(middle_left);
  int x = MM_LABEL_X, y = 572;
  d.fillSmoothRoundRect(x, y - 7, 28, 14, 3, C_BLUE);
  d.drawString("Current level", x + 38, y);
  x += 38 + d.textWidth("Current level") + 28;
  d.fillRect(x, y - 11, 6, 22, C_AMBER);
  d.drawString("Recent peak", x + 16, y);
  x += 16 + d.textWidth("Recent peak") + 28;
  d.drawString("Talk or clap near the device to see it move.", x, y);

  mmQueued = 0;
  mmNextToQueue = mmNextToRead = 0;
  if (!audioStartMic()) drawTextBox(MM_BAR_X, 170, MM_BAR_W, 40, "Microphone did not start", FONT_SMALL, C_AMBER, C_BG);
}

void micMeterLoop() {
  int x, y;
  if (touchPressed(x, y) && hit(backBtn, x, y)) {
    goTo(HOME);
    return;
  }
  if (audioApp != AUDIO_MIC) return;

  // A block is done when the queue holds fewer than we submitted.
  if (mmQueued > 0 && (int)M5.Mic.isRecording() < mmQueued) {
    micMeterUpdate(mmBuf[mmNextToRead]);
    mmNextToRead ^= 1;
    mmQueued--;
  }
  while (mmQueued < 2) {
    M5.Mic.record(mmBuf[mmNextToQueue], MM_FRAMES * 2, MM_RATE, true);  // stereo
    mmNextToQueue ^= 1;
    mmQueued++;
  }
}

void micMeterExit() {
  audioRelease();
}
