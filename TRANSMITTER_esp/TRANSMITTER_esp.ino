
/*
 * ============================================================
 * Project : EMT Wireless Communication System
 * Device  : ESP32 Transmitter
 * Firmware: 3.0.0
 *
 * BUTTON CONNECTIONS:
 *   HELLO -> GPIO 25 -> GND
 *   SOS   -> GPIO 18 -> GND
 *   STOP  -> GPIO 19 -> GND
 *
 * Communication : ESP-NOW
 * Wi-Fi Channel  : 1
 * Serial Baud    : 115200
 * ============================================================
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_arduino_version.h>

// ============================================================
// CONFIGURATION
// ============================================================

namespace Config {
  constexpr uint8_t WIFI_CHANNEL = 1;

  constexpr uint8_t HELLO_PIN = 25;
  constexpr uint8_t SOS_PIN   = 18;
  constexpr uint8_t STOP_PIN  = 19;

  constexpr uint32_t SERIAL_BAUD = 115200;
  constexpr uint32_t DEBOUNCE_MS = 40;

  constexpr uint32_t PACKET_MAGIC = 0x454D5446;
}

// ============================================================
// COMMANDS
// ============================================================

enum class Command : uint8_t {
  HELLO = 1,
  SOS   = 2,
  STOP  = 3
};

// ============================================================
// MESSAGE FORMAT
// Must match the receiver's message structure.
// ============================================================

struct __attribute__((packed)) WirelessMessage {
  uint32_t magic;
  uint32_t sequence;
  uint8_t command;
};

static_assert(sizeof(WirelessMessage) == 9,
              "Unexpected message size");

// Broadcast MAC address
const uint8_t BROADCAST_MAC[6] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// ============================================================
// BUTTON STATE
// ============================================================

struct ButtonState {
  const char* name;
  uint8_t pin;
  Command command;

  bool lastRawState;
  bool stableState;

  uint32_t lastChangeTime;
};

ButtonState buttons[] = {
  {
    "HELLO", Config::HELLO_PIN, Command::HELLO,
    HIGH, HIGH, 0
  },
  {
    "SOS", Config::SOS_PIN, Command::SOS,
    HIGH, HIGH, 0
  },
  {
    "STOP", Config::STOP_PIN, Command::STOP,
    HIGH, HIGH, 0
  }
};

constexpr size_t BUTTON_COUNT =
  sizeof(buttons) / sizeof(buttons[0]);

// ============================================================
// COMMUNICATION STATE
// ============================================================

uint32_t sequenceNumber = 0;

volatile uint32_t successfulSends = 0;
volatile uint32_t failedSends = 0;

// ============================================================
// COMMAND NAME
// ============================================================

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

// ============================================================
// ESP-NOW SEND CALLBACK
// ============================================================

#if ESP_ARDUINO_VERSION_MAJOR >= 3

void onDataSent(
  const wifi_tx_info_t* info,
  esp_now_send_status_t status
) {
  (void)info;

  if (status == ESP_NOW_SEND_SUCCESS) {
    successfulSends++;
    Serial.println("[RADIO] MAC transmission succeeded.");
  } else {
    failedSends++;
    Serial.println("[RADIO] MAC transmission failed.");
  }
}

#else

void onDataSent(
  const uint8_t* macAddress,
  esp_now_send_status_t status
) {
  (void)macAddress;

  if (status == ESP_NOW_SEND_SUCCESS) {
    successfulSends++;
    Serial.println("[RADIO] MAC transmission succeeded.");
  } else {
    failedSends++;
    Serial.println("[RADIO] MAC transmission failed.");
  }
}

#endif

// ============================================================
// INITIALIZE ESP-NOW
// ============================================================

bool initializeWireless() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (
    esp_wifi_set_channel(
      Config::WIFI_CHANNEL,
      WIFI_SECOND_CHAN_NONE
    ) != ESP_OK
  ) {
    Serial.println("[ERROR] Failed to set Wi-Fi channel.");
    return false;
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] ESP-NOW initialization failed.");
    return false;
  }

  if (
    esp_now_register_send_cb(onDataSent) != ESP_OK
  ) {
    Serial.println("[ERROR] Send callback registration failed.");
    return false;
  }

  esp_now_peer_info_t peerInfo = {};

  memcpy(
    peerInfo.peer_addr,
    BROADCAST_MAC,
    sizeof(BROADCAST_MAC)
  );

  peerInfo.channel = Config::WIFI_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[ERROR] Failed to add broadcast peer.");
    return false;
  }

  Serial.print("[INFO] Transmitter MAC: ");
  Serial.println(WiFi.macAddress());

  Serial.println("[INFO] ESP-NOW initialized.");
  return true;
}

// ============================================================
// SEND COMMAND
// ============================================================

void sendCommand(Command command) {
  WirelessMessage message = {};

  message.magic = Config::PACKET_MAGIC;
  message.sequence = ++sequenceNumber;
  message.command = static_cast<uint8_t>(command);

  Serial.println();
  Serial.println("----------------------------------------");

  Serial.print("[BUTTON] ");
  Serial.println(commandName(command));

  Serial.print("[INFO] Sequence: ");
  Serial.println(message.sequence);

  Serial.print("[INFO] GPIO: ");

  switch (command) {
    case Command::HELLO:
      Serial.println(Config::HELLO_PIN);
      break;

    case Command::SOS:
      Serial.println(Config::SOS_PIN);
      break;

    case Command::STOP:
      Serial.println(Config::STOP_PIN);
      break;
  }

  esp_err_t result = esp_now_send(
    BROADCAST_MAC,
    reinterpret_cast<const uint8_t*>(&message),
    sizeof(message)
  );

  if (result == ESP_OK) {
    Serial.println("[INFO] Packet queued for transmission.");
  } else {
    Serial.print("[ERROR] Send request failed. Error: ");
    Serial.println(static_cast<int>(result));
  }

  Serial.println("----------------------------------------");
}

// ============================================================
// BUTTON PROCESSING
// ============================================================

void updateButtons() {
  const uint32_t now = millis();

  for (size_t i = 0; i < BUTTON_COUNT; i++) {
    ButtonState& button = buttons[i];

    const bool rawState = digitalRead(button.pin);

    // Detect raw changes
    if (rawState != button.lastRawState) {
      button.lastRawState = rawState;
      button.lastChangeTime = now;

      Serial.print("[DEBUG] GPIO ");
      Serial.print(button.pin);
      Serial.print(" -> ");
      Serial.println(rawState == HIGH ? "HIGH" : "LOW");
    }

    // Debounce the input
    if (
      (uint32_t)(now - button.lastChangeTime) >=
        Config::DEBOUNCE_MS &&
      rawState != button.stableState
    ) {
      button.stableState = rawState;

      Serial.print("[BUTTON] ");
      Serial.print(button.name);
      Serial.print(" is ");
      Serial.println(
        rawState == LOW ? "PRESSED" : "RELEASED"
      );

      // INPUT_PULLUP: LOW means pressed
      if (button.stableState == LOW) {
        sendCommand(button.command);
      }
    }
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(Config::SERIAL_BAUD);
  delay(500);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" EMT WIRELESS TRANSMITTER");
  Serial.println(" Firmware 3.0.0");
  Serial.println("========================================");

  // Configure button inputs
  for (size_t i = 0; i < BUTTON_COUNT; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);

    bool initialState = digitalRead(buttons[i].pin);

    buttons[i].lastRawState = initialState;
    buttons[i].stableState = initialState;
    buttons[i].lastChangeTime = millis();

    Serial.print("[INIT] ");
    Serial.print(buttons[i].name);
    Serial.print(" GPIO ");
    Serial.print(buttons[i].pin);
    Serial.print(" = ");
    Serial.println(initialState == HIGH ? "HIGH" : "LOW");
  }

  if (!initializeWireless()) {
    Serial.println("[FATAL] Wireless initialization failed.");

    while (true) {
      delay(1000);
    }
  }

  Serial.println();
  Serial.println("========== TRANSMITTER READY ==========");
  Serial.println("GPIO 25 = HELLO");
  Serial.println("GPIO 18 = SOS");
  Serial.println("GPIO 19 = STOP");
  Serial.println("========================================");
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
  updateButtons();
  delay(1);
}

