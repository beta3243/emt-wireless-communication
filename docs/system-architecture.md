# System Architecture

This document presents the architecture and data-flow of the EMT Wireless Communication System as implemented in the existing firmware.

---

## High-Level Component Diagram

```mermaid
graph LR
    subgraph Transmitter["ESP32 Transmitter — Firmware 3.0.0"]
        B1["[HELLO]\nGPIO 25"]
        B2["[SOS]\nGPIO 18"]
        B3["[STOP]\nGPIO 19"]
        DEBNC["Debounce\n(40 ms)"]
        BUILD["Build WirelessMessage\nmagic / sequence / command"]
        ESPNOW_TX["esp_now_send()\nBroadcast FF:FF:FF:FF:FF:FF"]
        CB_TX["onDataSent()\ncallback"]

        B1 --> DEBNC
        B2 --> DEBNC
        B3 --> DEBNC
        DEBNC -->|"Stable LOW"| BUILD
        BUILD --> ESPNOW_TX
        ESPNOW_TX --> CB_TX
    end

    subgraph Channel["Wi-Fi Channel 1"]
        RF["9-byte packet\nbroadcast"]
    end

    subgraph Receiver["ESP32 Receiver — Firmware 2.0.0"]
        CB_RX["onDataReceived()\ncallback"]
        VALID["validateMessage()\nlength / magic / command"]
        QUEUE["pendingMessage\n(mutex-protected)"]
        LOOP["loop()\ndequeue"]
        PROC["processCommand()"]
        BLINK["updateLedPattern()\n300 ms SOS blink"]
        GLED["Green LED\nGPIO 18"]
        RLED["Red LED\nGPIO 19"]

        CB_RX --> VALID
        VALID -->|"Valid"| QUEUE
        QUEUE --> LOOP
        LOOP --> PROC
        PROC -->|"HELLO"| GLED
        PROC -->|"SOS"| RLED
        PROC -->|"STOP"| GLED
        PROC -->|"STOP"| RLED
        BLINK -->|"Toggle 300 ms"| RLED
    end

    ESPNOW_TX -->|"ESP-NOW broadcast"| Channel
    Channel -->|"Radio delivery"| CB_RX
    LOOP --> BLINK
```

---

## Data-Flow Sequence Diagram

```mermaid
sequenceDiagram
    participant USER as User
    participant TX_GPIO as TX GPIO
    participant TX_FW as TX Firmware
    participant RADIO as ESP-NOW Radio
    participant RX_FW as RX Firmware
    participant RX_LED as RX LEDs

    USER->>TX_GPIO: Press button (GPIO → GND)
    TX_GPIO-->>TX_FW: digitalRead() returns LOW
    TX_FW->>TX_FW: Debounce 40 ms
    TX_FW->>TX_FW: Build WirelessMessage (magic, seq, cmd)
    TX_FW->>RADIO: esp_now_send(FF:FF:FF:FF:FF:FF, 9 bytes)
    RADIO-->>TX_FW: onDataSent() → ESP_NOW_SEND_SUCCESS
    RADIO->>RX_FW: onDataReceived() fires
    RX_FW->>RX_FW: validateMessage() — check length, magic, command
    RX_FW->>RX_FW: Store in pendingMessage (critical section)
    RX_FW->>RX_FW: loop() dequeues pendingMessage
    RX_FW->>RX_FW: processCommand() — check duplicate seq
    RX_FW->>RX_LED: Set LED outputs (HELLO / SOS / STOP)
    loop SOS mode active
        RX_FW->>RX_LED: Toggle red LED every 300 ms
    end
```

---

## Software Layer Stack

```
┌─────────────────────────────────────────────────┐
│           Application Logic (Arduino .ino)       │
│  updateButtons() / sendCommand() / processCommand()│
├─────────────────────────────────────────────────┤
│               ESP-NOW API                        │
│  esp_now_init / esp_now_send / callbacks         │
├─────────────────────────────────────────────────┤
│               ESP-IDF Wi-Fi Driver               │
│  esp_wifi_set_channel / WIFI_STA mode            │
├─────────────────────────────────────────────────┤
│           ESP32 Hardware (Radio / GPIO)           │
└─────────────────────────────────────────────────┘
```

---

## Key Design Decisions Found in the Source Code

| Decision | Location | Rationale (as observed) |
|----------|----------|-------------------------|
| Broadcast MAC (`FF:FF:FF:FF:FF:FF`) | `TRANSMITTER_esp.ino` L66–68 | No receiver MAC needs to be hard-coded |
| Packed 9-byte struct + `static_assert` | Both files | Ensures sender and receiver agree on layout |
| `portENTER_CRITICAL` / `portEXIT_CRITICAL` | `RECEIVER_esp32.ino` L167–172, L378–386 | Safe handoff from Wi-Fi task to loop task |
| Dual SDK version guards (`#if ESP_ARDUINO_VERSION_MAJOR >= 3`) | Both files | Supports Arduino ESP32 core v2 and v3 |
| 40 ms debounce | `TRANSMITTER_esp.ino` L36 | Prevents contact bounce from generating duplicate sends |
| Sequence number duplicate check | `RECEIVER_esp32.ino` L215–220 | Suppresses repeated processing of the same packet |
| Non-blocking SOS blink | `RECEIVER_esp32.ino` L271–290 | `millis()` comparison avoids `delay()` in loop |
