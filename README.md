# Tab5 Mini PC

<img src="https://github.com/jobitjoseph/Getting-Started-with-M5Stack-Tab5-ESP32-P4-Touchscreen-Development-Board/blob/cc4c9315997ab370b4a0101d43e6844f70cad542/M5Stack-TAB5.jpg" width="" alt="alt_text" title="image_tooltip">

A small touchscreen "desktop" for the [M5Stack Tab5](https://docs.m5stack.com/en/core/Tab5), written as a single Arduino sketch. It boots into a home screen with a status bar and a grid of apps, and each app exercises one piece of the Tab5's hardware: the camera, the two microphones, the speaker, Wi-Fi and the RTC.

It started as a tutorial project, so the code is deliberately plain. There's no LVGL and no UI framework. Everything is drawn with M5GFX primitives, and each screen is a single header file you can read top to bottom.

## What's in it

| App | What it does |
| --- | --- |
| **Home** | Status bar with Wi-Fi, clock, battery, brightness and a screen-transition picker. Tap a tile to open an app. |
| **Camera** | Live 768 × 576 preview from the SC2356 sensor, cropped and scaled in hardware by the P4's PPA. *Snap* freezes the frame. Nothing is saved. |
| **Recorder** | Records up to one minute into PSRAM, auto-levels it and plays it back, with a live waveform. |
| **Mic Meter** | Separate RMS level meters (dBFS) for MIC 1 and MIC 2, with peak hold. |
| **Melody** | Plays a handful of public-domain tunes through the speaker without blocking the UI. |
| **Internet Radio** | Streams MP3 radio over Wi-Fi. Ten preset stations, plus room for five of your own. |
| **Wi-Fi** | Scans, lets you pick a network and type the password on an on-screen keyboard, then connects. The last good network is remembered and reconnected at boot. |
| **Date & Time** | Tap the clock. Shows the time, sets the UTC offset, syncs over NTP or lets you set it by hand. Time is kept in the RTC across power cycles. |

## Hardware

- M5Stack Tab5 (ESP32-P4 with an ESP32-C6 handling Wi-Fi)
- That's it. Everything the sketch uses is on the board.

## Software setup

### Board package

You need **"esp32 by Espressif Systems" 3.3.11 or newer**. The camera depends on `ESP_Video`, and 3.3.11 is the first release that ships it.

> The M5Stack board package also has a board called "M5Tab5", but it doesn't include `ESP_Video` yet. If you pick that one the sketch stops with a clear `#error` telling you to switch.

In the Arduino IDE, choose **Tools → Board → esp32 → M5Tab5** and set:

| Option | Value |
| --- | --- |
| PSRAM | Enabled |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | Enabled |
| Partition Scheme | Default |
| Chip Variant | Match your board's P4 revision |

If you use `arduino-cli`, the equivalent FQBN is:

```
esp32:esp32:m5stack_tab5:ChipVariant=postv3,PSRAM=enabled,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=default
```

(Use `ChipVariant=prev3` on older pre-v3 silicon.)

### Libraries

Install these through the Library Manager:

- **M5Unified** (pulls in **M5GFX**)
- **ESP32-audioI2S** by schreibfaul1, **version 4.x**

If you also have Adafruit's "Audio" library installed, don't worry. `app_radio.h` includes a header that only ESP32-audioI2S provides, which makes the IDE pick the right `Audio.h`.

### Build and flash

1. Open `Tab5MiniPC/Tab5MiniPC.ino` in the Arduino IDE.
2. Select the board and options above.
3. Upload. Open the Serial Monitor at 115200 baud if you want to see what's going on. The camera, radio, Wi-Fi and clock all log their progress there.

## Project layout

```
Tab5MiniPC/
├── Tab5MiniPC.ino     setup(), loop(), screen state machine, transitions
├── ui.h               palette, fonts, Button, slider row, icons, app header
├── audio_app.h        mic/speaker arbitration and the shared volume
├── clock.h            time zone, NTP and RTC handling
├── keyboard.h         on-screen keyboard (Wi-Fi password, stream URL)
├── app_home.h         home screen and status bar
├── app_camera.h
├── app_recorder.h
├── app_micmeter.h
├── app_melody.h
├── app_radio.h
├── app_addstream.h
├── app_wifi.h
└── app_datetime.h
desgin/                HTML mockups the screens were built from
```

Every screen has the same three functions: `xxxEnter()` draws it and starts whatever hardware it needs, `xxxLoop()` runs every pass through `loop()`, and `xxxExit()` releases the hardware again. Screens switch by calling `goTo(SCREEN)`. The actual switch happens at the top of the next `loop()`, so an app is never torn down while its own loop is still running.

## A few implementation notes

These are the parts that took some digging, in case you want to adapt them.

**Camera clock.** The SC2356 needs a 24 MHz XCLK, generated with LEDC on GPIO 36. By default the P4's LEDC runs from the 40 MHz crystal and can't reach 24 MHz, so `setup()` switches it to the 80 MHz PLL *before* `M5.begin()`. That order matters because the backlight also uses LEDC and the clock source is locked once a channel is attached.

**Camera pipeline.** Frames come in at 1280 × 720 RGB565. The PPA crops the centre 944 × 708, scales by 13/16 (it only scales in 1/16 steps), mirrors and byte-swaps directly into an `M5Canvas` buffer. Capture runs in its own FreeRTOS task, so a stalled sensor never freezes the UI.

**One audio app at a time.** The mics (ES7210) and the speaker codec (ES8388) share I2S port 0, and M5Unified can drive it as one or the other but never both. `audio_app.h` tracks which app is using audio and cleanly releases it before another one starts.

**Radio through M5.Speaker.** ESP32-audioI2S 4.x paces its decoder off its own I2S DMA events. It gets the spare `I2S_NUM_1` purely for timing, and every decoded block is intercepted in `audio_process_i2s()` and handed to `M5.Speaker.playRaw()`. This keeps the codec, amplifier and volume under M5Unified's control.

**Volume curve.** M5.Speaker applies volume as a square, so the slider takes a square root first to make 50 % sound half as loud. The speaker's digital gain is also raised to 8 (M5Unified's Tab5 default is 4, which is quiet).

**Time.** Both the system clock and the RX8130 RTC hold UTC. The UTC offset only changes the `TZ` variable, so adjusting it never touches the stored time. The default offset is +05:30 (IST). Change `DEFAULT_UTC_OFFSET_MIN` in `clock.h` for your zone.

## Things that aren't persisted

Only the Wi-Fi credentials survive a reboot (in NVS, as plain text, like most Wi-Fi devices). Brightness, volume, the UTC offset, the transition style, user-added radio stations and recordings all live in RAM and reset on power-off.

## Troubleshooting

- **Camera says "sensor not detected" or shows no frames.** Check that you're on the Espressif board package 3.3.11+, not M5Stack's. A full power cycle helps if a previous session left the sensor in a bad state.
- **Wi-Fi never finds networks.** The Serial Monitor prints the ESP-Hosted version on both the P4 and the C6. A very old C6 firmware is the usual cause.
- **A radio preset won't play.** Internet radio URLs change. Add a working one with *+ Add stream URL*, or edit the `stations[]` table in `app_radio.h`.
- **Picture is upside down.** Change `SCREEN_ROTATION` in `ui.h` from 3 to 1.

## License

MIT. See [LICENSE](LICENSE).

Copyright © 2026 Jobit Joseph and Semicon Media.
