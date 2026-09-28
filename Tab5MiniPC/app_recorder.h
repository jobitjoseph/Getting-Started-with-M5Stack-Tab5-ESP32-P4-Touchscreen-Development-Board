// Recorder: record up to one minute from the microphone and play it back.
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
//
// M5.Mic.record() is non-blocking and queues up to two requests that join
// without gaps, so the minute is captured as 100 ms chunks kept topped up
// from loop(). The 1.9 MB buffer lives in PSRAM.
#pragma once
#include "ui.h"
#include "audio_app.h"

constexpr uint32_t REC_RATE = 16000;
constexpr uint32_t REC_SECONDS = 60;
constexpr uint32_t REC_MAX = REC_RATE * REC_SECONDS;
constexpr uint32_t REC_CHUNK = REC_RATE / 10;
constexpr uint32_t REC_CHUNKS = REC_MAX / REC_CHUNK;

int16_t* recBuffer = nullptr;
uint32_t recLength = 0;

enum RecState { REC_READY, REC_RECORDING, REC_PLAYING };
RecState recState = REC_READY;
uint32_t recQueued = 0;
uint32_t playStartMs = 0;

Button recRecordBtn = {282, 404, 220, 100, "Record", C_AMBER, C_BG};
Button recStopBtn   = {530, 404, 220, 100, "Stop",   C_PANEL, C_TEXT};
Button recPlayBtn   = {778, 404, 220, 100, "Play",   C_PANEL, C_TEXT};
const SliderRow recVolume = volumeRow(200, 526, 880, 72, C_BG);
constexpr int WAVE_X = 64, WAVE_Y = 244, WAVE_W = 1152, WAVE_H = 140;
constexpr int WAVE_BARS = 60;

M5Canvas waveCanvas(&M5.Display);
int lastWaveBars = -1;
int lastTimerSec = -1;

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
uint32_t recChunksDone() { return recQueued - M5.Mic.isRecording(); }

uint32_t recAvailable() { return recState == REC_RECORDING ? recChunksDone() * REC_CHUNK : recLength; }

// One bar per second of the minute. Recorded bars are blue, played bars amber.
void drawWaveform(int playedBars) {
  auto& c = waveCanvas;
  c.fillSprite(C_BG);
  c.fillSmoothRoundRect(0, 0, WAVE_W, WAVE_H, 16, C_DARK);

  const uint32_t perBar = REC_MAX / WAVE_BARS;
  const float slot = (WAVE_W - 40) / (float)WAVE_BARS;
  const int barW = (int)slot - 6;
  uint32_t available = recAvailable();

  for (int i = 0; i < WAVE_BARS; i++) {
    int x = 20 + (int)(i * slot);
    uint32_t start = i * perBar;
    int h = 8;
    uint16_t color = C_BORDER;
    if (start + perBar <= available) {
      int peak = 0;
      for (uint32_t s = start; s < start + perBar; s += 4) peak = max(peak, abs((int)recBuffer[s]));
      // sqrt lifts quiet passages so speech stays visible.
      h = 8 + (int)((WAVE_H - 28) * min(1.0f, sqrtf(peak / 32768.0f) * 1.3f));
      color = (i < playedBars) ? C_AMBER : C_BLUE;
    }
    c.fillSmoothRoundRect(x, (WAVE_H - h) / 2, barW, h, 3, color);
  }
  c.pushSprite(WAVE_X, WAVE_Y);
}

void drawTimer(int seconds, int total) {
  char big[16], small[20];
  snprintf(big, sizeof(big), "%02d:%02d", seconds / 60, seconds % 60);
  snprintf(small, sizeof(small), "/ %02d:%02d", total / 60, total % 60);
  auto& d = M5.Display;
  d.fillRect(0, 100, SCREEN_W, 90, C_BG);
  d.setFont(&fonts::Font8);
  int bigW = d.textWidth(big);
  d.setFont(&fonts::FreeMonoBold18pt7b);
  int smallW = d.textWidth(small);
  int x = (SCREEN_W - bigW - 16 - smallW) / 2;

  d.setTextDatum(bottom_left);
  d.setTextColor(C_TEXT);
  d.setFont(&fonts::Font8);
  d.drawString(big, x, 182);
  d.setTextColor(C_TEXT2);
  d.setFont(&fonts::FreeMonoBold18pt7b);
  d.drawString(small, x + bigW + 16, 176);
}

void drawRecStatus(const char* text, uint16_t color, bool dot) {
  auto& d = M5.Display;
  d.fillRect(0, 196, SCREEN_W, 40, C_BG);
  d.setFont(FONT_LABEL);
  int w = d.textWidth(text) + (dot ? 24 : 0);
  int x = (SCREEN_W - w) / 2;
  if (dot) {
    d.fillSmoothCircle(x + 7, 216, 7, color);
    x += 24;
  }
  d.setTextColor(color);
  d.setTextDatum(middle_left);
  d.drawString(text, x, 216);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------
void recorderStartRecording() {
  if (!audioStartMic()) {
    drawRecStatus("Microphone did not start", C_AMBER, false);
    return;
  }
  memset(recBuffer, 0, REC_MAX * sizeof(int16_t));
  recLength = 0;
  recQueued = 0;
  recState = REC_RECORDING;
  lastWaveBars = -1;
  lastTimerSec = -1;
  drawRecStatus("Recording", C_AMBER, true);
}

// The mic signal is quiet: scale the recording so its peak is near full
// scale, capped at 20x so near-silence doesn't become loud hiss.
void recorderNormalize() {
  int peak = 1;
  for (uint32_t i = 0; i < recLength; i++) peak = max(peak, abs((int)recBuffer[i]));
  float gain = min(20.0f, 30000.0f / peak);
  if (gain <= 1.0f) return;
  for (uint32_t i = 0; i < recLength; i++) recBuffer[i] = (int16_t)(recBuffer[i] * gain);
}

void recorderFinishRecording() {
  while (M5.Mic.isRecording()) delay(1);
  recLength = recQueued * REC_CHUNK;
  recorderNormalize();
  recState = REC_READY;
  drawRecStatus("Ready", C_TEXT2, false);
  drawWaveform(0);
  drawTimer(recLength / REC_RATE, REC_SECONDS);
}

void recorderStartPlayback() {
  if (recLength == 0) {
    drawRecStatus("Nothing recorded yet", C_TEXT2, false);
    return;
  }
  if (!audioStartSpeaker()) {
    drawRecStatus("Speaker did not start", C_AMBER, false);
    return;
  }
  M5.Speaker.playRaw(recBuffer, recLength, REC_RATE, false, 1, 0);
  recState = REC_PLAYING;
  playStartMs = millis();
  lastTimerSec = -1;
  drawRecStatus("Playing", C_BLUE, true);
}

void recorderStop() {
  if (recState == REC_RECORDING) recorderFinishRecording();
  if (recState == REC_PLAYING) {
    M5.Speaker.stop();
    recState = REC_READY;
    drawRecStatus("Ready", C_TEXT2, false);
    drawWaveform(0);
    drawTimer(recLength / REC_RATE, REC_SECONDS);
  }
}

// ---------------------------------------------------------------------------
// Screen functions
// ---------------------------------------------------------------------------
void recorderEnter() {
  if (!recBuffer) recBuffer = (int16_t*)heap_caps_calloc(REC_MAX, sizeof(int16_t), MALLOC_CAP_SPIRAM);
  if (waveCanvas.width() == 0) {
    waveCanvas.setPsram(true);
    waveCanvas.createSprite(WAVE_W, WAVE_H);
  }

  drawHeader("Recorder");
  recState = REC_READY;
  drawTimer(recLength / REC_RATE, REC_SECONDS);
  drawRecStatus(recBuffer ? "Ready" : "Not enough memory", C_TEXT2, false);
  drawWaveform(0);
  drawButton(recRecordBtn, FONT_TITLE, R_BTN, BI_DOT, C_BG);
  drawButton(recStopBtn, FONT_TITLE, R_BTN, BI_SQUARE, C_TEXT);
  drawButton(recPlayBtn, FONT_TITLE, R_BTN, BI_PLAY, C_BLUE);
  volumeRowDraw(recVolume);

  auto& d = M5.Display;
  d.setFont(FONT_SMALL);
  d.setTextColor(C_TEXT2);
  d.setTextDatum(middle_center);
  d.drawString("Up to 1 minute, kept in memory until you record again or power off.", SCREEN_W / 2, 640);
}

void recorderLoop() {
  if (!recBuffer) {
    int x, y;
    if (touchPressed(x, y) && hit(backBtn, x, y)) goTo(HOME);
    return;
  }

  volumeRowUpdate(recVolume);

  int x, y;
  if (touchPressed(x, y)) {
    if (hit(backBtn, x, y)) { goTo(HOME); return; }
    if (hit(recRecordBtn, x, y) && recState != REC_RECORDING) recorderStartRecording();
    else if (hit(recStopBtn, x, y)) recorderStop();
    else if (hit(recPlayBtn, x, y) && recState != REC_RECORDING) recorderStartPlayback();
  }

  if (recState == REC_RECORDING) {
    while (recQueued < REC_CHUNKS && M5.Mic.isRecording() < 2) {
      M5.Mic.record(recBuffer + recQueued * REC_CHUNK, REC_CHUNK, REC_RATE);
      recQueued++;
    }
    uint32_t done = recChunksDone();
    int bars = done * REC_CHUNK * WAVE_BARS / REC_MAX;
    if (bars != lastWaveBars) {
      lastWaveBars = bars;
      drawWaveform(0);
    }
    int sec = done * REC_CHUNK / REC_RATE;
    if (sec != lastTimerSec) {
      lastTimerSec = sec;
      drawTimer(sec, REC_SECONDS);
    }
    if (done >= REC_CHUNKS) recorderFinishRecording();
  }

  if (recState == REC_PLAYING) {
    uint32_t playedMs = millis() - playStartMs;
    int sec = min(playedMs / 1000, recLength / REC_RATE);
    if (sec != lastTimerSec) {
      lastTimerSec = sec;
      drawTimer(sec, recLength / REC_RATE);
    }
    int bars = (uint64_t)playedMs * REC_RATE / 1000 * WAVE_BARS / REC_MAX;
    if (bars != lastWaveBars) {
      lastWaveBars = bars;
      drawWaveform(bars);
    }
    if (!M5.Speaker.isPlaying()) recorderStop();
  }
}

void recorderExit() {
  if (recState == REC_RECORDING) recorderFinishRecording();
  recState = REC_READY;
  audioRelease();
}
