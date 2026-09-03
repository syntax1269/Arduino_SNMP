# SNMP_Agent
### (Previously Arduino_SNMP)

SNMP Agent built with Arduino

This is a fully-compliant SNMPv2c Agent built for Arduino's, but will work on any OS, providing API code is written for packet serialization (See tests/mock.cpp for an example)

## Current Version: 3.3.1

> **New since v3.1.5:** a hardware-validated reliability campaign (v3.1.23–v3.1.25: pool double-release fix, loud `tooBig` rejection of over-cap GetBulk, pool-baseline freeze, 17 KB slot right-size), **v3.2.0 derived pool sizing** (the ASN pool auto-sizes at compile time from the number of handlers your sketch registers — no more one-size-fits-all arena), and **v3.3.0 boot-time lock-in** (the arena is claimed in the `SNMPAgent` constructor, before `setup()` and WiFi, so the heap can never fragment it). See [Memory Model](#memory-model-v320--v330) below.

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
  * COUNTER64 `uint64_t`
* SNMP PDU Support
  * GetRequest
  * GetNextRequest
  * GetResponse (For SNMPv2c INFORM Responses only for now)
  * SetRequest
  * SNMPv2 Trap
  * GetBulkRequest
  * InformRequest
* Deterministic memory (v3.2.0+)
  * Compile-time derived pool sizing — the arena scales with the handlers you register
  * Boot-time lock-in — one contiguous arena claimed before `setup()`/WiFi; zero per-packet heap traffic
  * Loud failure modes — over-cap GetBulk answers an RFC 3416 `tooBig` error PDU instead of silently truncating
  * Hardware-validated: ESP-01 30-minute soak campaigns, 0 pool alarms / 0 reboots / flat heap

It was designed and tested around an ESP32, but will work with any Arduino-based device that has a UDP object available. Optimized for ESP-01 (ESP8266) and other memory-constrained embedded targets — the library auto-tunes a reduced "TINY" profile on ESP8266 and was soak-tested on a 1 MB ESP-01 against upstream v2.1.0 (same sketch: flat heap, zero allocation failures, bounded deterministic RAM).

The example goes into detail around how to use, or look at `src/SNMP_Agent.h` for the API.

If you're coming from v1, most, but not all APIs are drop-in replaceable.
Some of the API's, especially around strings have changed. Look in `SNMP_Agent.h` for details.

If you're upgrading from v2.0/v2.1, note the **2.2.0 string model change** below.

If you're upgrading from v2.2, v3.0.0 is **source-compatible** (no API changes) but fixes several critical BER TLV encoding/decoding bugs. Mandatory upgrade if you use GetBulk, large responses (length ≥ 128 bytes, especially exactly 256 bytes), or snmpbulkwalk.

If you're upgrading from v3.0.0 / v3.0.6 to v3.1.0: **100% source + wire compatible, zero API changes, zero breaking changes.** v3.1.0 closes out the 4-phase zero-heap refactor (eliminates all remaining `std::vector` / `std::deque` / `std::list` from library source; replaces last `make_shared` temp-allocations with pool-allocated raw BER objects; drops 3 dead standard-container includes + 1 dead inline method that was pulling shared_ptr machinery per-TU). Flash is slightly smaller on every target (−0.71% average vs v3.0.0 baseline; largest win PlatformIO esp32dev −1.3% = −9.6 KB), BSS is deterministic +48.9 KB (linker-reported, no mid-packet fragmentation, tuneable down via `SNMP_POOL_ASN_OBJECTS` if you're on esp01_1m). Mandatory upgrade if you've ever seen ESP-01 heap-fragmentation panics after 30+ days of SNMP polling.

---

## Memory Model (v3.2.0 / v3.3.0)

Since v3.2.0 the ASN pool is **derived at compile time** from the knobs your build already sets — there is no fixed pool size to hand-tune:

```
pool = SNMP_WORST_TICK_TRANSIENTS + SNMP_MAX_CALLBACKS_PER_AGENT

SNMP_WORST_TICK_TRANSIENTS
    = (SNMP_MAX_VARBINDS + 2) * 3        // GetBulk response at the varbind cap
    + (SNMP_MAX_TRAPS_INFLIGHT) * 3      // inflight informs decoded while a response builds
    + 24                                 // measured parse/response headroom
```

A sketch registering 12 handlers derives a smaller arena than one registering 40; a 40-handler deployment gets the pool the old fixed arena would have starved. The headroom term is measured, not guessed: 60 s of sustained Cr6-walk + SET + trap saturation on an ESP-01 peaked at 66 slots at TINY caps.

Since v3.3.0 the arena is **locked in at boot**: every `SNMPAgent` constructor calls `ASNPool::lockInArena()`, which claims the one contiguous `sizeof(Slot) x SNMP_POOL_ASN_OBJECTS` block at global-constructor time — before `setup()`, before WiFi — while the heap is pristine. This replaced the v3.1.x lazy first-parse allocation, which could fail on a fragmented heap and abort the sketch (ESP8266 runs with exceptions disabled). Lock-in is no-throw: on failure it logs a warning and falls back to the lazy path. `SNMP_POOL_LOCK_AT_BOOT=0` (global build flag) restores lazy allocation; `SNMP_POOLS_IN_BSS=1` places the arena in static BSS instead.

### Current profile defaults (tunable in [defs.h](src/include/defs.h))

| Constant | ESP8266 TINY (auto) | Default (ESP32 etc.) |
|---|---|---|
| `MAX_SNMP_PACKET_LENGTH` | 1024 | 1400 |
| `SNMP_MAX_OID_STR_LEN` | 192 | 256 |
| `SNMP_MAX_COMPLEX_CHILDREN` | 16 | 24 |
| `SNMP_MAX_VARBINDS` | 6 | 16 |
| `SNMP_MAX_CALLBACKS_PER_AGENT` | 24 | 64 |
| `SNMP_MAX_TRAPS_INFLIGHT` | 4 | 8 |
| `SNMP_MAX_CALLBACKS_PER_TRAP` | 8 | 16 |
| `SNMP_POOL_SLOT_SIZE` | 288 | 312 |
| `SNMP_POOL_ASN_OBJECTS` | derived (84 at TINY defaults) | derived (96 at default caps) |

Slot size is pinned to the measured largest BER container by exhaustive `static_assert`s — if a container grows, the build fails loudly instead of silently corrupting.

### !! Size overrides and the One-Definition Rule !!

Never `#define` size-affecting knobs (`SNMP_MAX_CALLBACKS_PER_AGENT`, `SNMP_POOL_*`, `OCTET_TYPE_MAX_LENGTH`, `SNMP_MAX_OID_STR_LEN`, ...) **inside a sketch file**. They change class layout, so a sketch-only define makes the sketch and the compiled library disagree about object sizes → undefined behavior (on ESP8266, observed live: instant reboot loops). Set them as **global build flags** so every translation unit agrees:

```ini
; platformio.ini
build_flags = -DSNMP_MAX_CALLBACKS_PER_AGENT=64
```
```bash
# arduino-cli
arduino-cli compile --fqbn esp8266:esp8266:d1_mini \
  --build-flags "-DSNMP_MAX_CALLBACKS_PER_AGENT=64" MySketch
```

Behavior-only knobs (e.g. `SNMP_POOL_LOCK_AT_BOOT`) do not change layout and are safe anywhere, but global flags remain the recommended style.

**Best practice on constrained targets:** after registering handlers in `setup()`, call `ASNPool::freezePermCount()` — it pins the exact permanent baseline so no slot is reserved pessimistically. (Without it, the v3.1.25 auto-freeze safety net guarantees correctness, but may freeze a slightly larger baseline if a startup trap allocated transients first.)

---

## 3.1.0 Zero-Heap Refactor (Deterministic Memory)

v3.1.0 is the final release of the 4-phase deterministic-memory refactor. Steady-state packet processing (`agent.loop()`, `sendTrapTo`, GET/GETNEXT/GETBULK/SET decode + response build) now performs **zero** `malloc`/`new`/`calloc`/`realloc`. All ASN.1 BER objects come from a compile-time-sized global placement pool. All VarBind/PDU/agent callback lists use fixed C-arrays with explicit compile-time capacity caps. (v3.1.0-era pool values below are historical — the current derived sizing and profile defaults are in the [Memory Model](#memory-model-v320--v330) section above.)

### Why this change
- **Zero heap fragmentation under load** — field units that previously crashed after 30+ days of 1 Hz SNMP polling (heap fragmented so a 512-byte UDP packet buffer couldn't allocate) now run indefinitely with linker-reported deterministic memory.
- **Smaller flash** (−0.71% geometric mean across all 4 DoD targets) — removing dead `<vector>`/`<deque>` includes, deleting a zero-call-site inline method that pulled shared_ptr machinery per-TU, and collapsing 3 separate `remove_if` lambda-template instantiations into one C-style predicate dispatcher collectively saved ~4.6 KB.
- **Compile-time sizing, fail-safe** — every queue/array/buffer has a `constexpr` maximum (see table below). Overflows return well-defined error codes instead of undefined-behavior heap exhaustion panics.

### Compile-Time Sizing Constants (historical v3.1.0 table — current values in [defs.h](src/include/defs.h) and the Memory Model section)

| Constant                     | Default | Purpose |
|------------------------------|---------|---------|
| `SNMP_MAX_OID_SUBIDENTIFIERS`| 32      | OID sub-IDs (realistic max ~17) |
| `SNMP_MAX_COMPLEX_CHILDREN`  | 16      | Children per decoded PDU/VarBind-list ComplexType |
| `SNMP_MAX_VARBINDS`          | 16      | VarBinds per request/response (snmpbulkwalk default is 10) |
| `SNMP_MAX_CALLBACKS_PER_AGENT` | 64    | Registered OID handlers per SNMPAgent instance |
| `SNMP_MAX_AGENTS`            | 2       | Concurrent SNMPAgent instances (usually just 1) |
| `SNMP_MAX_UDP_PER_AGENT`     | 2       | UDP transport interfaces per agent (WiFi + ETH fallback) |
| `SNMP_MAX_TRAPS_INFLIGHT`    | 8       | INFORM retry queue + pending TRAP depth |
| `SNMP_MAX_CALLBACKS_PER_TRAP`| 16      | OID pointers embedded in a single SNMPTrap object |
| `SNMP_POOL_ASN_OBJECTS`      | 64      | ASNPool slots — global BER_CONTAINER placement pool (decode + build + trap + clone) |
| `SNMP_POOL_VARBIND_OBJECTS`  | 32      | VarBind placement pool (packet build/trap path) |

**ESP-01 / esp01_1m tuning note** — since v3.2.0 the pool derives itself from `SNMP_MAX_CALLBACKS_PER_AGENT`, so shrinking the handler count is what shrinks the arena; on ESP8266 the TINY profile applies automatically. Declaring only the handlers you need is the tuning. Size overrides must be **global build flags** (see the One-Definition Rule warning above) — sketch-side `#define`s of layout-affecting knobs are undefined behavior:
```ini
; platformio.ini — every TU must agree on class layout
build_flags = -DSNMP_MAX_CALLBACKS_PER_AGENT=16 -DSNMP_MAX_VARBINDS=4
```
Prefer static BSS over the startup-heap arena? Add `-DSNMP_POOLS_IN_BSS=1`.

### Verification (all green, Definition of Done)
| Check | Result |
|---|---|
| Host catch2 (native clang 14 / g++) | ✅ 101/101 assertions in 10 test cases |
| AddressSanitizer (memory + leak) | ✅ 0 errors / 0 leaks (ASNPool path + heap-fallback both clean) |
| Arduino-CLI `esp8266:esp8266:generic` | ✅ Flash 254,424 B (24%), RAM 100% |
| Arduino-CLI `esp32:esp32:esp32` | ✅ Flash 904,223 B (68%), RAM 29% |
| PlatformIO env esp8266 (board esp01_1m, dout) | ✅ Flash 283,351 B (37.2%), RAM 97.8% |
| PlatformIO env esp32 (board esp32dev) | ✅ Flash 740,693 B (56.5%), RAM 29.1% |
| Build flags all targets | ✅ `-Wall -Wextra -Werror` across all 4 |
| `src/` header standard-container audit | ✅ 0 `<vector>` / 0 `<deque>` / 0 `<list>` / 0 `<functional>`. Only 2 `<memory>` retained for public shared_ptr compat (OIDType::cloneOID + VarBind legacy ctors) |

### Detailed Changelog (3.0.6 → 3.1.0)
1. **Phase 1.5 (last deque):** Changed 3 PDU-handler out-parameter signatures from `std::deque<VarBind>&` → `VarBind out[SNMP_MAX_VARBINDS] + int& outCount`. All 15 `emplace_back` → placement-construct via `appendResponseVarBind` helper. Deleted the last `std::deque<VarBind>` instance in `SNMPParser.cpp`. 15 internal `make_shared<ImplicitNullType>` / `make_shared<IntegerType>` temp-allocations → direct `asn_new<T>` pool raw-pointers.
2. **Phase 1.1–1.4 structural audit (no rework):** Verified all 4 Phase 1 conversions were already complete and operational from prior tags:
   - OpaqueType `uint8_t* _value` calloc → `_value[OCTET_TYPE_MAX_LENGTH]` fixed array.
   - OIDType `std::vector<uint8_t> data` → `uint8_t data[SNMP_MAX_OID_SUBIDENTIFIERS+1] + int dataLen`.
   - SortableOIDType `std::vector<unsigned long>` → `unsigned long sortingMap[SNMP_MAX_OID_SUBIDENTIFIERS] + int sortingMapLen`.
   - ComplexType `std::vector<shared_ptr<BER_CONTAINER>> values` → `BER_CONTAINER* values[SNMP_MAX_COMPLEX_CHILDREN] + int valuesLen + bool _ownsChildren` (uniform recursive ownership via asn_delete in dtor).
3. **Phase 3 (init-only callback new):** *Accepted as-is, no code change.* All `addXxxHandler` invocations happen exactly once inside `setup()`; this is explicitly allowed by the zero-heap blueprint (the ban applies to packet-hot-path / ISR / loop). Users who want 100% static BSS can still use `static IntegerCallback cb(…)` + `addHandler(&cb)` overload (already supported).
4. **Phase 4 (dead header + dead method sweep):** Dropped stale `<vector>` includes from [BER.h](src/include/BER.h) and [SNMPResponse.h](src/include/SNMPResponse.h); dropped last `<deque>` from [SNMPParser.h](src/include/SNMPParser.h) coincident with sig change; deleted dead inline `ComplexType::addValueToList(const shared_ptr<BER_CONTAINER>&)` method (zero call sites anywhere in library/tests, was pulling shared_ptr refcount machinery into every TU that included BER.h).

**Footprint delta vs v3.0.0 (baseline):**
- Arduino-CLI esp8266: Flash −2,648 B (−1.0%); BSS +48,901 B (+49.0 KB, deterministic ASNPool).
- Arduino-CLI esp32: Flash −4,976 B (−0.55%); BSS +48,901 B (+49.0 KB).
- PlatformIO esp01_1m: Flash −3,160 B (−1.1%); BSS +49,824 B (+49.8 KB).
- PlatformIO esp32dev: Flash −14,112 B (−1.87%); BSS +49,896 B (+49.9 KB).
- **Geometric mean flash reduction: −0.71% (−4,649 B avg).**

---

## 3.0.0 Critical Bug Fixes + BER Hardening (Major Release)

v3.0.0 is a major hardening release focused on the BER TLV engine — fixes real-world interoperability failures with `snmpbulkwalk` and `snmpset` for responses ≥ 128 bytes, corrects 3-byte BER length header handling, removes undefined behavior in signed 3-byte integer decode, adds defensive overflow bounds checks, and removes a stack buffer overread from the test harness.

### Summary
- **Breaking for build/test tooling? No.** No API changes.
- **Breaking for binary flash footprint? Slightly smaller.** Removed dead memset, removed redundant `.reserve()` before `.assign()`, inlined compound-operator, hoisted temp array.
- **Breaking for network behavior? Fixes silent corruption.** Packets that used to be truncated/zero-length on the wire are now correctly formatted.

### Upstreamed from 0neblock/Arduino_SNMP PR #60
All 5 commits of PR #60 (Aidan Cyr + N1IOX, 2024-08 to 2026-01) are integrated:

1. **BER `_length + 2` hardcoded-return bug** — BERDecode functions `OIDType`, `Counter64`, `ComplexType` all returned a hardcoded `_length + 2` instead of actual consumed bytes. When the BER length field encoded into long-form (≥ 128 byte payload = 3+ byte header instead of 2), the return value under-reported bytes consumed → packet parser walked off-structure into garbage.
   * Now use `j + _length` / `i + _length` where j/i = actual TLV header bytes consumed.

2. **`length == 256` encoded as `0`** (catastrophic) — `encode_ber_length_integer()` had `if(integer > 256)` off-by-one. A response exactly 256 bytes long was encoded as `0x81 0x00` (= length 0 per ASN.1 BER). Net-snmp `snmpbulkwalk` fails immediately because the PDU is zero-length. Same bug existed in the paired byte-counter `encode_ber_length_integer_count()` used by `ComplexType::serialise()` shift-array logic → internal length/bookkeeping mismatch for any 256 ≤ len < 512 packet.
   * Changed **both functions** to `if(integer >= 256)`.

3. **Undefined behavior: `tempVal = tempVal |= 0xFF000000`** — 3-byte signed IntegerType sign-extend code used double-store (`= result of |=`) which is undefined order-of-operations → treated as error by `-Werror=sequence-point`.
   * Simplified to `tempVal |= 0xFF000000;`

4. **Stack buffer overread in tests** — `memcpy(&buffer[i], &randomLong, 10)` copied 10 bytes from a `long` (4 bytes on 32-bit, 8 bytes on 64-bit). Read 2–6 bytes past stack end. Also disabled flaky random-data "corrupt buffer" test section (random noise → sometimes valid BER → false CI failures) and exact-133-byte reparse false-negative (both temporarily `/* */` disabled per PR #60 guidance).

### Additional Optimizations (not in PR #60)
Applied per the workspace [optimizations.md](.trae/rules/optimizations.md) Blueprint rules:

* **[BER_CONTAINER::fromBuffer](src/BERDecode.cpp#L35-L51)** — boundary check used `_length + 2` (hardcoded header assumption). Now uses `_length + actual_header_len` computed via `ptr - buf`. Length-decoder's max iteration bound corrected: we've already consumed the type byte so decoder may iterate at most `max_len - 1` bytes, not `max_len`.
* **[OIDType::fromBuffer](src/BERDecode.cpp#L129-L142)** — Added overflow pre-check before dereferencing `*dataPtr`. Removed redundant `.reserve(_length)` immediately before `.assign(...)` (assign already sizes-to-fit → one allocator pass, not two).
* **[Counter64::fromBuffer](src/BERDecode.cpp#L216-L232)** — overflow pre-check added before value decode loop.
* **[ComplexType::fromBuffer](src/BERDecode.cpp#L286-L317)** — overflow pre-check + correct `max_len - outer_used` child-bounds propagation. Replaced buggy dual-condition `while(i < _length && i <= max_len)` with clean descending `remaining > 0` counter.
* **[SortableOIDType::generateSortingMap](src/BERDecode.cpp#L190-L208)** — removed dead `*1` multiplier in `.reserve(size * 1)` call.
* **[OIDType::generateInternalData](src/BEREncode.cpp#L145-L176)** — added `.reserve(SNMP_MAX_OID_STR_LEN)` to avoid mid-parse reallocations on each push_back; hoisted `uint8_t temp[10]` out of loop (was declared + zeroed every iteration); inverted the OID-component-valid guard (removes one nesting level + dead else); used `sizeof(temp)` instead of magic `10`.

### Test Status
```
All tests passed (97 assertions in 10 test cases)
```
Built with `-std=c++11 -Wall -Wpedantic -Wextra -Werror`.

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
| Constant                    | Value | Purpose                          |
|-----------------------------|-------|----------------------------------|
| `SNMP_MAX_COMMUNITY_LEN`    | 64    | Community string (RO/RW)         |
| `SNMP_MAX_OID_STR_LEN`      | 256   | OID dotted-decimal representation|
| `SNMP_MAX_STRING_LEN`       | 500   | OctetString (OID value payload)  |

These are conservative defaults; tune them in `defs.h` if you need smaller RAM footprint on the ESP-01.

### User-Code String Migration (v2.1 → v2.2)
Replace any `std::string` variables used with `addReadOnlyStaticStringHandler`, `addReadWriteStringHandler`, or `GETSTRING_FUNC` callbacks:

Before (v2.1):
```cpp
#include <string>
std::string sysDescr = "ESP32 SNMP Agent";
snmp.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.1.1.0", sysDescr);
```

After (v2.2):
```cpp
char sysDescr[] = "ESP32 SNMP Agent";     // or const char* to a PROGMEM literal
snmp.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.1.1.0", sysDescr);
```

Read-write string: keep the `char**` pattern but use a static buffer (no `malloc`):
```cpp
char _sysContactBuf[255];                    // static storage, NOT malloc'd
char* sysContact = _sysContactBuf;
snprintf(sysContact, sizeof(_sysContactBuf), "admin@example.com");
snmp.addReadWriteStringHandler(".1.3.6.1.2.1.1.4.0", &sysContact, sizeof(_sysContactBuf), true);
```

`GETSTRING_FUNC` return type changed: now returns `const char*` (was `const std::string`):
```cpp
const char* getFirmwareVersion(void) { return LIBRARY_VERSION; }  // "3.0.0" from defs.h
snmp.addReadOnlyStringHandler(".1.3.6.1.4.1.99.0", getFirmwareVersion);
```

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

If you want the Arduino to respond to an SNMP server at some specified OIDs, you need to implement a `ValueCallback` for each OID, attached to a variable to respond with.
Whenever an OID is requested by an SNMP manager, the ValueCallback for that OID is found, and the latest value of that variable is used to respond.

For example, to respones to the OID ".1.3.6.1.4.1.5.0" with the number: 5.
```
int testNumber = 5;
snmp.addIntegerHandler(".1.3.6.1.4.1.5.0", &testNumber);
```

You can enable SNMPSet requests, by setting `isSettable = true` as a parameter when adding the handler, for example:
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

There are a few requirements in setting up a trap in order to comply with the SNMP RFC.

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

There is currently no mechanism to know (with code) if an SNMP INFORM request has been responded to. I hope to work on this in the future.

### SNMP Manager

I am working on adding the functionality to act as an SNMP Server or Manager. In the meantime, if you need to do this, look at the library here: https://github.com/shortbloke/Arduino_SNMP_Manager

---

## Version History

| Version | Changes                                                              |
|---------|----------------------------------------------------------------------|
| **3.3.0** | **Feature: boot-time pool lock-in.** The ASN arena is claimed in the `SNMPAgent` constructor at global-ctor time (before `setup()`/WiFi) via no-throw `ASNPool::lockInArena()`, replacing the lazy first-parse allocation that could abort the sketch on a fragmented heap. ESP-01 measured lock at t=68 ms. `SNMP_POOL_LOCK_AT_BOOT=0` restores lazy. 8-min soak: 703 ops, 0 alarms, 0 reboots, heap flat. |
| **3.2.0** | **Feature: derived pool sizing.** `SNMP_POOL_ASN_OBJECTS` now derives at compile time from `SNMP_WORST_TICK_TRANSIENTS + SNMP_MAX_CALLBACKS_PER_AGENT` (measured worst-tick headroom), replacing the fixed 76/80-slot arena. Declared-12 sketch derives 72 (wire-verified cap=72 peak=66); TINY defaults derive 84. Documents the ODR hazard of sketch-side size overrides. |
| **3.1.25** | **P0 pool-baseline freeze inside `resetAll()`** (fixes live-object clobbering when a trap precedes the first `loop()` tick) + **P1 slot right-size** 512→288 / 640→312 (+17.2 KB free heap on ESP-01, frag 18%→1%). |
| **3.1.24** | **Loud truncation rejection:** over-cap GetBulk expansion answers an RFC 3416 `tooBig` error PDU instead of silently truncating. |
| **3.1.23** | **Pool double-release fix + telemetry** (usedCountPeak, alarms), 30-min soak tooling and campaign. |
| **3.1.6 – 3.1.22** | Sizing passes, CI hardening, walkthrough — see [changelog.md](changelog.md). |
| **3.1.5** | **Patch: arduino-lint LD003 compliance + README absolute-path PII sanitization.** Two targeted fixes, zero code/API/wire changes, fully on top of v3.1.4. (1) **Arduino library spec Rule LD003:** the `demos/` folder contained 3 `.ino` files (`arduino_cli_esp32`, `arduino_cli_esp8266`, `platformio_minimal/src/main`) which triggered `ERROR: Sketch(es) found outside examples and extras folders` under `arduino/arduino-lint-action@v1.0.0` on GitHub Actions. Relocated the entire `demos/` tree to `extras/demos/` (Arduino spec allows sketches under `extras/` or `examples/` only); 3 pure `.ino` moves recorded by git with 100% rename detection. Updated [platformio.ini](extras/demos/platformio_minimal/platformio.ini) `lib_extra_dirs` from `../../..` → `../../../..` plus the `cd` comment banner to account for the one extra nesting level. (2) **README local-path PII sanitization sweep:** all maintainer-local filesystem absolute paths (deep home-folder `file:///...` links) across `README.md`, `README2.md`, and `github-pull-request-post-v2.md` stripped down to bare repo-root-relative links (`src/include/defs.h#L59-L75` etc.), which GitHub renders natively as correct jump-to-line links. Same sanitization applied to the `tozip/README.md` release-zip mirror copy. Final repo-wide audit returns zero matches for local-home absolute paths anywhere in tracked files or release artifacts. Version bumped 3.1.4 → 3.1.5 in [library.properties](library.properties#L2) and [defs.h](src/include/defs.h#L34-L37). Host Catch2 101/101 green. No functional, API, or on-the-wire behavior changes — 100% consumer compatible. |
| **3.1.4** | **Major footprint pass — one-shot startup-heap pools + SortableOID shrink + universal OCTET 256 default.** All per-call / hot-path allocation remains 100% placement-new into fixed-capacity buffers (zero `new`/`malloc`/realloc after first `asn_new`). Changes: (1) **ASNPool startup-heap allocation (default on, opt-out via `#define SNMP_POOLS_IN_BSS 1`):** `ASNPool::Slot slots[N]` static BSS array replaced by one-shot `new Slot[N]()` called on first `asn_new<T>()` from `_ensurePools()`. Count + layout still fixed at compile-time; never deallocated, never reallocated, never grows. Effect: moves the single biggest static BSS sink out of `.bss` onto the heap, clawing back **15–49 KB** of DRAM (depending on `SNMP_POOL_ASN_OBJECTS` + `SNMP_POOL_SLOT_SIZE`) on 80 KB-DRAM ESP8266 parts, which is the exact headroom needed to run WiFi + LittleFS + ArduinoJson alongside SNMP. API for `asn_new/asn_delete/isInPool/release` is byte-identical, public sigs unchanged. Guard: `SNMP_POOLS_IN_BSS 1` restores v3.1.3 behavior for codebases that prefer static pools. (2) **SortableOIDType `sortingMap` narrowed:** `unsigned long sortingMap[32]` (256 B on 64-bit / 128 B on 32-bit) → `uint32_t sortingMap[32]` (128 B / 128 B exactly on both word widths). Drops per-instance SortableOID footprint by 128 B on 64-bit; on 32-bit ESP8266/ESP32 the change is semantically cleaner because SNMP sub-IDs fit in 32 bits per SMIv2 rules, and the storage width now matches the encoder math with zero truncation risk. Updated [BERDecode.cpp generateSortingMap](src/BERDecode.cpp#L245) and [ValueCallbacks.cpp sort_oids](src/ValueCallbacks.cpp#L207) signatures accordingly. (3) **Universal `OCTET_TYPE_MAX_LENGTH` default tightened 500 → 256**, no longer ESP8266-tiny-exclusive. 256 B still covers 99% of real MIB strings (ifDescr, sysContact, sysName, sysLocation, EntityMIB strings all routinely < 128 B). Sketch-side override preserved: `#define OCTET_TYPE_MAX_LENGTH 500` before library include restores previous max. Effect on 32-bit targets: every OctetType/OpaqueType shrinks by −244 B, which in turn keeps SortableOIDType / ComplexType / VarBind layouts fitting into the smaller 640 B ESP8266-tiny slot. (4) Slot-size static assertion guards retained at the bottom of [BER.h](src/include/BER.h#L455) to loudly break any build where a concrete subclass grows larger than `SNMP_POOL_SLOT_SIZE`. (5) `_SNMP_ESP8266_TINY` auto-tune profile + `SNMP_SKIP_ESP8266_AUTOTUNE` opt-out still work as in v3.1.3. (6) Example sketches: BSS headroom further improved on ESP8266; ESP32 RAM also drops ~8 KB because the generic non-ESP8266 `ASNPool slots[32]` no longer occupies .bss (one-shot heap instead). Verification: host Catch2 101/101 green, 4x arduino-cli matrix (2 sketches × esp8266:d1_mini + esp32:esp32:esp32) all compile + link with measured BSS deltas documented above. Zero API changes, 100% wire-identical output (no BER code paths modified). |
| **3.1.3** | **Zero-Heap Footprint + ESP8266 Auto-Tune patch.** Library is safe for ESP-01 / D1 mini sketches that combine WiFi + LittleFS + ArduinoJson + SNMPAgent. Changes: (1) **defs.h ESP8266 auto-tune profile `_SNMP_ESP8266_TINY`:** when targeting ESP8266 (and user hasn't set `SNMP_SKIP_ESP8266_AUTOTUNE=1`), automatically reduces pool/buffer constants so static BSS shrinks by ≈ 28 KB vs v3.1.2 defaults. Tunes: `MAX_SNMP_PACKET_LENGTH 1400→1024`, `OCTET_TYPE_MAX_LENGTH 500→256`, `SNMP_MAX_OID_STR_LEN 256→192`, `SNMP_MAX_COMPLEX_CHILDREN 16→8`, `SNMP_MAX_VARBINDS 16→6`, `SNMP_MAX_CALLBACKS_PER_AGENT 64→24`, `SNMP_MAX_TRAPS_INFLIGHT 8→4`, `SNMP_MAX_CALLBACKS_PER_TRAP 16→8`, `SNMP_POOL_ASN_OBJECTS 64→24`, `SNMP_POOL_VARBIND_OBJECTS 32→8`, and new tunable `SNMP_POOL_SLOT_SIZE 768→640` (ESP8266 tiny only). (2) **Generic defaults (ESP32 / non-ESP8266) also reduced 30–50% where safe:** `SNMP_POOL_ASN_OBJECTS 64→32`, `SNMP_POOL_VARBIND_OBJECTS 32→12`, `SNMP_MAX_CALLBACKS_PER_AGENT 64→64` kept, but all other pool counters halved, yielding ≈ 25 KB RAM clawback on every non-ESP8266 target too. (3) **All constants remain sketch-overridable via `#ifndef` guards;** opt-out of ESP8266 auto-tune by `#define SNMP_SKIP_ESP8266_AUTOTUNE 1` before including `SNMP_Agent.h`. (4) `SNMP_Sensor.ino` no longer needs a sketch-side shrink-block (centralized), banner simplified. (5) Example sketches — cross-platform build matrix: `ESP32_SNMP.ino` + `SNMP_Sensor.ino` both link on esp8266:d1_mini (80 KB DRAM) at v3.1.3, with ≈ 10–14 KB globals headroom vs 101% BSS overflow on v3.1.2. Zero API changes, 100% wire-identical. Host Catch2 101/101 green. |
| **3.1.2** | **Patch: SNMPv2 Trap/Inform snmpTrapOID.0 RFC 3416 fix (issue #64 upstream).** Fixes a generic (non-hardware-specific) bug on every target: SNMPv2c Trap/Inform varbind #2 used the wrong **name** OID — `sysObjectID.0` (`.1.3.6.1.2.1.1.2.0`) instead of RFC-3416-required `snmpTrapOID.0` (`.1.3.6.1.6.3.1.1.4.1.0`). The VB #2 value (user-supplied notification-type OID set via `SNMPTrap::setTrapOID()`) was always correct; only the VB's *name* was mismatched. Effect on real receivers: net-snmp `snmptrapd` logged `Cannot find TrapOID in TRAP2 PDU` and was unable to look up the trap's NOTIFICATION-TYPE definition. Fix is two-part: (1) added new compile-time constant `SNMPv2_SNMPTRAP_OID_0` (".1.3.6.1.6.3.1.1.4.1.0") in [defs.h](src/include/defs.h#L174-L175) next to existing RFC1213 sysObjectID/sysUpTime constants; (2) [SNMPTrap.cpp](src/SNMPTrap.cpp#L5-L6) static OID initializers now use the named constants instead of duplicated magic literals — `SNMPTrap::s_timestampOID(RFC1213_OID_sysUpTime)` + `SNMPTrap::s_snmpTrapOID(SNMPv2_SNMPTRAP_OID_0)` — prevents re-typoing the 24-digit OID literals in future edits. 100% wire-compatible fix, zero API changes, zero footprint impact (constant folding produces identical binary). Verification: host catch2 101/101 green. |
| **3.1.1** | **Minor release — user-coder-friendly tuning + example hardening (patch).** Four targeted changes, zero source-API changes, zero footprint delta vs v3.1.0. (1) **defs.h user-overridable sizing:** every tuneable size/pool/buffer constant wrapped with `#ifndef … #endif` (15 in total: MAX_SNMP_PACKET_LENGTH, OCTET_TYPE_MAX_LENGTH, 3 × SNMP_MAX_*_LEN, 10 × SNMP_MAX_* / SNMP_POOL_*, DEBUG). Users can now write `#define SNMP_MAX_COMPLEX_CHILDREN 8` BEFORE `#include <SNMP_Agent.h>` in their .ino, or pass `-DSNMP_POOL_ASN_OBJECTS=32` via build_flags, and their value wins over the library default without needing to patch defs.h. Added a large banner comment in defs.h documenting override ordering + ESP-01 clawback recipe. (2) **Examples teach the new tuning:** added identical COMPILE-TIME TUNING banners to both sketches (ESP32_SNMP.ino, SNMP_Sensor.ino) showing the 6-constant ESP-01 tune-down recipe + exact byte savings estimate (24,576 B BSS from halving ASNPool). SNMP_Sensor banner additionally warns ESP8266 users to swap LITTLEFS→LittleFS and install ESP8266LittleFS/ArduinoJson libraries. (3) **SNMP_Sensor const-correct OIDs:** 33 variables declared `char* oidFoo = ".1.3.6.1…"` → `const char* oidFoo`, matching the const string literal they point to. Eliminates deprecated `-Wwrite-strings` warnings on modern ESP32/ESP8266 toolchains. (4) **CRITICAL SNMP_Sensor SET length fix:** three `addReadWriteStringHandler(&sysContact, 25, true)` length 25 → `sizeof(sysContactValue)` (actual buffer size 255). Previously any SNMP SET of sysContact/sysName/sysLocation > 25 bytes was incorrectly rejected even though the declared storage was [255] and the load-from-flash path used strlcpy(…, 255); now SET max length, declared buffer size, and persistent-storage copy max length all agree, eliminating the inconsistent truncation. Verification: host catch2 101/101 green; examples/ESP32_SNMP compiled clean 0 warnings 0 errors (Arduino-CLI esp32:esp32:esp32, 72% Flash / 14% RAM). |
| **3.1.0** | **4-Phase Zero-Heap Refactor, deterministic-memory final release.** (a) Phase 1.5 last deque removed from hot path: 3 PDU handler out-param sigs changed `std::deque<VarBind>& → VarBind out[SNMP_MAX_VARBINDS] + int& outCount`; added placement-construct `appendResponseVarBind` helper; 15 emplace_back → helper; 15 internal `make_shared<ImplicitNullType/IntegerType>` temp allocations → direct raw `asn_new<T>` pool pointers. (b) Phase 1.1–1.4 structural audit confirmed all 4 fixed-array conversions complete (OpaqueType `_value[]`, OIDType `data[]`, SortableOIDType `sortingMap[]`, ComplexType `values[]` + `_ownsChildren`). (c) Phase 3 init-only callback new documented compliant with Rules02 §1 (setup-only, exempt); no code change. (d) Phase 4 dead header/method sweep: dropped stale `<vector>` from BER.h + SNMPResponse.h; dropped last `<deque>` include from SNMPParser.h coincident with sig change; deleted dead inline ComplexType::addValueToList(shared_ptr<>) method (zero call sites, pulled shared_ptr machinery per-TU). Final `src/` audit: 0 `<vector>`/`<deque>`/`<list>`/`<functional>` includes anywhere in library (only 2 `<memory>` retained for public shared_ptr API compat). 10 new compile-time sizing constants added to defs.h (SNMP_MAX_OID_SUBIDENTIFIERS, SNMP_MAX_COMPLEX_CHILDREN, SNMP_MAX_VARBINDS, SNMP_MAX_CALLBACKS_PER_AGENT, SNMP_MAX_AGENTS, SNMP_MAX_UDP_PER_AGENT, SNMP_MAX_TRAPS_INFLIGHT, SNMP_MAX_CALLBACKS_PER_TRAP, SNMP_POOL_ASN_OBJECTS, SNMP_POOL_VARBIND_OBJECTS); compile tuning guidance added for esp01_1m (SNMP_POOL_ASN_OBJECTS 64→32 = −24.5 KB BSS). Verification: host catch2 101/101 green, ASAN clean, 4/4 strict DoD cross builds green (-Wall -Wextra -Werror on Arduino-CLI esp8266/esp32 + PlatformIO esp01_1m/esp32dev). Flash −0.71% geometric mean vs v3.0.0 baseline (−4.6 KB avg; esp32dev biggest win −14.1 KB = −1.87%). BSS deterministic +48.9 KB (ASNPool 64×768 B, linker-reported at build time, tuneable). 100% source + wire compatible with v3.0.0; zero API changes; zero breaking changes. |
| **3.0.0** | **BER TLV hardening + PR #60 upstream integration.** Fixed 3 critical BER length bugs: (1) OIDType/Counter64/ComplexType returned hardcoded `_length + 2` regardless of actual TLV header bytes consumed → wrong for payloads ≥128B with 3+ byte long-form headers. (2) `encode_ber_length_integer` off-by-one used `integer > 256` instead of `>=` → length=256 encoded as `0x81 0x00` (=0) breaking snmpbulkwalk. (3) Double-store UB in IntegerType 3-byte sign-extend: `tempVal = tempVal |= 0xFF000000` → `tempVal |= 0xFF000000`. Added defensive boundary pre-checks at BER_CONTAINER/OIDType/Counter64/ComplexType fromBuffer entry. Removed redundant `.reserve()` before `.assign()` in OIDType decode. Hoisted `temp[10]` + added `.reserve(SNMP_MAX_OID_STR_LEN)` in OIDType encode. Replaced ComplexType dual-condition loop with descending `remaining` counter. Stack-buffer-overread fixed in tests memcpy(randomLong, 10) → sizeof(randomLong). Version bumped: defs.h LIBRARY_VERSION_{3,0,0} and library.properties 3.0.0. All tests pass: 97 assertions / 10 cases / 0 failures. |
| **2.2.0** | **Embedded refactor.** Removed all `std::string` usage across the entire library, examples, and test suite. All strings use compile-time fixed `char[]` buffers with explicit length tracking for binary OctetTypes. Added `LIBRARY_VERSION_*` defines in `defs.h`. Eliminated three `malloc` calls from example sketches. Estimated ~3–8 KB Flash savings on ESP8266 and zero heap fragmentation from string handling. All callbacks that previously took/returned `std::string` now take/return `const char*` or bounded `char**`. |
| 2.1.0   | (Previous) API cleanups, ESP-01 / ESP8266 support.                  |
| 2.0.x   | Rewrite from Arduino_SNMP v1. RFC-compliant SNMPv2c engine.         |
| 1.x     | Original Arduino_SNMP project.                                      |

Pull requests/comments are welcome