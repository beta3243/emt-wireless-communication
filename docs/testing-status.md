# Testing Status

This document records what is known about the operational status of the EMT Wireless Communication System. Entries are clearly labelled by evidence type.

---

## Evidence Categories

| Label | Meaning |
|-------|---------|
| **SOURCE-VERIFIED** | Behaviour confirmed by reading the firmware source code |
| **DEMONSTRATED** | Behaviour observed during a live hardware test |
| **UNVERIFIED** | Status not established by available evidence |

---

## Transmitter Firmware (Firmware 3.0.0)

| Test Item | Status | Evidence |
|-----------|--------|----------|
| Serial port opens at 115200 baud | SOURCE-VERIFIED | `Serial.begin(115200)` in `setup()` |
| GPIO 25, 18, 19 configured as INPUT_PULLUP | SOURCE-VERIFIED | `pinMode(buttons[i].pin, INPUT_PULLUP)` loop |
| Debounce gate of 40 ms applied | SOURCE-VERIFIED | `Config::DEBOUNCE_MS = 40`; checked in `updateButtons()` |
| Command sent on stable LOW (press) only | SOURCE-VERIFIED | `if (button.stableState == LOW) sendCommand(...)` |
| WirelessMessage struct is 9 bytes | SOURCE-VERIFIED | `static_assert(sizeof(WirelessMessage) == 9, ...)` |
| Magic field set to 0x454D5446 | SOURCE-VERIFIED | `message.magic = Config::PACKET_MAGIC` |
| Sequence number incremented per send | SOURCE-VERIFIED | `message.sequence = ++sequenceNumber` |
| Broadcast to FF:FF:FF:FF:FF:FF | SOURCE-VERIFIED | `BROADCAST_MAC` constant; passed to `esp_now_add_peer` |
| SDK v2 and v3 callback compatibility | SOURCE-VERIFIED | `#if ESP_ARDUINO_VERSION_MAJOR >= 3` guards |
| Halts on wireless init failure | SOURCE-VERIFIED | `while(true) { delay(1000); }` on `!initializeWireless()` |
| End-to-end transmission on hardware | UNVERIFIED | No live test evidence available in this repository |

---

## Receiver Firmware (Firmware 2.0.0)

| Test Item | Status | Evidence |
|-----------|--------|----------|
| Serial port opens at 115200 baud | SOURCE-VERIFIED | `Serial.begin(115200)` in `setup()` |
| Both LEDs initialised OFF at startup | SOURCE-VERIFIED | `turnBothLedsOff()` called in `setup()` before `initializeWireless()` |
| Packet rejected on wrong length | SOURCE-VERIFIED | `if (length != sizeof(WirelessMessage)) return false` |
| Packet rejected on wrong magic | SOURCE-VERIFIED | `if (message.magic != Config::PACKET_MAGIC) return false` |
| Packet rejected on unknown command | SOURCE-VERIFIED | Command range check in `validateMessage()` |
| Duplicate sequence suppressed | SOURCE-VERIFIED | `if (haveSequenceNumber && message.sequence == lastSequenceNumber)` |
| HELLO → green LED HIGH | SOURCE-VERIFIED | `digitalWrite(Config::GREEN_LED_PIN, HIGH)` in HELLO case |
| SOS → red LED flashing at 300 ms | SOURCE-VERIFIED | `SOS_BLINK_INTERVAL_MS = 300`; toggled in `updateLedPattern()` |
| STOP → both LEDs LOW | SOURCE-VERIFIED | `turnBothLedsOff()` in STOP case |
| Critical section around pending message | SOURCE-VERIFIED | `portENTER_CRITICAL` / `portEXIT_CRITICAL` in callback and loop |
| SDK v2 and v3 callback compatibility | SOURCE-VERIFIED | `#if ESP_ARDUINO_VERSION_MAJOR >= 3` guards |
| Halts on wireless init failure | SOURCE-VERIFIED | `while(true) { delay(1000); }` on `!initializeWireless()` |
| End-to-end reception on hardware | UNVERIFIED | No live test evidence available in this repository |

---

## Integration Tests

| Test | Status | Notes |
|------|--------|-------|
| Button press → LED response on live hardware | UNVERIFIED | Hardware test not independently performed as part of this documentation effort |
| Operation confirmed by project author | DEMONSTRATED | Per project author's report — the system was built and operated successfully |
| Serial Monitor output matches documented format | SOURCE-VERIFIED | All `Serial.print` / `Serial.println` calls match the expected output shown in README |

---

## Notes

- All SOURCE-VERIFIED items represent behaviour that the firmware is written to provide. Actual hardware behaviour could differ due to wiring errors, GPIO conflicts, power supply issues, or radio interference — none of which can be assessed from source code alone.
- The project author confirmed operational success prior to this documentation being created.
- No automated test framework is present in the existing firmware.
