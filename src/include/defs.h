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
#define LIBRARY_VERSION_PATCH 5
#define LIBRARY_VERSION "3.1.5"

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
    #define SNMP_MAX_COMMUNITY_LEN   64
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
    #define SNMP_MAX_COMPLEX_CHILDREN       8
  #else
    #define SNMP_MAX_COMPLEX_CHILDREN      16   /* Maximum children inside a single BER ComplexType (STRUCTURE / PDU / VarBindList).
                                                   Real-world GetResponses contain < 8 VarBinds; 16 covers bulkwalk default=10 + some slack.
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
    #define SNMP_POOL_ASN_OBJECTS           24
  #else
    #define SNMP_POOL_ASN_OBJECTS           32   /* Global placement-pool slot count for BER_CONTAINER-derived ASN objects.
                                                   Upper bound: 4 traps in flight × 8 VBs each + ~16 for request decode = 48;
                                                   32 (default) or 24 (ESP8266 tiny) both allow concurrent GET + response safely. */
  #endif
#endif
#ifndef SNMP_POOL_VARBIND_OBJECTS
  #ifdef _SNMP_ESP8266_TINY
    #define SNMP_POOL_VARBIND_OBJECTS        8
  #else
    #define SNMP_POOL_VARBIND_OBJECTS       12   /* Global placement-pool slot count for transient VarBind objects. */
  #endif
#endif

/* SNMP_POOL_SLOT_SIZE is the raw payload size of each ASNPool::Slot.
 * Because each slot must fit the LARGEST concrete BER subclass we
 * instantiate (SortableOIDType 576B + vtable ptr + tail bytes) the
 * default is 768, wasting ~192 B per slot.  ESP8266 TINY sets the
 * slot to 640 B (exactly SortableOIDType on 64-bit) which still
 * accommodates SortableOIDType on 32-bit Xtensa (576 B). */
#ifndef SNMP_POOL_SLOT_SIZE
  #ifdef _SNMP_ESP8266_TINY
    #define SNMP_POOL_SLOT_SIZE 640
  #else
    #define SNMP_POOL_SLOT_SIZE 768
  #endif
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
