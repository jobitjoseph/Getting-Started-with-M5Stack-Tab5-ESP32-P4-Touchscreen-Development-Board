// Wi-Fi: scan, pick a network, enter the password, connect.
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
//
// Wi-Fi runs on the Tab5's ESP32-C6 over SDIO (ESP-Hosted); with the M5Tab5
// board selected the regular WiFi.h API works unchanged. The last successful
// network is saved to NVS (plain text) and reconnected at boot.
#pragma once
#include <WiFi.h>
#include <Preferences.h>
#if __has_include("esp32-hal-hosted.h")
#include "esp32-hal-hosted.h"
#endif
#include "ui.h"
#include "keyboard.h"
#include "clock.h"

struct WifiNetwork {  // "Network" is taken by the Arduino core
  String ssid;
  int32_t rssi;
  bool open;
};

constexpr int MAX_NETWORKS = 20;
constexpr int WIFI_ROWS = 4;
WifiNetwork networks[MAX_NETWORKS];
int networkCount = 0;
int wifiSelected = -1;
int wifiScroll = 0;

bool wifiStarted = false;
bool wifiScanning = false;
bool wifiConnecting = false;
uint32_t wifiConnectStartMs = 0;
String wifiPassword;
bool wifiShowPassword = false;

constexpr int WL_X = 32, WL_Y = 108, WL_ROW_W = 480, WL_ROW_H = 60, WL_ROW_GAP = 6;
Button wifiUpBtn = {520, 108, 72, 126, "", C_PANEL, C_TEXT};
Button wifiDownBtn = {520, 240, 72, 126, "", C_PANEL, C_TEXT};
constexpr int WP_X = 620, WP_W = 628;
constexpr int WP_FIELD_W = WP_W - 130;
Button wifiShowBtn = {WP_X + WP_FIELD_W + 12, 140, 118, 62, "Show", C_PANEL, C_TEXT};
Button wifiConnectBtn = {WP_X, 222, 220, 64, "Connect", C_AMBER, C_BG};
Button wifiScanBtn;
Button wifiForgetBtn;

String wifiConnectSsid, wifiConnectPass;

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
const char* signalWord(int32_t rssi) {
  if (rssi >= -60) return "strong";
  if (rssi >= -72) return "good";
  return "weak";
}

Button wifiRowRect(int row) {
  return {WL_X, WL_Y + row * (WL_ROW_H + WL_ROW_GAP), WL_ROW_W, WL_ROW_H, "", C_PANEL, C_TEXT};
}

void drawArrowButton(const Button& b, bool up, bool enabled) {
  drawButton(b, FONT_LABEL, R_SMALL);
  uint16_t c = enabled ? C_TEXT : C_BORDER2;
  int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
  if (up) M5.Display.fillTriangle(cx - 14, cy + 8, cx + 14, cy + 8, cx, cy - 10, c);
  else M5.Display.fillTriangle(cx - 14, cy - 8, cx + 14, cy - 8, cx, cy + 10, c);
}

void drawNetworkList() {
  auto& d = M5.Display;
  d.fillRect(WL_X, WL_Y, WL_ROW_W, WIFI_ROWS * (WL_ROW_H + WL_ROW_GAP), C_BG);

  if (wifiScanning || networkCount == 0) {
    const char* msg = wifiScanning ? "Scanning..." : (wifiStarted ? "No networks found" : "Starting Wi-Fi...");
    drawTextBox(WL_X, WL_Y, WL_ROW_W, WL_ROW_H, msg, FONT_LABEL, C_TEXT2, C_BG, middle_center);
  }
  for (int row = 0; row < WIFI_ROWS && !wifiScanning; row++) {
    int i = wifiScroll + row;
    if (i >= networkCount) break;
    Button r = wifiRowRect(row);
    bool sel = (i == wifiSelected);
    d.fillSmoothRoundRect(r.x, r.y, r.w, r.h, 12, sel ? C_AMBER : C_BORDER);
    int b = sel ? 3 : 1;
    d.fillSmoothRoundRect(r.x + b, r.y + b, r.w - 2 * b, r.h - 2 * b, 12 - b, C_PANEL);

    String info = String(networks[i].open ? "open" : "locked") + " - " + signalWord(networks[i].rssi);
    d.setFont(FONT_MONO);
    int infoW = d.textWidth(info);
    d.setTextColor(C_TEXT2);
    d.setTextDatum(middle_right);
    d.drawString(info, r.x + r.w - 20, r.y + r.h / 2);

    d.setTextColor(C_TEXT);
    d.setTextDatum(middle_left);
    d.drawString(fitText(networks[i].ssid, FONT_LABEL, r.w - 60 - infoW), r.x + 20, r.y + r.h / 2);
  }
  drawArrowButton(wifiUpBtn, true, wifiScroll > 0);
  drawArrowButton(wifiDownBtn, false, wifiScroll + WIFI_ROWS < networkCount);
}

void drawWifiStatus(const char* text, uint16_t color = C_TEXT2) {
  int x = wifiConnectBtn.x + wifiConnectBtn.w + 20;
  drawTextBox(x, wifiConnectBtn.y, SCREEN_W - 32 - x, wifiConnectBtn.h, text, FONT_SMALL, color, C_BG);
}

void drawPasswordPanel() {
  String label = "Password for ";
  if (wifiSelected >= 0) label += networks[wifiSelected].ssid;
  else label = "Pick a network on the left";
  drawTextBox(WP_X, WL_Y, WP_W, 30, fitText(label, FONT_SMALL, WP_W).c_str(), FONT_SMALL, C_TEXT2, C_BG);

  bool open = wifiSelected >= 0 && networks[wifiSelected].open;
  if (open) {
    drawTextField(WP_X, 140, WP_W, 62, "", false, FONT_MONO);
    drawTextBox(WP_X + 18, 150, WP_W - 36, 42, "Open network - no password needed", FONT_SMALL, C_TEXT2, C_PANEL);
  } else {
    M5.Display.fillRect(WP_X + WP_FIELD_W, 140, WP_W - WP_FIELD_W, 62, C_BG);
    drawTextField(WP_X, 140, WP_FIELD_W, 62, wifiPassword, true, &fonts::FreeMonoBold18pt7b, !wifiShowPassword);
    wifiShowBtn.label = wifiShowPassword ? "Hide" : "Show";
    drawButton(wifiShowBtn, FONT_LABEL, 12);
  }
}

void wifiToggleShowPassword() {
  wifiShowPassword = !wifiShowPassword;
  keyboardSetPasswordShown(wifiShowPassword);
  drawPasswordPanel();
}

void drawWifiConnectionState() {
  if (WiFi.status() == WL_CONNECTED) {
    String s = "Connected: " + WiFi.localIP().toString();
    drawWifiStatus(s.c_str(), C_BLUE);
  } else {
    drawWifiStatus("Not connected");
  }
}

// ---------------------------------------------------------------------------
// Saved network (NVS)
// ---------------------------------------------------------------------------
Preferences wifiPrefs;

bool wifiLoadSaved(String& ssid, String& pass) {
  wifiPrefs.begin("wifi", true);
  ssid = wifiPrefs.getString("ssid", "");
  pass = wifiPrefs.getString("pass", "");
  wifiPrefs.end();
  return ssid.length() > 0;
}

void wifiSave(const String& ssid, const String& pass) {
  wifiPrefs.begin("wifi", false);
  wifiPrefs.putString("ssid", ssid);
  wifiPrefs.putString("pass", pass);
  wifiPrefs.end();
}

void wifiForgetSaved() {
  wifiPrefs.begin("wifi", false);
  wifiPrefs.clear();
  wifiPrefs.end();
}

bool wifiHasSaved() {
  String ssid, pass;
  return wifiLoadSaved(ssid, pass);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------
void wifiInit() {
  if (wifiStarted) return;
  WiFi.persistent(false);  // we store credentials ourselves
  WiFi.mode(WIFI_STA);     // also brings up the C6 link
  wifiStarted = true;
#if __has_include("esp32-hal-hosted.h")
  // Useful for spotting outdated C6 firmware.
  uint32_t hMaj, hMin, hPat, sMaj, sMin, sPat;
  hostedGetHostVersion(&hMaj, &hMin, &hPat);
  hostedGetSlaveVersion(&sMaj, &sMin, &sPat);
  Serial.printf("Wi-Fi: ESP-Hosted on P4 %lu.%lu.%lu, on C6 %lu.%lu.%lu\n", (unsigned long)hMaj,
                (unsigned long)hMin, (unsigned long)hPat, (unsigned long)sMaj, (unsigned long)sMin,
                (unsigned long)sPat);
#endif
}

void wifiStartScan() {
  if (wifiConnecting) return;
  wifiScanning = true;
  drawNetworkList();
  WiFi.scanNetworks(true);
}

// Strongest first, de-duplicated by SSID, hidden networks skipped.
void wifiCollectResults(int found) {
  networkCount = 0;
  String selectedSsid = (wifiSelected >= 0) ? networks[wifiSelected].ssid : "";
  wifiSelected = -1;
  for (int i = 0; i < found; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    int existing = -1;
    for (int j = 0; j < networkCount; j++) {
      if (networks[j].ssid == ssid) existing = j;
    }
    if (existing >= 0) {
      networks[existing].rssi = max(networks[existing].rssi, WiFi.RSSI(i));
      continue;
    }
    if (networkCount >= MAX_NETWORKS) continue;
    networks[networkCount++] = {ssid, WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN};
  }
  WiFi.scanDelete();

  for (int i = 1; i < networkCount; i++) {
    for (int j = i; j > 0 && networks[j].rssi > networks[j - 1].rssi; j--) std::swap(networks[j], networks[j - 1]);
  }
  // Pre-select the saved network if nothing is selected yet.
  String savedSsid, savedPass;
  if (selectedSsid.length() == 0 && wifiLoadSaved(savedSsid, savedPass)) {
    selectedSsid = savedSsid;
    wifiPassword = savedPass;
  }
  for (int i = 0; i < networkCount; i++) {
    if (networks[i].ssid == selectedSsid) wifiSelected = i;
  }
  wifiScroll = 0;
}

// Called from setup(); connects to the saved network in the background.
void wifiAutoConnect() {
  String ssid, pass;
  if (!wifiLoadSaved(ssid, pass)) return;
  Serial.printf("Wi-Fi: connecting to saved network \"%s\"\n", ssid.c_str());
  wifiInit();
  WiFi.setAutoReconnect(true);
  if (pass.length() == 0) WiFi.begin(ssid.c_str());
  else WiFi.begin(ssid.c_str(), pass.c_str());
}

void wifiConnect() {
  if (wifiSelected < 0) {
    drawWifiStatus("Pick a network first", C_AMBER);
    return;
  }
  const WifiNetwork& n = networks[wifiSelected];
  wifiConnectSsid = n.ssid;
  wifiConnectPass = n.open ? "" : wifiPassword;
  WiFi.disconnect();
  WiFi.setAutoReconnect(true);
  if (n.open) WiFi.begin(n.ssid.c_str());
  else WiFi.begin(n.ssid.c_str(), wifiPassword.c_str());
  wifiConnecting = true;
  wifiConnectStartMs = millis();
  drawWifiStatus("Connecting...");
}

void drawForgetButton() {
  auto& d = M5.Display;
  d.setFont(FONT_LABEL);
  int w = d.textWidth("Forget") + 48;
  wifiForgetBtn = {wifiScanBtn.x - 16 - w, 14, w, 60, "Forget", C_PANEL, C_TEXT};
  if (wifiHasSaved()) drawButton(wifiForgetBtn, FONT_LABEL, R_SMALL);
  else d.fillRect(wifiForgetBtn.x, wifiForgetBtn.y, wifiForgetBtn.w, wifiForgetBtn.h, C_BG);
}

// ---------------------------------------------------------------------------
// Screen functions
// ---------------------------------------------------------------------------
void wifiEnter() {
  drawHeader("Wi-Fi");
  auto& d = M5.Display;
  d.setFont(FONT_LABEL);
  int w = d.textWidth("Scan again") + 48;
  wifiScanBtn = {SCREEN_W - 32 - w, 14, w, 60, "Scan again", C_PANEL, C_TEXT};
  drawButton(wifiScanBtn, FONT_LABEL, R_SMALL);
  drawForgetButton();

  drawNetworkList();
  drawPasswordPanel();
  drawButton(wifiConnectBtn, FONT_LABEL, R_SMALL);
  drawWifiConnectionState();
  kbPasswordShown = wifiShowPassword;
  keyboardDraw(KB_WIFI);

  wifiInit();
  if (networkCount == 0) wifiStartScan();
  else drawNetworkList();
}

void wifiLoop() {
  keyboardLoop();

  if (wifiScanning) {
    int found = WiFi.scanComplete();  // -1 running, -2 failed
    if (found >= 0 || found == -2) {
      wifiScanning = false;
      if (found > 0) wifiCollectResults(found);
      else networkCount = 0;
      drawNetworkList();
      drawPasswordPanel();
    }
  }

  if (wifiConnecting) {
    wl_status_t s = WiFi.status();
    if (s == WL_CONNECTED) {
      wifiConnecting = false;
      wifiSave(wifiConnectSsid, wifiConnectPass);
      drawWifiConnectionState();
      drawForgetButton();
    } else if (s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL || millis() - wifiConnectStartMs > 15000) {
      wifiConnecting = false;
      WiFi.disconnect();
      drawWifiStatus("Failed - check the password", C_AMBER);
    }
  }

  int x, y;
  if (!touchPressed(x, y)) return;
  if (hit(backBtn, x, y)) { goTo(HOME); return; }
  if (hit(wifiScanBtn, x, y)) { wifiStartScan(); return; }
  if (hit(wifiForgetBtn, x, y) && wifiHasSaved()) {
    wifiForgetSaved();
    WiFi.setAutoReconnect(false);
    WiFi.disconnect();
    wifiPassword = "";
    drawForgetButton();
    drawPasswordPanel();
    drawWifiStatus("Forgotten - not connected");
    return;
  }
  if (hit(wifiConnectBtn, x, y)) { wifiConnect(); return; }

  if (hit(wifiUpBtn, x, y) && wifiScroll > 0) {
    wifiScroll--;
    drawNetworkList();
    return;
  }
  if (hit(wifiDownBtn, x, y) && wifiScroll + WIFI_ROWS < networkCount) {
    wifiScroll++;
    drawNetworkList();
    return;
  }
  for (int row = 0; row < WIFI_ROWS && !wifiScanning; row++) {
    int i = wifiScroll + row;
    if (i < networkCount && hit(wifiRowRect(row), x, y)) {
      if (i != wifiSelected) wifiPassword = "";
      wifiSelected = i;
      drawNetworkList();
      drawPasswordPanel();
      return;
    }
  }

  bool open = wifiSelected >= 0 && networks[wifiSelected].open;
  if (!open && hit(wifiShowBtn, x, y)) {
    wifiToggleShowPassword();
    return;
  }

  // Open networks ignore typing.
  String before = wifiPassword;
  KeyResult r = keyboardHandleTouch(x, y, wifiPassword, 63);  // WPA2 max length
  if (open) wifiPassword = before;
  if (r == KEY_TOGGLE_SHOW && !open) wifiToggleShowPassword();
  else if (r == KEY_EDITED && !open) drawPasswordPanel();
}

// Wi-Fi stays connected for the radio and clock.
void wifiExit() {}
