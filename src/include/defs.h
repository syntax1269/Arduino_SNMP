/* -------------------------------------------------------------------------- *
 * USER-TUNEABLE COMPILE-TIME CONSTANTS (override BEFORE including this file)
 *
 *   To change any of these values from library default, do this in your .ino:
 *
 *       #define SNMP_MAX_COMPLEX_CHILDREN        8
 *       #define SNMP_POOL_ASN_OBJECTS           32
 *       // ... etc ...
 *       #include <SNMP_Agent.h>
 *
 *   Because each constant below is guarded with `#ifndef ... #endif`, the
 *   user's earlier #define in the sketch (or via -D on the compiler command
 *   line, or via build_flags in platformio.ini) will be used instead of the
 *   library default listed here.
 *
 *   Good targets to reduce for ESP-01 1MB / 80KB RAM sensor builds:
 *       SNMP_POOL_ASN_OBJECTS      64 -> 32    (-24,576 B BSS)
 *       SNMP_POOL_VARBIND_OBJECTS  32 -> 12
 *       SNMP_MAX_CALLBACKS_PER_AGENT 64 -> 16
 *       SNMP_MAX_VARBINDS          16 -> 4
 *       SNMP_MAX_COMPLEX_CHILDREN  16 -> 8
 *       SNMP_MAX_TRAPS_INFLIGHT     8 -> 4
 * -------------------------------------------------------------------------- */

#ifndef SNMP_DEFS_h
#define SNMP_DEFS_h

#include <stdint.h>

#ifndef DEBUG
    #define DEBUG           0       /* 0  or  1  or  2 */
#endif

#define LIBRARY_VERSION_MAJOR 3
#define LIBRARY_VERSION_MINOR 1
#define LIBRARY_VERSION_PATCH 23
#define LIBRARY_VERSION "3.1.23"

typedef enum SNMP_ERROR_RESPONSE {
    SNMP_NO_UDP = -10,
    SNMP_REQUEST_TOO_LARGE = -5,
    SNMP_REQUEST_INVALID = -4,
    SNMP_REQUEST_INVALID_COMMUNITY = -3,
    SNMP_FAILED_SERIALISATION = -2,
    SNMP_GENERIC_ERROR = -1,
    SNMP_NO_PACKET = 0,
    SNMP_NO_ERROR = 1,
    SNMP_GET_OCCURRED = 2,
    SNMP_GETNEXT_OCCURRED = 3,
    SNMP_GETBULK_OCCURRED = 4,
    SNMP_SET_OCCURRED = 5,
    SNMP_ERROR_PACKET_SENT = 6, // A packet indicating that an error occurred was sent
    SNMP_INFORM_RESPONSE_OCCURRED = 7,
    SNMP_UNKNOWN_PDU_OCCURRED = 8
} SNMP_ERROR_RESPONSE;

typedef unsigned long snmp_request_id_t;

#define INVALID_SNMP_REQUEST_ID 0


typedef enum SnmpVersionEnum {
    SNMP_VERSION_1,
    SNMP_VERSION_2C,
    SNMP_VERSION_MAX
} SNMP_VERSION;

typedef enum {
     SNMP_PERM_NONE,
     SNMP_PERM_READ_ONLY,
     SNMP_PERM_READ_WRITE
} SNMP_PERMISSION;

extern const char* SNMP_TAG;


/* -------------------------------------------------------------------------- *
 * PLATFORM AUTO-TUNE (ESP8266)
 *
 *   On ESP8266 (80 KB DRAM typical, hard 64-80 KB BSS budget once WiFi
 *   libraries are loaded) the library defaults below would otherwise
 *   overflow RAM for any sketch that also uses WiFi + a filesystem.
 *   Enable ESP8266_TINY to swap in conservative BSS values (~ -28 KB
 *   vs generic defaults).  Set SNMP_SKIP_ESP8266_AUTOTUNE to 1 before
 *   including this file if you want full control on ESP8266 too.
 * -------------------------------------------------------------------------- */
#if defined(ESP8266) && !defined(SNMP_SKIP_ESP8266_AUTOTUNE)
    #define _SNMP_ESP8266_TINY  1
#endif

#ifndef MAX_SNMP_PACKET_LENGTH
  #ifdef _SNMP_ESP8266_TINY
    #define MAX_SNMP_PACKET_LENGTH  1024
  #else
    #define MAX_SNMP_PACKET_LENGTH  1400
  #endif
#endif
#ifndef OCTET_TYPE_MAX_LENGTH
  /* OctetType/OpaqueType internal fixed buffer length.  256 B covers 99% of
     MIB string columns (sysDescr is typically ~120 B, ifDescr ~64 B, etc.).
     Sketch-side override: #define OCTET_TYPE_MAX_LENGTH 500 BEFORE including
     SNMP_Agent.h for endpoints that need to serve 500 B MIB strings. */
  #define OCTET_TYPE_MAX_LENGTH      256
#endif

#ifndef SNMP_MAX_COMMUNITY_LEN
    /* RFC 3418 §2.6 SNMPv2c community strings are traditionally short tokens;
       32 B is the practical RFC ceiling.  Users who need legacy long strings
       (e.g. v3-style views over v2c) can #define SNMP_MAX_COMMUNITY_LEN 64
       BEFORE including SNMP_Agent.h — value is guarded so sketch-side wins. */
    #define SNMP_MAX_COMMUNITY_LEN   32
#endif
#ifndef SNMP_MAX_OID_STR_LEN
  #ifdef _SNMP_ESP8266_TINY
    #define SNMP_MAX_OID_STR_LEN     192
  #else
    #define SNMP_MAX_OID_STR_LEN     256
  #endif
#endif
#ifndef SNMP_MAX_STRING_LEN
    #define SNMP_MAX_STRING_LEN      OCTET_TYPE_MAX_LENGTH
#endif

#ifndef SNMP_MAX_OID_SUBIDENTIFIERS
    #define SNMP_MAX_OID_SUBIDENTIFIERS    32   /* BER-encoded OID: max 32 sub-IDs  =>  32 bytes w/ header byte.
                                                   Real-world: "1.3.6.1.4.1.318.1.1.27.3.2.7.1.7.2.1001" = 17 sub-IDs.
                                                   32 is 2x safe.  Used to size OIDType::data fixed buffer. */
#endif
#ifndef SNMP_MAX_COMPLEX_CHILDREN
  #ifdef _SNMP_ESP8266_TINY
    #define SNMP_MAX_COMPLEX_CHILDREN      16
  #else
    #define SNMP_MAX_COMPLEX_CHILDREN      24   /* Maximum children inside a single BER ComplexType (STRUCTURE / PDU / VarBindList).
                                                   Real-world GetResponses contain < 8 VarBinds; 16 covers bulkwalk default=10 + some slack.
                                                   24 (default) / 16 (ESP8266_TINY) safely accommodates GetBulk + walk response envelopes.
                                                   Used to size ComplexType::values[] fixed array. */
  #endif
#endif
#ifndef SNMP_MAX_VARBINDS
  #ifdef _SNMP_ESP8266_TINY
    #define SNMP_MAX_VARBINDS               6
  #else
    #define SNMP_MAX_VARBINDS              16   /* Maximum VarBinds per request / response.  Typical < 8; SNMP GetBulk default is 10.
                                                   Used to size SNMPPacket::varbindList[] fixed array. */
  #endif
#endif
#ifndef SNMP_MAX_CALLBACKS_PER_AGENT
  #ifdef _SNMP_ESP8266_TINY
    #define SNMP_MAX_CALLBACKS_PER_AGENT   24
  #else
    #define SNMP_MAX_CALLBACKS_PER_AGENT   64   /* Maximum OID handlers registered per SNMPAgent instance.  Large sensor deployments
                                                   have ~32; 64 covers a device exposing every numeric column of a 30-row table. */
  #endif
#endif
#ifndef SNMP_MAX_AGENTS
    #define SNMP_MAX_AGENTS                  2   /* Concurrent SNMPAgent instances.  ESP-01 almost always 1; 2 allows dual-interface. */
#endif
#ifndef SNMP_MAX_UDP_PER_AGENT
    #define SNMP_MAX_UDP_PER_AGENT           2   /* Maximum UDP interfaces bound per single SNMPAgent (e.g. WiFi + Ethernet on ESP32). */
#endif
#ifndef SNMP_MAX_TRAPS_INFLIGHT
  #ifdef _SNMP_ESP8266_TINY
    #define SNMP_MAX_TRAPS_INFLIGHT          4
  #else
    #define SNMP_MAX_TRAPS_INFLIGHT          8   /* Maximum outstanding InformItem messages queued for retry.  ESP-01 real-world hard cap. */
  #endif
#endif
#ifndef SNMP_MAX_CALLBACKS_PER_TRAP
  #ifdef _SNMP_ESP8266_TINY
    #define SNMP_MAX_CALLBACKS_PER_TRAP      8
  #else
    #define SNMP_MAX_CALLBACKS_PER_TRAP     16   /* Maximum OIDs / VBs in a single SNMPTrap.  Typical trap payload < 8 VBs. */
  #endif
#endif
#ifndef SNMP_POOL_ASN_OBJECTS
  #ifdef _SNMP_ESP8266_TINY
    #define SNMP_POOL_ASN_OBJECTS           76
  #else
    #define SNMP_POOL_ASN_OBJECTS           80   /* Global placement-pool slot count for BER_CONTAINER-derived ASN objects.
                                                   Headroom budget (per-tick transient, reset by ASNPool::resetAll() at each loop()):
                                                   - 14 inbound BER decode tree (v2c PDU with 6 VBs)
                                                   - 10 response-copy duplicates (version/community/reqid/clones)
                                                   - 30 response encode tree (bulkwalk/10 VB responses + envelope)
                                                   - 10 trap-send scratch during auto-trap / coldStart
                                                   76 (ESP8266_TINY, tradeoff with WiFi heap) / 80 (default) covers the
                                                   23-handler hwtest sketch with GET/GETNEXT/GETBULK/SET/auto-traps all working. */
  #endif
#endif
#ifndef SNMP_POOL_VARBIND_OBJECTS
  #ifdef _SNMP_ESP8266_TINY
    #define SNMP_POOL_VARBIND_OBJECTS       24
  #else
    #define SNMP_POOL_VARBIND_OBJECTS       32   /* Global placement-pool slot count for transient VarBind objects. */
  #endif
#endif


/* SNMP_POOL_SLOT_SIZE is the raw payload size of each ASNPool::Slot.
 * Each slot must fit the LARGEST concrete BER subclass we instantiate.
 * v3.1.25: right-sized to MEASURED sizeof() of the largest container
 * (host sizeof probe, Xtensa-compatible layout):
 *   TINY   config: OctetType = 288 B (_value[256] + base)  -> slot 288
 *   default config: OIDType/SortableOIDType = 312 B         -> slot 312
 * The previous 512/640 values predated the v3.1.9 sortingMap removal
 * and padded every slot with 224/328 dead bytes (~44% of the arena —
 * 17 KB of the ESP-01's 39.5 KB pool was padding). static_assert
 * guards in BER.h now pin every container <= slot so this can never
 * silently go stale again; a sketch raising OCTET_TYPE_MAX_LENGTH or
 * SNMP_MAX_OID_STR_LEN gets a compile error instead of a runtime
 * placement failure. */
#ifndef SNMP_POOL_SLOT_SIZE
  #ifdef _SNMP_ESP8266_TINY
    #define SNMP_POOL_SLOT_SIZE 288
  #else
    #define SNMP_POOL_SLOT_SIZE 312
  #endif
#endif

/* =====================================================================
 *  COMPILE-TIME POOL FLOOR GUARDS
 *  ---------------------------------------------------------------------
 *  Minimum-safe values empirically proven on ESP-01 (ESP8266EX, 80 MHz,
 *  80 KB RAM, CH340 USB).  Anything BELOW these caused a NULL-pointer
 *  dereference crash (Exception 28 on Xtensa) the INSTANT the first UDP
 *  GetRequest arrived on port 161.  Root cause: ASNPool::alloc() or
 *  VarBind pool returned nullptr on exhaustion, or callbacks[] array
 *  overflow (23 handlers into 20-slot array) silently corrupted the
 *  adjacent UDP pointer/cbCount members → SNMP socket went deaf.
 *
 *  Full incident report:  _hwtest_esp01/HARDWARE_TEST_REPORT.md §2 §9
 *  =====================================================================
 */
#if defined(__cplusplus)

    #define SNMP_FLOOR_MSG_HEAD \
        "\n[SNMP_Agent compile guard] Pool cap below empirically-proven minimum." \
        "\n  Smaller values cause NULL allocation or silent array overflow on first " \
        "\n  inbound SNMP packet → ESP8266 Exception 28 / hard fault." \
        "\n  Full analysis: _hwtest_esp01/HARDWARE_TEST_REPORT.md section 2."

    static_assert(SNMP_MAX_CALLBACKS_PER_AGENT >= 24,
        SNMP_FLOOR_MSG_HEAD
        "\n  -> SNMP_MAX_CALLBACKS_PER_AGENT must be >= 24 (test sketch registered 23"
        "\n     = 7 RFC1213 + 14 test leaves + 2 trap regs; anything <23 overwrites udp[]."
        "\n     Suggested: 32 (or leave default, which is 24 for ESP8266_TINY).");

    static_assert(SNMP_POOL_ASN_OBJECTS >= 24,
        SNMP_FLOOR_MSG_HEAD
        "\n  -> SNMP_POOL_ASN_OBJECTS must be >= 24."
        "\n     With 16: GetRequest BER parse ran dry before GetResponse was encoded,"
        "\n     crashing at excvaddr=0x2C (nullptr + 44 bytes) inside handlePacket().");

    static_assert(SNMP_POOL_VARBIND_OBJECTS >= 8,
        SNMP_FLOOR_MSG_HEAD
        "\n  -> SNMP_POOL_VARBIND_OBJECTS must be >= 8."
        "\n     With 4: request + response VB lifetimes overlap → NULL deref.");

    static_assert(SNMP_MAX_COMPLEX_CHILDREN >= 6,
        SNMP_FLOOR_MSG_HEAD
        "\n  -> SNMP_MAX_COMPLEX_CHILDREN must be >= 6."
        "\n     Absolute minimum for a v2c PDU wrapper (4 children + vb_list + headroom).");

    static_assert(SNMP_MAX_VARBINDS >= 4,
        SNMP_FLOOR_MSG_HEAD
        "\n  -> SNMP_MAX_VARBINDS must be >= 4."
        "\n     (ESP8266_TINY default is 6; 4 is rock-bottom for a GET pair.)");

    static_assert(MAX_SNMP_PACKET_LENGTH >= 512,
        SNMP_FLOOR_MSG_HEAD
        "\n  -> MAX_SNMP_PACKET_LENGTH must be >= 512 bytes."
        "\n     RFC 3417 sets 484 as the historic floor; 512 safely covers snmpwalk"
        "\n     responses with 12 varbinds plus PDU headers.  ESP8266 default 1024.");

    static_assert(OCTET_TYPE_MAX_LENGTH >= 64,
        SNMP_FLOOR_MSG_HEAD
        "\n  -> OCTET_TYPE_MAX_LENGTH must be >= 64."
        "\n     sysContact/Name/Location in RFC1213 use 64-byte buffers minimum.");

    static_assert(SNMP_MAX_OID_SUBIDENTIFIERS >= 16,
        SNMP_FLOOR_MSG_HEAD
        "\n  -> SNMP_MAX_OID_SUBIDENTIFIERS must be >= 16."
        "\n     enterprises.99999.1.14 = 12 sub-IDs.  16 is 33% headroom minimum.");

    #undef SNMP_FLOOR_MSG_HEAD

#endif

#define SNMP_ERROR_OK 1

#define SNMP_PACKET_PARSE_ERROR_OFFSET -20
#define SNMP_BUFFER_PARSE_ERROR_OFFSET -10
#define SNMP_BUFFER_ENCODE_ERROR_OFFSET -30

typedef enum ERROR_STATUS_WITH_VALUE {
    // V1 Errors
    NO_ERROR = 0,
    TOO_BIG = 1,
    NO_SUCH_NAME = 2,
    BAD_VALUE = 3,
    READ_ONLY = 4,
    GEN_ERR = 5,
        
    // V2c Errors
    NO_ACCESS = 6,
    WRONG_TYPE = 7,
    WRONG_LENGTH = 8,
    WRONG_ENCODING = 9,
    WRONG_VALUE = 10,
    NO_CREATION = 11,
    INCONSISTENT_VALUE = 12,
    RESOURCE_UNAVAILABLE = 13,
    COMMIT_FAILED = 14,
    UNDO_FAILED = 15,
    AUTHORIZATION_ERROR = 16,
    NOT_WRITABLE = 17,
    INCONSISTENT_NAME = 18
} SNMP_ERROR_STATUS;

#define SNMP_V1_MAX_ERROR GEN_ERR

#define SNMP_ERROR_VERSION_CTRL(error, version) (((version) == SNMP_VERSION_1 && (error) > SNMP_V1_MAX_ERROR) ? SNMP_V1_MAX_ERROR : (error))

// Used for situations where in V2 an error exists but in V1 a less-specific error exists that isn't GEN_ERR
#define SNMP_ERROR_VERSION_CTRL_DEF(error, version, elseError) (((version) == SNMP_VERSION_1 && (error) > SNMP_V1_MAX_ERROR) ? SNMP_ERROR_VERSION_CTRL(elseError, version) : error)

// RFC1213 OIDs
#define RFC1213_OID_sysDescr            (".1.3.6.1.2.1.1.1.0")
#define RFC1213_OID_sysObjectID         (".1.3.6.1.2.1.1.2.0")
#define RFC1213_OID_sysUpTime           (".1.3.6.1.2.1.1.3.0")
#define SNMPv2_SNMPTRAP_OID_0           (".1.3.6.1.6.3.1.1.4.1.0")   /* RFC 3416 §4.2.6/§4.2.7: mandatory varbind #2 NAME in SNMPv2c Trap/Inform.
                                                                         Value is the NOTIFICATION-TYPE OID supplied via SNMPTrap::setTrapOID() */
#define RFC1213_OID_sysContact          (".1.3.6.1.2.1.1.4.0")
#define RFC1213_OID_sysName             (".1.3.6.1.2.1.1.5.0")
#define RFC1213_OID_sysLocation         (".1.3.6.1.2.1.1.6.0")
#define RFC1213_OID_sysServices         (".1.3.6.1.2.1.1.7.0")

typedef struct RFC1213SystemStruct {
        char*           sysDescr;               /* .1.3.6.1.2.1.1.1.0   Read-only   */
        char*           sysObjectID;            /* .1.3.6.1.2.1.1.2.0   Read-only   */
        uint32_t        sysUpTime;              /* .1.3.6.1.2.1.1.3.0   Read-only   */
        char*           sysContact;             /* .1.3.6.1.2.1.1.4.0   Read-only   */
        char*           sysName;                /* .1.3.6.1.2.1.1.5.0   Read-only   */
        char*           sysLocation;            /* .1.3.6.1.2.1.1.6.0   Read-Write  */
        int32_t         sysServices;            /* .1.3.6.1.2.1.1.7.0   Read-only   */
    } RFC1213_list;

// DEBUG
#if defined(COMPILING_TESTS)
    #define _LOGD(...)          printf(__VA_ARGS__)
    #define _LOGI(...)          printf(__VA_ARGS__)
    #define _LOGW(...)          printf(__VA_ARGS__)
    #define _LOGE(...)          printf(__VA_ARGS__)
#elif defined(ESP32)
    #include <esp_log.h>
    
    #define _LOGD(...)          ESP_LOGD(SNMP_TAG, __VA_ARGS__)
    #define _LOGI(...)          ESP_LOGI(SNMP_TAG, __VA_ARGS__)
    #define _LOGW(...)          ESP_LOGW(SNMP_TAG, __VA_ARGS__)
    #define _LOGE(...)          ESP_LOGE(SNMP_TAG, __VA_ARGS__)
#else
    #define _LOGD(...)          printf(__VA_ARGS__)
    #define _LOGI(...)          printf(__VA_ARGS__)
    #define _LOGW(...)          printf(__VA_ARGS__)
    #define _LOGE(...)          printf(__VA_ARGS__)
#endif

// ----
#if (DEBUG ==1)
    #define SNMP_LOGD           _LOGD
    #define SNMP_LOGI           _LOGI
    #define SNMP_LOGW           _LOGW
    #define SNMP_LOGE           _LOGE
#elif (DEBUG ==2)
    #define SNMP_LOGD(...)
    #define SNMP_LOGI           _LOGI
    #define SNMP_LOGW           _LOGW
    #define SNMP_LOGE           _LOGE
#else
    #define SNMP_LOGD(...)
    #define SNMP_LOGI(...)
    #define SNMP_LOGW(...)
    #define SNMP_LOGE(...)
#endif

#endif
