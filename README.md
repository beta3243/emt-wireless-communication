# EMT Wireless Communication System

An ESP32-based wireless communication demonstration using **ESP-NOW**, three push-button commands, and an LED-based receiver display.

---

## Overview

This project demonstrates low-latency, infrastructure-free wireless communication between two ESP32 boards using Espressif's **ESP-NOW** protocol. A button press on the transmitter board is captured, encoded into a compact validated packet, and broadcast over Wi-Fi to the receiver board, which responds by controlling two LEDs.

The project is named after its EMT (Emergency Medical Technician) inspiration: rapid, reliable, one-press command signalling.

---

## Problem Statement and Objectives

| Goal | Implementation |
|------|----------------|
| Send a one-press command wirelessly | Three dedicated buttons on the transmitter |
| No Wi-Fi router or internet required | ESP-NOW peer-to-peer protocol |
| Immediate visual feedback at receiver | Green LED (HELLO), flashing red LED (SOS), both off (STOP) |
| Packet integrity | 4-byte magic field validates every received packet |
| Duplicate suppression | Sequence number compared against previous value |
| Stable button input | 40 ms software debounce in the transmitter firmware |

---

## System Architecture

```
┌─────────────────────────┐          ESP-NOW broadcast          ┌─────────────────────────┐
│   ESP32 TRANSMITTER     │  ──────────────────────────────►   │   ESP32 RECEIVER        │
│   Firmware 3.0.0        │         Wi-Fi Channel 1             │   Firmware 2.0.0        │
│                         │         FF:FF:FF:FF:FF:FF            │                         │
│  [HELLO] → GPIO 25      │                                      │  GPIO 18 → Green LED    │
│  [SOS]   → GPIO 18      │                                      │  GPIO 19 → Red LED      │
│  [STOP]  → GPIO 19      │                                      │                         │
└─────────────────────────┘                                      └─────────────────────────┘
```

See [`docs/system-architecture.md`](docs/system-architecture.md) for the full Mermaid data-flow diagram.

---

## Hardware Components

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32 development board | 2 | Any standard ESP32 DevKit |
| Push button (momentary) | 3 | Wired to transmitter GPIOs |
| Green LED | 1 | Wired to receiver GPIO 18 |
| Red LED | 1 | Wired to receiver GPIO 19 |
| 220 Ω resistor | 2 | Current-limiting for LEDs (per receiver header) |
| Jumper wires | — | As needed |
| USB cables | 2 | One per ESP32 |

---

## Transmitter GPIO Mapping

| Button Label | GPIO | Connection |
|-------------|------|------------|
| HELLO | GPIO 25 | Button → GND |
| SOS | GPIO 18 | Button → GND |
| STOP | GPIO 19 | Button → GND |

All three inputs are configured as `INPUT_PULLUP`. The pin reads **HIGH** at rest and **LOW** when the button is pressed.

---

## Receiver Pin Mapping

| Signal | GPIO | Connection |
|--------|------|------------|
| Green LED | GPIO 18 | GPIO → 220 Ω → LED → GND |
| Red LED | GPIO 19 | GPIO → 220 Ω → LED → GND |

Both pins are configured as `OUTPUT` and default to `LOW` at startup.

---

## Software Requirements

| Requirement | Version |
|-------------|---------|
| Arduino IDE | 1.8.x or 2.x |
| Arduino ESP32 core | 2.x **or** 3.x (both supported) |
| ESP-NOW library | Bundled with ESP32 Arduino core |
| WiFi library | Bundled with ESP32 Arduino core |

No external libraries are required.

> The firmware uses compile-time `#if ESP_ARDUINO_VERSION_MAJOR >= 3` guards to select the correct callback signature for both SDK generations. No manual code changes are needed when switching between SDK versions.

---

## Setup and Upload Instructions

### Step 1 — Upload Transmitter Firmware

1. Open `TRANSMITTER_esp/TRANSMITTER_esp.ino` in Arduino IDE.
2. Connect the **transmitter** ESP32 via USB.
3. Select **Tools → Board → ESP32 Dev Module** (or your specific variant).
4. Select the correct COM port.
5. Click **Upload**.
6. Open **Serial Monitor** at **115200 baud** to verify startup output.

### Step 2 — Upload Receiver Firmware

1. Open `RECEIVER_esp32/RECEIVER_esp32.ino` in Arduino IDE.
2. Connect the **receiver** ESP32 via USB.
3. Select the same board type and the correct COM port.
4. Click **Upload**.
5. Open **Serial Monitor** at **115200 baud** to verify startup output.

### Step 3 — Wire the Hardware

Refer to the GPIO tables above and [`docs/hardware-and-wiring.md`](docs/hardware-and-wiring.md) for wiring details.

### Step 4 — Operate

Press any of the three buttons on the transmitter. The receiver should respond within milliseconds.

---

## ESP-NOW Communication Workflow

1. Both ESP32 boards set Wi-Fi mode to **Station (STA)** and fix the radio to **channel 1**.
2. The transmitter registers a broadcast peer (`FF:FF:FF:FF:FF:FF`) on channel 1.
3. On a confirmed button press, the transmitter builds a 9-byte `WirelessMessage` packet and sends it via `esp_now_send()`.
4. The ESP-NOW radio layer delivers the packet to any listening ESP-NOW node on channel 1.
5. The receiver's `onDataReceived` callback fires, validates the packet (length, magic field, command value), and places it in a mutex-protected single-slot queue.
6. The main `loop()` dequeues the message and calls `processCommand()`, which updates the LED outputs.

See [`docs/communication-protocol.md`](docs/communication-protocol.md) for the full packet specification.

---

## Commands

| Command | Button | Packet Value | Receiver Response |
|---------|--------|-------------|-------------------|
| `HELLO` | GPIO 25 | `0x01` | Green LED turns ON |
| `SOS` | GPIO 18 | `0x02` | Red LED flashes at 300 ms interval |
| `STOP` | GPIO 19 | `0x03` | Both LEDs turn OFF |

---

## Expected Serial Monitor Output

### Transmitter (on button press)

```
----------------------------------------
[BUTTON] HELLO
[INFO] Sequence: 1
[INFO] GPIO: 25
[INFO] Packet queued for transmission.
----------------------------------------
[RADIO] MAC transmission succeeded.
```

### Receiver (on packet received)

```
[EVENT] Wireless command received.
[INFO] Command: HELLO
[INFO] Sequence number: 1
[INFO] Green LED ON; red LED OFF.
```

---

## Known Limitations

- The transmitter broadcasts to `FF:FF:FF:FF:FF:FF`. Any ESP-NOW device on channel 1 running compatible firmware will receive the packets.
- The receiver implements a single-slot packet queue. If packets arrive faster than the `loop()` can process them, earlier packets may be overwritten.
- No acknowledgement or retry mechanism is implemented on the application layer. The ESP-NOW MAC-layer acknowledgement is logged but not acted upon by the application.
- The SOS blink state persists until a STOP or HELLO command is received.
- Both boards must be on Wi-Fi channel 1. If the environment already uses channel 1 heavily, interference may occur.

---

## Future Improvement Suggestions

These are optional ideas and are **not** present in the existing firmware. See [`docs/future-improvements.md`](docs/future-improvements.md) for details.

- Targeted unicast (receiver MAC address instead of broadcast)
- Application-layer retry with configurable attempt count
- Bi-directional acknowledgement from receiver to transmitter
- Buzzer alert on SOS in addition to the LED
- OLED display on the receiver for text feedback
- Battery-powered operation with deep-sleep between presses

---

## Documentation

| File | Contents |
|------|----------|
| [`docs/working-principle.md`](docs/working-principle.md) | End-to-end flow from button press to LED response |
| [`docs/system-architecture.md`](docs/system-architecture.md) | Mermaid architecture and data-flow diagrams |
| [`docs/hardware-and-wiring.md`](docs/hardware-and-wiring.md) | Wiring reference for both boards |
| [`docs/communication-protocol.md`](docs/communication-protocol.md) | Packet structure and field specifications |
| [`docs/testing-status.md`](docs/testing-status.md) | Evidence of demonstrated functionality |
| [`docs/future-improvements.md`](docs/future-improvements.md) | Optional improvement ideas |

---

## Repository Structure

```
emt-wireless-communication/
├── README.md
├── .gitignore
├── TRANSMITTER_esp/
│   └── TRANSMITTER_esp.ino      ← Transmitter firmware (Firmware 3.0.0)
├── RECEIVER_esp32/
│   └── RECEIVER_esp32.ino       ← Receiver firmware (Firmware 2.0.0)
└── docs/
    ├── working-principle.md
    ├── system-architecture.md
    ├── hardware-and-wiring.md
    ├── communication-protocol.md
    ├── testing-status.md
    └── future-improvements.md
```
