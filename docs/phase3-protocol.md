# Phase 3: GMC-300 Device Protocol

This document describes the serial protocol used to communicate with the GMC-300 Geiger counter. It covers serial link settings, command set, response parsing, protocol flow, function roles, and error handling. The protocol was reverse-engineered from GMC300.exe.

**Official protocol reference:** [GQ-RFC1201 – GQ Geiger Counter Communication Protocol](https://www.gqelectronicsllc.com/download/GQ-RFC1201.txt) (GQ Electronics LLC). Defines command format (`<` … `>>`), serial settings (57600 8N1 for GMC-300 V3.xx and earlier; 115200 for GMC-300 Plus V4.xx+), GETVER (14 bytes), GETCPM (2 bytes, MSB first), HEARTBEAT0/HEARTBEAT1, and other commands.

---

## 1. Scope

- **Device:** GMC-300 (Geiger counter)
- **Interface:** Serial (COM port)
- **Contents:** Serial link parameters, command strings and responses, parsing rules, order of use, function reference, and error handling.

---

## 2. Serial Link

| Setting | Value |
|--------|--------|
| **Port** | `COM%d` (e.g. COM1, COM2) — port number from configuration |
| **Baud rate** | 57600 |
| **Data format** | 8 data bits, no parity, 1 stop bit (8N1) |
| **Read timeout** | Constant: 1000 ms; Multiplier: 100 ms per byte |
| **Write timeout** | Constant: 1000 ms; Multiplier: 100 ms per byte |

Effective behavior: open the configured COM port at **57600 8N1**, with 1 second base timeout plus 100 ms per byte for both read and write. Default baud 57600; can be overridden by `gmc.xml` `<baud>` (see phase2-config and boinc-xml-reference).

---

## 3. Command Set

Commands are sent as literal ASCII strings (no extra terminator). The number of bytes given by the string length is written to the port.

| Command | Request string | Request length | Response | Response size/format |
|---------|----------------|-----------------|----------|----------------------|
| **GETCPM** | `<GETCPM>>` | 8 bytes | CPM value | 2 bytes, big-endian (first byte MSB, second byte LSB per GQ-RFC1201) |
| **GETVER** | `<GETVER>>` | 9 bytes | Version string | 14 bytes, ASCII |
| **HEARTBEAT0** | `<HEARTBEAT0>>` | 12 bytes | — | No response read (not used in init; see §5) |

**Request → response:**

- **GETCPM:** Send `<GETCPM>>` → wait → read 2 bytes (CPM: first byte MSB, second byte LSB; big-endian).
- **GETVER:** Send `<GETVER>>` → wait → read **14 bytes** (version string; validate contains `"GMC"` so other device versions e.g. GMC-320 are accepted). Some devices may send 2 leading non-ASCII bytes; the host can discard them and keep reading until 14 bytes of version data are received (see `recovered/com_port.cpp`).
- **HEARTBEAT0:** Send `<HEARTBEAT0>>` → no response read. Not used in the current init sequence.

---

## 4. Response Parsing

### 4.1 CPM (GETCPM)

- **Response:** 2 bytes; **first byte MSB, second byte LSB** (big-endian, per GQ-RFC1201). Implemented in `recovered/detector.cpp` as `buf[0]*256 + buf[1]`.
- **Formula:**  
  **CPM = byte0 × 256 + byte1**  
  (16-bit unsigned integer; byte0 = MSB, byte1 = LSB).
- Example: bytes `0x00 0x1C` → CPM = 0×256 + 28 = **28** (per GQ-RFC1201).

### 4.2 Version string (GETVER)

- **Response:** 14 bytes, ASCII version string (e.g. `GMC-300Re 4.2`). The host reads exactly 14 bytes after sending GETVER.
- **Validation:** The 14-byte string must contain the substring **`"GMC"`** so that multiple device versions (e.g. GMC-300, GMC-320) are accepted. Initialization continues only when this check passes.

### 4.3 HEARTBEAT0

- No response is read after sending `<HEARTBEAT0>>`. Not used in the current init flow (see §5).

---

## 5. Order of Use (Protocol Flow)

1. **Open port** — Open the configured COM port (e.g. `COM1`) with the serial link settings above.
2. **Initialize after open** — Run initialization once after the port is open:
   - Send **GETVER** → read **14 bytes** (version string; discard up to 2 leading non-ASCII bytes if present, then read until 14 bytes) → verify response contains `"GMC"`.
   - **HEARTBEAT0 is not sent** in the current implementation (see `recovered/com_port.cpp`: `init_com_after_open`).
3. **Main loop** — Repeatedly:
   - Send **GETCPM** → read 2 bytes → compute CPM = byte0×256 + byte1 (big-endian).

So: **open port → init (GETVER only; read 14 bytes; validate "GMC") → main loop (GETCPM repeatedly).**

---

## 6. Functions Reference

| Function | Role in protocol |
|----------|-------------------|
| **open_com_port** | Opens COM port (`COM%d` from config), applies 57600 8N1 and read/write timeouts (1 s constant, 100 ms/byte). |
| **init_com_after_open** | Sends GETVER, reads 14 bytes (discarding leading non-version bytes if needed), checks for `"GMC"`. Does not send HEARTBEAT0. See `recovered/com_port.cpp`. |
| **read_detector_sample** | Sends GETCPM, reads 2 bytes, computes CPM = byte0×256 + byte1 (big-endian; see `recovered/detector.cpp`). |

---
