# Homework 5 – Stop the Counter!

## Project Description
Stop the Counter is a reaction game developed using an ESP32, a 74HC595 shift register, a four-digit seven-segment display, and a joystick.

The goal is to stop the countdown as close as possible to the target number (5000).

## Hardware Components
- ESP32
- 74HC595 shift register
- Four-digit seven-segment display
- Analog joystick
- Breadboard and jumper wires
- 1 kΩ resistors

## Pin Connections

| Component | ESP32 GPIO |
|---|---|
| 74HC595 DS | 23 |
| 74HC595 SHCP | 18 |
| 74HC595 STCP | 22 |
| Display D1–D4 | 25, 26, 27, 33 |
| Joystick VRx | 34 |
| Joystick VRy | 35 |
| Joystick SW | 32 |

## Game Controls
- **Joystick UP:** Start countdown from 9999.
- **Joystick button:** Stop the countdown.
- **Joystick DOWN:** Reset the game.

**Target number:** 5000

**Countdown interval:** 2 ms per unit.

The final error is calculated using:

`Error = abs(Stopped Value - Target)`

## Implementation
- The 74HC595 controls the display segments.
- Multiplexing refreshes the four display digits.
- Bitwise operations and masks control segment patterns.
- `millis()` provides non-blocking timing.
- Button debouncing and joystick edge detection prevent repeated actions.
- Leading zeros are suppressed.

## Results
The Serial Monitor displays the target, stopped value, and calculated error.

## Software
- Visual Studio Code
- PlatformIO
- Arduino Framework
- C++
