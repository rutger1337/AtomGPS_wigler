# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Arduino firmware for the M5Stack Atom GPS kit (ESP32): scans Wi-Fi networks and logs them with GPS position to a Wigle.net-compatible CSV on the SD card. The whole program is one sketch, `AtomGPS_wigler.ino`. `build/*.bin` are prebuilt release images committed by hand. There are no tests, no linter and no build scripts.

## Build and flash

- Arduino IDE (the documented path): install the esp32 board package plus the `M5Atom` and `TinyGPSPlus` (`TinyGPS++.h`) libraries. Set the board to **esp32 > M5Stack-Core-ESP32**, then upload.
- arduino-cli, verified with esp32 core **2.0.17** (the version the prebuilt images were built with). Newer FastLED drops the `boolean` type that M5Atom 0.1.3 uses, so the force-include of `Arduino.h` is needed:
  ```bash
  arduino-cli compile --fqbn esp32:esp32:m5stack-atom --build-property "compiler.cpp.extra_flags=-include Arduino.h" --output-dir build-out .
  arduino-cli upload  --fqbn esp32:esp32:m5stack-atom:UploadSpeed=115200 -p /dev/cu.usbserial-XXXX --input-dir build-out .
  ```
  `arduino-cli upload` writes the bootloader, partition table and app together.
- Flash a prebuilt `.bin`. The README command (`erase_flash` + `write_flash 0x1000`) is **wrong**: `build/*.bin` are app-only images, with no bootloader or partition table. Write them to `app0` at 0x10000 on a device that still has the default Arduino bootloader and partitions. Clear `otadata` so the device boots `app0`:
  ```bash
  esptool --port /dev/cu.usbserial-XXXX --baud 115200 erase-region 0xe000 0x2000
  esptool --port /dev/cu.usbserial-XXXX --baud 115200 write-flash -z 0x10000 build/AtomGPS_wigler_v1_2.bin
  ```
  The Atom's USB-serial chip fails at 921600 baud, so use 115200.
- Debug output goes to USB serial at 115200 baud.

Real hardware is the only way to verify a change. If you can't test on the device, compiling is the most you can check.

## Runtime flow

1. `setup()`: starts the SD card over SPI (pins SCK 23, MISO 33, MOSI 19, 40 MHz). Puts Wi-Fi in STA mode and disconnects. Starts the GPS UART (`Serial1`, 9600 baud, RX pin 22). Calls `waitForGPSFix()`, which blocks with a purple blink, then `initializeFile()`.
2. `initializeFile()`: picks the first free name `/wifi-scans-YYYY-MM-DD-N.csv`, using the GPS date in UTC. It writes the two Wigle header lines: the `WigleWifi-1.4,...` preamble and the column row. Both lines must stay in Wigle's format.
3. `loop()`: feeds the GPS parser. When the fix is valid it runs a blocking active scan (`WiFi.scanNetworks(false, true, false, 110)`, 110 ms per channel, hidden SSIDs included) and appends one CSV row per BSSID it hasn't seen yet. Every row is written through `logData()`, which opens and closes the file for each append.
4. Auto-upload: `loadConfig()` reads `/wigle.cfg` from the SD card (format in the README). After each scan, including one in `setup()` before the GPS wait, `maybeUpload()` checks the results for a known SSID. If it finds one, `uploadPending()` connects and POSTs each scan CSV to `api.wigle.net/api/v2/file/upload`. The request is multipart, streamed from the SD card, with Basic auth and TLS pinned to ISRG Root X1. Each uploaded file is moved to `/uploaded/`. `initializeFile()` also checks `/uploaded/`, so file names aren't reused.
5. Duplicate suppression is only a 75-entry ring buffer of BSSIDs in RAM (`macAddressArray`), not a full history.

## Known quirks (keep in mind when editing)

- If the SD card fails, `setup()` blinks red for 5 s and then returns, but `loop()` still runs. Every write then fails and flashes red. The README says the LED is "solid red".
- `isMACSeen()` only checks indices below `macArrayIndex`. After the ring buffer wraps, the older entries are skipped until they are overwritten.
- The SSID is wrapped in double quotes, but any `"` inside the SSID is not escaped.
- The `AccuracyMeters` column is filled with HDOP, which is not a distance in meters.
- LED colours mean: red = SD error, purple = waiting for GPS fix, green = scanning, blue = uploading.
- README.md has a "User manual" section for non-technical users: buttons, LED colours, SD files, `wigle.cfg`. Update it with every change to user-visible behaviour: LED, button, files, config keys, boot flow. Its images are in `docs/`: `device.svg` is the device drawing. The `led-*.svg` icons are animated with SMIL, and their `dur` and `keyTimes` match the firmware's blink timings, so update them when the timings change.
