# Communication Protocol

This document specifies the wireless communication protocol as implemented in the existing firmware. All field sizes, values, and behaviours are derived directly from the source code.

---

## Transport Layer

| Property | Value | Source |
|----------|-------|--------|
| Protocol | ESP-NOW | `#include <esp_now.h>` in both files |
| Wi-Fi mode | Station (STA) | `WiFi.mode(WIFI_STA)` |
| Wi-Fi channel | 1 | `Config::WIFI_CHANNEL = 1` |
| Secondary channel | None | `WIFI_SECOND_CHAN_NONE` |
| Encryption | Disabled | `peerInfo.encrypt = false` |
| Addressing | Broadcast | `FF:FF:FF:FF:FF:FF` |
| Interface | STA | `peerInfo.ifidx = WIFI_IF_STA` |

### Why Broadcast?

The transmitter registers `FF:FF:FF:FF:FF:FF` as its sole peer. This means the transmitter does not need to know the receiver's MAC address. Any ESP-NOW-enabled node on channel 1 that has registered a receive callback will receive the packets.

---

## Packet Structure — `WirelessMessage`

The packet is defined identically in both firmware files as a C++ packed struct:

```cpp
struct __attribute__((packed)) WirelessMessage {
  uint32_t magic;     // bytes 0–3
  uint32_t sequence;  // bytes 4–7
  uint8_t  command;   // byte  8
};
// Total: 9 bytes (confirmed by static_assert)
```

### Field Reference

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0 | 4 bytes | `magic` | `uint32_t` | Fixed validation token: `0x454D5446` (ASCII `EMTF`) |
| 4 | 4 bytes | `sequence` | `uint32_t` | Packet counter. Starts at 1, increments by 1 each send. Never reset during a session. |
| 8 | 1 byte | `command` | `uint8_t` | Command identifier (see table below) |

Total packet size: **9 bytes**

The `static_assert(sizeof(WirelessMessage) == 9, ...)` in both files ensures the compiler does not add padding.

---

## Command Values

| Command name | `command` byte value | Receiver action |
|-------------|---------------------|-----------------|
| `HELLO` | `0x01` | Green LED ON, red LED OFF |
| `SOS` | `0x02` | Red LED flashing at 300 ms interval |
| `STOP` | `0x03` | Both LEDs OFF |

Any other value causes the receiver's `validateMessage()` to reject the packet silently.

---

## Packet Validation (Receiver)

`validateMessage()` in `RECEIVER_esp32.ino` performs four checks in order:

1. **Null pointer check** — `data != nullptr`
2. **Length check** — `length == 9` (must match `sizeof(WirelessMessage)`)
3. **Magic field check** — `message.magic == 0x454D5446`
4. **Command range check** — `message.command` is one of `{1, 2, 3}`

A packet failing any check is discarded without logging.

---

## Duplicate Suppression (Receiver)

After a packet passes validation, `processCommand()` checks the `sequence` field:

- If `haveSequenceNumber == true` and the incoming `sequence` equals `lastSequenceNumber`, the command is discarded with the log entry `[WARN] Duplicate command ignored.`
- Otherwise, `lastSequenceNumber` is updated to the incoming value and the command is executed.

This prevents the same physical button press from being acted on twice, which could occur if the OS or radio layer delivers a retransmission.

---

## ESP-NOW SDK Version Compatibility

Both firmware files use compile-time guards to select the correct callback signature:

```cpp
#if ESP_ARDUINO_VERSION_MAJOR >= 3
// SDK v3 callback signature
void onDataSent(const wifi_tx_info_t* info, esp_now_send_status_t status)
void onDataReceived(const esp_now_recv_info_t* receiveInfo, const uint8_t* data, int length)
#else
// SDK v2 callback signature
void onDataSent(const uint8_t* macAddress, esp_now_send_status_t status)
void onDataReceived(const uint8_t* macAddress, const uint8_t* data, int length)
#endif
```

No manual edits are needed when building against either SDK generation.

---

## Thread Safety (Receiver)

The ESP-NOW receive callback (`onDataReceived`) executes on the ESP32's Wi-Fi task, which is separate from the Arduino `loop()` task. The firmware uses FreeRTOS critical sections to safely transfer the validated packet to the application:

```
Wi-Fi task:                         Loop task:
  portENTER_CRITICAL(&messageMux)     portENTER_CRITICAL(&messageMux)
  pendingMessage = message            if (messagePending) {
  messagePending = true                 message = pendingMessage
  portEXIT_CRITICAL(&messageMux)        messagePending = false
                                      }
                                      portEXIT_CRITICAL(&messageMux)
```

Only one pending message is held at a time. If a new valid packet arrives before the loop processes the previous one, the older message is overwritten.
