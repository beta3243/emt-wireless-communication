# Future Improvements

The following suggestions are **not** implemented in the existing firmware and are presented here purely as optional ideas for future development. No existing source code should be modified to realise these suggestions.

---

## 1. Targeted Unicast Instead of Broadcast

**Current behaviour:** The transmitter broadcasts to `FF:FF:FF:FF:FF:FF`. Any ESP-NOW node on channel 1 with a compatible receive callback will receive the packets.

**Suggestion:** Replace the broadcast peer with the receiver's specific MAC address. This prevents unintended reception by other ESP-NOW devices in range.

**Implementation note:** The receiver's MAC address would need to be read from its Serial Monitor startup output and hardcoded (or stored in flash) on the transmitter side.

---

## 2. Application-Layer Acknowledgement

**Current behaviour:** The transmitter relies on the MAC-layer `ESP_NOW_SEND_SUCCESS` status from `onDataSent()`. There is no confirmation from the receiver application that the command was processed.

**Suggestion:** Add a return channel where the receiver sends a short acknowledgement packet back to the transmitter after successfully processing a command. The transmitter could retry up to N times if no acknowledgement is received within a timeout.

---

## 3. Configurable Retry Count

**Current behaviour:** Each button press results in exactly one transmission attempt. If the packet is lost at the radio layer, no retry occurs.

**Suggestion:** On `ESP_NOW_SEND_FAIL` from `onDataSent()`, retransmit the same `WirelessMessage` (same sequence number) up to a configurable maximum, with a short delay between attempts.

---

## 4. Buzzer Alert for SOS

**Current behaviour:** SOS is indicated only by a flashing red LED on the receiver.

**Suggestion:** Connect a passive buzzer to a receiver GPIO. When SOS mode is active, drive the buzzer at an audible frequency (e.g., 1 kHz) alongside the LED flash. This would improve detectability in bright-light environments.

---

## 5. OLED Display on Receiver

**Current behaviour:** The receiver provides visual feedback only through two LEDs. Command names and sequence numbers are available only over Serial Monitor.

**Suggestion:** Add a small I2C OLED display (e.g., 0.96-inch SSD1306) to the receiver. Display the last received command name, sequence number, and an uptime counter. This eliminates the need for a connected PC to observe receiver state.

---

## 6. Battery-Powered Operation with Deep Sleep

**Current behaviour:** Both boards are assumed to be powered via USB continuously. The firmware has no power-management code.

**Suggestion:** For portable use, equip the transmitter with a LiPo battery and implement ESP32 deep sleep between button presses. The transmitter would wake on a GPIO interrupt (button press), send the command, wait for the `onDataSent` callback, then return to deep sleep. This could reduce idle current from ~100 mA to under 100 µA.

---

## 7. Wi-Fi Channel Auto-Detection

**Current behaviour:** Both boards are hard-coded to Wi-Fi channel 1 (`Config::WIFI_CHANNEL = 1`).

**Suggestion:** Implement a simple channel scan on the receiver and a broadcast-based discovery sequence on the transmitter, allowing the pair to agree on a less congested channel at startup. This would reduce the impact of nearby Wi-Fi interference.

---

## 8. Non-Volatile Sequence Number Persistence

**Current behaviour:** `sequenceNumber` on the transmitter and `lastSequenceNumber` on the receiver start at 0 each boot. A reboot of either board resets the counter, which could temporarily confuse the duplicate-suppression logic.

**Suggestion:** Store `sequenceNumber` in ESP32 NVS (Non-Volatile Storage) flash. On boot, read the last stored value and continue from there. This ensures the sequence number is strictly monotonically increasing across power cycles.

---

## Implementation Reminder

All suggestions above require changes to the firmware source files. Before implementing any of them, create a new branch or a copy of the project. **Do not modify the existing `TRANSMITTER_esp.ino` or `RECEIVER_esp32.ino` files directly.**
