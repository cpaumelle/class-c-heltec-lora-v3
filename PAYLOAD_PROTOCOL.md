# Heltec LoRa V3 Payload Protocol Specification

**Version**: 1.1
**Last Updated**: 2025-12-09
**Firmware Repository**: [class-c-heltec-lora-v3](https://github.com/cpaumelle/class-c-heltec-lora-v3)
**Minimum Firmware Version**: Commit `95daeb6` or later (ParkingZone display fix applied)

---

## Overview

This document describes the **generic LoRaWAN payload protocol** for Heltec WiFi LoRa 32 V3 Class C display devices. While the reference implementation demonstrates parking space monitoring, the protocol is **application-agnostic** and can be used for:

- 🏢 Room/desk occupancy indicators
- 📦 Inventory level counters
- 🚪 Access status displays
- 👥 Queue length displays
- 🎯 Any binary state indicator (2-8 states)
- 🔢 Any numeric counter (0-9999)

The firmware provides two **protocol patterns**:

1. **Binary State Display** (`LoRaWAN_OLED_SingleSpace.ino`) - Maps 1-byte payload to different display states
2. **Numeric Counter Display** (`LoRaWAN_OLED_ParkingZone.ino`) - Displays ASCII-encoded numbers with customizable labels

Both variants operate in **LoRaWAN Class C** mode (always listening for downlinks).

**Example Implementation:** The Smart Parking Platform uses these protocols for parking management, but this is just one possible application. The payload protocol itself is generic.

---

## FPort Configuration

**All communication uses FPort 1**:
- ✅ Downlinks: FPort 1
- ✅ Uplinks: FPort 1

---

## Downlink Format (Server → Device)

### Binary State Display

**Purpose**: Update display to show one of several predefined states
**FPort**: 1
**Payload Length**: 1 byte
**Format**: Single status code byte (interpreted by firmware)

**Protocol:** Send any byte value (0x00-0xFF), firmware maps to display text via switch/case.

**Example Implementation: Parking Space Monitoring**

| Hex Payload | Decimal | Display Text | Use Case |
|-------------|---------|--------------|----------|
| `01` | 1 | AVAILABLE | Space is free |
| `02` | 2 | OCCUPIED | Space is occupied |
| `03` | 3 | RESERVED | Space is reserved |
| Other | - | UNKNOWN | Invalid/error state |

**Example Implementation: Meeting Room Status**

| Hex Payload | Decimal | Display Text | Use Case |
|-------------|---------|--------------|----------|
| `01` | 1 | FREE | Room available |
| `02` | 2 | IN USE | Meeting in progress |
| `03` | 3 | BOOKED | Reserved for later |
| `04` | 4 | CLEANING | Maintenance |
| Other | - | OFFLINE | Error state |

**Customization:** Modify the `displayRoomStatus()` function in firmware to change text for each byte value. The LoRaWAN protocol remains unchanged.

**Examples**:

```bash
# ChirpStack downlink examples
FPort: 1, Payload: 01  # Display shows "AVAILABLE"
FPort: 1, Payload: 02  # Display shows "OCCUPIED"
FPort: 1, Payload: 03  # Display shows "RESERVED"
```

**Implementation**:
```cpp
// Firmware decoding (LoRaWAN_OLED_SingleSpace.ino)
switch(downlinkData[0]) {
  case 1: displayText = "AVAILABLE"; break;
  case 2: displayText = "OCCUPIED"; break;
  case 3: displayText = "RESERVED"; break;
  default: displayText = "UNKNOWN"; break;
}
```

**Display Persistence**: Status remains on screen until next downlink received.

---

### Numeric Counter Display

**Purpose**: Update numeric display (counts, levels, quantities)
**FPort**: 1
**Payload Length**: 1-4 bytes (variable, ASCII encoded)
**Format**: ASCII digit characters

**Protocol:** Send ASCII representation of number as hex bytes (e.g., "24" → `0x32 0x34`)

**Example Implementation: Parking Availability**

| Hex Payload | ASCII Interpretation | Display Output |
|-------------|---------------------|----------------|
| `35` | "5" | PL. LIBRES : 5 |
| `32 34` | "24" | PL. LIBRES : 24 |
| `31 35 36` | "156" | PL. LIBRES : 156 |
| `30` | "0" | PL. LIBRES : 0 |

**Example Implementation: Stock Level**

| Hex Payload | ASCII Interpretation | Display Output |
|-------------|---------------------|----------------|
| `31 30 30` | "100" | STOCK: 100 units |
| `35` | "5" | STOCK: 5 units |
| `30` | "0" | STOCK: 0 units |

**Example Implementation: Queue Length**

| Hex Payload | ASCII Interpretation | Display Output |
|-------------|---------------------|----------------|
| `37` | "7" | WAITING: 7 people |
| `32 33` | "23" | WAITING: 23 people |

**Customization:** Modify the `displayCarParkStatus()` function to change label text and formatting. The ASCII-to-number conversion is automatic.

**Encoding Rules**:
- Each byte represents one ASCII digit character
- No null terminator needed
- Leading zeros are displayed (e.g., `30 35` → "05")
- Maximum recommended: 4 digits (9999)

**Examples**:

```bash
# ChirpStack downlink examples
FPort: 1, Payload: 35          # Shows "PL. LIBRES : 5"
FPort: 1, Payload: 3234        # Shows "PL. LIBRES : 24"
FPort: 1, Payload: 313536      # Shows "PL. LIBRES : 156"
FPort: 1, Payload: 30          # Shows "PL. LIBRES : 0"
```

**Implementation**:
```cpp
// Firmware decoding (LoRaWAN_OLED_ParkingZone.ino)
String numberString = "";
for (size_t i = 0; i < downlinkSize; i++) {
  numberString += (char)downlinkData[i];  // Convert bytes to ASCII string
}
displayCarParkStatus(numberString);  // Display "PL. LIBRES : " + number
```

**Display Format**:
- Header: "PL. LIBRES :" (French: "Available Spaces")
- Number: Large 24-point font
- Persistence: Remains until next downlink

---

## Uplink Format (Device → Server)

### Keep-Alive Transmission

**Purpose**: Periodic heartbeat with device status
**FPort**: 1
**Frequency**: Every 60 minutes (configurable)
**Payload Length**: 3 bytes
**Format**: `[Counter_High, Counter_Low, Last_Status]`

| Byte Offset | Name | Type | Description |
|-------------|------|------|-------------|
| 0 | Counter High | uint8 | High byte of transmission counter |
| 1 | Counter Low | uint8 | Low byte of transmission counter |
| 2 | Last Status | uint8 | Last received downlink status byte |

**Counter Behavior**:
- Starts at 0 after device boot/join
- Increments by 1 after each uplink transmission
- 16-bit value: range 0-65535
- Rolls over to 0 after 65535

**Last Status Byte**:
- **Single Space Display**: Copy of last received status byte (0x01/0x02/0x03)
- **Zone Counter Display**: First ASCII character of last received number
- **Initial Value**: 0x00 if no downlink received yet

**Examples**:

```
# First uplink after 5 transmissions, last status was OCCUPIED (0x02)
Payload: 00 05 02
         └──┴── counter = 5
              └── last status = 0x02

# After 300 transmissions, last zone count was "24" (first char '2' = 0x32)
Payload: 01 2C 32
         └──┴── counter = 300 (0x012C)
              └── last status = 0x32 ('2')
```

**Implementation**:
```cpp
// Firmware encoding (both variants)
uint8_t payload[3];
payload[0] = highByte(counter);     // Counter high byte
payload[1] = lowByte(counter);      // Counter low byte
payload[2] = lastStatusByte;        // Last received status

node.sendReceive(payload, sizeof(payload), 1, false);  // FPort 1, unconfirmed
counter++;  // Increment for next uplink
```

---

### Special Case: Initial Activation Uplink

**Purpose**: Signal Class C activation after OTAA join
**FPort**: 1
**Timing**: Immediately after successful LoRaWAN join
**Payload**: `FF C3` (2 bytes)

| Byte Offset | Value | Meaning |
|-------------|-------|---------|
| 0 | 0xFF | Magic marker (activation signal) |
| 1 | 0xC3 | Class C identifier |

**Purpose**:
- Signals to network server that device is now in Class C mode
- Some network servers use this to enable immediate downlink queue processing
- Occurs once per join/rejoin sequence

**Example**:
```
# Immediately after OTAA JOIN_ACCEPT
Uplink on FPort 1: FF C3
```

**Note**: After this initial uplink, device switches to standard 3-byte keep-alive format.

---

## Integration with Smart Parking Platform

### Backend Handler Configuration

The Smart Parking Platform uses two device handlers for Heltec devices:

#### HeltecDisplayHandler (Single Space)

**Device Type Code**: `heltec_display`
**Category**: `status_light`
**Handler Class**: `HeltecDisplayHandler`

**Downlink Encoding**:
```python
# Python backend (src/device_handlers.py)
def encode_downlink(self, command: str, params: Dict[str, Any]) -> bytes:
    if command == "set_color":
        state = params.get("state", "FREE")
        # Maps: FREE→0x01, OCCUPIED→0x02, RESERVED→0x03
        color_hex = get_display_color(state)
        return bytes.fromhex(color_hex)
```

**Uplink Parsing**:
```python
# Basic telemetry extraction (3-byte format not parsed in detail)
def parse_uplink(self, data: Dict[str, Any]) -> SensorUplink:
    parsed = self.parse_chirpstack_uplink(data)
    return SensorUplink(
        device_eui=parsed["device_eui"],
        timestamp=parsed["timestamp"],
        rssi=parsed["rssi"],
        snr=parsed["snr"],
        raw_payload=parsed.get("payload")
    )
```

---

#### HeltecTextDisplayHandler (Zone Counter)

**Device Type Code**: `heltec_text_display`
**Category**: `zone_counter`
**Handler Class**: `HeltecTextDisplayHandler`

**Downlink Encoding**:
```python
# Python backend (src/device_handlers.py)
@staticmethod
def encode_count(count: int) -> bytes:
    """Convert integer to ASCII digit payload"""
    return str(int(count)).encode('ascii')
    # Examples:
    # 5 → b'5' → 0x35
    # 24 → b'24' → 0x32 0x34
    # 156 → b'156' → 0x31 0x35 0x36
```

**Uplink Parsing**:
```python
# Parses 3-byte keep-alive structure
def parse_uplink(self, data: Dict[str, Any]) -> SensorUplink:
    parsed = self.parse_chirpstack_uplink(data)

    try:
        payload_bytes = base64.b64decode(parsed.get("payload"))
        if len(payload_bytes) >= 3:
            counter = (payload_bytes[0] << 8) | payload_bytes[1]
            last_status = payload_bytes[2]
            logger.debug(
                f"Heltec text display uplink: counter={counter}, "
                f"last_status=0x{last_status:02X}"
            )
    except Exception as e:
        logger.warning(f"Failed to parse Heltec uplink: {e}")

    return SensorUplink(
        device_eui=parsed["device_eui"],
        timestamp=parsed["timestamp"],
        rssi=parsed["rssi"],
        snr=parsed["snr"],
        raw_payload=parsed.get("payload")
    )
```

---

## Payload Size Limits

### LoRaWAN Constraints

| Spreading Factor | Max Payload | Recommended |
|------------------|-------------|-------------|
| SF7 | 230 bytes | Use SF7 for best performance |
| SF8 | 230 bytes | - |
| SF9 | 123 bytes | - |
| SF10 | 59 bytes | - |
| SF11 | 59 bytes | Avoid for frequent updates |
| SF12 | 59 bytes | Avoid for frequent updates |

**EU868 Region**: Maximum application payload ~230 bytes on SF7

### Practical Limits

**Single Space Display**:
- Fixed: 1 byte
- Well within all SF limits ✅

**Zone Counter Display**:
- Recommended: 1-4 bytes (1-4 digit numbers)
- Maximum tested: 4 bytes ("9999")
- Display buffer: 16 characters (OLED screen constraint)

**Uplink**:
- Fixed: 3 bytes (standard keep-alive)
- Fixed: 2 bytes (initial activation)
- Well within all SF limits ✅

---

## Error Handling

### Invalid Downlink Payloads

**Single Space Display**:
```cpp
// Unknown status codes default to "UNKNOWN"
if (downlinkData[0] < 1 || downlinkData[0] > 3) {
  displayText = "UNKNOWN";
}
```

**Zone Counter Display**:
```cpp
// Non-ASCII or empty payloads
if (downlinkSize == 0) {
  // Display shows previous value (no update)
}
// Non-numeric ASCII characters will be displayed as-is
// Example: "AB" → "PL. LIBRES : AB" (not ideal but won't crash)
```

**Best Practice**: Server-side validation recommended to ensure:
- Single space: Only send 0x01, 0x02, 0x03
- Zone counter: Only send ASCII digits (0x30-0x39)

---

### Network Issues

**Class C Downlink Reception**:
- Device listens continuously (RX2 window always open)
- No need to wait for uplink RX windows
- Typical latency: < 5 seconds from server to device

**Downlink Queue Full**:
- ChirpStack queues downlinks (typically 10 max)
- Oldest downlinks may be dropped if queue overflows
- Monitor ChirpStack logs for queue warnings

**Device Not Reachable**:
- Check device is powered and joined
- Verify gateway connectivity
- Check RSSI/SNR values in ChirpStack
- Consider SF/power adjustments if signal weak

---

## Testing Procedures

### 1. Verify Downlink Reception

**Single Space Test**:
```bash
# Send via ChirpStack UI or API
curl -X POST "http://chirpstack-host:8080/api/devices/{devEUI}/queue" \
  -H "Grpc-Metadata-Authorization: Bearer $API_KEY" \
  -d '{
    "queueItem": {
      "devEui": "70B3D57ED0067001",
      "fPort": 1,
      "data": "AQ=="  # Base64 for 0x01 (AVAILABLE)
    }
  }'

# Expected: Display updates to "AVAILABLE" within 5 seconds
```

**Zone Counter Test**:
```bash
# Send count of 24
curl -X POST "http://chirpstack-host:8080/api/devices/{devEUI}/queue" \
  -H "Grpc-Metadata-Authorization: Bearer $API_KEY" \
  -d '{
    "queueItem": {
      "devEui": "70B3D57ED0067002",
      "fPort": 1,
      "data": "MjQ="  # Base64 for 0x3234 ("24")
    }
  }'

# Expected: Display shows "PL. LIBRES : 24" within 5 seconds
```

### 2. Monitor Uplinks

**Expected Behavior**:
- Uplink every 60 minutes (default)
- 3-byte payload with incrementing counter
- Last status byte reflects last downlink

**ChirpStack Verification**:
1. Navigate to Device → LoRaWAN Frames
2. Filter: Uplink frames, FPort 1
3. Check payload: Should be 3 bytes
4. Decode manually: `[counter_high, counter_low, last_status]`

**Example Log**:
```
Uplink #1:  00 00 00  (counter=0, no status yet)
Downlink:   02        (set to OCCUPIED)
Uplink #2:  00 01 02  (counter=1, last status=0x02)
Uplink #3:  00 02 02  (counter=2, last status=0x02)
```

### 3. Test Edge Cases

**Maximum Counter Value**:
```
# After ~65,535 uplinks (45 days at 60-min interval)
Payload: FF FF 02  (counter rolls over to 0x0000 on next uplink)
Next:    00 00 02
```

**Large Zone Numbers**:
```bash
# Test 4-digit number
FPort: 1, Payload: 39393939  # "9999"
# Verify display doesn't truncate
```

**Rapid Downlink Updates**:
```bash
# Send 5 downlinks in quick succession
# Verify: Device processes all (Class C = always listening)
# Verify: Display shows final state
```

---

## Troubleshooting

### Display Not Updating

**Symptoms**: Downlink sent but display unchanged

**Checklist**:
1. ✅ Verify FPort is 1 (not 2, 10, etc.)
2. ✅ Check payload format matches variant:
   - Single space: 1 byte (0x01/0x02/0x03)
   - Zone counter: ASCII digits only
3. ✅ Confirm device is joined (check ChirpStack device status)
4. ✅ Verify gateway connectivity (last seen < 5 minutes ago)
5. ✅ Check downlink was transmitted (ChirpStack LoRaWAN frames tab)

**Common Mistakes**:
- ❌ Using FPort 2 (device only listens on FPort 1)
- ❌ Sending Base64 string instead of hex (API encoding issue)
- ❌ Zone counter: Sending decimal instead of ASCII (24 as 0x18 vs 0x3234)

### Uplinks Not Received

**Symptoms**: No uplinks in ChirpStack

**Checklist**:
1. ✅ Device powered on and serial shows "Transmission success"
2. ✅ Device joined successfully (not stuck in join loop)
3. ✅ Gateway in range (check gateway last seen)
4. ✅ Uplink interval not too short (minimum 60 seconds recommended)
5. ✅ Check duty cycle limits (EU868: 1% per sub-band)

**Debug via Serial**:
```
# Connect USB serial at 115200 baud
# Expected output every 60 minutes:
"Transmitting uplink..."
"Transmission done"
"Waiting for possible downlink..."
```

### Counter Not Incrementing

**Symptoms**: Uplink counter stuck at same value

**Possible Causes**:
- Device rebooting between uplinks (power issue)
- Counter variable overflow (unlikely, 16-bit range)
- Firmware modification error

**Verification**:
```cpp
// Check firmware has this line after transmission:
counter++;  // Should be present in loop()
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.1 | 2025-12-09 | Updated to reflect firmware commit 95daeb6 (OLED display fix for ParkingZone variant) |
| 1.0 | 2025-01-06 | Initial specification document |

### Firmware Changes

**Commit 95daeb6** (2025-12-09): Fixed OLED display initialization in ParkingZone code
- **Issue**: Display not turning on when powered via USB
- **Fix**: Corrected SDA/SCL pin order in SSD1306Wire constructor
- **Impact**: Hardware initialization only - **no payload protocol changes**
- **Affected**: ParkingZone variant (`LoRaWAN_OLED_ParkingZone.ino`) only
- **Recommendation**: Use this commit or later for all new deployments

---

## References

- [Firmware Repository](https://github.com/cpaumelle/class-c-heltec-lora-v3)
- [LoRaWAN Specification v1.0.3](https://lora-alliance.org/resource_hub/lorawan-specification-v1-0-3/)
- [RadioLib Documentation](https://jgromes.github.io/RadioLib/)
- [ChirpStack Documentation](https://www.chirpstack.io/docs/)
- [Smart Parking Platform: Display Device Onboarding](../DISPLAY_DEVICE_ONBOARDING.md)

---

**Questions or Issues?**
Open an issue in the [firmware repository](https://github.com/cpaumelle/class-c-heltec-lora-v3/issues) or contact the Smart Parking Platform team.
