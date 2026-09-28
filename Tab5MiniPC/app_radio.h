// Internet radio: preset stations plus up to five user-added stream URLs.
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
//
// ESP32-audioI2S 4.x decodes the stream in its own task. It is given the
// spare I2S_NUM_1 only so its DMA clock paces the decoder; every decoded block
// is intercepted in audio_process_i2s() and queued on M5.Speaker, which keeps
// the ES8388 codec, amp and volume under M5Unified's control.
//
// <audiolib_structs.hpp> exists only in ESP32-audioI2S. Including it first
// makes the IDE resolve <Audio.h> to that library rather than e.g. Adafruit's.
#pragma once
#include <audiolib_structs.hpp>
#include <Audio.h>
#include <WiFi.h>
#include "ui.h"
#include "audio_app.h"

// ---------------------------------------------------------------------------
// Stations (RAM only)
// ---------------------------------------------------------------------------
struct Station {
  String name;
  String url;
  const char* note;
  bool addedByUser;
};

// Public MP3 streams, checked September 2026. Stream URLs change over time.
constexpr int PRESET_COUNT = 10;
constexpr int MAX_USER_STATIONS = 5;
constexpr int MAX_STATIONS = PRESET_COUNT + MAX_USER_STATIONS;
Station stations[MAX_STATIONS] = {
  {"SomaFM Groove Salad", "http://ice1.somafm.com/groovesalad-128-mp3", "English - chill", false},
  {"SomaFM Drone Zone", "http://ice2.somafm.com/dronezone-128-mp3", "English - ambient", false},
  {"Radio Paradise", "http://stream.radioparadise.com/mp3-128", "English - rock/pop", false},
  {"Sooriyan FM", "https://radio.lotustechnologieslk.net:8006/;stream.mp3", "Tamil", false},
  {"89.4 Tamil FM", "https://centova.aarenworld.com/proxy/894tamilfm/stream", "Tamil", false},
  {"Tamil Panpalai Gold", "https://tamilpanpalai.radioca.st/ind", "Tamil - old songs", false},
  {"Tamil Old Hits FM", "https://a9oldhits-a9media.radioca.st/stream", "Tamil - old songs", false},
  {"Radio Suno 91.7 FM", "http://playerservices.streamtheworld.com/api/livestream-redirect/SUNO917_SC", "Malayalam", false},
  {"Radio Malayalam 98.6 FM", "https://stream.zeno.fm/512rbf1e3qzuv", "Malayalam", false},
  {"Yesudas Malayalam Hits", "https://stream.zeno.fm/9x1sw687nf9uv", "Malayalam - classics", false},
};
int stationCount = PRESET_COUNT;
int radioSelected = 0;
int radioScroll = 0;

// Returns nullptr on success, otherwise a message for the user.
const char* radioAddStation(const String& name, const String& url) {
  if (stationCount >= MAX_STATIONS) return "List is full (5 of your own stations)";
  stations[stationCount] = {name, url, "Added by you", true};
  radioSelected = stationCount++;
  return nullptr;
}

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------
Audio* radioPlayer = nullptr;
enum RadioState { RADIO_STOPPED, RADIO_CONNECTING, RADIO_PLAYING, RADIO_ERROR };
RadioState radioState = RADIO_STOPPED;
String radioError;
uint32_t radioStartMs = 0;

// M5.Speaker holds up to two buffers per channel until played, so three
// rotating buffers are enough.
constexpr int RADIO_CHANNEL = 0;
constexpr size_t RADIO_BUF_SAMPLES = 4096;  // 2048 stereo frames, ~46 ms
int16_t* radioBuf[3] = {nullptr, nullptr, nullptr};
int radioBufIndex = 0;
size_t radioBufFill = 0;
volatile bool radioOutputOn = false;
volatile bool radioGotAudio = false;

// ESP32-audioI2S hook, called from the decoder task. Samples are interleaved
// stereo int32 with the 16-bit value in the upper half.
void audio_process_i2s(int32_t* samples, int16_t words, bool* continueI2S) {
  *continueI2S = false;
  if (!radioOutputOn || !radioPlayer) return;

  uint32_t rate = radioPlayer->getSampleRate();
  for (int i = 0; i < words; i++) {
    radioBuf[radioBufIndex][radioBufFill++] = (int16_t)(samples[i] >> 16);
    if (radioBufFill < RADIO_BUF_SAMPLES) continue;

    // Blocking here throttles the decoder to the speaker's pace.
    uint32_t t0 = millis();
    while (radioOutputOn && M5.Speaker.isPlaying(RADIO_CHANNEL) >= 2 && millis() - t0 < 300) vTaskDelay(1);
    if (!radioOutputOn) return;
    M5.Speaker.playRaw(radioBuf[radioBufIndex], RADIO_BUF_SAMPLES, rate, true, 1, RADIO_CHANNEL);
    radioBufIndex = (radioBufIndex + 1) % 3;
    radioBufFill = 0;
    radioGotAudio = true;
  }
}

// Created once and kept; the library isn't meant to be recreated.
//
// setPinout() must succeed for the decoder to start, so it gets the real
// speaker pins. It runs before M5.Speaker.begin(), which then reclaims the
// pins for I2S0 (last peripheral attached wins). I2S1 keeps clocking with no
// pins, which is exactly what paces the decoder.
bool radioCreatePlayer() {
  if (radioPlayer) return true;
  for (int i = 0; i < 3; i++) {
    radioBuf[i] = (int16_t*)heap_caps_malloc(RADIO_BUF_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!radioBuf[i]) return false;
  }
  audioRelease();
  radioPlayer = new Audio(I2S_NUM_1);
  // BCLK 27, WS 29, DOUT 26, MCLK 30
  if (!radioPlayer->setPinout(27, 29, 26, 30)) return false;
  radioPlayer->setVolume(21);  // full scale; M5.Speaker applies the volume
  radioPlayer->setConnectionTimeout(4000, 6000);
  Audio::audio_info_callback = [](Audio::msg_t m) {
    if (m.e != Audio::evt_vu && m.e != Audio::evt_spectrum && m.msg) {
      Serial.printf("Radio %s: %s\n", Audio::eventStr[m.e], m.msg);
    }
  };
  return true;
}

void radioStopStream() {
  radioOutputOn = false;
  if (radioPlayer && radioPlayer->isRunning()) radioPlayer->stopSong();
  M5.Speaker.stop(RADIO_CHANNEL);
  radioState = RADIO_STOPPED;
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
constexpr int RL_X = 32, RL_W = 520, RL_ROWS_Y = 150, RL_ROW_H = 66, RL_ROW_GAP = 8, RL_VISIBLE = 6;
Button radioAddBtn = {RL_X, 604, RL_W, 66, "+ Add stream URL", C_BG, C_BLUE};
Button radioAddBtnShort = {RL_X, 604, 360, 66, "+ Add stream URL", C_BG, C_BLUE};
Button radioUpBtn = {RL_X + 368, 604, 72, 66, "", C_PANEL, C_TEXT};
Button radioDownBtn = {RL_X + 448, 604, 72, 66, "", C_PANEL, C_TEXT};

constexpr int CARD_X = 584, CARD_Y = 116, CARD_W = 664, CARD_H = 576, CARD_PAD = 40;
Button radioPlayBtn = {CARD_X + CARD_PAD, 330, 200, 92, "Play", C_AMBER, C_BG};
Button radioStopBtn = {CARD_X + CARD_PAD + 220, 330, 200, 92, "Stop", C_BG, C_TEXT};
Button radioWifiBtn = {CARD_X + CARD_PAD, 330, 300, 92, "Open Wi-Fi", C_BLUE, C_BG};
const SliderRow radioVolume = volumeRow(CARD_X + CARD_PAD, 470, CARD_W - 2 * CARD_PAD, 72, C_PANEL);

bool radioOnlineShown = false;
uint32_t radioLastCheckMs = 0;

bool radioNeedsScroll() { return stationCount > RL_VISIBLE; }

Button radioRowRect(int row) {
  return {RL_X, RL_ROWS_Y + row * (RL_ROW_H + RL_ROW_GAP), RL_W, RL_ROW_H, "", C_PANEL, C_TEXT};
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
void drawStationList() {
  auto& d = M5.Display;
  d.fillRect(RL_X, RL_ROWS_Y, RL_W, SCREEN_H - RL_ROWS_Y, C_BG);
  for (int row = 0; row < RL_VISIBLE; row++) {
    int i = radioScroll + row;
    if (i >= stationCount) break;
    Button r = radioRowRect(row);
    bool sel = (i == radioSelected);
    int b = sel ? 3 : 1;
    d.fillSmoothRoundRect(r.x, r.y, r.w, r.h, R_SMALL, sel ? C_AMBER : C_BORDER);
    d.fillSmoothRoundRect(r.x + b, r.y + b, r.w - 2 * b, r.h - 2 * b, R_SMALL - b, C_PANEL);
    d.setTextDatum(middle_left);
    d.setTextColor(C_TEXT);
    d.drawString(fitText(stations[i].name, FONT_LABEL, r.w - 44), r.x + 22, r.y + 23);
    d.setFont(&fonts::FreeSans9pt7b);
    d.setTextColor(stations[i].addedByUser ? C_BLUE : C_TEXT2);
    d.drawString(stations[i].note, r.x + 22, r.y + 49);
  }

  // Dashed outline for "+ Add stream URL".
  const Button& add = radioNeedsScroll() ? radioAddBtnShort : radioAddBtn;
  for (int x = add.x; x < add.x + add.w - 10; x += 16) {
    d.fillRect(x, add.y, 10, 2, C_BORDER2);
    d.fillRect(x, add.y + add.h - 2, 10, 2, C_BORDER2);
  }
  for (int y = add.y; y < add.y + add.h - 6; y += 12) {
    d.fillRect(add.x, y, 2, 6, C_BORDER2);
    d.fillRect(add.x + add.w - 2, y, 2, 6, C_BORDER2);
  }
  d.setFont(FONT_LABEL);
  d.setTextColor(C_BLUE);
  d.setTextDatum(middle_center);
  d.drawString(add.label, add.x + add.w / 2, add.y + add.h / 2);

  if (radioNeedsScroll()) {
    for (int k = 0; k < 2; k++) {
      const Button& b = k ? radioDownBtn : radioUpBtn;
      drawButton(b, FONT_LABEL, R_SMALL);
      int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
      if (k == 0) d.fillTriangle(cx - 14, cy + 8, cx + 14, cy + 8, cx, cy - 10, C_TEXT);
      else d.fillTriangle(cx - 14, cy - 8, cx + 14, cy - 8, cx, cy + 10, C_TEXT);
    }
  }
}

void drawRadioStatus() {
  auto& d = M5.Display;
  int y = 258, x = CARD_X + CARD_PAD;
  d.fillRect(x, y - 18, CARD_W - 2 * CARD_PAD, 36, C_PANEL);
  String text;
  uint16_t dot = C_TEXT2;
  switch (radioState) {
    case RADIO_STOPPED:    text = "Stopped"; break;
    case RADIO_CONNECTING: text = "Connecting..."; dot = C_AMBER; break;
    case RADIO_PLAYING:
      text = String("Playing - ") + (radioPlayer ? radioPlayer->getCodecname() : "MP3") + " stream";
      dot = C_BLUE;
      break;
    case RADIO_ERROR:      text = "Error: " + radioError; dot = C_AMBER; break;
  }
  d.fillSmoothCircle(x + 6, y, 6, dot);
  d.setFont(FONT_MONO);
  d.setTextColor(C_TEXT2);
  d.setTextDatum(middle_left);
  d.drawString(fitText(text, FONT_MONO, CARD_W - 2 * CARD_PAD - 22), x + 22, y);
}

void drawNowPlayingCard() {
  auto& d = M5.Display;
  d.fillSmoothRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, R_BTN, C_PANEL);
  int x = CARD_X + CARD_PAD;
  d.setTextDatum(middle_left);
  d.setFont(FONT_LABEL);
  d.setTextColor(C_TEXT2);
  d.drawString("NOW PLAYING", x, 170);

  radioOnlineShown = (WiFi.status() == WL_CONNECTED);
  if (!radioOnlineShown) {
    d.setFont(FONT_TITLE);
    d.setTextColor(C_TEXT);
    d.drawString("Connect to Wi-Fi first", x, 214);
    d.setFont(FONT_SMALL);
    d.setTextColor(C_TEXT2);
    d.drawString("The radio streams over the internet.", x, 258);
    drawButton(radioWifiBtn, FONT_TITLE);
    return;
  }

  d.setFont(FONT_TITLE);
  d.setTextColor(C_TEXT);
  d.drawString(fitText(stations[radioSelected].name, FONT_TITLE, CARD_W - 2 * CARD_PAD), x, 214);
  drawRadioStatus();
  drawButton(radioPlayBtn, FONT_TITLE, R_BTN, BI_PLAY, C_BG);
  drawButton(radioStopBtn, FONT_TITLE, R_BTN, BI_SQUARE, C_TEXT);
  volumeRowDraw(radioVolume);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------
void radioPlay() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!radioCreatePlayer()) {
    radioState = RADIO_ERROR;
    radioError = "audio player did not start";
    drawRadioStatus();
    return;
  }
  radioStopStream();
  if (!audioStartSpeaker(AUDIO_RADIO)) {
    radioState = RADIO_ERROR;
    radioError = "speaker did not start";
    drawRadioStatus();
    return;
  }
  radioBufFill = 0;
  radioBufIndex = 0;
  radioGotAudio = false;
  radioOutputOn = true;
  radioState = RADIO_CONNECTING;
  drawRadioStatus();

  // Blocks for a second or two while connecting; decoding then runs in the background.
  radioStartMs = millis();
  if (!radioPlayer->connecttohost(stations[radioSelected].url.c_str())) {
    radioOutputOn = false;
    radioState = RADIO_ERROR;
    radioError = "could not connect";
    drawRadioStatus();
  }
}

void radioStopPressed() {
  if (audioApp == AUDIO_RADIO) audioRelease();
  radioState = RADIO_STOPPED;
  drawRadioStatus();
}

// ---------------------------------------------------------------------------
// Screen functions
// ---------------------------------------------------------------------------
void radioEnter() {
  drawHeader("Internet Radio");
  auto& d = M5.Display;
  d.setFont(FONT_LABEL);
  d.setTextColor(C_TEXT2);
  d.setTextDatum(middle_left);
  d.drawString("STATIONS", RL_X, 132);
  if (radioSelected >= radioScroll + RL_VISIBLE) radioScroll = radioSelected - RL_VISIBLE + 1;
  radioState = RADIO_STOPPED;
  drawStationList();
  drawNowPlayingCard();
}

void radioLoop() {
  if (radioPlayer && audioApp == AUDIO_RADIO) radioPlayer->loop();

  if (radioState == RADIO_CONNECTING && radioGotAudio) {
    radioState = RADIO_PLAYING;
    drawRadioStatus();
  } else if (radioState == RADIO_CONNECTING && millis() - radioStartMs > 15000) {
    radioError = "no audio received";
    audioRelease();
    radioState = RADIO_ERROR;
    drawRadioStatus();
  } else if (radioState == RADIO_PLAYING && !radioPlayer->isRunning()) {
    radioError = "stream ended";
    audioRelease();
    radioState = RADIO_ERROR;
    drawRadioStatus();
  }

  // Redraw the card if Wi-Fi came or went.
  if (millis() - radioLastCheckMs > 1000) {
    radioLastCheckMs = millis();
    if ((WiFi.status() == WL_CONNECTED) != radioOnlineShown) {
      if (audioApp == AUDIO_RADIO) audioRelease();
      radioState = RADIO_STOPPED;
      drawNowPlayingCard();
    }
  }

  if (radioOnlineShown) volumeRowUpdate(radioVolume);

  int x, y;
  if (!touchPressed(x, y)) return;
  if (hit(backBtn, x, y)) { goTo(HOME); return; }

  const Button& add = radioNeedsScroll() ? radioAddBtnShort : radioAddBtn;
  if (hit(add, x, y)) { goTo(ADDSTREAM); return; }
  if (radioNeedsScroll() && hit(radioUpBtn, x, y) && radioScroll > 0) {
    radioScroll--;
    drawStationList();
    return;
  }
  if (radioNeedsScroll() && hit(radioDownBtn, x, y) && radioScroll + RL_VISIBLE < stationCount) {
    radioScroll++;
    drawStationList();
    return;
  }
  for (int row = 0; row < RL_VISIBLE; row++) {
    int i = radioScroll + row;
    if (i < stationCount && hit(radioRowRect(row), x, y)) {
      bool wasPlaying = (radioState == RADIO_CONNECTING || radioState == RADIO_PLAYING);
      radioSelected = i;
      drawStationList();
      drawNowPlayingCard();
      if (wasPlaying) radioPlay();
      return;
    }
  }

  if (!radioOnlineShown) {
    if (hit(radioWifiBtn, x, y)) goTo(WIFI);
    return;
  }
  if (hit(radioPlayBtn, x, y)) radioPlay();
  if (hit(radioStopBtn, x, y)) radioStopPressed();
}

void radioExit() {
  audioRelease();
}
