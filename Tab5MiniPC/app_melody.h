// Melody: plays built-in tunes with M5.Speaker.tone(), one note per loop()
// check so the UI never blocks. All tunes are traditional / public domain.
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
#pragma once
#include "ui.h"
#include "audio_app.h"

struct Note {
  uint16_t freq;  // Hz, 0 = rest
  uint16_t ms;
};

// Around an octave above middle C, which sounds clearest on the small speaker.
constexpr uint16_t NG4 = 392, NA4 = 440, NB4 = 494;
constexpr uint16_t NC5 = 523, ND5 = 587, NE5 = 659, NF5 = 698, NG5 = 784, NA5 = 880, NB5 = 988;
constexpr uint16_t NC6 = 1047;
constexpr uint16_t REST = 0;

// Eighth, quarter, dotted quarter, half, dotted half, whole (ms).
constexpr uint16_t L8 = 175, L4 = 350, L4D = 525, L2 = 700, L2D = 1050, L1 = 1400;

const Note TWINKLE[] = {
  {NC5, L4}, {NC5, L4}, {NG5, L4}, {NG5, L4}, {NA5, L4}, {NA5, L4}, {NG5, L2},
  {NF5, L4}, {NF5, L4}, {NE5, L4}, {NE5, L4}, {ND5, L4}, {ND5, L4}, {NC5, L2},
  {NG5, L4}, {NG5, L4}, {NF5, L4}, {NF5, L4}, {NE5, L4}, {NE5, L4}, {ND5, L2},
  {NG5, L4}, {NG5, L4}, {NF5, L4}, {NF5, L4}, {NE5, L4}, {NE5, L4}, {ND5, L2},
  {NC5, L4}, {NC5, L4}, {NG5, L4}, {NG5, L4}, {NA5, L4}, {NA5, L4}, {NG5, L2},
  {NF5, L4}, {NF5, L4}, {NE5, L4}, {NE5, L4}, {ND5, L4}, {ND5, L4}, {NC5, L2},
};

const Note HAPPY_BIRTHDAY[] = {
  {NG4, 260}, {NG4, 90}, {NA4, L4}, {NG4, L4}, {NC5, L4}, {NB4, L2},
  {NG4, 260}, {NG4, 90}, {NA4, L4}, {NG4, L4}, {ND5, L4}, {NC5, L2},
  {NG4, 260}, {NG4, 90}, {NG5, L4}, {NE5, L4}, {NC5, L4}, {NB4, L4}, {NA4, L2},
  {NF5, 260}, {NF5, 90}, {NE5, L4}, {NC5, L4}, {ND5, L4}, {NC5, L2},
};

const Note JINGLE_BELLS[] = {
  {NE5, L4}, {NE5, L4}, {NE5, L2}, {NE5, L4}, {NE5, L4}, {NE5, L2},
  {NE5, L4}, {NG5, L4}, {NC5, L4D}, {ND5, L8}, {NE5, L1},
  {NF5, L4}, {NF5, L4}, {NF5, L4D}, {NF5, L8}, {NF5, L4}, {NE5, L4}, {NE5, L4}, {NE5, L8}, {NE5, L8},
  {NE5, L4}, {ND5, L4}, {ND5, L4}, {NE5, L4}, {ND5, L2}, {NG5, L2},
  {NE5, L4}, {NE5, L4}, {NE5, L2}, {NE5, L4}, {NE5, L4}, {NE5, L2},
  {NE5, L4}, {NG5, L4}, {NC5, L4D}, {ND5, L8}, {NE5, L1},
  {NF5, L4}, {NF5, L4}, {NF5, L4D}, {NF5, L8}, {NF5, L4}, {NE5, L4}, {NE5, L4}, {NE5, L8}, {NE5, L8},
  {NG5, L4}, {NG5, L4}, {NF5, L4}, {ND5, L4}, {NC5, L1},
};

const Note ODE_TO_JOY[] = {
  {NE5, L4}, {NE5, L4}, {NF5, L4}, {NG5, L4}, {NG5, L4}, {NF5, L4}, {NE5, L4}, {ND5, L4},
  {NC5, L4}, {NC5, L4}, {ND5, L4}, {NE5, L4}, {NE5, L4D}, {ND5, L8}, {ND5, L2},
  {NE5, L4}, {NE5, L4}, {NF5, L4}, {NG5, L4}, {NG5, L4}, {NF5, L4}, {NE5, L4}, {ND5, L4},
  {NC5, L4}, {NC5, L4}, {ND5, L4}, {NE5, L4}, {ND5, L4D}, {NC5, L8}, {NC5, L2},
};

const Note MARY_LAMB[] = {
  {NE5, L4}, {ND5, L4}, {NC5, L4}, {ND5, L4}, {NE5, L4}, {NE5, L4}, {NE5, L2},
  {ND5, L4}, {ND5, L4}, {ND5, L2}, {NE5, L4}, {NG5, L4}, {NG5, L2},
  {NE5, L4}, {ND5, L4}, {NC5, L4}, {ND5, L4}, {NE5, L4}, {NE5, L4}, {NE5, L4}, {NE5, L4},
  {ND5, L4}, {ND5, L4}, {NE5, L4}, {ND5, L4}, {NC5, L1},
};

const Note FRERE_JACQUES[] = {
  {NC5, L4}, {ND5, L4}, {NE5, L4}, {NC5, L4}, {NC5, L4}, {ND5, L4}, {NE5, L4}, {NC5, L4},
  {NE5, L4}, {NF5, L4}, {NG5, L2}, {NE5, L4}, {NF5, L4}, {NG5, L2},
  {NG5, L8}, {NA5, L8}, {NG5, L8}, {NF5, L8}, {NE5, L4}, {NC5, L4},
  {NG5, L8}, {NA5, L8}, {NG5, L8}, {NF5, L8}, {NE5, L4}, {NC5, L4},
  {NC5, L4}, {NG4, L4}, {NC5, L2}, {NC5, L4}, {NG4, L4}, {NC5, L2},
};

const Note SCALE[] = {
  {NC5, L8}, {ND5, L8}, {NE5, L8}, {NF5, L8}, {NG5, L8}, {NA5, L8}, {NB5, L8}, {NC6, L4},
  {REST, L8},
  {NC6, L8}, {NB5, L8}, {NA5, L8}, {NG5, L8}, {NF5, L8}, {NE5, L8}, {ND5, L8}, {NC5, L2D},
};

struct Tune {
  const char* name;
  const Note* notes;
  int count;
};

const Tune tunes[] = {
  {"Twinkle, Twinkle, Little Star", TWINKLE, sizeof(TWINKLE) / sizeof(Note)},
  {"Happy Birthday", HAPPY_BIRTHDAY, sizeof(HAPPY_BIRTHDAY) / sizeof(Note)},
  {"Jingle Bells", JINGLE_BELLS, sizeof(JINGLE_BELLS) / sizeof(Note)},
  {"Ode to Joy", ODE_TO_JOY, sizeof(ODE_TO_JOY) / sizeof(Note)},
  {"Mary Had a Little Lamb", MARY_LAMB, sizeof(MARY_LAMB) / sizeof(Note)},
  {"Frere Jacques", FRERE_JACQUES, sizeof(FRERE_JACQUES) / sizeof(Note)},
  {"Scale up and down", SCALE, sizeof(SCALE) / sizeof(Note)},
};
const int TUNE_COUNT = sizeof(tunes) / sizeof(tunes[0]);

int tuneIndex = 0;
bool melodyPlaying = false;
int noteIndex = 0;
uint32_t noteStartMs = 0;

Button tunePrevBtn = {260, 210, 80, 80, "", C_PANEL, C_TEXT};
Button tuneNextBtn = {940, 210, 80, 80, "", C_PANEL, C_TEXT};
Button melPlayBtn = {386, 406, 240, 100, "Play", C_AMBER, C_BG};
Button melStopBtn = {654, 406, 240, 100, "Stop", C_PANEL, C_TEXT};
const SliderRow melVolume = volumeRow(200, 542, 880, 80, C_BG);

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
void drawArrow(const Button& b, bool left) {
  drawButton(b, FONT_LABEL, 16);
  int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
  if (left) M5.Display.fillTriangle(cx + 8, cy - 14, cx + 8, cy + 14, cx - 10, cy, C_TEXT);
  else M5.Display.fillTriangle(cx - 8, cy - 14, cx - 8, cy + 14, cx + 10, cy, C_TEXT);
}

void drawTuneName() {
  const int x = tunePrevBtn.x + tunePrevBtn.w + 20;
  const int w = tuneNextBtn.x - 20 - x;
  String name = fitText(tunes[tuneIndex].name, FONT_TITLE, w);
  drawTextBox(x, 214, w, 44, name.c_str(), FONT_TITLE, C_TEXT, C_BG, middle_center);
  char buf[24];
  snprintf(buf, sizeof(buf), "Tune %d of %d", tuneIndex + 1, TUNE_COUNT);
  drawTextBox(x, 262, w, 30, buf, FONT_SMALL, C_TEXT2, C_BG, middle_center);
}

void drawMelodyProgress() {
  char buf[40];
  int total = tunes[tuneIndex].count;
  if (melodyPlaying) snprintf(buf, sizeof(buf), "note %d of %d", noteIndex + 1, total);
  else snprintf(buf, sizeof(buf), "%d notes - tap Play", total);
  drawTextBox(340, 330, 600, 32, buf, FONT_MONO, C_TEXT2, C_BG, middle_center);
}

// ---------------------------------------------------------------------------
// Playing
// ---------------------------------------------------------------------------
void melodyPlayNote(int i) {
  const Note& n = tunes[tuneIndex].notes[i];
  noteIndex = i;
  noteStartMs = millis();
  // Shorter than the slot so repeated notes stay distinct.
  if (n.freq != REST) M5.Speaker.tone(n.freq, n.ms - 40);
  drawMelodyProgress();
}

void melodyStart() {
  if (!audioStartSpeaker()) return;
  melodyPlaying = true;
  melodyPlayNote(0);
}

void melodyStop() {
  melodyPlaying = false;
  M5.Speaker.stop();
  drawMelodyProgress();
}

void selectTune(int delta) {
  bool wasPlaying = melodyPlaying;
  if (wasPlaying) melodyStop();
  tuneIndex = (tuneIndex + delta + TUNE_COUNT) % TUNE_COUNT;
  drawTuneName();
  drawMelodyProgress();
  if (wasPlaying) melodyStart();
}

// ---------------------------------------------------------------------------
// Screen functions
// ---------------------------------------------------------------------------
void melodyEnter() {
  drawHeader("Melody");
  iconMusicNote(SCREEN_W / 2 - 36, 120, 72, C_AMBER, C_BG);
  drawArrow(tunePrevBtn, true);
  drawArrow(tuneNextBtn, false);
  melodyPlaying = false;
  drawTuneName();
  drawMelodyProgress();
  drawButton(melPlayBtn, FONT_TITLE, R_BTN, BI_PLAY, C_BG);
  drawButton(melStopBtn, FONT_TITLE, R_BTN, BI_SQUARE, C_TEXT);
  volumeRowDraw(melVolume);
}

void melodyLoop() {
  volumeRowUpdate(melVolume);

  int x, y;
  if (touchPressed(x, y)) {
    if (hit(backBtn, x, y)) { goTo(HOME); return; }
    if (hit(tunePrevBtn, x, y)) selectTune(-1);
    if (hit(tuneNextBtn, x, y)) selectTune(+1);
    if (hit(melPlayBtn, x, y)) melodyStart();
    if (hit(melStopBtn, x, y)) melodyStop();
  }

  const Tune& t = tunes[tuneIndex];
  if (melodyPlaying && millis() - noteStartMs >= t.notes[noteIndex].ms) {
    if (noteIndex + 1 < t.count) melodyPlayNote(noteIndex + 1);
    else melodyStop();
  }
}

void melodyExit() {
  melodyPlaying = false;
  audioRelease();
}
