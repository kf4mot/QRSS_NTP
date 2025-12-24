# QRSS DFCW Transmitter with NTP sync and State Machine

## Overview
This project implements a QRSS (very slow Morse) transmitter using Frequency Shift Keying (DFCW) on a Si5351 oscillator. Transmission is managed by a state machine for precise timing synchronized to UTC via NTP.

The design ensures reliable 10-minute transmission cycles and provides visual feedback via an OLED display and serial logging for debugging.

## Features
- **DFCW Transmission**: Alternates between MARK and SPACE frequencies to encode Morse symbols.
- **State Machine Control**: Handles the following states:
  - `IDLE` – waiting for next transmit window.
  - `TX_CHAR` – starting a new character.
  - `TX_SYMBOL` – transmitting a dot or dash.
  - `TX_PAUSE` – inter-symbol or inter-character pause.
- **NTP Time Sync**: Automatically synchronizes to UTC for accurate TX cycles.
- **OLED Display Feedback**: Shows "ON AIR" during transmission, "WAIT" otherwise.
- **Serial Logging**: Prints state transitions and TX events.

## Hardware
- **ESP32-C3-Zero** (https://www.waveshare.com/wiki/ESP32-C3-Zero)
- **Si5351 Clock Generator** (https://learn.adafruit.com/adafruit-si5351-clock-generator-breakout/downloads)
  - CLK1 used for RF output.
- **SSD1306 OLED Display**
  - SDA → GPIO 6
  - SCL → GPIO 7
- Wi-Fi required for NTP synchronization.

## Timing Constants
- `dit = 6 s` (dot)
- `dah = 18 s` (dash)
- `sym_pause = 6 s` (pause between symbols)
- `char_pause = 18 s` (pause between characters)
- Transmission window: every 10 minutes (UTC-based).

## Setup & Usage
1. Configure Wi-Fi credentials in `Credentials.h`.
2. Connect the ESP32, Si5351, and OLED as described.
3. Upload the sketch to the ESP32.
4. Monitor the OLED or Serial output:
   - **Serial** logs initialization, TX cycles, and state transitions.
   - **OLED** shows "ON AIR" during TX and "WAIT" otherwise.
5. Transmission repeats automatically every 10 minutes.

## Credentials
Create a `Credentials.h` file in the project folder with your Wi-Fi credentials:

```cpp
#pragma once

//Wifi
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
// Morse sequence for transmission
const char* message[] = {"-.-", "..-.", "....-", "--", "---", "-", NULL}; //KF4MOT. Replace with USER CALLSIGN
```

## Known limitations
-Requires Wi-Fi for initial time sync
-No automatic re-sync scheduling
-Message is currently hard-coded

