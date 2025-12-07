#include <Preferences.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include "config.h"

Preferences store;

// OLED pins for Heltec V3
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
#define OLED_ADDR 0x3C
#define Vext 36

SSD1306Wire display(OLED_ADDR, OLED_SDA, OLED_SCL);

RTC_DATA_ATTR uint8_t LWsession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
RTC_DATA_ATTR uint16_t bootCount = 0;

// Persistent status tracking
String currentStatus = "WAITING...";
uint8_t lastStatusByte = 0;
unsigned long lastStatusUpdate = 0;

// Uplink interval: 1 hour (3600 seconds) for keep-alive
const unsigned long UPLINK_INTERVAL_MS = 3600000UL;  // 1 hour

void displayStatus(String status) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 5, "ROOM STATUS:");
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 25, status);
  display.display();
  currentStatus = status;
  Serial.print(F("Display updated: ")); Serial.println(status);
}

void displayText(String line1, String line2 = "", String line3 = "") {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, line1);
  if (line2.length() > 0) display.drawString(0, 16, line2);
  if (line3.length() > 0) display.drawString(0, 32, line3);
  display.display();
}

void debug(bool failed, const String& message, int state, bool halt) {
  if(failed) {
    Serial.print(message); Serial.print(" - Code: "); Serial.println(state);
    displayText("ERROR!", message, "Code: " + String(state));
    while(halt) delay(1);
  }
}

String decodeStatus(uint8_t statusByte) {
  switch(statusByte) {
    case 1:
      return "AVAILABLE";
    case 2:
      return "OCCUPIED";
    case 3:
      return "RESERVED";
    default:
      return "UNKNOWN";
  }
}

int16_t lwActivate() {
  Serial.println(F("\n=== Activating LoRaWAN ==="));
  displayText("LoRaWAN", "Activating...");

  Serial.print(F("JoinEUI: 0x")); Serial.println((unsigned long)(joinEUI >> 32), HEX);
  Serial.print(F("DevEUI:  0x")); Serial.println((unsigned long)(devEUI >> 32), HEX);

  node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  Serial.println(F("beginOTAA() called"));

  store.begin("lorawan");

  if (store.isKey("nonces")) {
    Serial.println(F("Restoring nonces..."));
    displayText("LoRaWAN", "Restoring...");
    uint8_t buffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
    store.getBytes("nonces", buffer, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
    node.setBufferNonces(buffer);

    int16_t state = node.setBufferSession(LWsession);
    if (state == RADIOLIB_ERR_NONE && bootCount > 0) {
      state = node.activateOTAA();
      if (state == RADIOLIB_LORAWAN_SESSION_RESTORED) {
        Serial.println(F("✓ Session restored!"));
        displayText("LoRaWAN", "Session", "Restored!");
        store.end();
        return state;
      }
    }
  }

  Serial.println(F("Joining network..."));
  Serial.println(F("Sending JOIN REQUEST..."));
  displayText("LoRaWAN", "Joining...");

  unsigned long joinStart = millis();
  int16_t state = node.activateOTAA();
  unsigned long joinDuration = millis() - joinStart;

  Serial.print(F("Join attempt took: ")); Serial.print(joinDuration); Serial.println(F("ms"));
  Serial.print(F("Join result: ")); Serial.print(state);
  Serial.print(F(" (")); Serial.print(stateDecode(state)); Serial.println(F(")"));

  uint8_t buffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
  memcpy(buffer, node.getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  store.putBytes("nonces", buffer, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);

  if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
    Serial.println(F("\n✓✓✓ JOIN SUCCESSFUL! ✓✓✓"));
    displayText("LoRaWAN", "JOIN", "SUCCESSFUL!");
    delay(2000);
    memcpy(LWsession, node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  }

  store.end();
  return state;
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Initialize OLED
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);  // Enable external power
  delay(100);

  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);

  display.init();
  display.flipScreenVertically();
  display.setContrast(255);

  displayText("LoRaWAN Class C", "Starting...");

  Serial.println(F("\n=== LoRaWAN Class C Demo ==="));
  Serial.print(F("Boot: ")); Serial.println(++bootCount);

  int16_t state = radio.begin();
  debug(state != RADIOLIB_ERR_NONE, F("Radio failed"), state, true);

  Serial.println(F("✓ Radio initialized"));
  Serial.println(F("Configuring radio..."));
  Serial.println(F("  - TCXO: 1.8V"));
  radio.setTCXO(1.8);
  Serial.println(F("  - DIO2 as RF switch: enabled"));
  radio.setDio2AsRfSwitch(true);
  Serial.println(F("  - Output power: 14 dBm"));
  radio.setOutputPower(14);

  Serial.println(F("✓ Radio configured and ready"));
  displayText("Radio OK", "Initializing...");

  // Activate LoRaWAN
  int16_t joinState = lwActivate();
  if (joinState != RADIOLIB_LORAWAN_NEW_SESSION &&
      joinState != RADIOLIB_LORAWAN_SESSION_RESTORED) {
    Serial.print(F("✗ Join failed: ")); Serial.println(joinState);
    debug(true, F("Join failed"), joinState, true);
  }

  // Switch to Class C
  Serial.println(F("\n=== Switching to Class C ==="));
  displayText("Switching to", "Class C...");

  state = node.setClass(RADIOLIB_LORAWAN_CLASS_C);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("✗ Failed to switch to Class C: ")); Serial.println(state);
    debug(true, F("Class C switch failed"), state, true);
  }

  Serial.println(F("✓✓✓ Class C ACTIVATED! ✓✓✓"));
  Serial.println(F("Device now listening continuously for downlinks"));

  // Send immediate uplink to inform network of Class C mode
  Serial.println(F("\n=== Sending Class C Activation Uplink ==="));
  displayText("Class C Active", "Sending uplink...");

  uint8_t payload[2] = {0xFF, 0xC3};  // Special marker: Class C activated
  uint8_t dummyDownlink[256];
  size_t dummySize = 0;
  state = node.sendReceive(payload, sizeof(payload), 1, dummyDownlink, &dummySize, false);

  if (state == RADIOLIB_ERR_NONE || state > 0) {
    Serial.println(F("✓ Class C activation uplink sent"));
    if (state > 0 && dummySize > 0) {
      Serial.print(F("✓ Immediate downlink received! Size: ")); Serial.println(dummySize);
      lastStatusByte = dummyDownlink[0];
      String newStatus = decodeStatus(lastStatusByte);
      displayStatus(newStatus);
      lastStatusUpdate = millis();
      delay(3000);
    }
  } else {
    Serial.print(F("⚠ Uplink failed: ")); Serial.println(state);
  }

  Serial.println(F("Uplink interval: 1 hour (keep-alive only)"));
  Serial.println(F("\n=== Ready! Send downlink anytime ===\n"));

  delay(2000);
  displayStatus("WAITING...");
}

void loop() {
  static unsigned long lastUplink = 0;
  static uint16_t counter = 0;

  // Check for Class C downlinks (non-blocking)
  uint8_t downlinkData[256];
  size_t downlinkSize = 0;

  int16_t state = node.getDownlinkClassC(downlinkData, &downlinkSize);

  if (state > 0) {
    // Downlink received!
    Serial.println(F("\n============================================================"));
    Serial.println(F("CLASS C DOWNLINK RECEIVED!"));
    Serial.print(F("Size: ")); Serial.println(downlinkSize);
    Serial.print(F("Data: "));
    for(size_t i = 0; i < downlinkSize; i++) {
      if(downlinkData[i] < 0x10) Serial.print('0');
      Serial.print(downlinkData[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
    Serial.println(F("============================================================\n"));

    // Decode and update persistent display
    if (downlinkSize > 0) {
      lastStatusByte = downlinkData[0];
      String newStatus = decodeStatus(lastStatusByte);
      displayStatus(newStatus);
      lastStatusUpdate = millis();

      // Save session after downlink
      memcpy(LWsession, node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
    }
  } else if (state != RADIOLIB_ERR_NONE && state != RADIOLIB_ERR_RX_TIMEOUT) {
    // Only log actual errors, not timeout (which is normal)
    Serial.print(F("Class C receive state: ")); Serial.println(state);
  }

  // Send hourly keep-alive uplink
  if (millis() - lastUplink > UPLINK_INTERVAL_MS) {
    lastUplink = millis();

    Serial.println(F("\n=========================================================="));
    Serial.println(F("--- Sending Keep-Alive Uplink ---"));
    Serial.print(F("Counter: ")); Serial.println(counter);
    Serial.print(F("FCnt before send: ")); Serial.println(node.getFCntUp());
    Serial.print(F("Current status byte: 0x"));
    if(lastStatusByte < 0x10) Serial.print('0');
    Serial.println(lastStatusByte, HEX);

    // Payload: counter (2 bytes) + last status (1 byte)
    uint8_t payload[3];
    payload[0] = highByte(counter);
    payload[1] = lowByte(counter);
    payload[2] = lastStatusByte;

    Serial.print(F("Payload hex: "));
    for(size_t i = 0; i < sizeof(payload); i++) {
      if(payload[i] < 0x10) Serial.print('0');
      Serial.print(payload[i], HEX);
      Serial.print(' ');
    }
    Serial.println();

    // Temporarily show sending status
    String savedStatus = currentStatus;
    displayText("Sending uplink...", "Count: " + String(counter));

    // Send uplink (Class C devices don't wait for downlink in RX windows)
    uint8_t dummyDownlink[256];
    size_t dummySize = 0;
    state = node.sendReceive(payload, sizeof(payload), 1, dummyDownlink, &dummySize, false);
    counter++;

    Serial.println(F("--- Result ---"));
    Serial.print(F("State code: ")); Serial.print(state);
    Serial.print(F(" (")); Serial.print(stateDecode(state)); Serial.println(F(")"));
    Serial.print(F("FCnt after send: ")); Serial.println(node.getFCntUp());
    Serial.println(F("==========================================================\n"));

    if (state == RADIOLIB_ERR_NONE || state > 0) {
      Serial.println(F("✓ Keep-alive uplink sent"));
    } else {
      Serial.print(F("✗ Uplink Failed: ")); Serial.print(state);
      Serial.print(F(" (")); Serial.print(stateDecode(state)); Serial.println(F(")"));

      // Check if session is broken
      if (node.getFCntUp() == 0 && state < 0) {
        Serial.println(F("⚠ Session appears lost (FCnt=0), clearing and rejoining..."));
        displayText("Session Lost", "Clearing NVS", "Rejoining...");

        // Clear stored session
        store.begin("lorawan");
        store.remove("nonces");
        store.end();
        memset(LWsession, 0, RADIOLIB_LORAWAN_SESSION_BUF_SIZE);

        int16_t rejoinState = lwActivate();
        if (rejoinState == RADIOLIB_LORAWAN_NEW_SESSION ||
            rejoinState == RADIOLIB_LORAWAN_SESSION_RESTORED) {
          Serial.println(F("✓ Rejoin successful!"));

          // Re-enable Class C
          state = node.setClass(RADIOLIB_LORAWAN_CLASS_C);
          if (state == RADIOLIB_ERR_NONE) {
            Serial.println(F("✓ Class C re-enabled"));
          }
          delay(2000);
        } else {
          Serial.print(F("✗ Rejoin failed: ")); Serial.println(rejoinState);
          displayText("Rejoin Failed", "Code: " + String(rejoinState));
          delay(5000);
        }
      }
    }

    // Restore persistent status display
    delay(1000);
    displayStatus(savedStatus);

    // Save session
    memcpy(LWsession, node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  }

  // Small delay to prevent tight loop
  delay(100);
}
