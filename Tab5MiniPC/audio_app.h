// Audio arbitration, shared volume and the volume slider row.
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
//
// The ES7210 mics and the ES8388 speaker codec share I2S port 0, and
// M5Unified drives it as either a mic or a speaker, never both. The radio also
// plays through M5.Speaker. So only one app uses audio at a time; starting a
// new one releases the previous one, and every app's Exit() calls
// audioRelease().
//
// Not named audio.h: on case-insensitive file systems it would shadow the
// radio library's <Audio.h>.
#pragma once
#include "ui.h"

enum AudioApp { AUDIO_NONE, AUDIO_MIC, AUDIO_SPEAKER, AUDIO_RADIO };
AudioApp audioApp = AUDIO_NONE;

void radioStopStream();  // app_radio.h

int volumePercent = 80;

// M5Unified defaults to 4 on the Tab5, which is quiet. 8 adds +6 dB; loud
// radio streams may clip near 100 % with higher values.
constexpr uint8_t SPEAKER_BOOST = 8;

// Call once in setup(), while the speaker is stopped.
void audioBegin() {
  auto cfg = M5.Speaker.config();
  cfg.magnification = SPEAKER_BOOST;
  M5.Speaker.config(cfg);
}

// M5.Speaker applies volume squared; sqrt makes the percentage linear in loudness.
void applyVolume() {
  M5.Speaker.setVolume((uint8_t)lroundf(255.0f * sqrtf(volumePercent / 100.0f)));
}

void audioRelease() {
  switch (audioApp) {
    case AUDIO_MIC:
      M5.Mic.end();
      break;
    case AUDIO_RADIO:
      radioStopStream();
      [[fallthrough]];
    case AUDIO_SPEAKER:
      M5.Speaker.stop();
      M5.Speaker.end();
      break;
    case AUDIO_NONE:
      break;
  }
  audioApp = AUDIO_NONE;
}

bool audioStartMic() {
  if (audioApp == AUDIO_MIC) return true;
  audioRelease();
  if (!M5.Mic.begin()) return false;
  audioApp = AUDIO_MIC;
  return true;
}

bool audioStartSpeaker(AudioApp who = AUDIO_SPEAKER) {
  applyVolume();
  if (audioApp == who) return true;
  audioRelease();
  if (!M5.Speaker.begin()) return false;
  applyVolume();
  audioApp = who;
  return true;
}

SliderRow volumeRow(int x, int y, int w, int btn, uint16_t bg) {
  return {x, y, w, btn, bg, "Volume", &volumePercent, 0};
}

void volumeRowDraw(const SliderRow& r) { sliderRowDraw(r); }

bool volumeRowUpdate(const SliderRow& r) {
  if (!sliderRowUpdate(r)) return false;
  applyVolume();
  return true;
}
