#ifndef _RADIOLIB_EX_LORAWAN_CONFIG_H
#define _RADIOLIB_EX_LORAWAN_CONFIG_H

#include <RadioLib.h>

// Heltec V3 SX1262 pin mapping
SX1262 radio = new Module(8, 14, 12, 13);

const uint32_t uplinkIntervalSeconds = 1UL * 60UL;

// LoRaWAN 1.0.3 credentials - NEW DEVEUI
#define RADIOLIB_LORAWAN_JOIN_EUI  0x16ED77AD6ABFE51DULL
#define RADIOLIB_LORAWAN_DEV_EUI   0x49351A037F5327AAULL  // Changed last byte from E8 to AA
#define RADIOLIB_LORAWAN_APP_KEY   0x55,0x26,0x0C,0xF5, 0xE9,0x32,0xF9,0xA1, 0x4A,0x39,0x4D,0x1D, 0xF4,0x2F,0xED,0x24

const LoRaWANBand_t Region = EU868;
const uint8_t subBand = 0;

uint64_t joinEUI =   RADIOLIB_LORAWAN_JOIN_EUI;
uint64_t devEUI  =   RADIOLIB_LORAWAN_DEV_EUI;
uint8_t appKey[] = { RADIOLIB_LORAWAN_APP_KEY };
uint8_t nwkKey[] = { RADIOLIB_LORAWAN_APP_KEY };  // Same as AppKey for LoRaWAN 1.0.x

LoRaWANNode node(&radio, &Region, subBand);

String stateDecode(const int16_t result) {
  switch (result) {
  case RADIOLIB_ERR_NONE:
    return "ERR_NONE";
  case RADIOLIB_ERR_CHIP_NOT_FOUND:
    return "ERR_CHIP_NOT_FOUND";
  case RADIOLIB_LORAWAN_SESSION_RESTORED:
    return "LORAWAN_SESSION_RESTORED";
  case RADIOLIB_LORAWAN_NEW_SESSION:
    return "LORAWAN_NEW_SESSION";
  default:
    return "Error " + String(result);
  }
}

void debug(bool failed, const __FlashStringHelper* message, int state, bool halt) {
  if(failed) {
    Serial.print(message);
    Serial.print(" - ");
    Serial.print(stateDecode(state));
    Serial.print(" (");
    Serial.print(state);
    Serial.println(")");
    while(halt) { delay(1); }
  }
}

#endif
