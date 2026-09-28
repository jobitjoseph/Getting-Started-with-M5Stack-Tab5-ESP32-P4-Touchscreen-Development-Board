// Camera: live preview from the SC2356 sensor with Snap (freeze) and Resume.
//
// Copyright (c) 2026 Jobit Joseph, Semicon Media
// SPDX-License-Identifier: MIT
//
// Pipeline: SC2356 (MIPI-CSI, 24 MHz XCLK on GPIO 36) -> ESP_Video / ISP
// (RGB565, AE/AWB) -> PPA crops the centre 4:3, scales to 768 x 576, mirrors
// and byte-swaps straight into an M5Canvas buffer -> pushed to the display.
//
// Capture runs in its own FreeRTOS task because waiting for frames blocks;
// loop() only presents finished frames, so the UI never freezes.
#pragma once

#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 3, 11)
#error "Please select Tools > Board > esp32 > M5Tab5 (package 'esp32 by Espressif Systems' 3.3.11 or newer). The M5Stack board package has no camera library (ESP_Video)."
#endif

#include <ESP_Video.h>
#include <driver/ppa.h>
#include <driver/i2c_master.h>
#include <esp_heap_caps.h>
#include "ui.h"

constexpr int PREVIEW_W = 768, PREVIEW_H = 576;
constexpr int PREVIEW_X = 32, PREVIEW_Y = HEADER_H + 28;

// The PPA scales in 1/16 steps: 944 x 708 * 13/16 = 767 x 575.
constexpr float CAM_SCALE = 13.0f / 16.0f;
constexpr int CROP_W = 944, CROP_H = 708;

constexpr bool CAM_MIRROR_X = true;
constexpr bool CAM_MIRROR_Y = false;

constexpr int CAM_XCLK_PIN = 36;
constexpr int CAM_BUFFERS = 2;
constexpr uint32_t CAM_NO_FRAME_MS = 5000;

ESPVideoClass camVideo;
ESPVideoCaptureDevClass camCapture;
ppa_client_handle_t camPpa = nullptr;
uint16_t* camPixels = nullptr;
size_t camPixelsSize = 0;
M5Canvas camCanvas(&M5.Display);

// Shared between camTask and loop().
enum CamStep { CAM_IDLE, CAM_CLOCK, CAM_SENSOR, CAM_STREAM, CAM_WAITING, CAM_LIVE, CAM_FAILED };
volatile CamStep camStep = CAM_IDLE;
const char* volatile camError = "";
volatile bool camFrameReady = false;
volatile bool camStopRequest = false;
volatile bool camTaskDone = true;
TaskHandle_t camTaskHandle = nullptr;

bool camFrozen = false;
CamStep camShownStep = CAM_IDLE;
uint32_t camStartMs = 0;
bool camNoFrameShown = false;
uint32_t camFrames = 0, camFpsMs = 0;

Button camSnapBtn = {950, 220, 180, 180, "Snap", C_AMBER, C_BG};
Button camResumeBtn = {890, 428, 300, 72, "Resume live", C_PANEL, C_TEXT};

// ---------------------------------------------------------------------------
// Drawing (loop() only)
// ---------------------------------------------------------------------------
void drawCamBadge() {
  auto& c = camCanvas;
  const char* text = camFrozen ? "FROZEN" : "LIVE";
  c.setFont(FONT_MONO);
  int w = 14 + 12 + 8 + c.textWidth(text) + 14;
  c.fillSmoothRoundRect(18, 18, w, 36, 18, C_PANEL);
  c.fillSmoothCircle(18 + 14 + 6, 36, 6, camFrozen ? C_BLUE : C_AMBER);
  c.setTextColor(C_TEXT);
  c.setTextDatum(middle_left);
  c.drawString(text, 18 + 14 + 12 + 8, 37);
}

void drawSnapButton(bool enabled) {
  auto& d = M5.Display;
  int cx = camSnapBtn.x + 90, cy = camSnapBtn.y + 90;
  uint16_t fill = enabled ? C_AMBER : C_BORDER2;
  d.fillSmoothCircle(cx, cy, 90, lgfx::color565(0x3a, 0x2a, 0x12));
  d.fillSmoothCircle(cx, cy, 84, fill);
  iconCamera(cx - 20, cy - 44, 40, C_BG, fill);
  d.setFont(FONT_TITLE);
  d.setTextColor(C_BG);
  d.setTextDatum(middle_center);
  d.drawString("Snap", cx, cy + 22);
}

void drawCamMessage(const char* line1, const char* line2) {
  auto& d = M5.Display;
  d.fillRect(PREVIEW_X, PREVIEW_Y + PREVIEW_H / 2 - 60, PREVIEW_W, 120, C_DARK);
  d.setTextDatum(middle_center);
  d.setFont(FONT_TITLE);
  d.setTextColor(C_TEXT2);
  d.drawString(line1, PREVIEW_X + PREVIEW_W / 2, PREVIEW_Y + PREVIEW_H / 2 - 20);
  d.setFont(FONT_SMALL);
  d.drawString(line2, PREVIEW_X + PREVIEW_W / 2, PREVIEW_Y + PREVIEW_H / 2 + 24);
}

const char* camStepText(CamStep s) {
  switch (s) {
    case CAM_CLOCK:   return "Step 1 of 4: 24 MHz sensor clock";
    case CAM_SENSOR:  return "Step 2 of 4: detecting the sensor";
    case CAM_STREAM:  return "Step 3 of 4: starting the video stream";
    case CAM_WAITING: return "Step 4 of 4: waiting for the first frame";
    default:          return "";
  }
}

// ---------------------------------------------------------------------------
// Camera task
// ---------------------------------------------------------------------------
bool camFail(const char* reason) {
  camError = reason;
  camStep = CAM_FAILED;
  Serial.printf("Camera: failed - %s\n", reason);
  return false;
}

bool camStartHardware() {
  // Needs the LEDC PLL clock source selected in setup().
  camStep = CAM_CLOCK;
  Serial.println("Camera: starting the 24 MHz clock");
  if (!ledcAttach(CAM_XCLK_PIN, 24000000, 1)) return camFail("could not start the 24 MHz clock");
  ledcWrite(CAM_XCLK_PIN, 1);
  delay(50);

  // SCCB shares the internal I2C bus M5Unified already opened on port 1.
  camStep = CAM_SENSOR;
  Serial.println("Camera: detecting the sensor");
  i2c_master_bus_handle_t bus = nullptr;
  if (i2c_master_get_bus_handle(I2C_NUM_1, &bus) != ESP_OK || !bus) return camFail("internal I2C bus not found");
  ESPVideoCamConfigClass camConfig;
  camConfig.begin(bus, 400000);
  ESPVideoCSIConfigClass csiConfig;
  // The MIPI LDO is shared with the display; join it like M5Stack's demo does.
  csiConfig.begin(camConfig, false);
  if (!camVideo.begin(csiConfig)) return camFail("sensor not detected");

  camStep = CAM_STREAM;
  Serial.println("Camera: starting the stream");
  if (!camCapture.begin(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, CAM_BUFFERS)) return camFail("could not open the video device");
  if (!camCapture.setFormat(ESP_VIDEO_FORMAT_RGB565)) return camFail("RGB565 not supported");
  if (!camCapture.startCapture()) return camFail("could not start streaming");
  Serial.printf("Camera: stream is %lu x %lu\n", (unsigned long)camCapture.getWidth(),
                (unsigned long)camCapture.getHeight());

  // PPA writes via DMA: buffer must be 64-byte aligned and sized.
  camPixelsSize = (PREVIEW_W * PREVIEW_H * 2 + 63) & ~63;
  camPixels = (uint16_t*)heap_caps_aligned_calloc(64, 1, camPixelsSize, MALLOC_CAP_SPIRAM);
  if (!camPixels) return camFail("not enough PSRAM");

  ppa_client_config_t ppaConfig = {};
  ppaConfig.oper_type = PPA_OPERATION_SRM;
  if (ppa_register_client(&ppaConfig, &camPpa) != ESP_OK) return camFail("PPA not available");
  return true;
}

bool camConvertFrame(ESPVideoBufferClass& frame) {
  uint32_t w = camCapture.getWidth(), h = camCapture.getHeight();
  uint32_t cropW = (w < (uint32_t)CROP_W) ? w : CROP_W;
  uint32_t cropH = (h < (uint32_t)CROP_H) ? h : CROP_H;
  ppa_srm_oper_config_t op = {};
  op.in.buffer = frame.data();
  op.in.pic_w = w;
  op.in.pic_h = h;
  op.in.block_w = cropW;
  op.in.block_h = cropH;
  op.in.block_offset_x = (w - cropW) / 2;
  op.in.block_offset_y = (h - cropH) / 2;
  op.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  op.out.buffer = camPixels;
  op.out.buffer_size = camPixelsSize;
  op.out.pic_w = PREVIEW_W;
  op.out.pic_h = PREVIEW_H;
  op.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  op.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  op.scale_x = CAM_SCALE;
  op.scale_y = CAM_SCALE;
  op.mirror_x = CAM_MIRROR_X;
  op.mirror_y = CAM_MIRROR_Y;
  // ISP output is little-endian; M5Canvas expects big-endian RGB565.
  op.byte_swap = true;
  op.mode = PPA_TRANS_MODE_BLOCKING;
  esp_err_t err = ppa_do_scale_rotate_mirror(camPpa, &op);
  if (err != ESP_OK) {
    Serial.printf("Camera: PPA error %s\n", esp_err_to_name(err));
    return false;
  }
  return true;
}

void camStopHardware() {
  if (camCapture.isCaptureStarted()) camCapture.stopCapture();
  camCapture.end();
  camVideo.end();
  if (camPpa) {
    ppa_unregister_client(camPpa);
    camPpa = nullptr;
  }
  ledcDetach(CAM_XCLK_PIN);
}

void camTask(void*) {
  if (camStartHardware()) {
    camStep = CAM_WAITING;
    Serial.println("Camera: waiting for the first frame");
    while (!camStopRequest) {
      if (camFrameReady) {
        vTaskDelay(1);
        continue;
      }
      ESPVideoBufferClass frame = camCapture.captureBuffer();
      if (camStopRequest) break;
      if (!frame.valid()) {
        vTaskDelay(10);
        continue;
      }
      if (camConvertFrame(frame)) {
        if (camStep != CAM_LIVE) Serial.println("Camera: first frame received");
        camStep = CAM_LIVE;
        camFrameReady = true;
      }
    }
  }
  camStopHardware();
  camTaskDone = true;
  vTaskDelete(nullptr);
}

// ---------------------------------------------------------------------------
// Screen functions
// ---------------------------------------------------------------------------
void cameraEnter() {
  drawHeader("Camera");
  auto& d = M5.Display;
  d.fillSmoothRoundRect(PREVIEW_X - 2, PREVIEW_Y - 2, PREVIEW_W + 4, PREVIEW_H + 4, 16, C_BORDER2);
  d.fillRect(PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H, C_DARK);
  drawSnapButton(false);
  drawButton(camResumeBtn, FONT_LABEL, 16);
  d.setFont(FONT_SMALL);
  d.setTextColor(C_TEXT2);
  d.setTextDatum(middle_center);
  d.drawString("Snap freezes the current frame", 1040, 540);
  d.drawString("on screen. Nothing is saved.", 1040, 568);

  camFrozen = false;
  camShownStep = CAM_IDLE;
  if (!camTaskDone) {
    // A previous task is stuck in the driver and still holds the hardware.
    drawCamMessage("Camera is still busy", "Restart the Tab5 to try again.");
    return;
  }
  drawCamMessage("Starting camera...", "");
  camStep = CAM_IDLE;
  camError = "";
  camFrameReady = false;
  camStopRequest = false;
  camTaskDone = false;
  camStartMs = millis();
  camNoFrameShown = false;
  camFrames = 0;
  camFpsMs = millis();
  xTaskCreatePinnedToCore(camTask, "camTask", 8192, nullptr, 2, &camTaskHandle, 1);
}

void cameraLoop() {
  CamStep step = camStep;
  if (step != camShownStep) {
    camShownStep = step;
    if (step == CAM_FAILED) {
      drawCamMessage("Camera did not start", (const char*)camError);
    } else if (step != CAM_LIVE && step != CAM_IDLE) {
      drawCamMessage("Starting camera...", camStepText(step));
    } else if (step == CAM_LIVE) {
      drawSnapButton(true);
      camCanvas.setColorDepth(16);
      camCanvas.setBuffer(camPixels, PREVIEW_W, PREVIEW_H, 16);
    }
  }
  if (step == CAM_WAITING && !camNoFrameShown && millis() - camStartMs > CAM_NO_FRAME_MS) {
    camNoFrameShown = true;
    drawCamMessage("No picture from the camera", "The sensor started, but sent no frames.");
    Serial.println("Camera: no frame after 5 s");
  }

  if (camFrameReady) {
    if (!camFrozen) {
      drawCamBadge();
      camCanvas.pushSprite(PREVIEW_X, PREVIEW_Y);
      camFrames++;
      if (millis() - camFpsMs >= 5000) {
        Serial.printf("Camera: %.1f fps\n", camFrames * 1000.0f / (millis() - camFpsMs));
        camFrames = 0;
        camFpsMs = millis();
      }
    }
    camFrameReady = false;
  }

  int x, y;
  if (!touchPressed(x, y)) return;
  if (hit(backBtn, x, y)) { goTo(HOME); return; }
  if (camShownStep != CAM_LIVE) return;
  if (hit(camSnapBtn, x, y) && !camFrozen) {
    camFrozen = true;
    drawCamBadge();
    camCanvas.pushSprite(PREVIEW_X, PREVIEW_Y);
  }
  if (hit(camResumeBtn, x, y)) camFrozen = false;
}

void cameraExit() {
  if (camTaskDone) return;
  camStopRequest = true;
  uint32_t t0 = millis();
  while (!camTaskDone && millis() - t0 < 1500) delay(5);
  if (camTaskDone) {
    camCanvas.deleteSprite();  // detaches camPixels, does not free it
    if (camPixels) {
      heap_caps_free(camPixels);
      camPixels = nullptr;
    }
  } else {
    // The driver may still be writing; leak rather than free under it.
    Serial.println("Camera: task did not stop (no frames); restart to retry");
  }
}
