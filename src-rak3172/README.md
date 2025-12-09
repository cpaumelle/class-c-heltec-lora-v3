# RAK3172 + ILI9341 TFT Version

This directory contains an adapted version of the LoRaWAN Class C parking display for the **RAK3172 module** with an **ILI9341 TFT display**.

## Hardware Differences

### RAK3172 Module
- **MCU:** STM32WLE5JC (ARM Cortex-M4 @ 48MHz)
- **Radio:** Integrated SX126x-compatible LoRa transceiver
- **Memory:** 256KB Flash, 64KB RAM
- **LoRa:** Built into the STM32WL chip (no external radio module needed)

### ILI9341 TFT Display
- **Size:** 2.4" or 2.8" (320x240 pixels)
- **Interface:** SPI
- **Colors:** 65K RGB (16-bit)
- **Much larger and more visible than the 128x64 OLED**

## Pin Configuration

### ILI9341 TFT Display Connections

```cpp
// SPI Pins (Hardware SPI1)
MOSI -> PA7
MISO -> PA6  (not needed for display-only operation)
SCK  -> PA5

// Display Control Pins (adjust as needed for your wiring)
TFT_CS  -> PA4  (Chip Select)
TFT_DC  -> PA3  (Data/Command)
TFT_RST -> PA2  (Reset)
```

**Note:** Adjust the pin definitions in `LoRaWAN_TFT_ParkingZone.ino` to match your actual wiring:

```cpp
#define TFT_CS    PA4   // Chip Select
#define TFT_DC    PA3   // Data/Command
#define TFT_RST   PA2   // Reset
```

### LoRa Radio

The RAK3172 has an **integrated LoRa transceiver** (STM32WLE5's built-in SX126x radio), so no external wiring is needed. The radio is accessed via the `STM32WLx` class in RadioLib.

## Building and Flashing

### PlatformIO

```bash
# Build for RAK3172
pio run -e rak3172

# Upload to RAK3172
pio run -e rak3172 --target upload
```

### Configuration

Edit `src-rak3172/config.h` to set your LoRaWAN credentials:

```cpp
#define RADIOLIB_LORAWAN_JOIN_EUI  0x16ED77AD6ABFE51DULL
#define RADIOLIB_LORAWAN_DEV_EUI   0x70B3D57ED0067002ULL
#define RADIOLIB_LORAWAN_APP_KEY   0xA1,0xB2,0xC3,0xD4, ...
#define RADIOLIB_LORAWAN_NWK_KEY   0xA1,0xB2,0xC3,0xD4, ...
```

## Display Features

The ILI9341 version takes advantage of the larger, color display:

- **Large text:** Number displayed in size 8 font (very visible)
- **Color coding:**
  - Header: White
  - Number: Green
  - Errors: Red background
- **Higher resolution:** 320x240 pixels vs 128x64 OLED
- **Landscape orientation** for better space utilization

## Key Differences from Heltec Version

| Feature | Heltec V3 | RAK3172 |
|---------|-----------|---------|
| MCU | ESP32-S3 (240MHz) | STM32WLE5 (48MHz) |
| Radio | External SX1262 | Integrated SX126x |
| Display | 0.96" OLED 128x64 | 2.4"/2.8" TFT 320x240 |
| Display Library | SSD1306Wire | Adafruit_ILI9341 |
| Storage | ESP32 Preferences (NVS) | Static RAM (volatile) |
| Power | USB-C / Battery | 3.3V power supply |

## Critical Configuration

### RF Switch Setup ⚠️

The RAK3172 **requires proper RF switch configuration** to transmit/receive. The code includes:

```cpp
// RF switch pins for RAK3172
static const uint32_t rfswitch_pins[] = {PB8, PC13, ...};
static const Module::RfSwitchMode_t rfswitch_table[] = {
  {STM32WLx::MODE_IDLE,  {LOW,  LOW}},
  {STM32WLx::MODE_RX,    {HIGH, LOW}},
  {STM32WLx::MODE_TX_HP, {LOW,  HIGH}},  // RAK3172 uses HP only
  END_OF_MODE_TABLE,
};
radio.setRfSwitchTable(rfswitch_pins, rfswitch_table);
```

**Without this configuration, the radio will not work!** The RF switch controls which internal path (TX/RX) is active.

### Current Limiting

The code sets over-current protection to 140mA to ensure full power transmission:

```cpp
radio.setCurrentLimit(140);  // Required for full 14 dBm output
```

## Storage Notes

⚠️ **Important:** The RAK3172 version uses **volatile RAM** for session storage. This means:

- Session data is preserved across resets (soft reset)
- Session data is **lost on power cycling**
- The device will rejoin the network after power loss

For persistent storage across power cycles, you would need to implement EEPROM emulation on the STM32's flash memory (not included in this basic version).

## Downlink Protocol

Same as the Heltec version:

- Send ASCII-encoded numbers as downlinks
- Example: `"24"` → Hex `32 34` displays "24" on screen
- Device listens continuously (Class C)
- Port 1 recommended

See the main [PAYLOAD_PROTOCOL.md](../PAYLOAD_PROTOCOL.md) for details.

## Testing

1. Power the RAK3172 via USB or external 3.3V
2. Watch serial output (115200 baud) for join status
3. Display should show "LoRaWAN Class C - Starting..."
4. After successful join, display shows "PLACES LIBRES: --"
5. Send a downlink to test display update

## Troubleshooting

### Display not working
- Check SPI wiring (especially CS, DC, RST pins)
- Verify 3.3V power supply to display
- Adjust pin definitions if your wiring differs

### Radio errors
- RAK3172 must have antenna connected
- Check that RadioLib version supports STM32WLx
- Verify LoRaWAN credentials in config.h

### Join failures
- Ensure device is in range of gateway
- Double-check JoinEUI, DevEUI, and AppKey
- Check that gateway is forwarding to network server

## Future Enhancements

Potential improvements for this version:

- [ ] Add EEPROM emulation for persistent session storage
- [ ] Implement low-power sleep modes between downlinks
- [ ] Add touch screen support for interactive features
- [ ] Display additional status information (RSSI, SNR, etc.)
- [ ] Add graphics/icons for different states
