// On-screen keyboard used by the Wi-Fi and Add Stream screens.
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
//
//   keyboardDraw(KB_WIFI);                          // in Enter()
//   KeyResult r = keyboardHandleTouch(x, y, text);  // on each tap
//   keyboardLoop();                                 // every loop
#pragma once
#include "ui.h"

enum KeyboardMode { KB_WIFI, KB_URL };
enum KeyResult { KEY_NONE, KEY_EDITED, KEY_TOGGLE_SHOW };

// Special keys use non-printable codes.
enum SpecialKey : char { K_SHIFT = 1, K_SYMBOLS, K_SPACE, K_SHOW, K_DELETE };

constexpr int KB_X = 32, KB_Y = 390, KB_W = 1216, KB_H = 310;
constexpr int KEY_W = 106, KEY_H = 50, KEY_GAP = 8;
constexpr int KB_MAX_KEYS = 64;

struct Key {
  int x, y, w, h;
  char ch;
};

Key kbKeys[KB_MAX_KEYS];
int kbKeyCount = 0;
KeyboardMode kbMode = KB_WIFI;
bool kbShift = false;          // one-shot
bool kbSymbols = false;
bool kbPasswordShown = false;
int kbFlashKey = -1;
uint32_t kbFlashMs = 0;

const char* const KB_LETTERS[4] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
const char* const KB_SYMBOLS[4] = {"1234567890", "?=&_-.:/@!", "#$%*+~^()", "[]{}<>|;,"};

void kbAddRow(int y, const char* chars, const int* widths, int n) {
  int total = (n - 1) * KEY_GAP;
  for (int i = 0; i < n; i++) total += widths ? widths[i] : KEY_W;
  int x = (SCREEN_W - total) / 2;
  for (int i = 0; i < n && kbKeyCount < KB_MAX_KEYS; i++) {
    int w = widths ? widths[i] : KEY_W;
    kbKeys[kbKeyCount++] = {x, y, w, KEY_H, chars[i]};
    x += w + KEY_GAP;
  }
}

void kbLayout() {
  kbKeyCount = 0;
  const int rows = 5;
  int y = KB_Y + (KB_H - rows * KEY_H - (rows - 1) * KEY_GAP) / 2;
  const char* const* page = kbSymbols ? KB_SYMBOLS : KB_LETTERS;
  for (int r = 0; r < 4; r++) {
    kbAddRow(y, page[r], nullptr, strlen(page[r]));
    y += KEY_H + KEY_GAP;
  }
  if (kbMode == KB_WIFI) {
    const char keys[] = {K_SHIFT, K_SYMBOLS, K_SPACE, K_SHOW, K_DELETE};
    const int widths[] = {150, 150, 520, 150, 150};
    kbAddRow(y, keys, widths, 5);
  } else {
    // URLs always need : / . - so they stay on the bottom row.
    const char keys[] = {K_SHIFT, K_SYMBOLS, ':', '/', '.', K_SPACE, '-', K_DELETE};
    const int widths[] = {150, 106, 106, 106, 106, 216, 106, 150};
    kbAddRow(y, keys, widths, 8);
  }
}

String kbLabel(char ch) {
  switch (ch) {
    case K_SHIFT:   return "Shift";
    case K_SYMBOLS: return kbSymbols ? "abc" : "#+=";
    case K_SPACE:   return "space";
    case K_SHOW:    return kbPasswordShown ? "Hide" : "Show";
    case K_DELETE:  return "Delete";
  }
  if (kbShift && ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
  return String(ch);
}

void kbDrawKey(int i, bool pressed) {
  const Key& k = kbKeys[i];
  bool special = k.ch < 32 && k.ch != K_SPACE;
  uint16_t bg = special ? C_BORDER2 : C_BORDER;
  if (k.ch == K_SHIFT && kbShift) bg = C_AMBER;
  if (pressed) bg = C_BLUE;
  uint16_t fg = (k.ch == K_SHIFT && kbShift) ? C_BG : (k.ch == K_SPACE ? C_TEXT2 : C_TEXT);

  auto& d = M5.Display;
  d.fillSmoothRoundRect(k.x, k.y, k.w, k.h, R_KEY, bg);
  d.setFont(k.ch < 32 ? FONT_LABEL : &fonts::FreeMonoBold18pt7b);
  d.setTextColor(fg);
  d.setTextDatum(middle_center);
  d.drawString(kbLabel(k.ch), k.x + k.w / 2, k.y + k.h / 2 + 1);
}

void kbDrawKeys() {
  for (int i = 0; i < kbKeyCount; i++) kbDrawKey(i, false);
}

void keyboardDraw(KeyboardMode mode) {
  kbMode = mode;
  kbShift = false;
  kbSymbols = false;
  kbFlashKey = -1;
  kbLayout();
  M5.Display.fillSmoothRoundRect(KB_X, KB_Y, KB_W, KB_H, 16, C_DARK);
  kbDrawKeys();
}

// Edits 'text' in place. Returns KEY_NONE if the tap missed the keyboard.
KeyResult keyboardHandleTouch(int x, int y, String& text, size_t maxLen = 200) {
  if (x < KB_X || x >= KB_X + KB_W || y < KB_Y || y >= KB_Y + KB_H) return KEY_NONE;

  // Half the gap around each key counts as part of it.
  int hitKey = -1;
  for (int i = 0; i < kbKeyCount; i++) {
    const Key& k = kbKeys[i];
    if (x >= k.x - KEY_GAP / 2 && x < k.x + k.w + KEY_GAP / 2 && y >= k.y - KEY_GAP / 2 &&
        y < k.y + k.h + KEY_GAP / 2) {
      hitKey = i;
      break;
    }
  }
  if (hitKey < 0) return KEY_NONE;
  char ch = kbKeys[hitKey].ch;

  switch (ch) {
    case K_SHIFT:
      kbShift = !kbShift;
      kbDrawKeys();
      return KEY_NONE;
    case K_SYMBOLS:
      kbSymbols = !kbSymbols;
      kbShift = false;
      kbLayout();
      M5.Display.fillSmoothRoundRect(KB_X, KB_Y, KB_W, KB_H, 16, C_DARK);
      kbDrawKeys();
      return KEY_NONE;
    case K_SHOW:
      return KEY_TOGGLE_SHOW;
    case K_DELETE:
      if (text.length() > 0) text.remove(text.length() - 1);
      break;
    case K_SPACE:
      ch = ' ';
      [[fallthrough]];
    default:
      if (text.length() >= maxLen) return KEY_NONE;
      if (kbShift && ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
      text += ch;
      if (kbShift) {
        kbShift = false;
        kbDrawKeys();
      }
      break;
  }

  if (kbFlashKey >= 0) kbDrawKey(kbFlashKey, false);
  kbFlashKey = hitKey;
  kbFlashMs = millis();
  kbDrawKey(hitKey, true);
  return KEY_EDITED;
}

void keyboardSetPasswordShown(bool shown) {
  kbPasswordShown = shown;
  for (int i = 0; i < kbKeyCount; i++) {
    if (kbKeys[i].ch == K_SHOW) kbDrawKey(i, false);
  }
}

// Clears the key-press highlight after 120 ms.
void keyboardLoop() {
  if (kbFlashKey >= 0 && millis() - kbFlashMs > 120) {
    kbDrawKey(kbFlashKey, false);
    kbFlashKey = -1;
  }
}
