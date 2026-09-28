// Add Stream: enter a name and MP3 stream URL for the radio's station list.
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
#pragma once
#include "ui.h"
#include "keyboard.h"
#include "app_radio.h"

String newStationName;
String newStationUrl;
int addFocus = 1;  // 0 = name, 1 = URL

constexpr int AF_X = 32, AF_W = 952, AF_H = 60;
const int AF_LABEL_Y[2] = {108, 208};
const int AF_FIELD_Y[2] = {134, 234};
Button addSaveBtn = {1008, 152, 240, 72, "Save", C_AMBER, C_BG};
Button addCancelBtn = {1008, 236, 240, 60, "Cancel", C_PANEL, C_TEXT};
constexpr int ADD_MSG_Y = 304;

void drawAddFields() {
  drawTextField(AF_X, AF_FIELD_Y[0], AF_W, AF_H, newStationName, addFocus == 0, FONT_LABEL);
  drawTextField(AF_X, AF_FIELD_Y[1], AF_W, AF_H, newStationUrl, addFocus == 1, FONT_MONO);
}

void drawAddMessage(const char* text) {
  drawTextBox(AF_X, ADD_MSG_Y, AF_W, 36, text, FONT_SMALL, C_AMBER, C_BG);
}

void addStreamSave() {
  String name = newStationName;
  String url = newStationUrl;
  name.trim();
  url.trim();
  if (name.length() == 0) {
    drawAddMessage("Please enter a station name.");
    return;
  }
  bool httpOk = (url.startsWith("http://") && url.length() > 7) || (url.startsWith("https://") && url.length() > 8);
  if (!httpOk) {
    drawAddMessage("The URL must start with http:// or https://");
    return;
  }
  const char* error = radioAddStation(name, url);
  if (error) {
    drawAddMessage(error);
    return;
  }
  goTo(RADIO);
}

void addStreamEnter() {
  drawHeader("Add stream", "Radio");
  auto& d = M5.Display;
  d.setFont(FONT_SMALL);
  d.setTextColor(C_TEXT2);
  d.setTextDatum(top_left);
  d.drawString("Station name", AF_X, AF_LABEL_Y[0]);
  d.drawString("Stream URL (MP3)", AF_X, AF_LABEL_Y[1]);

  newStationName = "My Stream";
  newStationUrl = "http://";
  addFocus = 1;
  drawAddFields();
  drawButton(addSaveBtn, FONT_TITLE, 16);
  drawButton(addCancelBtn, FONT_LABEL, 16);
  keyboardDraw(KB_URL);
}

void addStreamLoop() {
  keyboardLoop();
  int x, y;
  if (!touchPressed(x, y)) return;
  if (hit(backBtn, x, y) || hit(addCancelBtn, x, y)) { goTo(RADIO); return; }
  if (hit(addSaveBtn, x, y)) { addStreamSave(); return; }

  for (int f = 0; f < 2; f++) {
    Button field = {AF_X, AF_FIELD_Y[f], AF_W, AF_H, "", C_PANEL, C_TEXT};
    if (hit(field, x, y) && addFocus != f) {
      addFocus = f;
      drawAddFields();
      return;
    }
  }

  String& target = (addFocus == 0) ? newStationName : newStationUrl;
  if (keyboardHandleTouch(x, y, target, addFocus == 0 ? 40 : 200) == KEY_EDITED) {
    drawTextField(AF_X, AF_FIELD_Y[addFocus], AF_W, AF_H, target, true, addFocus == 0 ? FONT_LABEL : FONT_MONO);
  }
}

void addStreamExit() {}
