// Shared UI: screen list, palette, fonts, buttons, sliders, icons and header.
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
#pragma once
#include <M5Unified.h>

enum Screen { HOME, CAMERA, RECORDER, MICMETER, MELODY, RADIO, ADDSTREAM, WIFI, DATETIME };

// Requests a screen change; it happens at the start of the next loop().
void goTo(Screen next);

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------
// The panel is natively 720 x 1280 portrait. Use 1 instead of 3 if the
// picture is upside down.
constexpr int SCREEN_ROTATION = 3;
constexpr int SCREEN_W = 1280;
constexpr int SCREEN_H = 720;
constexpr int HEADER_H = 88;

// ---------------------------------------------------------------------------
// Palette (RGB565; color888 would not fit the uint16_t Button fields)
// ---------------------------------------------------------------------------
constexpr uint16_t C_BG      = lgfx::color565(0x11, 0x14, 0x18);
constexpr uint16_t C_PANEL   = lgfx::color565(0x1c, 0x21, 0x27);
constexpr uint16_t C_DARK    = lgfx::color565(0x0b, 0x0d, 0x10);
constexpr uint16_t C_BORDER  = lgfx::color565(0x2a, 0x30, 0x38);
constexpr uint16_t C_BORDER2 = lgfx::color565(0x3a, 0x42, 0x4d);
constexpr uint16_t C_TEXT    = lgfx::color565(0xe8, 0xea, 0xed);
constexpr uint16_t C_TEXT2   = lgfx::color565(0x9a, 0xa3, 0xad);
constexpr uint16_t C_AMBER   = lgfx::color565(0xf0, 0xa5, 0x3a);
constexpr uint16_t C_BLUE    = lgfx::color565(0x4e, 0xa1, 0xff);

constexpr int R_TILE = 24;
constexpr int R_BTN  = 20;
constexpr int R_SMALL = 14;
constexpr int R_KEY  = 10;

static const lgfx::IFont* const FONT_SMALL = &fonts::FreeSans12pt7b;
static const lgfx::IFont* const FONT_LABEL = &fonts::FreeSansBold12pt7b;
static const lgfx::IFont* const FONT_TITLE = &fonts::FreeSansBold18pt7b;
static const lgfx::IFont* const FONT_MONO  = &fonts::FreeMonoBold12pt7b;

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
struct Button {
  int x, y, w, h;
  const char* label;
  uint16_t bg, fg;
};

// Drawn symbols, since the built-in fonts are ASCII only.
enum ButtonIcon { BI_NONE, BI_DOT, BI_SQUARE, BI_PLAY };

void drawButton(const Button& b, const lgfx::IFont* font = FONT_TITLE, int radius = R_BTN,
                ButtonIcon icon = BI_NONE, uint16_t iconColor = C_TEXT) {
  auto& d = M5.Display;
  if (b.bg == C_PANEL || b.bg == C_BG) {
    d.fillSmoothRoundRect(b.x, b.y, b.w, b.h, radius, C_BORDER2);
    d.fillSmoothRoundRect(b.x + 1, b.y + 1, b.w - 2, b.h - 2, radius - 1, b.bg);
  } else {
    d.fillSmoothRoundRect(b.x, b.y, b.w, b.h, radius, b.bg);
  }
  d.setFont(font);
  int textW = (b.label && b.label[0]) ? d.textWidth(b.label) : 0;
  int iconW = (icon == BI_NONE) ? 0 : 26;
  int gap = (iconW && textW) ? 14 : 0;
  int x = b.x + (b.w - iconW - gap - textW) / 2;
  int cy = b.y + b.h / 2;

  switch (icon) {
    case BI_DOT:    d.fillSmoothCircle(x + 13, cy, 13, iconColor); break;
    case BI_SQUARE: d.fillSmoothRoundRect(x + 1, cy - 12, 24, 24, 4, iconColor); break;
    case BI_PLAY:   d.fillTriangle(x + 3, cy - 13, x + 3, cy + 13, x + 25, cy, iconColor); break;
    case BI_NONE:   break;
  }
  if (textW) {
    d.setTextColor(b.fg);
    d.setTextDatum(middle_left);
    d.drawString(b.label, x + iconW + gap, cy);
  }
}

bool hit(const Button& b, int x, int y) {
  return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

// True once per new finger-down, so a long press fires a single action.
bool touchPressed(int& x, int& y) {
  auto t = M5.Touch.getDetail();
  if (!t.wasPressed()) return false;
  x = t.x;
  y = t.y;
  return true;
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------
void drawTextBox(int x, int y, int w, int h, const char* text, const lgfx::IFont* font,
                 uint16_t fg, uint16_t bg, textdatum_t datum = middle_left) {
  auto& d = M5.Display;
  d.fillRect(x, y, w, h, bg);
  d.setFont(font);
  d.setTextColor(fg);
  d.setTextDatum(datum);
  int tx = x;
  if (datum == middle_center) tx = x + w / 2;
  if (datum == middle_right) tx = x + w;
  d.drawString(text, tx, y + h / 2);
}

// Truncates with "..." to fit maxW pixels.
String fitText(const String& text, const lgfx::IFont* font, int maxW) {
  auto& d = M5.Display;
  d.setFont(font);
  if (d.textWidth(text) <= maxW) return text;
  String s = text;
  while (s.length() > 0 && d.textWidth(s + "...") > maxW) s.remove(s.length() - 1);
  return s + "...";
}

// One-line input box. Overlong text shows its end, where the cursor is.
void drawTextField(int x, int y, int w, int h, const String& text, bool focused,
                   const lgfx::IFont* font, bool masked = false) {
  auto& d = M5.Display;
  if (focused) {
    d.fillSmoothRoundRect(x, y, w, h, 12, C_BLUE);
    d.fillSmoothRoundRect(x + 3, y + 3, w - 6, h - 6, 10, C_PANEL);
  } else {
    d.fillSmoothRoundRect(x, y, w, h, 12, C_BORDER2);
    d.fillSmoothRoundRect(x + 1, y + 1, w - 2, h - 2, 11, C_PANEL);
  }
  int tx = x + 18, cy = y + h / 2, maxW = w - 36 - 6;
  int endX = tx;
  if (masked) {
    int n = min((int)text.length(), maxW / 20);
    for (int i = 0; i < n; i++) d.fillSmoothCircle(tx + 7 + i * 20, cy, 7, C_TEXT);
    endX = tx + n * 20;
  } else {
    d.setFont(font);
    String shown = text;
    while (shown.length() > 0 && d.textWidth(shown) > maxW) shown.remove(0, 1);
    d.setTextColor(C_TEXT);
    d.setTextDatum(middle_left);
    d.drawString(shown, tx, cy);
    endX = tx + d.textWidth(shown);
  }
  if (focused) d.fillRect(endX + 2, cy - 16, 3, 32, C_BLUE);
}

// ---------------------------------------------------------------------------
// Slider row: [-]  label + slider  [+]  "70%"
// ---------------------------------------------------------------------------
// Edits the int that 'value' points to, from minValue to 100 percent.
struct SliderRow {
  int x, y, w, btn;
  uint16_t bg;
  const char* label;
  int* value;
  int minValue;
};

constexpr int SLIDER_PCT_W = 100;
constexpr int SLIDER_GAP = 20;

Button sliderMinusBtn(const SliderRow& r) { return {r.x, r.y, r.btn, r.btn, "-", r.bg, C_TEXT}; }
Button sliderPlusBtn(const SliderRow& r) {
  return {r.x + r.w - SLIDER_PCT_W - SLIDER_GAP - r.btn, r.y, r.btn, r.btn, "+", r.bg, C_TEXT};
}
int sliderX(const SliderRow& r) { return r.x + r.btn + SLIDER_GAP; }
int sliderW(const SliderRow& r) { return r.w - 2 * r.btn - 3 * SLIDER_GAP - SLIDER_PCT_W; }

M5Canvas sliderCanvas(&M5.Display);

void sliderRowDrawValue(const SliderRow& r) {
  int sw = sliderW(r), sh = 40;
  if (sliderCanvas.width() != sw) {
    sliderCanvas.deleteSprite();
    sliderCanvas.setPsram(true);
    sliderCanvas.createSprite(sw, sh);
  }
  // The knob travels between knobR and (sw - knobR) so it is never clipped.
  int knobR = 16, trackH = 10;
  int kx = knobR + (sw - 2 * knobR) * (*r.value) / 100;
  sliderCanvas.fillSprite(r.bg);
  sliderCanvas.fillSmoothRoundRect(0, (sh - trackH) / 2, sw, trackH, trackH / 2, C_BORDER2);
  sliderCanvas.fillSmoothRoundRect(0, (sh - trackH) / 2, kx, trackH, trackH / 2, C_BLUE);
  sliderCanvas.fillSmoothCircle(kx, sh / 2, knobR, C_BLUE);
  sliderCanvas.fillSmoothCircle(kx, sh / 2, knobR - 5, C_TEXT);
  sliderCanvas.pushSprite(sliderX(r), r.y + r.btn - sh);

  char pct[16];
  snprintf(pct, sizeof(pct), "%d%%", *r.value);
  drawTextBox(r.x + r.w - SLIDER_PCT_W, r.y, SLIDER_PCT_W, r.btn, pct, &fonts::FreeMonoBold18pt7b, C_TEXT,
              r.bg, middle_right);
}

void sliderRowDraw(const SliderRow& r) {
  drawButton(sliderMinusBtn(r), FONT_TITLE, 16);
  drawButton(sliderPlusBtn(r), FONT_TITLE, 16);
  auto& d = M5.Display;
  d.setFont(FONT_SMALL);
  d.setTextColor(C_TEXT2);
  d.setTextDatum(top_left);
  d.drawString(r.label, sliderX(r), r.y);
  sliderRowDrawValue(r);
}

// Call every loop. Returns true when the value changed.
bool sliderRowUpdate(const SliderRow& r) {
  auto t = M5.Touch.getDetail();
  int old = *r.value;

  if (t.wasPressed()) {
    if (hit(sliderMinusBtn(r), t.x, t.y)) *r.value = max(r.minValue, *r.value - 10);
    if (hit(sliderPlusBtn(r), t.x, t.y)) *r.value = min(100, *r.value + 10);
  }

  // Drag only if the touch started on the slider, so swipes elsewhere don't move it.
  int sx = sliderX(r), sw = sliderW(r);
  bool startedOnSlider = t.base_x >= sx - 20 && t.base_x < sx + sw + 20 &&
                         t.base_y >= r.y && t.base_y < r.y + r.btn;
  if (t.isPressed() && startedOnSlider) {
    *r.value = constrain((t.x - sx - 16) * 100 / (sw - 32), r.minValue, 100);
  }

  if (*r.value == old) return false;
  sliderRowDrawValue(r);
  return true;
}

// ---------------------------------------------------------------------------
// Brightness and transition settings (RAM only, reset on reboot)
// ---------------------------------------------------------------------------
constexpr int BRIGHTNESS_MIN = 10;
int brightnessPercent = 60;

uint8_t brightnessValue() { return (uint8_t)(255 * brightnessPercent / 100); }
void applyBrightness() { M5.Display.setBrightness(brightnessValue()); }

enum Transition { TR_DIM, TR_FADE, TR_WIPE, TR_NONE, TR_COUNT };
const char* const TRANSITION_NAMES[TR_COUNT] = {"Soft dim", "Fade", "Wipe", "None"};
Transition transitionMode = TR_DIM;

// ---------------------------------------------------------------------------
// Line icons
// ---------------------------------------------------------------------------
// Icons are defined on a 24 x 24 grid (same as the mockup SVGs) and scaled to
// the requested size with anti-aliased primitives.
static float    ico_x, ico_y, ico_s, ico_w;
static uint16_t ico_fg, ico_bg;
static lgfx::LGFXBase* ico_dst = &M5.Display;

// Redirects icon drawing to a canvas (e.g. the Home status bar).
void iconDrawOn(lgfx::LGFXBase& dst) { ico_dst = &dst; }

void iconBegin(int x, int y, int size, float stroke, uint16_t fg, uint16_t bg) {
  ico_x = x;
  ico_y = y;
  ico_s = size / 24.0f;
  ico_w = stroke * ico_s;
  ico_fg = fg;
  ico_bg = bg;
}
static int ipx(float u) { return (int)lroundf(ico_x + u * ico_s); }
static int ipy(float u) { return (int)lroundf(ico_y + u * ico_s); }

void iconLine(float x0, float y0, float x1, float y1) {
  ico_dst->drawWideLine(ipx(x0), ipy(y0), ipx(x1), ipy(y1), ico_w / 2, ico_fg);
}

// pts = x0, y0, x1, y1, ...
void iconPoly(const float* pts, int n, bool closed) {
  for (int i = 0; i + 1 < n; i++) iconLine(pts[i * 2], pts[i * 2 + 1], pts[i * 2 + 2], pts[i * 2 + 3]);
  if (closed) iconLine(pts[(n - 1) * 2], pts[(n - 1) * 2 + 1], pts[0], pts[1]);
}

// Angles in degrees, 0 = right, 90 = down. Ends get round caps.
void iconArc(float cx, float cy, float r, float a0, float a1) {
  int rr = lroundf(r * ico_s), hw = lroundf(ico_w / 2);
  ico_dst->fillArc(ipx(cx), ipy(cy), rr - hw, rr + hw, a0, a1, ico_fg);
  for (float a : {a0, a1}) {
    float rad = a * DEG_TO_RAD;
    ico_dst->fillSmoothCircle(ipx(cx + r * cosf(rad)), ipy(cy + r * sinf(rad)), hw, ico_fg);
  }
}

void iconCircle(float cx, float cy, float r) {
  ico_dst->fillSmoothCircle(ipx(cx), ipy(cy), lroundf(r * ico_s + ico_w / 2), ico_fg);
  ico_dst->fillSmoothCircle(ipx(cx), ipy(cy), lroundf(r * ico_s - ico_w / 2), ico_bg);
}

void iconDot(float cx, float cy, float r) {
  ico_dst->fillSmoothCircle(ipx(cx), ipy(cy), lroundf(r * ico_s), ico_fg);
}

void iconRoundRect(float x, float y, float w, float h, float r) {
  float hw = ico_w / 2;
  int ox = lroundf(ico_x + (x * ico_s) - hw), oy = lroundf(ico_y + (y * ico_s) - hw);
  int ow = lroundf(w * ico_s + ico_w), oh = lroundf(h * ico_s + ico_w);
  int iw = lroundf(ico_w);
  ico_dst->fillSmoothRoundRect(ox, oy, ow, oh, lroundf(r * ico_s + hw), ico_fg);
  ico_dst->fillSmoothRoundRect(ox + iw, oy + iw, ow - 2 * iw, oh - 2 * iw, max(0L, lroundf(r * ico_s - hw)), ico_bg);
}

void iconCamera(int x, int y, int size, uint16_t fg, uint16_t bg) {
  iconBegin(x, y, size, 1.6f, fg, bg);
  const float body[] = {3, 8, 6.5f, 8, 8.5f, 5, 15.5f, 5, 17.5f, 8, 21, 8, 21, 19, 3, 19};
  iconPoly(body, 8, true);
  iconCircle(12, 13, 3.5f);
}

void iconMic(int x, int y, int size, uint16_t fg, uint16_t bg) {
  iconBegin(x, y, size, 1.6f, fg, bg);
  iconRoundRect(9, 3, 6, 11, 3);
  iconArc(12, 11, 7, 0, 180);
  iconLine(12, 18, 12, 21);
}

void iconBars(int x, int y, int size, uint16_t fg, uint16_t bg) {
  iconBegin(x, y, size, 1.8f, fg, bg);
  iconLine(5, 20, 5, 12);
  iconLine(10, 20, 10, 6);
  iconLine(15, 20, 15, 9);
  iconLine(20, 20, 20, 4);
}

void iconRadio(int x, int y, int size, uint16_t fg, uint16_t bg) {
  iconBegin(x, y, size, 1.6f, fg, bg);
  iconRoundRect(3, 8, 18, 12, 2);
  iconLine(7, 8, 17, 3);
  iconCircle(15.5f, 14, 2.5f);
  iconLine(6.5f, 12.5f, 10.5f, 12.5f);
  iconLine(6.5f, 15.5f, 10.5f, 15.5f);
}

void iconWifi(int x, int y, int size, float stroke, uint16_t fg, uint16_t bg) {
  iconBegin(x, y, size, stroke, fg, bg);
  iconArc(12, 19.8f, 15, 270 - 42, 270 + 42);
  iconArc(12, 19.8f, 10, 270 - 44, 270 + 44);
  iconArc(12, 19.8f, 5, 270 - 44, 270 + 44);
  iconDot(12, 19.5f, 1.2f);
}

void iconMusicNote(int x, int y, int size, uint16_t fg, uint16_t bg) {
  iconBegin(x, y, size, 1.6f, fg, bg);
  const float flag[] = {9, 18, 9, 5, 20, 3, 20, 16};
  iconPoly(flag, 4, false);
  iconCircle(6.5f, 18, 2.5f);
  iconCircle(17.5f, 16, 2.5f);
}

void iconBattery(int x, int y, int size, int percent, uint16_t fg, uint16_t bg) {
  iconBegin(x, y, size, 2.0f, fg, bg);
  iconRoundRect(2, 7, 19, 10, 2);
  iconLine(24, 11, 24, 13);
  percent = constrain(percent, 0, 100);
  int fillW = lroundf(12 * ico_s * percent / 100.0f);
  if (fillW > 0) ico_dst->fillRect(ipx(4.5f), ipy(9.5f), fillW, lroundf(5 * ico_s), fg);
}

void iconSun(int x, int y, int size, uint16_t fg, uint16_t bg) {
  iconBegin(x, y, size, 2.0f, fg, bg);
  iconCircle(12, 12, 4);
  for (int i = 0; i < 8; i++) {
    float a = i * 45 * DEG_TO_RAD;
    iconLine(12 + 7.5f * cosf(a), 12 + 7.5f * sinf(a), 12 + 10.5f * cosf(a), 12 + 10.5f * sinf(a));
  }
}

void iconLayers(int x, int y, int size, uint16_t fg, uint16_t bg) {
  iconBegin(x, y, size, 2.0f, fg, bg);
  iconRoundRect(9, 3, 12, 12, 2);
  iconRoundRect(3, 9, 12, 12, 2);
}

void iconChevronLeft(int x, int y, int size, uint16_t fg, uint16_t bg) {
  iconBegin(x, y, size, 2.4f, fg, bg);
  iconLine(15, 18, 9, 12);
  iconLine(9, 12, 15, 6);
}

// ---------------------------------------------------------------------------
// App header: clears the screen and draws "< Home" + title. Sets backBtn.
// ---------------------------------------------------------------------------
Button backBtn;

void drawHeader(const char* title, const char* backLabel = "Home") {
  auto& d = M5.Display;
  d.fillScreen(C_BG);

  d.setFont(FONT_LABEL);
  int textW = d.textWidth(backLabel);
  backBtn = {32, 14, 16 + 26 + 8 + textW + 22, 60, backLabel, C_PANEL, C_TEXT};

  d.fillSmoothRoundRect(backBtn.x, backBtn.y, backBtn.w, backBtn.h, R_SMALL, C_PANEL);
  iconChevronLeft(backBtn.x + 16, backBtn.y + 17, 26, C_TEXT, C_PANEL);
  d.setTextColor(C_TEXT);
  d.setTextDatum(middle_left);
  d.drawString(backLabel, backBtn.x + 16 + 26 + 8, backBtn.y + backBtn.h / 2);

  d.setFont(FONT_TITLE);
  d.drawString(title, backBtn.x + backBtn.w + 24, HEADER_H / 2);

  d.drawFastHLine(0, HEADER_H - 1, SCREEN_W, C_BORDER);
}
