# Working Principle

This document describes the complete, verified operation of the EMT Wireless Communication System from the moment a button is pressed on the transmitter to the moment the receiver updates its LED outputs. All claims are derived directly from the source code.

---

## 1. Startup — Transmitter

When the transmitter ESP32 powers on, `setup()` executes the following sequence:

1. **Serial port opens** at 115200 baud. A 500 ms delay allows the host terminal to connect before output begins.
2. **Button GPIOs are configured** — GPIOs 25, 18, and 19 are set to `INPUT_PULLUP`. Each pin reads its initial state and stores it in the corresponding `ButtonState` struct so that the debouncer starts in the correct resting position.
3. **`initializeWireless()` is called:**
   - Wi-Fi mode is set to `WIFI_STA`.
   - `esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE)` locks the radio to channel 1.
   - `esp_now_init()` activates the ESP-NOW subsystem.
   - `esp_now_register_send_cb(onDataSent)` registers the transmission-result callback.
   - A broadcast peer entry (`FF:FF:FF:FF:FF:FF`, channel 1, unencrypted) is added with `esp_now_add_peer()`.
4. The transmitter's own MAC address is printed to Serial.
5. If any step above fails, the firmware halts in an infinite loop and prints a fatal error.
6. On success, the firmware prints `TRANSMITTER READY` and the GPIO-to-command mapping.

---

## 2. Startup — Receiver

When the receiver ESP32 powers on, `setup()` executes:

1. **Serial port opens** at 115200 baud. A 300 ms delay is applied before output.
2. **LED GPIOs are configured** — GPIOs 18 and 19 are set to `OUTPUT`. `turnBothLedsOff()` is called immediately to ensure both LEDs start in the OFF state.
3. **`initializeWireless()` is called:**
   - Wi-Fi mode is set to `WIFI_STA`.
   - `esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE)` locks the radio to channel 1.
   - `esp_now_init()` activates the ESP-NOW subsystem.
   - `esp_now_register_recv_cb(onDataReceived)` registers the receive callback.
4. The receiver's own MAC address is printed to Serial.
5. If any step fails, the firmware halts in an infinite loop and prints an error.
6. On success, the firmware prints `RECEIVER READY` and the LED-to-command mapping.

---

## 3. Button Polling and Debounce (Transmitter)

The transmitter `loop()` calls `updateButtons()` on every iteration (approximately every 1 ms due to `delay(1)`).

`updateButtons()` iterates over the three `ButtonState` structs:

1. **Raw state read** — `digitalRead(button.pin)` returns `HIGH` (button not pressed) or `LOW` (button pressed, because of `INPUT_PULLUP` and the button being wired to GND).
2. **Edge detection** — If the raw state differs from `button.lastRawState`, the new raw state and the current timestamp (`millis()`) are stored. A debug log is printed.
3. **Debounce gate** — If the elapsed time since the last raw change is at least **40 ms** (`Config::DEBOUNCE_MS`) and the raw state still differs from `button.stableState`, the stable state is updated and a button-event log is printed.
4. **Press action** — If the newly stable state is `LOW` (pressed), `sendCommand(button.command)` is called immediately. Release events produce a log but take no further action.

This debounce approach ensures that rapid contact bounces shorter than 40 ms do not generate duplicate transmissions.

---

## 4. Building and Sending the Packet (Transmitter)

`sendCommand(Command command)` constructs a `WirelessMessage` struct:

| Field | Type | Value assigned |
|-------|------|----------------|
| `magic` | `uint32_t` | `0x454D5446` (ASCII "EMTF") |
| `sequence` | `uint32_t` | `++sequenceNumber` (starts at 1, increments per send) |
| `command` | `uint8_t` | Cast of `Command` enum: HELLO=1, SOS=2, STOP=3 |

The struct is declared `__attribute__((packed))` and a `static_assert` confirms its size is exactly **9 bytes**.

`esp_now_send(BROADCAST_MAC, &message, sizeof(message))` is called. If the call fails immediately, an error code is printed. Otherwise, `[INFO] Packet queued for transmission.` is logged.

The radio layer delivers the packet and fires `onDataSent()` asynchronously:
- `ESP_NOW_SEND_SUCCESS` → increments `successfulSends`, logs success.
- Any other status → increments `failedSends`, logs failure.

---

## 5. Receiving the Packet (Receiver)

When the ESP-NOW radio delivers a packet, `onDataReceived()` fires on the ESP32's Wi-Fi task (not the Arduino `loop()` task). To avoid performing Serial I/O or GPIO changes inside an interrupt-like context, the firmware uses a mutex-protected single-slot queue:

1. **`validateMessage()`** is called first:
   - Returns `false` if `data` is null.
   - Returns `false` if `length != 9` (wrong size).
   - Copies bytes into a local `WirelessMessage` via `memcpy`.
   - Returns `false` if `message.magic != 0x454D5446`.
   - Returns `false` if `message.command` is not 1, 2, or 3.
2. If validation passes, `portENTER_CRITICAL(&messageMux)` is called to safely write the validated message into `pendingMessage` and set `messagePending = true`.
3. `portEXIT_CRITICAL(&messageMux)` releases the critical section.

---

## 6. Processing the Command (Receiver Main Loop)

The receiver `loop()` runs every ~1 ms (`delay(1)`):

1. Inside a critical section, if `messagePending` is true, the pending message is copied to a local variable and `messagePending` is cleared.
2. If a message was dequeued, `processCommand()` is called:
   - **Duplicate check** — if `haveSequenceNumber` is true and the incoming `sequence` equals `lastSequenceNumber`, the command is silently discarded with a warning log.
   - Otherwise `lastSequenceNumber` is updated and both LEDs are turned off before applying the new mode.
   - **HELLO** → `activeLedMode = LedMode::HELLO`; GPIO 18 (green LED) set HIGH.
   - **SOS** → `activeLedMode = LedMode::SOS`; GPIO 19 (red LED) set HIGH; `lastBlinkTime` captured.
   - **STOP** → `activeLedMode = LedMode::OFF`; both LEDs set LOW.
3. `updateLedPattern()` is called every loop iteration. If `activeLedMode == LedMode::SOS`, it checks whether `SOS_BLINK_INTERVAL_MS` (300 ms) has elapsed and toggles the red LED accordingly. This produces a continuous ~1.67 Hz blink without blocking.

---

## 7. End-to-End Timeline (Illustrative)

```
T+0 ms    Button pressed — GPIO reads LOW
T+40 ms   Debounce stable — sendCommand(HELLO) called
T+40 ms   WirelessMessage built: magic=0x454D5446, seq=N, cmd=0x01
T+40 ms   esp_now_send() queues packet
T+40–42   ESP-NOW radio transmits 9-byte packet on channel 1
T+42 ms   onDataSent() fires — logs "MAC transmission succeeded"
T+42 ms   Receiver onDataReceived() fires — packet validated
T+42 ms   pendingMessage set, messagePending = true
T+43 ms   Receiver loop() dequeues message
T+43 ms   processCommand(): green LED HIGH, activeLedMode = HELLO
```

Actual timings depend on ESP-NOW radio scheduling and Arduino loop frequency.
