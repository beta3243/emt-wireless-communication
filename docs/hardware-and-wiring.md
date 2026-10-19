# Hardware and Wiring

This document records the hardware connections as specified in the firmware source-code header comments and confirmed by the GPIO configuration code. Connections not mentioned in either location are labelled **unverified**.

---

## Transmitter Board

### Source Reference

From `TRANSMITTER_esp.ino` header (lines 8–15) and `Config` namespace (lines 31–33):

```
HELLO -> GPIO 25 -> GND
SOS   -> GPIO 18 -> GND
STOP  -> GPIO 19 -> GND
```

### Button Wiring Diagram

```
ESP32 GPIO 25 ──┬── [HELLO button] ── GND
                │   (normally open)
                └── (internal pull-up to 3.3 V via INPUT_PULLUP)

ESP32 GPIO 18 ──┬── [SOS button] ── GND
                │   (normally open)
                └── (internal pull-up to 3.3 V via INPUT_PULLUP)

ESP32 GPIO 19 ──┬── [STOP button] ── GND
                │   (normally open)
                └── (internal pull-up to 3.3 V via INPUT_PULLUP)
```

### Logic Level Interpretation

| Pin state | Meaning |
|-----------|---------|
| HIGH (3.3 V) | Button not pressed — pull-up holds the line high |
| LOW (0 V) | Button pressed — button connects pin to GND |

The firmware fires a command on the **falling edge** (transition from HIGH to LOW after 40 ms debounce).

### Transmitter Power

Power source is **unverified** in the source code. Typical options are USB (5 V regulated to 3.3 V on-board) or a LiPo battery via a regulator. No power-related code is present in the firmware.

---

## Receiver Board

### Source Reference

From `RECEIVER_esp32.ino` header (lines 7–9) and `Config` namespace (lines 27–28):

```
Green LED -> GPIO 18 through 220-ohm resistor
Red LED   -> GPIO 19 through 220-ohm resistor
```

### LED Wiring Diagram

```
ESP32 GPIO 18 ── [220 Ω] ── [Green LED anode] ── [Green LED cathode] ── GND

ESP32 GPIO 19 ── [220 Ω] ── [Red LED anode]   ── [Red LED cathode]   ── GND
```

### LED Output States by Command

| Command received | Green LED (GPIO 18) | Red LED (GPIO 19) |
|-----------------|--------------------|--------------------|
| HELLO | HIGH (ON) | LOW (OFF) |
| SOS | LOW (OFF) | Flashing at 300 ms interval |
| STOP | LOW (OFF) | LOW (OFF) |
| Startup (before any command) | LOW (OFF) | LOW (OFF) |

### Receiver Power

Power source is **unverified** in the source code. Assumed to be USB during development.

---

## Component Notes

- **No external pull-up resistors** are required on the transmitter button inputs. The ESP32's internal pull-up (approximately 45 kΩ) is enabled by `INPUT_PULLUP`.
- **220 Ω current-limiting resistors** are confirmed for both LEDs by the receiver header comment. This value limits current to approximately 15 mA at 3.3 V output (suitable for standard 2 V forward-voltage LEDs).
- The exact LED color specifications (forward voltage, luminous intensity) are **unverified** in the source code. Standard 5 mm or 3 mm LEDs rated for 3.3 V GPIO output are assumed.
- No buzzer, OLED, or additional peripheral is present in or referenced by the existing firmware.
