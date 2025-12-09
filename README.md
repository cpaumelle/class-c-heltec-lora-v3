# Heltec WiFi LoRa 32 V3 - Class C LoRaWAN Display

LoRaWAN Class C implementation for Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262) with persistent OLED status display using RadioLib.

## Firmware Versions

This repository contains two firmware variants:

| Version | File | Use Case | Display |
|---------|------|----------|---------|
| **Single Space** | `LoRaWAN_OLED_SingleSpace.ino` | Individual parking space status | `AVAILABLE` / `OCCUPIED` / `RESERVED` |
| **Parking Zone** | `LoRaWAN_OLED_ParkingZone.ino` | Zone availability counter | `PL. LIBRES :` + number (e.g., `24`) |

## Features

✅ **LoRaWAN 1.0.3A OTAA** - Over-the-Air Activation with session persistence
✅ **Class C Operation** - Always listening for downlinks (instant updates!)
✅ **Persistent Status Display** - Shows status until next update
✅ **Hourly Keep-Alive Uplinks** - Minimal network traffic (not frequent uplinks)
✅ **Session Recovery** - NVS + RTC RAM for seamless reboots
✅ **EU868 Region** - Configurable for other regions
✅ **RadioLib 7.3.0** - Full Class C support

---

## Hardware

**Board:** Heltec WiFi LoRa 32 V3
- **MCU:** ESP32-S3FN8 (240MHz, 320KB RAM, 8MB Flash)
- **Radio:** Semtech SX1262 LoRa transceiver
- **Display:** 0.96" OLED (128x64, SSD1306)
- **Antenna:** External antenna via IPEX/U.FL connector (**REQUIRED!**)

### Pin Configuration

```cpp
// SX1262 Radio
NSS   = 8
DIO1  = 14
RESET = 12
BUSY  = 13

// OLED Display (I2C)
SDA   = 17
SCL   = 18
RST   = 21
Vext  = 36  // External power enable (active LOW)
```

---

## Quick Start

### 1. Install PlatformIO

```bash
pip install platformio
```

### 2. Configure LoRaWAN Credentials

Edit `src/config.h` with your network credentials:

```cpp
#define RADIOLIB_LORAWAN_JOIN_EUI  0x16ED77AD6ABFE51DULL
#define RADIOLIB_LORAWAN_DEV_EUI   0x70B3D57ED0067001ULL
#define RADIOLIB_LORAWAN_APP_KEY   0xA1,0xB2,0xC3,0xD4, 0xE5,0xF6,0x01,0x23, 0x45,0x67,0x89,0xAB, 0xCD,0xEF,0x01,0x23
#define RADIOLIB_LORAWAN_NWK_KEY   0xA1,0xB2,0xC3,0xD4, 0xE5,0xF6,0x01,0x23, 0x45,0x67,0x89,0xAB, 0xCD,0xEF,0x01,0x23
```

**Note:** For LoRaWAN 1.0.x, `nwkKey` should equal `appKey`.

### 3. Configure ChirpStack for Class C

**CRITICAL:** Your device MUST be configured as Class C in ChirpStack:

1. Go to ChirpStack web UI
2. Navigate to your device (DevEUI from config.h)
3. Use a **Class C device profile** or configure device class as **Class C**
4. Save configuration

### 4. Build and Upload

```bash
cd heltec-lorawan
pio run -e heltec_wifi_lora_32_V3 -t upload
```

### 5. Monitor Serial Output

```bash
pio device monitor -p /dev/ttyUSB0 -b 115200
```

---

## How It Works

### Boot Sequence

1. **Radio Initialization** - Configures SX1262 with TCXO and RF switch
2. **LoRaWAN Join** - OTAA join or session restore from NVS/RTC
3. **Class C Activation** - Switches to Class C mode
4. **Activation Uplink** - Sends immediate uplink to inform network
5. **Continuous Listening** - Device now listens on RX2 continuously

### Class C Operation

- **Device is ALWAYS listening** for downlinks (except during TX)
- **Instant updates** - Downlinks received within seconds
- **Persistent display** - Status stays on screen until next downlink
- **Hourly keep-alives** - Device sends uplink every hour with status byte

### Display States

| State | Display |
|-------|---------|
| **Startup** | `LoRaWAN Class C` / `Starting...` |
| **Radio Init** | `Radio OK` / `Initializing...` |
| **Joining** | `LoRaWAN` / `Joining...` |
| **Join Success** | `LoRaWAN` / `JOIN` / `SUCCESSFUL!` |
| **Session Restored** | `LoRaWAN` / `Session` / `Restored!` |
| **Class C Switch** | `Switching to` / `Class C...` |
| **Activation Uplink** | `Class C Active` / `Sending uplink...` |
| **Waiting** | `WAITING...` (large, centered) |
| **Status Update** | `AVAILABLE` / `OCCUPIED` / `RESERVED` (large, centered) |

---

## Sending Downlinks

### From ChirpStack Web UI

1. Navigate to device (DevEUI: `70b3d57ed0067001`)
2. Go to **Queue** tab
3. Set **FPort**: `1`
4. Enter **Hex Payload** (see format below)
5. Click **Enqueue**
6. Status displays **immediately** (within ~1-5 seconds)
7. Status **persists** on screen until next downlink

### Single Space Version (LoRaWAN_OLED_SingleSpace.ino)

| Status | Hex Payload | Display |
|--------|-------------|---------|
| **Available** | `01` | `AVAILABLE` |
| **Occupied** | `02` | `OCCUPIED` |
| **Reserved** | `03` | `RESERVED` |

### Parking Zone Version (LoRaWAN_OLED_ParkingZone.ino)

Send ASCII digits as hex bytes:

| Spaces | Hex Payload | Display |
|--------|-------------|---------|
| **5** | `35` | `PL. LIBRES : 5` |
| **24** | `3234` | `PL. LIBRES : 24` |
| **156** | `313536` | `PL. LIBRES : 156` |

**ASCII Reference:** `0`=0x30, `1`=0x31, `2`=0x32, ... `9`=0x39

### Important Notes

- **Class C = Instant delivery** - No need to wait for uplinks!
- **Persistent display** - Unlike Class A, status stays visible
- **Hourly uplinks** - Device reports current status every hour
- **MAC commands** - Network may send LinkADRReq, RxParamSetupReq (normal operation)

---

## Payload Protocol

### Uplink Format

The device sends periodic keep-alive uplinks every 60 minutes.

**FPort**: 1
**Payload**: 3 bytes

| Byte | Description | Example |
|------|-------------|---------|
| 0 | Counter (high byte) | `0x00` |
| 1 | Counter (low byte) | `0x05` (counter=5) |
| 2 | Last status byte | `0x02` (last received status) |

**Byte 2 Contents**:
- **Single Space**: Last received status code (0x01/0x02/0x03)
- **Zone Counter**: First ASCII character of last number (e.g., '2' from "24")

**Example Sequence**:
```
Uplink #1:  00 00 00  (counter=0, no downlink yet)
Downlink:   02        (set to OCCUPIED)
Uplink #2:  00 01 02  (counter=1, last status=0x02)
Uplink #3:  00 02 02  (counter=2, last status=0x02)
```

**Counter Behavior**:
- Starts at 0 after device boot
- Increments by 1 after each uplink
- 16-bit value (0-65535), then rolls over to 0

### Initial Activation Uplink

Immediately after OTAA join, the device sends a special 2-byte payload:

**FPort**: 1
**Payload**: `FF C3`

This signals to the network server that the device is now in Class C mode and ready for immediate downlinks.

### Full Technical Specification

For complete payload protocol details, see **[PAYLOAD_PROTOCOL.md](PAYLOAD_PROTOCOL.md)** which includes:
- Detailed downlink/uplink formats
- Integration with Smart Parking Platform
- Payload size limits
- Error handling
- Testing procedures

---

## Configuration

### Change Uplink Interval

Edit `src/LoRaWAN_OLED.ino`:

```cpp
// Default: 1 hour (3600000 ms)
const unsigned long UPLINK_INTERVAL_MS = 3600000UL;

// For testing: 5 minutes
const unsigned long UPLINK_INTERVAL_MS = 300000UL;
```

### Change Region

Edit `src/config.h`:

```cpp
const LoRaWANBand_t Region = EU868;  // Change to US915, AU915, etc.
```

---

## File Structure

```
class-c-heltec-lora-v3/
├── platformio.ini                    # PlatformIO configuration
├── firmware.bin                      # Pre-built firmware
├── src/
│   ├── LoRaWAN_OLED_SingleSpace.ino  # Single space status display
│   ├── LoRaWAN_OLED_ParkingZone.ino  # Zone availability counter
│   └── config.h                      # LoRaWAN credentials & radio config
├── examples/                         # Example code
└── README.md                         # This file
```

### Building a Specific Version

To build the Parking Zone version, rename/copy the desired `.ino` file:

```bash
# For Parking Zone version
cp src/LoRaWAN_OLED_ParkingZone.ino src/main.cpp
pio run -e heltec_wifi_lora_32_V3 -t upload

# For Single Space version
cp src/LoRaWAN_OLED_SingleSpace.ino src/main.cpp
pio run -e heltec_wifi_lora_32_V3 -t upload
```

---

## Troubleshooting

### Downlinks Not Received

**MOST COMMON ISSUE:** ChirpStack device not configured for Class C

1. ✅ Verify device profile is **Class C** in ChirpStack web UI
2. ✅ Check serial output for "✓✓✓ Class C ACTIVATED!"
3. ✅ Ensure external antenna is connected
4. ✅ Verify gateway is in range and online
5. ✅ Use FPort 1 for downlinks
6. ✅ Check ChirpStack Events tab for transmission confirmation

### Display Not Working

1. **Check Vext**: GPIO 36 must be LOW to power OLED
2. **I2C Address**: Display at 0x3C
3. **OLED Reset**: GPIO 21 toggled during initialization
4. **Pins**: SDA=17, SCL=18

### Session Lost / Join Failures

1. **Clear NVS**: Device automatically clears and rejoins if session corrupted
2. **DevNonce Replay**: Change DevEUI last byte if "DevNonce already used"
3. **Check Credentials**: Verify JoinEUI, DevEUI, AppKey match network server
4. **Check Antenna**: Must be connected for LoRaWAN to work

### Serial Monitor Issues

If serial output not visible over SSH:

```bash
# Kill existing processes
pkill -9 cat

# Start simple monitor
cat /dev/ttyUSB0

# Or use PlatformIO
pio device monitor -p /dev/ttyUSB0 -b 115200
```

---

## Performance

- **Flash Usage:** ~380 KB (11.3%)
- **RAM Usage:** ~22 KB (6.8%)
- **Join Time:** ~5-7 seconds (first join)
- **Session Restore:** Instant (on reboot)
- **Class C Switch:** ~2 seconds after join
- **Downlink Latency:** 1-5 seconds (Class C continuous RX)
- **Display Update:** <100ms
- **Uplink Interval:** 1 hour (configurable)

---

## Network Server Configuration

### ChirpStack v4

1. Create application
2. Create **Class C device profile**:
   - **Device Class:** Class C
   - **LoRaWAN Version:** 1.0.3 (or 1.0.4)
   - **Regional Parameters:** RP001 1.0.3 revision A
   - **MAC Version:** 1.0.3
3. Add device with:
   - **DevEUI:** `70b3d57ed0067001`
   - **JoinEUI/AppEUI:** `16ed77ad6abfe51d`
   - **AppKey:** `A1B2C3D4E5F60123456789ABCDEF0123`
   - **Device Profile:** Select your Class C profile
4. Device will join and switch to Class C automatically

### The Things Network (TTN)

- **LoRaWAN version:** MAC V1.0.3
- **Regional Parameters:** RP001 1.0.3 revision A
- **Class:** Class C
- Configure same credentials as ChirpStack

---

## Technical Details

### Class C vs Class A

| Feature | Class A | Class C (This Project) |
|---------|---------|------------------------|
| **RX Windows** | Only after TX | Continuous (RX2) |
| **Downlink Latency** | Up to 1 hour | 1-5 seconds |
| **Power Consumption** | Low | Higher (always listening) |
| **Use Case** | Battery sensors | Mains-powered displays |
| **Uplink Frequency** | As needed | Hourly keep-alive |

### Session Persistence

- **NVS (Flash):** Stores DevNonce and join nonces
- **RTC RAM:** Stores session keys (survives deep sleep/reboot)
- Session automatically restored on boot
- No rejoin needed after power cycle

### RadioLib Class C Implementation

```cpp
// After OTAA join
node.setClass(RADIOLIB_LORAWAN_CLASS_C);

// Main loop: Check for Class C downlinks
int16_t state = node.getDownlinkClassC(downlinkData, &downlinkSize);
if (state > 0) {
  // Downlink received!
  processDownlink(downlinkData, downlinkSize);
}
```

---

## License

This project uses:
- [RadioLib](https://github.com/jgromes/RadioLib) by Jan Gromeš (MIT)
- [ESP8266 and ESP32 OLED driver](https://github.com/ThingPulse/esp8266-oled-ssd1306) by ThingPulse (MIT)

---

## Credits

- **RadioLib:** Jan Gromeš - https://github.com/jgromes/RadioLib
- **Board:** Heltec Automation - https://heltec.org
- **OLED Library:** ThingPulse - https://github.com/ThingPulse/esp8266-oled-ssd1306
- **Implementation:** Class C LoRaWAN room status display

---

## Support

For issues:
- **RadioLib:** https://github.com/jgromes/RadioLib/issues
- **Heltec Board:** https://community.heltec.cn/
- **ChirpStack:** https://forum.chirpstack.io/

---

## Changelog

### v3.1 - OLED Display Fix (2025-12-09, commit 95daeb6)
- ✅ Fixed OLED display initialization in ParkingZone variant
- ✅ Corrected SDA/SCL pin order in SSD1306Wire constructor
- ✅ Display now works correctly when powered via USB
- ✅ No payload protocol changes (fully backward compatible)

### v3.0 - Parking Zone Display
- ✅ New Parking Zone firmware variant
- ✅ Displays available space count with large font
- ✅ French header "PL. LIBRES :"
- ✅ ASCII number payload support

### v2.0 - Class C Implementation
- ✅ Class C operation with continuous RX
- ✅ Persistent status display (no timeout)
- ✅ Hourly keep-alive uplinks
- ✅ Immediate downlink reception
- ✅ Activation uplink after Class C switch

### v1.0 - Class A Implementation
- ✅ Class A with 30-second uplinks
- ✅ 5-second status display timeout
- ✅ Downlinks only in RX windows
