#ifndef _RADIOLIB_EX_LORAWAN_CONFIG_H
#define _RADIOLIB_EX_LORAWAN_CONFIG_H

// Must include RadioLib core first, then the LoRaWAN protocol headers so
// types like LoRaWANBand_t, LoRaWANNode and the RADIOLIB_* macros are defined.
#include <RadioLib.h>

// RAK3172 has integrated STM32WLE5 with SX126x radio
// The STM32WLx module handles the internal connections automatically
STM32WLx radio = new STM32WLx_Module();

// how often to send an uplink - consider legal & FUP constraints - see notes
const uint32_t uplinkIntervalSeconds = 1UL * 60UL;    // minutes x seconds

// Replace with your JoinEUI (AppEUI) - 8 bytes, MSB as ULL literal
#define RADIOLIB_LORAWAN_JOIN_EUI  0x16ED77AD6ABFE51DULL

// Device EUI (8 bytes)
#ifndef RADIOLIB_LORAWAN_DEV_EUI
#define RADIOLIB_LORAWAN_DEV_EUI   0x70B3D57ED0067002ULL
#endif

// AppKey (16 bytes) - provide as comma-separated byte list (MSB first)
#ifndef RADIOLIB_LORAWAN_APP_KEY
#define RADIOLIB_LORAWAN_APP_KEY   0xA1,0xB2,0xC3,0xD4, 0xE5,0xF6,0x01,0x23, 0x45,0x67,0x89,0xAB, 0xCD,0xEF,0x01,0x23
#endif

// NwkKey (16 bytes) - needed for LoRaWAN 1.1 and sometimes RadioLib internal operations
// For LoRaWAN 1.0.x, NwkKey should equal AppKey
#ifndef RADIOLIB_LORAWAN_NWK_KEY
#define RADIOLIB_LORAWAN_NWK_KEY   0xA1,0xB2,0xC3,0xD4, 0xE5,0xF6,0x01,0x23, 0x45,0x67,0x89,0xAB, 0xCD,0xEF,0x01,0x23
#endif

// Region selection — set to EU868 for most of Europe
const LoRaWANBand_t Region = EU868;
const uint8_t subBand = 0;  // US915-specific, leave 0 for EU868

// ============================================================================
// Internal glue — keep these, RadioLib example expects them

uint64_t joinEUI =   RADIOLIB_LORAWAN_JOIN_EUI;
uint64_t devEUI  =   RADIOLIB_LORAWAN_DEV_EUI;
uint8_t appKey[] = { RADIOLIB_LORAWAN_APP_KEY };
uint8_t nwkKey[] = { RADIOLIB_LORAWAN_NWK_KEY };

// Create LoRaWAN node instance used by RadioLib examples
LoRaWANNode node(&radio, &Region, subBand);

// Human-readable status helper (keeps the example logs friendly)
String stateDecode(const int16_t result) {
  switch (result) {
  case RADIOLIB_ERR_NONE:
    return "ERR_NONE";
  case RADIOLIB_ERR_CHIP_NOT_FOUND:
    return "ERR_CHIP_NOT_FOUND";
  case RADIOLIB_ERR_PACKET_TOO_LONG:
    return "ERR_PACKET_TOO_LONG";
  case RADIOLIB_ERR_RX_TIMEOUT:
    return "ERR_RX_TIMEOUT";
  case RADIOLIB_ERR_CRC_MISMATCH:
    return "ERR_CRC_MISMATCH";
  case RADIOLIB_ERR_INVALID_BANDWIDTH:
    return "ERR_INVALID_BANDWIDTH";
  case RADIOLIB_ERR_INVALID_SPREADING_FACTOR:
    return "ERR_INVALID_SPREADING_FACTOR";
  case RADIOLIB_ERR_INVALID_CODING_RATE:
    return "ERR_INVALID_CODING_RATE";
  case RADIOLIB_ERR_INVALID_FREQUENCY:
    return "ERR_INVALID_FREQUENCY";
  case RADIOLIB_ERR_INVALID_OUTPUT_POWER:
    return "ERR_INVALID_OUTPUT_POWER";
  case RADIOLIB_LORAWAN_SESSION_RESTORED:
    return "LORAWAN_SESSION_RESTORED";
  case RADIOLIB_LORAWAN_NEW_SESSION:
    return "LORAWAN_NEW_SESSION";
  case -1101:
    return "ERR_NETWORK_NOT_JOINED";
  case -1102:
    return "ERR_DOWNLINK_MALFORMED";
  case -1116:
    return "ERR_NO_JOIN_ACCEPT (check keys/range)";
  default:
    return "See https://jgromes.github.io/RadioLib/group__status__codes.html";
  }
}

// helper function to display any issues
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

// helper function to display a byte array (hex)
void arrayDump(uint8_t *buffer, uint16_t len) {
  for(uint16_t c = 0; c < len; c++) {
    uint8_t b = buffer[c];
    if(b < 0x10) { Serial.print('0'); }
    Serial.print(b, HEX);
  }
  Serial.println();
}

#endif
