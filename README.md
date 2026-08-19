# SNMP_Agent
### (Previously Arduino_SNMP)

SNMP Agent built with Arduino

This is a fully-compliant SNMPv2c Agent built for Arduino's, but will work on any OS, providing API code is written for packet serialization (See tests/mock.cpp for an example)

## Current Version: 3.1.5

## Features
* Full SNMPv2c Data Type support:
  * INTEGER `int`
  * STRING  `char[]` / `const char*` (C-style strings, no `std::string` or Arduino `String`)
  * NULLTYPE
  * OIDTYPE `const char*` (dotted-decimal, e.g. ".1.3.6.1.4.1.5.0")
* Complex data type support:
  * NETWORK ADDRESS
  * COUNTER32 `uint32_t`
  * GAUGE32 `uint32_t`
  * TIMESTAMP `uint32_t`
  * OPAQUE `uint8_t*`
  * CONTER64 `uint64_t`
* SNMP PDU Support
  * GetRequest
  * GetNextRequest
  * GetResponse (For SNMPv2c INFORM Responses only for now)
  * SetRequest
  * SNMPv2 Trap
  * GetBulkRequest
  * InformRequest
  * SNMPv2 Trap

It was designed and tested around an ESP32, but will work with any Arduino-based devied that has a UDP object available. Optimized for ESP-01 (ESP8266) and other memory-constrained embedded targets.

The example goes into detail around how to use, or look at `src/SNMP_Agent.h` for the API.

If you're coming from v1, most, but not all APIs are drop-in replaceable.
Some of the API's, especially around strings have changed. Look in `SNMP_Agent.h` for details.

If you're upgrading from any prior release (including the original Arduino_SNMP `v2.1.x`, or this fork's `v2.2.0` / `v3.0.0` / `v3.1.0` / `v3.1.1` / `v3.1.2` / `v3.1.3` / `v3.1.4`), read **"What's New in v3.1.5"** immediately below. v3.1.5 is a single cumulative release that folds: (1) the v2.2.0 C-style string-model refactor, (2) the v3.0.0 critical BER TLV bug fixes (PR #60), (3) the 4-phase zero-heap deterministic-memory refactor, (4) v3.1.1 sketch-overridable tuning + SNMP_Sensor bug fixes, (5) v3.1.2 snmpTrapOID.0 RFC-3416 fix, (6) v3.1.3 ESP8266 auto-tune profile, (7) v3.1.4 startup-heap ASNPool + narrowed SortableOID + universal OCTET=256 defaults, and (8) v3.1.5 arduino-lint LD003 extras/demos compliance.

---

## What's New in v3.1.5 (Cumulative: ALL changes since v2.1.0)

v3.1.5 is a single release combining every earlier in-tree milestone category plus the latest patch-level housekeeping fixes. If you are upgrading from the historical `Arduino_SNMP` v2.1.x (or from any prior v2.2.0 / v3.0.0 / v3.1.x fork release) you are getting everything at once in this tag. Eight broad buckets of change rolled into v3.1.5:

1. **v2.2.0 (embedded string model):** all `std::string` replaced with fixed C-style `char[]` / `const char*`. No heap fragmentation from string reallocs; 3–8 KB Flash saved on ESP8266.
2. **v3.0.0 (BER TLV hardening, upstream PR #60):** 3 critical real on-the-wire BER bugs fixed (long-form header off-by-one, `length == 256` silently encoded as 0 which broke `snmpbulkwalk`, and a double-store undefined-behavior sign-extend). 101/101 tests green.
3. **Zero-Heap Deterministic-Memory Refactor (4-phase):** hot-path packet processing (`agent.loop()`, GET/GETNEXT/GETBULK/SET decode + build, TRAP/INFORM send) now performs **zero** `malloc`/`new`/`calloc`/`realloc`. All ASN.1 BER objects come from a compile-time-sized global placement pool; all VarBind/PDU/agent callback lists use fixed C-arrays with explicit `constexpr` capacity caps. No mid-packet heap-fragmentation panics after 30+ days of polling.
4. **v3.1.1 (sketch-overridable tuning + SNMP_Sensor bug fixes):** every size/pool/buffer constant wrapped with `#ifndef … #endif` so sketch-side defines or build-flags win, no patching library sources required; SNMP_Sensor `char*` → `const char*` OID const-correctness; critical SNMP_Sensor `addReadWriteStringHandler(&sysContact, 25, true)` hardcoded 25-byte SET cap fixed → `sizeof(sysContactValue)`.
5. **v3.1.2 (RFC-3416 snmpTrapOID.0 fix, issue #64):** SNMPv2 Trap/Inform VarBind #2 name `sysObjectID.0` → correct `snmpTrapOID.0` (`.1.3.6.1.6.3.1.1.4.1.0`) per RFC 3416 §3.1, so net-snmp `snmptrapd` can look up the NOTIFICATION-TYPE.
6. **v3.1.3 (ESP8266 auto-tune profile):** on ESP8266, unless `SNMP_SKIP_ESP8266_AUTOTUNE=1` is set, automatically shrinks all pool/buffer constants (ASNPool 64→24, callbacks 64→24, VarBinds 16→6, OCTET max 500→256, packet 1400→1024, slot size 768→640, etc.) saving ~28 KB BSS vs v3.1.2 so WiFi + LittleFS + ArduinoJson + SNMPAgent fit on the 80 KB-DRAM D1 mini / ESP-01.
7. **v3.1.4 (startup-heap ASNPool + narrowed SortableOID + universal OCTET 256):** the single biggest static BSS sink — `ASNPool slots[N]` — moves out of `.bss` into a **one-shot startup-time `new Slot[N]()` allocation** done exactly once on the first `asn_new<T>()` call; never deallocated, never reallocated, never grows, count fixed at compile-time (opt-out back to static with `SNMP_POOLS_IN_BSS 1`). Effect: ESP8266 tiny gains a further ~15.5 KB BSS, ESP32 gains ~24.8 KB BSS free, and the ASNPool no longer occupies linker-reported global/static RAM on any target. Additionally: `SortableOIDType::sortingMap` narrows `unsigned long[32]` → `uint32_t[32]` (SMIv2 sub-IDs fit in 32 bits; saves 128 B/instantiation on 64-bit hosts + width matches encoder math); `OCTET_TYPE_MAX_LENGTH` universal default 500 → 256, still sketch-overridable.
8. **v3.1.5 (arduino-lint compliance, patch-level housekeeping):**
   (a) Rule LD003 fix: `demos/` folder contained 3 `.ino` files → moved whole tree to `extras/demos/` (Arduino library spec allows sketches only under `examples/` or `extras/`). 3 `.ino` files are pure 100% renames; `platformio.ini` `lib_extra_dirs` updated `../../..` → `../../../..` (+1 nesting level). Fixes the `arduino/arduino-lint-action@v1.0.0` CI failure: `ERROR: Sketch(es) found outside examples and extras folders`.
   (b) README absolute-path sanitization sweep: maintainer-local deep-home-folder `file:///...` references across README files stripped to GitHub-native repo-root-relative links. No code or API change; 100% wire and consumer compatible on top of v3.1.4.

### User-visible API changes since v2.1.0 (only 2, both from v2.2.0)
Everything else — `addXxxHandler` / `sortHandlers` / `sendTrapTo` / `setUDP` / `begin` / `stop` / `loop` — is **100% source + wire compatible** back to v2.2.0.

| Symbol | Change since v2.1.0 |
|---|---|
| `GETSTRING_FUNC` typedef | `const std::string (*)()` → **`const char* (*)()`** |
| `OIDType::string()` return type | `const std::string&` → **`const char*`** (zero-copy, returns a `const char*` into a fixed backing buffer) |

### String model change (v2.2.0 → present) — copy-paste migration

**Static string handlers** (no `std::string` anymore; use a static `char[]` literal):
```cpp
// OLD, v2.1:
std::string sysDescr = "ESP32 SNMP Agent";
snmp.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.1.1.0", sysDescr);

// NEW, v3.1:
char sysDescr[] = "ESP32 SNMP Agent";     // or const char* PROGMEM literal
snmp.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.1.1.0", sysDescr);
```

**Read-write string buffers** (static storage, no `malloc`):
```cpp
char _sysContactBuf[255];
char* sysContact = _sysContactBuf;
snprintf(sysContact, sizeof(_sysContactBuf), "admin@example.com");
snmp.addReadWriteStringHandler(".1.3.6.1.2.1.1.4.0", &sysContact, sizeof(_sysContactBuf), true);
```

**Dynamic-string callbacks** (`GETSTRING_FUNC` now returns `const char*`):
```cpp
const char* getFirmwareVersion(void) { return LIBRARY_VERSION; }  // "3.1.5" from defs.h
snmp.addReadOnlyStringHandler(".1.3.6.1.4.1.99.0", getFirmwareVersion);
```

### Critical BER TLV bug fixes (v3.0.0 → present)
These are real on-the-wire failures. Upgrade if you use `snmpbulkwalk`, responses ≥ 128 bytes, or SNMP Set with 3-byte signed integer payloads. All five are integrated from upstream PR #60 plus defensive boundary hardening grown out of the audit.

1. **Hardcoded `_length + 2` return bug (OIDType / Counter64 / ComplexType / BER_CONTAINER fromBuffer).** BER length fields ≥ 128 bytes use long-form headers (3+ bytes instead of 2). Before: returned a hardcoded `+ 2` regardless, walked off-structure. After: returns the actual TLV header bytes consumed.
2. **`length == 256` encoded as 0 (catastrophic).** `encode_ber_length_integer` used `if(integer > 256)` (off-by-one). Exactly 256-byte response PDUs serialized as `0x81 0x00` (= length 0 per ASN.1 BER), which net-snmp/pysnmp silently dropped. Fixed in both `encode_ber_length_integer` and its paired byte-counter `encode_ber_length_integer_count`.
3. **Undefined behavior `tempVal = tempVal |= 0xFF000000`** in IntegerType 3-byte signed decode (double-store, `-Wsequence-point` error). Reduced to `tempVal |= 0xFF000000;`.
4. **Stack buffer overread in test harness** `memcpy(&buffer[i], &randomLong, 10)` → `memcpy(…, sizeof(randomLong))` (2–6 bytes past stack end on 64-bit hosts).
5. **Defensive overflow pre-checks** added at BER_CONTAINER / OIDType / Counter64 / ComplexType `fromBuffer` entry. `ComplexType::fromBuffer` child-walk replaced buggy dual-condition loop with a descending `remaining` counter.

### Deterministic zero-heap (4-phase) — what this means for your firmware
Before the zero-heap refactor, a single decoded SNMP PDU did ≥24 `new`/`delete` pairs (one per ASN.1 field, two per shared_ptr refcount block). Under sustained 1 Hz polling this fragmented the ESP-01 heap so badly that after ≈30 days, the next incoming 512-byte UDP packet could not be allocated contiguously → **panic reboot**.

v3.1.0 replaces every hot-path allocation, and v3.1.4 extends the model with optional startup-heap pool allocation, using one of two storage strategies:
- **Compile-time-sized global placement pool** for all BER_CONTAINER subclass objects (`IntegerType`, `OctetType`, `OIDType`, `ComplexType`, …). Generic default: `SNMP_POOL_ASN_OBJECTS = 32` slots × `SNMP_POOL_SLOT_SIZE = 768 B` = ~24,576 B; ESP8266 tiny auto-profile drops that to 24 × 640 B = 15,360 B. v3.1.4 allocates the slot storage once at startup via `new Slot[N]()` (opt-out `SNMP_POOLS_IN_BSS 1` returns the old static `.bss` layout). If all N slots are ever simultaneously occupied (pathological trap storm), the code gracefully falls back to a regular `::new T` — defensive, never triggers in steady state (decode tree + serialise + free all return to pool before the next packet). `ASAN` is clean on both paths.
- **Fixed C-arrays with explicit count member** for every library list/queue: VarBinds per packet, OID handlers per agent, UDPs per agent, concurrent SNMPAgent instances, INFORM retry queue, callback list per SNMPTrap object, child-values inside ComplexType. Every such buffer has a compile-time `constexpr` maximum. Overflow returns a well-defined error code — no OOM panic.

#### 14 Compile-Time Sizing Constants (tune before `#include <SNMP_Agent.h>`)
Declared in [defs.h](src/include/defs.h):

| Constant                     | Default (non-ESP8266) | `_SNMP_ESP8266_TINY` auto (ESP8266, on by default) | Purpose |
|------------------------------|------------------------|------------------------------------------------------|---------|
| `MAX_SNMP_PACKET_LENGTH`     | 1400                   | 1024                                                 | Incoming/outgoing UDP packet scratch buffer |
| `OCTET_TYPE_MAX_LENGTH`      | 256                    | 256                                                  | OctetType / OpaqueType internal fixed buffer |
| `SNMP_MAX_COMMUNITY_LEN`     | 64                     | 64                                                   | Community-string buffer |
| `SNMP_MAX_OID_STR_LEN`       | 256                    | 192                                                  | Dotted-decimal OID storage (e.g. "1.3.6.1.4.1.…") |
| `SNMP_MAX_STRING_LEN`        | = `OCTET_TYPE_MAX_LENGTH` | same                                             | String column SET upper bound (alias) |
| `SNMP_MAX_OID_SUBIDENTIFIERS`| 32                     | 32                                                   | BER-encoded OID sub-ID count (realistic max ~17) |
| `SNMP_MAX_COMPLEX_CHILDREN`  | 16                     | 8                                                    | Children per decoded PDU/VarBind-list ComplexType |
| `SNMP_MAX_VARBINDS`          | 16                     | 6                                                    | VarBinds per request/response (snmpbulkwalk default = 10) |
| `SNMP_MAX_CALLBACKS_PER_AGENT` | 64                   | 24                                                   | Registered OID handlers per SNMPAgent instance |
| `SNMP_MAX_AGENTS`            | 2                      | 2                                                    | Concurrent SNMPAgent instances (usually just 1) |
| `SNMP_MAX_UDP_PER_AGENT`     | 2                      | 2                                                    | UDP transport interfaces per agent (WiFi + ETH fallback) |
| `SNMP_MAX_TRAPS_INFLIGHT`    | 8                      | 4                                                    | INFORM retry queue + pending trap depth |
| `SNMP_MAX_CALLBACKS_PER_TRAP`| 16                     | 8                                                    | OID pointers embedded in a single SNMPTrap object |
| `SNMP_POOL_ASN_OBJECTS`      | 32                     | 24                                                   | ASNPool slots — global BER_CONTAINER placement pool (decode + build + trap + clone) |
| `SNMP_POOL_VARBIND_OBJECTS`  | 12                     | 8                                                    | VarBind placement pool (packet build/trap path) |
| `SNMP_POOL_SLOT_SIZE`        | 768                    | 640                                                  | Byte payload size per ASNPool slot (must fit `SortableOIDType` largest subclass) |

**Two opt-out `#define` switches** (place BEFORE `#include <SNMP_Agent.h>`):
- `#define SNMP_SKIP_ESP8266_AUTOTUNE 1` — disable the ESP8266 `_SNMP_ESP8266_TINY` shrink profile on ESP8266, use generic defaults above.
- `#define SNMP_POOLS_IN_BSS 1` — force the ASNPool back into static `.bss` arrays (v3.1.3 behavior). Without it (default, v3.1.4+ → still current in v3.1.5), ASNPool storage is allocated once at startup via `new Slot[N]()` so it does not count against linker-reported globals — ~20–50 KB more headroom for your code.

#### ESP-01 1 MB / 80 KB-DRAM tuning (headroom: ~50 KB globals FREE / 80 KB)
Since v3.1.4 (still current in v3.1.5), on ESP8266 the `_SNMP_ESP8266_TINY` profile is **automatic** (no user defines needed) and ASNPool storage lives by default in the startup heap, not BSS. So most small sketches compile + link at **38–41% globals**, well under the 80 KB limit. Only re-tune if you need to serve >24 OIDs, >6 VarBinds/bulkwalk, or >4 in-flight traps.

**Important ordering: put overrides BEFORE `#include <SNMP_Agent.h>` in your sketch.ino.**

```c
/* Optional: increase capacity on D1 mini / bigger ESP8266 modules. */
#define SNMP_MAX_CALLBACKS_PER_AGENT   64
#define SNMP_POOL_ASN_OBJECTS          32
#define SNMP_MAX_VARBINDS               16
#define SNMP_MAX_COMPLEX_CHILDREN       16
#include <SNMP_Agent.h>
```
→ Saves ≈ `(default v3.1.2 pool of 64 × 768 B) − (tiny 24 × 640 B)` = **~33,792 B BSS + pool-on-heap = an additional ~15,360 B / slot** versus v3.1.2 on ESP8266, measured in real builds.

### Release verification matrix (all green)

| Check | Result |
|---|---|
| Host catch2 (native clang 14 / g++) | ✅ 101/101 assertions in 10 test cases |
| AddressSanitizer (memory + leak) | ✅ 0 errors / 0 leaks (ASNPool path + heap-fallback both clean) |
| Arduino-CLI `esp8266:esp8266:d1_mini` + `examples/ESP32_SNMP` | ✅ Flash 261,340 B (24%), **globals 30,668 / 80,192 B (38%)** — 49,524 B FREE |
| Arduino-CLI `esp8266:esp8266:d1_mini` + `examples/SNMP_Sensor`  | ✅ Flash 296,392 B (28%), **globals 33,128 / 80,192 B (41%)** — 47,064 B FREE |
| Arduino-CLI `esp32:esp32:esp32` + `examples/ESP32_SNMP`        | ✅ Flash 918,531 B (70%), **globals 49,856 / 327,680 B (15%)** |
| Arduino-CLI `esp32:esp32:esp32` + `examples/SNMP_Sensor`       | ✅ Flash 964,843 B (73%), **globals 50,984 / 327,680 B (15%)** |
| Build flags across all 4 targets | ✅ `-Wall -Wextra -Werror` clean |
| `src/` standard-container header audit | ✅ 0 `<vector>` / 0 `<deque>` / 0 `<list>` / 0 `<functional>`. Only 2 `<memory>` retained purely for backwards-compat public shared_ptr ctor signatures (OIDType::cloneOID + legacy VarBind ctors). |

### Net footprint vs v3.1.2 pre-optimization
Deterministic zero-hot-path heap unchanged (same deterministic pool sizing). Global BSS shrinks **per build**:
- ESP8266 d1_mini + `ESP32_SNMP.ino`: from **101% overflow (linker OOM)** (v3.1.2) → **30,668 B / 80,192 B (38%)** = **−~49 KB globals**.
- ESP32 DevKit + `ESP32_SNMP.ino`: from 74,680 B / 327,680 B (22%) (v3.1.3) → **49,856 B / 327,680 B (15%)** = **−24,824 B globals**, almost exactly the ASNPool slots[32] moved out of .bss into the startup heap.
- Flash: ≈ ±0.1% vs v3.1.3 (essentially unchanged; the startup `new Slot[]` code path is a handful of instructions).

---

## 2.2.0 Embedded Optimization: C-Style Strings Only

As of v2.2.0, the entire library uses **fixed-size C-style strings** (`char[]` + `const char*` + explicit length fields) exclusively. The C++ `std::string` type and Arduino `String` class have been completely removed from all library code, examples, and callback APIs.

### Why this change
* **Zero heap fragmentation** — no dynamic `malloc`/`new` for string storage
* **Smaller binary** — eliminates `<string>` template instantiation bloat (~3–8 KB Flash on ESP8266)
* **Deterministic memory** — all buffers are compile-time sized, no surprise OOM at runtime
* **Faster** — no SBO/COW indirection; fixed `memcpy`/`strcmp` paths that the compiler can heavily optimize

### Buffer Sizing
Fixed maximum sizes are declared in [defs.h](src/include/defs.h):
| Constant                    | Default | ESP8266 tiny auto | Purpose                          |
|-----------------------------|---------|-------------------|----------------------------------|
| `SNMP_MAX_COMMUNITY_LEN`    | 64      | 64                | Community string (RO/RW)         |
| `SNMP_MAX_OID_STR_LEN`      | 256     | 192               | OID dotted-decimal representation|
| `OCTET_TYPE_MAX_LENGTH` / `SNMP_MAX_STRING_LEN` | 256 | 256 | OctetString (OID value payload) |

These are sensible defaults; tune them from your sketch with `#define` BEFORE `#include <SNMP_Agent.h>` if you need a smaller RAM footprint on the ESP-01.

---

It you need a STRING OID that can be written to/updated, be very sure that you need to update it, because you will be dealing with raw pointers into fixed-size buffers. Always pass the true buffer size as the `maxLength` parameter of `addReadWriteStringHandler` to prevent overflow. It's safer to use `addReadOnlyStaticStringHandler()` whenever possible.

This library does **not** support the Arduino `String` class and does **not** use the C++ `std::string` class — all string handling uses C `<cstring>` primitives (`strncpy`, `memcpy`, `strcmp`, `strlen`, `strchr`, `strtol`) with explicit bounds checking.

## Getting Started

To setup a simple SNMP Agent, include the required libraries and declare an instance of the SNMPAgent class;

```
#include <SNMP_Agent.h>

/* Can declare read-write, or both read-only and read-write community strings */
SNMPAgent snmp("public", "private");
```

Depending on what arduino you are using, you will have to setup the wifi/internet conection for the device.
For ESP32, you can use `WiFi.begin()`, you will then need to supply a `UDP` object to the snmp library.

```
#include <WiFi.h>
#include <WiFiUdp.h>
WiFiUDP udp;

... later in setup()

WiFi.begin(ssid, password);

// Give snmp a pointer to the UDP object
snmp.setUDP(&udp);

// Add OID Handlers (see below)
...

snmp.begin();

... later in loop()
snmp.loop();
```

### Setting OID callbacks

If you want the Arduino to response to an SNMP server at some specified OIDs, you need to implement a `ValueCallback` for each OID, attached to a variable to respond with.
Whenever an OID is requested by sn SNMP manager, the ValueCallback for that OID is found, and the latest value of that variable is used to respond.

For example, to respones to the OID ".1.3.6.1.4.1.5.0" with the number: 5.
```
int testNumber = 5;
snmp.addIntegerHandler(".1.3.6.1.4.1.5.0", &testNumber);
```

Yopu can enable SNMPSet requests, by setting `isSettable = true` as a parameter when adding the handler, for example:
```
int settableNumber = 0;
snmp.addIntegerHandler(".1.3.6.1.4.1.5.1", &settableNumber, true);
// snmpset -v 2c -c private <IP> 1.3.6.1.4.1.5.1 i 24
```

You can store the return value of the handler calls in a variable `ValueCallback*`, and use them later for things like SNMP Traps, or for removing the handler later.

Be sure to call `snmp.sortHandlers()` after adding any OID handlers, to ensure functions like SNMP Walk work correctly.


The full list of ValueCallback handlers you can specify can be found in `SNMP_Agent.h`

### SNMP Traps

You can send SNMP v1 traps, as well as SNMPv2 Trap and INFORMS with this library.

Therre are a few requirements in setting up a trap in order to comply with the SNMP RFC.

```
// Setup a trap object for later use, specify the SNMP version to use 
// SNMP_VERSION_1 or SNMP_VERSION_2C

SNMPTrap* testTrap = new SNMPTrap("public", SNMP_VERSION_2C);

// SNMP Traps MUST send a timestamp value when sent. This timestamp doesn't have to be valid, but we have to create one anyway. The timestamp value is stored as "tens of milliseconds"
TimestampCallback* timestampCallback;
int tensOfMillisCounter = 0;
```
In `setup()`:
```
// The SNMP Trap spec requires an uptime counter to be sent along with the trap.
timestampCallback = (TimestampCallback*)snmp.addTimestampHandler(".1.3.6.1.2.1.1.3.0", &tensOfMillisCounter);

// Set UDP Object for trap to be sent on
testTrap->setUDP(&udp);

// OID of the trap (C-style string)
testTrap->setTrapOID(new OIDType(".1.3.6.1.2.1.33.2"));

// Specific Number of the trap
testTrap->setSpecificTrap(1);

// Set the uptime counter to use in the trap (required)
testTrap->setUptimeCallback(timestampCallback);

// Set some previously set OID Callbacks to send these values with the trap (optional)
testTrap->addOIDPointer(previouslySetValueCallback);

// Set our Source IP so the receiver knows where this is coming from
testTrap->setIP(WiFi.localIP());

// Set INFORM to be true or false (only works for SNMPV2 traps)
testTrap->setInform(true);
```

in `loop()`

```
// must be called as often as possible
snmp.loop();

// Update our timestamp value
tensOfMillisCounter = millis()/10;

// Send the trap to the specified IP address

IPAddress destinationIP = IPAddress(192, 168, 1, 243);

if(snmp.sendTrapTo(testTrap, destinationIP, true, 2, 5000) != INVALID_SNMP_REQUEST_ID){
    Serial.println("Sent SNMP Trap");
} else {
    Serial.println("Couldn't send SNMP Trap");
}
```

The `snmp.sendTrapTo()` values of `true, 2, 5000` indicate that if this is an INFORM request, it will try to send the INFORM up to 2 times, with a Timeout of 5000 milliseconds before it gives up, if it receives no response from the other end. The snmp.loop() will keep trying to resend the trap until the timeout or retry limit is reached.

There is currencly no mechanism to know (with code) if an SNMP INFORM request has been responded to. I hope to work on this in the future.

### SNMP Manager

I am working on adding the functionality to act as an SNMP Server or Manager. In the meantime, if you need to do this, look at the library here: https://github.com/shortbloke/Arduino_SNMP_Manager

---

## Version History

| Version | Changes                                                              |
|---------|----------------------------------------------------------------------|
| **3.1.5** | **Patch: arduino-lint LD003 compliance + README absolute-path (zero code/API/wire changes, on top of v3.1.4).** (1) Arduino library spec Rule LD003 fix: the `demos/` folder contained 3 `.ino` files (`arduino_cli_esp32`, `arduino_cli_esp8266`, `platformio_minimal/src/main`) that triggered `arduino/arduino-lint-action@v1.0.0` CI error `Sketch(es) found outside examples and extras folders`. Relocated entire `demos/` tree to `extras/demos/` (Arduino spec allows sketches under `extras/` or `examples/` only). 3 `.ino` files pure renames (git 100% match); `platformio.ini` `lib_extra_dirs` bumped `../../..` → `../../../..` plus `cd demos/platformio_minimal` → `cd extras/demos/platformio_minimal` comment banner. (2) README local-path sanitization: maintainer-local absolute paths stripped to bare repo-root-relative links, which GitHub renders natively as correct jump-to-line links. Version bump 3.1.4 → 3.1.5 in `library.properties` and `src/include/defs.h`. Host Catch2 101/101 green. No functional, API, or on-the-wire changes: 100% consumer compatible. |
| **3.1.4** | **Public milestone — v3.1.2 + v3.1.3 + v3.1.4 collapsed into one release (100% source & wire compatible, zero API breaks).** Delivered on top of v3.1.1, folding every change from the zero-heap baseline. (A) v3.1.2 RFC-3416 trap fix: SNMPv2c Trap/Inform VarBind #2 name `sysObjectID.0` → correct `snmpTrapOID.0` (`.1.3.6.1.6.3.1.1.4.1.0`), new named constant `SNMPv2_SNMPTRAP_OID_0` so net-snmp `snmptrapd` can look up NOTIFICATION-TYPE definitions (was logging "Cannot find TrapOID in TRAP2 PDU"). (B) v3.1.3 ESP8266 auto-tune + smaller generic defaults: on ESP8266, `_SNMP_ESP8266_TINY` profile activates automatically (opt-out `SNMP_SKIP_ESP8266_AUTOTUNE 1`) shrinking ASNPool/packet/VarBind/OCTET pools to safe small-sensor sizes; generic defaults also reduce ASNPool 64→32, VarBindPool 32→12; all constants now sketch-overridable via `#ifndef…#endif`; removes need for per-sketch shrink blocks. (C) v3.1.4 startup-heap ASNPool + narrower types: the single-biggest static BSS sink `ASNPool slots[N]` moves out of `.bss` into one-shot startup `new Slot[N]()` done exactly once on first `asn_new<T>()` (default; opt-out back to static `.bss` via `SNMP_POOLS_IN_BSS 1`); `SortableOIDType::sortingMap unsigned long[32]` → `uint32_t[32]` (saves 128 B/instantiation on 64-bit hosts; SMIv2 sub-IDs fit in 32 bits exactly); `OCTET_TYPE_MAX_LENGTH` universal default 500→256 (sketch-overridable); `SNMP_Sensor.ino` portability fixes (LittleFS header, ESP8266 `FS_BEGIN()` + `os_random()` rng, timestamp `uint32_t`, removal of the 25-byte sysContact sysName sysLocation SET length cap). Net measured footprint: ESP8266:d1_mini + SNMP_Sensor globals dropped from v3.1.2 **101% OVERFLOW (linker OOM)** → v3.1.4 **33,128 / 80,192 B (41%)** with 47,064 B FREE; ESP32 + ESP32_SNMP globals dropped 74,680→49,856 B = **−24,824 B**. Full 4-target Arduino CLI matrix (2 sketches × esp8266+esp32) links clean. Host Catch2 101/101 green; `src/` header audit confirms zero `<vector>/<deque>/<list>/<functional>`. |
| **3.1.1** | **Minor patch release — user tuning + example corrections.** Four targeted, zero-API-breakage changes on top of v3.1.0. (1) `defs.h` user-overridable sizing: every tuneable size/pool/buffer constant wrapped with `#ifndef … #endif` (15 total: `MAX_SNMP_PACKET_LENGTH`, `OCTET_TYPE_MAX_LENGTH`, 3 string `SNMP_MAX_*_LEN`, 10 `SNMP_MAX_*`/`SNMP_POOL_*`, `DEBUG`). Sketch-side `#define` placed BEFORE `#include <SNMP_Agent.h>` or compiler `-D` flags now win over library defaults, no patch to headers needed. Large banner comment in defs.h documents the override order + ESP-01 clawback recipe. (2) Examples teach tuning: both `ESP32_SNMP.ino` and `SNMP_Sensor.ino` gain a top-of-sketch `COMPILE-TIME TUNING` banner showing commented-out 6-constant halved-ASNPool recipe (~24,576 B BSS saved). SNMP_Sensor banner additionally reminds ESP8266 users to swap `LITTLEFS` → `LittleFS` + install ESP8266LittleFS/ArduinoJson libraries. (3) `SNMP_Sensor.ino` const-correct OIDs: ~33 `char* oidFoo = ".1.3.6.1…"` variables → `const char* oidFoo`, matching the const string literals they bind. Eliminates deprecated `-Wwrite-strings` warnings on modern ESP32/ESP8266 toolchains. (4) CRITICAL `SNMP_Sensor.ino` SET length fix: `addReadWriteStringHandler(&sysContact, 25, true)` → `sizeof(sysContactValue)` (actual buffer size 255). Previously a 100-byte `snmpset` of sysContact/sysName/sysLocation was incorrectly rejected by the 25-byte artificial cap even though `loadSNMPValues()` used `strlcpy(…, sizeof(buf)=255)`; now the three paths (SNMP SET cap, declared C buffer, flash load) use a single consistent 255-byte maximum. Verification: host catch2 101/101 green; `examples/ESP32_SNMP` compiles under Arduino-CLI `esp32:esp32:esp32` with 0 warnings/errors. Net footprint delta vs v3.1.0: 0. |
| **3.1.0** | **Public release — cumulative of ALL changes since v2.1.0 (string model v2.2.0 + BER v3.0.0 + zero-heap refactor).** Delivered as one tested, backwards-compatible tag for upstream/public repos. 2 API signatures changed only (both from v2.2.0 string model, see above); `addXxxHandler/sendTrapTo/begin/loop/setUDP` all 100% unchanged since v2.2.0. (a) String model: all `std::string` removed library/examples/test-wide → fixed `char[]` + `const char*` + explicit length; `GETSTRING_FUNC` typedef → `const char*(*)()`; `OIDType::string()` → `const char*`; example sketch 3 `malloc`s → static buffers; 3 size constants: `SNMP_MAX_COMMUNITY_LEN=64`, `SNMP_MAX_OID_STR_LEN=256`, `SNMP_MAX_STRING_LEN=500`. (b) BER TLV critical fixes (PR #60 upstream + defensive): (1) 4× `fromBuffer` return `_length+2` hardcode → actual consumed bytes (fixes long-form header ≥128 B). (2) `encode_ber_length_integer*` off-by-one `>256` → `>=256` (fixes `length==256` → `0x8100` zero-length; broke snmpbulkwalk). (3) IntegerType 3-byte signed extend `tempVal = tempVal|=mask` UB → `tempVal|=mask`; fixed test-harness `memcpy(randomLong,10)` overread → `sizeof(randomLong)`; defensive max_len pre-checks on 4× fromBuffer; ComplexType child loop dual-condition → remaining counter. (c) Zero-heap 4-phase refactor: hot-path (`loop()`, GET/SET/BULK decode+build, TRAP/INFORM) 100% `malloc/new/calloc`-free. BER objects served from global placement pool (`SNMP_POOL_ASN_OBJECTS=64`, 768 B slot, heap-fallback defensive, ASAN clean on both). All library lists/queues → fixed `T[N] + int count` with compile-time caps; 10 sizing constants added: `SNMP_MAX_OID_SUBIDENTIFIERS=32, SNMP_MAX_COMPLEX_CHILDREN=16, SNMP_MAX_VARBINDS=16, SNMP_MAX_CALLBACKS_PER_AGENT=64, SNMP_MAX_AGENTS=2, SNMP_MAX_UDP_PER_AGENT=2, SNMP_MAX_TRAPS_INFLIGHT=8, SNMP_MAX_CALLBACKS_PER_TRAP=16, SNMP_POOL_ASN_OBJECTS=64, SNMP_POOL_VARBIND_OBJECTS=32`. 3 PDU-handler out-sigs `deque<VarBind>&` → `VarBind out[16]+int&outCount`. Dead `<vector>`/`<deque>` includes + zero-call-site ComplexType shared_ptr-overload removed. Final `src/` audit: 0 `<vector>/<deque>/<list>/<functional>` (2 `<memory>` kept purely for backwards-compat shared_ptr public ctors). Verification: host catch2 101/101 green; ASAN clean; 4/4 strict DoD builds green under `-Wall -Wextra -Werror` (Arduino-CLI esp8266+esp32, PlatformIO esp01_1m+dout+esp32dev). Footprint: Flash −0.71% geometric mean vs pre-refactor (= −4.6 KB avg; esp32dev largest single win −14.1 KB = −1.87%). BSS +48.9 KB deterministic (linker-reported ASNPool; clawback via `#define SNMP_POOL_ASN_OBJECTS=32` before include cuts it in half for esp01_1m → drops from 97.8% to ~68% RAM). |
| 2.2.0   | (Internal precursor — absorbed into the cumulative v3.1.0 entry above for public release) |
| 2.1.0   | (Previous / original Arduino_SNMP) API cleanups, ESP-01 / ESP8266 support. |
| 2.0.x   | Rewrite from Arduino_SNMP v1. RFC-compliant SNMPv2c engine.         |
| 1.x     | Original Arduino_SNMP project.                                      |

Pull requests/comments are welcome
