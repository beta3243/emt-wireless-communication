/*
 * ============================================================
 * Project : EMT Wireless Communication System
 * Device  : ESP32 Receiver
 * Firmware: 2.0.0
 *
 * Hardware:
 *   Green LED -> GPIO 18 through 220-ohm resistor
 *   Red LED   -> GPIO 19 through 220-ohm resistor
 *
 * Protocol: ESP-NOW
 * Channel : 1
 * Baud rate: 115200
 * ============================================================
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_arduino_version.h>

// ---------------- Configuration ----------------

namespace Config {
  constexpr uint8_t WIFI_CHANNEL = 1;

  constexpr uint8_t GREEN_LED_PIN = 18;
  constexpr uint8_t RED_LED_PIN = 19;

  constexpr uint32_t SERIAL_BAUD = 115200;
  constexpr uint32_t PACKET_MAGIC = 0x454D5446;

  constexpr uint32_t SOS_BLINK_INTERVAL_MS = 300;
}

// ---------------- Command definitions ----------------

enum class Command : uint8_t {
  HELLO = 1,
  SOS   = 2,
  STOP  = 3
};

// ---------------- Message format ----------------

struct __attribute__((packed)) WirelessMessage {
  uint32_t magic;
  uint32_t sequence;
  uint8_t command;
};

static_assert(
  sizeof(WirelessMessage) == 9,
  "Unexpected wireless message size"
);

// ---------------- Receiver state ----------------

enum class LedMode : uint8_t {
  OFF,
  HELLO,
  SOS
};

LedMode activeLedMode = LedMode::OFF;

bool redLedState = false;
uint32_t lastBlinkTime = 0;

uint32_t lastSequenceNumber = 0;
bool haveSequenceNumber = false;

// ---------------- Thread-safe message handoff ----------------

// ESP-NOW callbacks may execute outside the normal Arduino loop.
// The callback copies a validated packet into this single-slot queue.

portMUX_TYPE messageMux = portMUX_INITIALIZER_UNLOCKED;

volatile bool messagePending = false;

WirelessMessage pendingMessage = {};

// ---------------- Diagnostics ----------------

void logInfo(const char* message) {
  Serial.print("[INFO] ");
  Serial.println(message);
}

void logWarning(const char* message) {
  Serial.print("[WARN] ");
  Serial.println(message);
}

void logError(const char* message) {
  Serial.print("[ERROR] ");
  Serial.println(message);
}

const char* commandName(Command command) {
  switch (command) {
    case Command::HELLO:
      return "HELLO";

    case Command::SOS:
      return "SOS";

    case Command::STOP:
      return "STOP";

    default:
      return "UNKNOWN";
  }
}

// ---------------- Validate incoming message ----------------

bool validateMessage(
  const uint8_t* data,
  int length,
  WirelessMessage& message
) {
  if (data == nullptr) {
    return false;
  }

  if (length != sizeof(WirelessMessage)) {
    return false;
  }

  memcpy(&message, data, sizeof(message));

  if (message.magic != Config::PACKET_MAGIC) {
    return false;
  }

  if (
    message.command !=
      static_cast<uint8_t>(Command::HELLO) &&
    message.command !=
      static_cast<uint8_t>(Command::SOS) &&
    message.command !=
      static_cast<uint8_t>(Command::STOP)
  ) {
    return false;
  }

  return true;
}

// ---------------- ESP-NOW receive handler ----------------

void handleReceivedPacket(
  const uint8_t* data,
  int length
) {
  WirelessMessage message = {};

  if (!validateMessage(data, length, message)) {
    return;
  }

  // Copy the message into the application queue.
  // Avoid Serial output and LED changes inside the callback.

  portENTER_CRITICAL(&messageMux);

  pendingMessage = message;
  messagePending = true;

  portEXIT_CRITICAL(&messageMux);
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3

void onDataReceived(
  const esp_now_recv_info_t* receiveInfo,
  const uint8_t* data,
  int length
) {
  (void)receiveInfo;

  handleReceivedPacket(data, length);
}

#else

void onDataReceived(
  const uint8_t* macAddress,
  const uint8_t* data,
  int length
) {
  (void)macAddress;

  handleReceivedPacket(data, length);
}

#endif

// ---------------- LED control ----------------

void turnBothLedsOff() {
  digitalWrite(Config::GREEN_LED_PIN, LOW);
  digitalWrite(Config::RED_LED_PIN, LOW);

  redLedState = false;
}

void processCommand(const WirelessMessage& message) {
  const Command command =
    static_cast<Command>(message.command);

  // Suppress repeated processing of the same sequence number.
  if (
    haveSequenceNumber &&
    message.sequence == lastSequenceNumber
  ) {
    logWarning("Duplicate command ignored.");
    return;
  }

  haveSequenceNumber = true;
  lastSequenceNumber = message.sequence;

  // Start every command with both LEDs off.
  turnBothLedsOff();

  Serial.println();
  Serial.println("[EVENT] Wireless command received.");

  Serial.print("[INFO] Command: ");
  Serial.println(commandName(command));

  Serial.print("[INFO] Sequence number: ");
  Serial.println(message.sequence);

  switch (command) {
    case Command::HELLO:
      activeLedMode = LedMode::HELLO;

      digitalWrite(Config::GREEN_LED_PIN, HIGH);

      logInfo("Green LED ON; red LED OFF.");
      break;

    case Command::SOS:
      activeLedMode = LedMode::SOS;

      redLedState = true;

      digitalWrite(Config::RED_LED_PIN, HIGH);

      lastBlinkTime = millis();

      logInfo("SOS alert active; red LED flashing.");
      break;

    case Command::STOP:
      activeLedMode = LedMode::OFF;

      turnBothLedsOff();

      logInfo("STOP received; both LEDs OFF.");
      break;
  }
}

// ---------------- Non-blocking SOS flashing ----------------

void updateLedPattern() {
  if (activeLedMode != LedMode::SOS) {
    return;
  }

  const uint32_t now = millis();

  if (
    (uint32_t)(now - lastBlinkTime) >=
      Config::SOS_BLINK_INTERVAL_MS
  ) {
    lastBlinkTime = now;
    redLedState = !redLedState;

    digitalWrite(
      Config::RED_LED_PIN,
      redLedState ? HIGH : LOW
    );
  }
}

// ---------------- Wireless initialization ----------------

bool initializeWireless() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  esp_err_t channelResult = esp_wifi_set_channel(
    Config::WIFI_CHANNEL,
    WIFI_SECOND_CHAN_NONE
  );

  if (channelResult != ESP_OK) {
    logError("Could not configure Wi-Fi channel.");
    return false;
  }

  if (esp_now_init() != ESP_OK) {
    logError("ESP-NOW initialization failed.");
    return false;
  }

  if (
    esp_now_register_recv_cb(onDataReceived) != ESP_OK
  ) {
    logError("Could not register receive callback.");
    return false;
  }

  Serial.print("[INFO] Receiver MAC: ");
  Serial.println(WiFi.macAddress());

  logInfo("ESP-NOW receive handler registered.");

  return true;
}

// ---------------- Arduino setup ----------------

void setup() {
  Serial.begin(Config::SERIAL_BAUD);

  delay(300);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" EMT WIRELESS RECEIVER");
  Serial.println(" Firmware Version: 2.0.0");
  Serial.println("========================================");

  pinMode(Config::GREEN_LED_PIN, OUTPUT);
  pinMode(Config::RED_LED_PIN, OUTPUT);

  turnBothLedsOff();

  activeLedMode = LedMode::OFF;

  logInfo("LED outputs initialized.");
  logInfo("Both LEDs forced OFF at startup.");

  if (!initializeWireless()) {
    logError("Startup failed. Restart the board to retry.");

    while (true) {
      delay(1000);
    }
  }

  Serial.println();
  logInfo("RECEIVER READY.");
  logInfo("Waiting for ESP-NOW messages.");

  Serial.println();
  Serial.println("LED mapping:");
  Serial.println("HELLO = Green LED ON");
  Serial.println("SOS   = Red LED flashing");
  Serial.println("STOP  = Both LEDs OFF");
  Serial.println();
}

// ---------------- Arduino loop ----------------

void loop() {
  WirelessMessage message = {};
  bool hasMessage = false;

  // Copy queued data safely out of the callback context.
  portENTER_CRITICAL(&messageMux);

  if (messagePending) {
    message = pendingMessage;
    messagePending = false;
    hasMessage = true;
  }

  portEXIT_CRITICAL(&messageMux);

  if (hasMessage) {
    processCommand(message);
  }

  updateLedPattern();

  delay(1);
}