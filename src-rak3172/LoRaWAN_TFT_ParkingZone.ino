#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include "config.h"

// ILI9341 TFT display pins for RAK3172
// Adjust these pins based on your actual wiring
#define TFT_CS    PA4   // Chip Select
#define TFT_DC    PA3   // Data/Command
#define TFT_RST   PA2   // Reset
// MOSI (PA7), MISO (PA6), SCK (PA5) are hardware SPI pins

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Session storage in STM32 backup SRAM (survives reset but not power loss)
// For RAK3172, we'll use a simple approach with static variables
// Note: Full persistent storage would require EEPROM emulation
uint8_t LWsession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
uint8_t noncesBuffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
uint16_t bootCount = 0;
bool sessionStored = false;

// Persistent status tracking
String currentStatus = "WAITING...";
uint8_t lastStatusByte = 0;
unsigned long lastStatusUpdate = 0;

// Uplink interval: 1 hour (3600 seconds) for keep-alive
const unsigned long UPLINK_INTERVAL_MS = 3600000UL;  // 1 hour

// Color definitions
#define ILI9341_DARKBLUE  0x0010
#define ILI9341_ORANGE    0xFD20

// === DEDICATED CAR PARK DISPLAY FUNCTION ===
// Uses the larger TFT display for better visibility
void displayCarParkStatus(String numberString) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);

  // Center the header text
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds("PLACES LIBRES:", 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((320 - w) / 2, 40);
  tft.println("PLACES LIBRES:");

  // Display the number in very large font
  tft.setTextSize(8);
  tft.setTextColor(ILI9341_GREEN);
  tft.getTextBounds(numberString, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((320 - w) / 2, 100);
  tft.println(numberString);

  // Update internal tracking
  currentStatus = "PLACES LIBRES: " + numberString;
  Serial.print(F("Display updated: ")); Serial.println(currentStatus);
}

// Generic display status function (kept for other messages like Joining/Error)
void displayText(String line1, String line2 = "", String line3 = "") {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 20);
  tft.println(line1);
  if (line2.length() > 0) {
    tft.setCursor(10, 60);
    tft.println(line2);
  }
  if (line3.length() > 0) {
    tft.setCursor(10, 100);
    tft.println(line3);
  }
}

void debugWithDisplay(bool failed, const String& message, int state, bool halt) {
  if(failed) {
    Serial.print(message); Serial.print(" - Code: "); Serial.println(state);
    tft.fillScreen(ILI9341_RED);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 20);
    tft.println("ERROR!");
    tft.setCursor(10, 60);
    tft.println(message);
    tft.setCursor(10, 100);
    tft.print("Code: ");
    tft.println(state);
    while(halt) delay(1);
  }
}

int16_t lwActivate() {
  Serial.println(F("\n=== Activating LoRaWAN ==="));
  displayText("LoRaWAN", "Activating...");

  Serial.print(F("JoinEUI: 0x")); Serial.println((unsigned long long)joinEUI, HEX);
  Serial.print(F("DevEUI:  0x")); Serial.println((unsigned long long)devEUI, HEX);

  node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  Serial.println(F("beginOTAA() called"));

  if (sessionStored && bootCount > 0) {
    Serial.println(F("Restoring nonces..."));
    displayText("LoRaWAN", "Restoring...");
    node.setBufferNonces(noncesBuffer);

    int16_t state = node.setBufferSession(LWsession);
    if (state == RADIOLIB_ERR_NONE) {
      state = node.activateOTAA();
      if (state == RADIOLIB_LORAWAN_SESSION_RESTORED) {
        Serial.println(F("✓ Session restored!"));
        displayText("LoRaWAN", "Session", "Restored!");
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

  memcpy(noncesBuffer, node.getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);

  if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
    Serial.println(F("\n✓✓✓ JOIN SUCCESSFUL! ✓✓✓"));
    displayText("LoRaWAN", "JOIN", "SUCCESSFUL!");
    delay(2000);
    memcpy(LWsession, node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
    sessionStored = true;
  }

  return state;
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Initialize TFT display
  tft.begin();
  tft.setRotation(1); // Landscape orientation
  tft.fillScreen(ILI9341_BLACK);

  displayText("LoRaWAN Class C", "Starting...");

  Serial.println(F("\n=== LoRaWAN Class C Demo ==="));
  Serial.print(F("Boot: ")); Serial.println(++bootCount);

  // Configure RF switch before radio initialization (CRITICAL for RAK3172)
  Serial.println(F("Configuring RF switch..."));
  radio.setRfSwitchTable(rfswitch_pins, rfswitch_table);

  int16_t state = radio.begin();
  debugWithDisplay(state != RADIOLIB_ERR_NONE, "Radio failed", state, true);

  Serial.println(F("✓ Radio initialized"));
  Serial.println(F("Configuring radio..."));
  Serial.println(F("  - Output power: 14 dBm"));
  radio.setOutputPower(14);

  // Set over current protection for full power transmission
  Serial.println(F("  - Current limit: 140 mA"));
  state = radio.setCurrentLimit(140);
  debugWithDisplay(state == RADIOLIB_ERR_INVALID_CURRENT_LIMIT, "Invalid current limit", state, true);

  Serial.println(F("✓ Radio configured and ready"));
  displayText("Radio OK", "Initializing...");

  // Activate LoRaWAN
  int16_t joinState = lwActivate();
  if (joinState != RADIOLIB_LORAWAN_NEW_SESSION &&
      joinState != RADIOLIB_LORAWAN_SESSION_RESTORED) {
    Serial.print(F("✗ Join failed: ")); Serial.println(joinState);
    debugWithDisplay(true, "Join failed", joinState, true);
  }

  // Switch to Class C
  Serial.println(F("\n=== Switching to Class C ==="));
  displayText("Switching to", "Class C...");

  state = node.setClass(RADIOLIB_LORAWAN_CLASS_C);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("✗ Failed to switch to Class C: ")); Serial.println(state);
    debugWithDisplay(true, "Class C switch failed", state, true);
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

      // Process initial downlink as a number string
      String initialNumber = "";
      for (size_t i = 0; i < dummySize; i++) {
          initialNumber += (char)dummyDownlink[i];
      }
      displayCarParkStatus(initialNumber);
      lastStatusUpdate = millis();
      lastStatusByte = dummyDownlink[0];

      delay(3000);
    }
  } else {
    Serial.print(F("⚠ Uplink failed: ")); Serial.println(state);
  }

  Serial.println(F("Uplink interval: 1 hour (keep-alive only)"));
  Serial.println(F("\n=== Ready! Send downlink anytime ===\n"));

  delay(2000);
  displayCarParkStatus("--"); // Set initial status to be ready for the first message
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

    // Decode raw payload as ASCII number string
    if (downlinkSize > 0) {
      String numberString = "";
      for (size_t i = 0; i < downlinkSize; i++) {
          // Convert each byte to its ASCII character
          numberString += (char)downlinkData[i];
      }

      // Use the dedicated car park display function
      displayCarParkStatus(numberString);

      lastStatusUpdate = millis();
      lastStatusByte = downlinkData[0]; // Set the first byte for the next uplink

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
    Serial.print(F("Current status byte (first byte of last number): 0x"));
    if(lastStatusByte < 0x10) Serial.print('0');
    Serial.println(lastStatusByte, HEX);

    // Payload: counter (2 bytes) + last status (1 byte)
    uint8_t payload[3];
    payload[0] = highByte(counter);
    payload[1] = lowByte(counter);
    payload[2] = lastStatusByte; // The first byte of the last received number string

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
        displayText("Session Lost", "Clearing session", "Rejoining...");

        // Clear stored session
        memset(LWsession, 0, RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
        memset(noncesBuffer, 0, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
        sessionStored = false;

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
    // Extract the number from the saved status to re-display it
    String numberToRestore = savedStatus.substring(savedStatus.lastIndexOf(' ') + 1);
    displayCarParkStatus(numberToRestore);

    // Save session
    memcpy(LWsession, node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  }

  // Small delay to prevent tight loop
  delay(100);
}
