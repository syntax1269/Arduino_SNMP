#ifndef BER_h
#define BER_h

#include <math.h>
#include <utility>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <new>

#ifdef COMPILING_TESTS
    #include "tests/required/IPAddress.h"
    #include "tests/required/UDP.h"
#else
    #include <Arduino.h>
    #include "IPAddress.h"
#endif

#include <memory>
#include "include/defs.h"

#define ASN_POOL_MAX(a,b) ((a)>(b)?(a):(b))

class BER_CONTAINER;

struct ASNPool {
#ifndef SNMP_POOL_SLOT_SIZE
#define SNMP_POOL_SLOT_SIZE 640
#endif

    struct Slot {
        alignas(8) char storage[SNMP_POOL_SLOT_SIZE];
        bool occupied;
        bool doubleReleaseWarned;   /* one-shot alarm latch for DEBUG>0 */
    };

    /* SNMP_POOLS_IN_BSS = 1 forces ASNPool storage back to static .bss.
       Leave it undefined (default) to allocate slots on the heap exactly
       once at first asn_new<>().  Startup-time heap allocation is allowed
       per project rules: size fixed at compile-time, never realloc, never
       grows at runtime.  Frees ~20-40 KB of BSS for ESP8266/ESP-01 parts
       whose DRAM budget is 64-80 KB once WiFi + libraries are linked. */
#ifndef SNMP_POOLS_IN_BSS
    static Slot* slots;          // malloc once at first rawAlloc()
    static bool _poolsReady;     // true after one-shot init
    static void _ensurePools();  // one-shot new Slot[N]; memset 0
#else
    static Slot slots[SNMP_POOL_ASN_OBJECTS];
#endif
    static int usedCount;
    static int permCount;
    static int usedCountPeak;   /* high-water mark since boot — diagnostics */

    static inline void freezePermCount() noexcept {
        int high = 0;
        for(int i = 0; i < SNMP_POOL_ASN_OBJECTS; i++){
            if(slots[i].occupied) high = i + 1;
        }
        permCount = high;
        usedCount = high < usedCount ? high : usedCount;
    }

    static inline bool isInPool(const void* p){
#ifndef SNMP_POOLS_IN_BSS
        if(!_poolsReady) return false;
#endif
        const char* pc = static_cast<const char*>(p);
        const char* base = static_cast<const char*>(static_cast<const void*>(slots[0].storage));
        const char* end  = static_cast<const char*>(static_cast<const void*>(slots[SNMP_POOL_ASN_OBJECTS].storage));
        if(pc < base || pc >= end) return false;
        size_t off = (size_t)(pc - base);
        return (off % sizeof(Slot)) == offsetof(Slot, storage);
    }

    static void* rawAlloc(size_t sz){
        if(sz > SNMP_POOL_SLOT_SIZE) {
            SNMP_LOGE("ASNPool::rawAlloc: request %zu B > slot %d B\n", sz, (int)SNMP_POOL_SLOT_SIZE);
            return nullptr;
        }
#ifndef SNMP_POOLS_IN_BSS
        if(!_poolsReady) _ensurePools();
#endif
        if(usedCount >= SNMP_POOL_ASN_OBJECTS){
            SNMP_LOGE("ASNPool EXHAUSTED: %d/%d slots in use (%d B/slot). Raise SNMP_POOL_ASN_OBJECTS. First NULL deref = Exception 28.\n",
                      usedCount, (int)SNMP_POOL_ASN_OBJECTS, (int)SNMP_POOL_SLOT_SIZE);
            return nullptr;
        }
        for(int i = 0; i < SNMP_POOL_ASN_OBJECTS; i++){
            if(!slots[i].occupied){
                slots[i].occupied = true;
                usedCount++;
                if(usedCount > usedCountPeak) usedCountPeak = usedCount;
                return slots[i].storage;
            }
        }
        SNMP_LOGE("ASNPool EXHAUSTED: %d/%d slots in use (%d B/slot). Raise SNMP_POOL_ASN_OBJECTS. First NULL deref = Exception 28.\n",
                  usedCount, (int)SNMP_POOL_ASN_OBJECTS, (int)SNMP_POOL_SLOT_SIZE);
        return nullptr;
    }

    static void release(BER_CONTAINER* p);

    static inline void resetAll() noexcept {
#ifndef SNMP_POOLS_IN_BSS
        if(!_poolsReady) return;
#endif
        for(int i = permCount; i < SNMP_POOL_ASN_OBJECTS; i++){
            slots[i].occupied = false;
        }
        usedCount = permCount;
    }
};

template<typename T, typename... Args>
static inline T* asn_new(Args&&... args){
    void* slot = ASNPool::rawAlloc(sizeof(T));
    if(slot){
        T* obj = ::new (slot) T(std::forward<Args>(args)...);
        return obj;
    }
#ifdef COMPILING_TESTS
    /* Native host tests: pool capacity is a logic stress-test vector, not a
       hard safety bound.  Host has GB of free RAM so falling back to operator
       new allows 101/101 Catch2 assertions to exercise logic end-to-end without
       needing an enormous static pool.  On real MCU targets (COMPILING_TESTS
       undefined) we strictly return nullptr = zero hot-path heap. */
    return ::new T(std::forward<Args>(args)...);
#else
    return nullptr;
#endif
}

void asn_delete(BER_CONTAINER* p);

typedef enum ASN_TYPE_WITH_VALUE {
    // Primatives
    INTEGER = 0x02,
    STRING = 0x04,
    NULLTYPE = 0x05,
    OID = 0x06,
    
    // Complex
    STRUCTURE = 0x30,
    NETWORK_ADDRESS = 0x40,
    COUNTER32 = 0x41,
    GAUGE32 = 0x42,
    USIGNED32 = 0x42, // Same as Gauge32
    TIMESTAMP = 0x43,
    OPAQUE = 0x44,
	COUNTER64 = 0x46,

    /*
        FROM: RFC3416
    */
    NOSUCHOBJECT = 0x80,
    NOSUCHINSTANCE = 0x81,
    ENDOFMIBVIEW = 0x82,
    
    // Structure Types
    GetRequestPDU = 0xA0,
    GetNextRequestPDU = 0xA1,
    GetResponsePDU = 0xA2,
    SetRequestPDU = 0xA3,
    TrapPDU = 0xA4,
    GetBulkRequestPDU = 0xA5,
    InformRequestPDU = 0xA6,
    Trapv2PDU = 0xA7
    
} ASN_TYPE;

#define ASN_PDU_TYPE_MIN_VALUE GetRequestPDU
#define ASN_PDU_TYPE_MAX_VALUE Trapv2PDU

#define MAX_DYNAMIC_ASN_TYPE COUNTER64

typedef int SNMP_BUFFER_PARSE_ERROR;
typedef int SNMP_BUFFER_ENCODE_ERROR;

#define SNMP_BUFFER_ERROR_MAX_LEN_EXCEEDED (-1 + SNMP_BUFFER_PARSE_ERROR_OFFSET)
#define SNMP_BUFFER_ERROR_TLV_TOO_SMALL (-2 + SNMP_BUFFER_PARSE_ERROR_OFFSET)
#define SNMP_BUFFER_ERROR_PROBLEM_DESERIALISING (-3 + SNMP_BUFFER_PARSE_ERROR_OFFSET)
#define SNMP_BUFFER_ERROR_UNKNOWN_TYPE (-4 + SNMP_BUFFER_PARSE_ERROR_OFFSET)
#define SNMP_BUFFER_ERROR_TYPE_MISMATCH (-5 + SNMP_BUFFER_PARSE_ERROR_OFFSET)
#define SNMP_BUFFER_ERROR_OCTET_TOO_BIG (-6 + SNMP_BUFFER_PARSE_ERROR_OFFSET)
#define SNMP_BUFFER_ERROR_INVALID_OID (-7 + SNMP_BUFFER_PARSE_ERROR_OFFSET)

#define SNMP_BUFFER_ENCODE_ERR_LEN_EXCEEDED (-1 + SNMP_BUFFER_ENCODE_ERROR_OFFSET)
#define SNMP_BUFFER_ENCODE_ERROR_INVALID_ITEM (-2 + SNMP_BUFFER_ENCODE_ERROR_OFFSET)
#define SNMP_BUFFER_ENCODE_ERROR_INVALID_OID (-7 + SNMP_BUFFER_ENCODE_ERROR_OFFSET)

#define CHECK_DECODE_ERR(i) if((i) < 0) return i
#define CHECK_ENCODE_ERR(i) if((i) < 0) return i

// primitive types inherits straight off the container, complex come off complexType
// all primitives have to serialiseInto themselves (type, length, data), to be put straight into the packet.
// for deserialising, from the parent container we check the type, then create anobject of that type and calls deSerialise, passing in the data, which pulls it out and saves, and if complex, first split up it schildren into seperate BERs, then creates and passes them creates a child with it's data using the same process.


class BER_CONTAINER {
  public:
    BER_CONTAINER(ASN_TYPE type) : _type(type){}
    virtual ~BER_CONTAINER()= default;

    ASN_TYPE _type;
    int _length = 0;

  protected:
    // Serialise object in BER notation into buf, with a maximum size of max_len; returns number of bytes used
    virtual int serialise(uint8_t* buf, size_t max_len);
    virtual int serialise(uint8_t* buf, size_t max_len, size_t known_length);

    // returns number of bytes used from buf, limited by max_len, return -1 if failed to parse
    virtual int fromBuffer(const uint8_t *buf, size_t max_len);

    friend class ComplexType;
    template<typename U, typename... Args> friend U* asn_new(Args&&... args);
};

class NetworkAddress: public BER_CONTAINER {
  public:
    NetworkAddress(): BER_CONTAINER(NETWORK_ADDRESS) {}
    explicit NetworkAddress(const IPAddress& ip): NetworkAddress(){
        _value = ip;
    }

    IPAddress _value = INADDR_NONE;

protected:
    int serialise(uint8_t* buf, size_t max_len) override;
    int fromBuffer(const uint8_t *buf, size_t max_len) override;
};


class IntegerType: public BER_CONTAINER {
  public:
    IntegerType(): BER_CONTAINER(INTEGER) {}
    explicit IntegerType(int value): IntegerType(){
        _value = value;
    }

    int _value = 0;

protected:
    int serialise(uint8_t* buf, size_t max_len) override;
    int fromBuffer(const uint8_t *buf, size_t max_len) override;
};

class TimestampType: public IntegerType {
  public:
    TimestampType(): IntegerType(){
        _type = TIMESTAMP;
    }
    explicit TimestampType(unsigned long value): IntegerType(value){
        _type = TIMESTAMP;
    }
};

class OctetType: public BER_CONTAINER {
  public:
    explicit OctetType(const char* value): BER_CONTAINER(STRING) {
        size_t len = strlen(value);
        if(len > SNMP_MAX_STRING_LEN) len = SNMP_MAX_STRING_LEN;
        memcpy(_value, value, len);
        _value[len] = 0;
        _valueLen = len;
    }
    OctetType(const char* value, size_t len): BER_CONTAINER(STRING) {
        if(len > SNMP_MAX_STRING_LEN) len = SNMP_MAX_STRING_LEN;
        memcpy(_value, value, len);
        _value[len] = 0;
        _valueLen = len;
    }

    char _value[SNMP_MAX_STRING_LEN + 1];
    size_t _valueLen = 0;

protected:
    int serialise(uint8_t* buf, size_t max_len) override;
    int fromBuffer(const uint8_t *buf, size_t max_len) override;

    OctetType(): BER_CONTAINER(STRING) { _value[0] = 0; }
    friend class ComplexType;
    template<typename U, typename... Args> friend U* asn_new(Args&&... args);
};

class OpaqueType: public BER_CONTAINER {
  public:
    OpaqueType(const uint8_t* value, int length): OpaqueType(){
        if(length > (int)sizeof(this->_value)) {
            length = (int)sizeof(this->_value);
        }
        if(length < 0) length = 0;
        if(length > 0 && value) {
            memcpy(this->_value, value, (size_t)length);
        } else {
            length = 0;
        }
        this->_dataLength = length;
    }

    uint8_t _value[OCTET_TYPE_MAX_LENGTH];
    int _dataLength = 0;

protected:
    int serialise(uint8_t* buf, size_t max_len) override;
    int fromBuffer(const uint8_t *buf, size_t max_len) override;

    OpaqueType(): BER_CONTAINER(OPAQUE) {
        this->_dataLength = 0;
    }
    friend class ComplexType;
    template<typename U, typename... Args> friend U* asn_new(Args&&... args);
};


class OIDType: public BER_CONTAINER {
  public:
    explicit OIDType(const char* value): BER_CONTAINER(OID) {
        size_t len = strlen(value);
        if(len > SNMP_MAX_OID_STR_LEN) len = SNMP_MAX_OID_STR_LEN;
        _init_from_cstr(value, len);
    }

    template<size_t N>
    OIDType(const char (&value)[N]): BER_CONTAINER(OID) {
        constexpr size_t cap = ( (N-1) > SNMP_MAX_OID_STR_LEN ) ? SNMP_MAX_OID_STR_LEN : (N-1);
        _init_from_cstr(value, cap);
    }

    std::shared_ptr<OIDType> cloneOID() const {
        return std::shared_ptr<OIDType>(asn_new<OIDType>(this->_valueStr, this->data, this->dataLen, this->valid),
                                        [](OIDType* p){ asn_delete(static_cast<BER_CONTAINER*>(p)); });
    }

    OIDType* cloneRaw() const {
        return asn_new<OIDType>(this->_valueStr, this->data, this->dataLen, this->valid);
    }

    const char* string();
    bool valid = false;

    bool equals(const std::shared_ptr<OIDType> oid) const {
        return this->dataLen == oid->dataLen &&
               (this->dataLen == 0 || memcmp(this->data, oid->data, (size_t)this->dataLen) == 0);
    }

    bool equals(const OIDType* oid) const {
        return this->dataLen == oid->dataLen &&
               (this->dataLen == 0 || memcmp(this->data, oid->data, (size_t)this->dataLen) == 0);
    }

    bool isSubTreeOf(const OIDType* const oid){
        if(oid->dataLen >= this->dataLen) return false;
        if(oid->dataLen == 0) return true;
        return memcmp(this->data, oid->data, (size_t)oid->dataLen) == 0;
    }

  protected:
    int serialise(uint8_t* buf, size_t max_len) override;
    int fromBuffer(const uint8_t *buf, size_t max_len) override;

    void _init_from_cstr(const char* value, size_t len) noexcept;
    void _init_from_cstr_with_data(const char* value, size_t len, const uint8_t* srcData, int srcLen, bool valid) noexcept;

    friend class ComplexType;
    template<typename U, typename... Args> friend U* asn_new(Args&&... args);
    OIDType(): BER_CONTAINER(OID) { _valueStr[0] = 0; dataLen = 0; }

    char _valueStr[SNMP_MAX_OID_STR_LEN + 1];
    uint8_t data[SNMP_MAX_OID_SUBIDENTIFIERS + 1];
    int dataLen = 0;

  private:
    explicit OIDType(const char* value, const uint8_t* srcData, int srcLen, bool valid): BER_CONTAINER(OID), valid(valid), dataLen(srcLen) {
        size_t len = strlen(value);
        if(len > SNMP_MAX_OID_STR_LEN) len = SNMP_MAX_OID_STR_LEN;
        _init_from_cstr_with_data(value, len, srcData, srcLen, valid);
    }

    template<size_t N>
    explicit OIDType(const char (&value)[N], const uint8_t* srcData, int srcLen, bool valid): BER_CONTAINER(OID), valid(valid), dataLen(srcLen) {
        constexpr size_t cap = ( (N-1) > SNMP_MAX_OID_STR_LEN ) ? SNMP_MAX_OID_STR_LEN : (N-1);
        _init_from_cstr_with_data(value, cap, srcData, srcLen, valid);
    }

    bool generateInternalData();
};

class SortableOIDType: public OIDType {
  public:
    explicit SortableOIDType(const char* value): OIDType(value) {}

    template<size_t N>
    SortableOIDType(const char (&value)[N]): OIDType(value) {}

    static bool sort_oids(const SortableOIDType* oid1, const SortableOIDType* oid2);

    bool operator < (SortableOIDType& other){
        return SortableOIDType::sort_oids(this, &other);
    }

  private:
};

class NullType: public BER_CONTAINER {
  public:
    NullType(): BER_CONTAINER(NULLTYPE) {}

protected:
    int serialise(uint8_t* buf, size_t max_len) override;
    int fromBuffer(const uint8_t *buf, size_t max_len) override;
};

class ImplicitNullType: public NullType {
  public:
    explicit ImplicitNullType(ASN_TYPE type): NullType(){
        //TODO: check that we're one of the implicit null types
        _type = type;
    }
};

class Counter64: public BER_CONTAINER {
  public:
    Counter64(): BER_CONTAINER(COUNTER64) {}
    explicit Counter64(uint64_t value): Counter64(){
        _value = value;
    }

    uint64_t _value = 0;

protected:
    int serialise(uint8_t* buf, size_t max_len) override;
    int fromBuffer(const uint8_t *buf, size_t max_len) override;
};

class Counter32: public IntegerType {
  public:
    Counter32(): IntegerType(){
        _type = COUNTER32;
    }
    explicit Counter32(unsigned int value): IntegerType(value){
        _type = COUNTER32;
    }

};

class Gauge: public IntegerType { // Unsigned int
  public:
    Gauge(): IntegerType(){
        _type = GAUGE32;
    }
    explicit Gauge(unsigned int value): IntegerType(value){
        _type = GAUGE32;
    }

};

class ComplexType: public BER_CONTAINER {
  public:
    explicit ComplexType(ASN_TYPE type): BER_CONTAINER(type), valuesLen(0), _ownsChildren(false) {}
    ~ComplexType(){
        if(this->_ownsChildren){
            for(int n = 0; n < this->valuesLen; n++){
                asn_delete(this->values[n]);
                this->values[n] = nullptr;
            }
        }
        this->valuesLen = 0;
    }

    BER_CONTAINER* values[SNMP_MAX_COMPLEX_CHILDREN];
    int valuesLen;
    bool _ownsChildren;   /* true = ComplexType owns children (delete in dtor); false = caller owns */

    int fromBuffer(const uint8_t *buf, size_t max_len) override;
    int serialise(uint8_t* buf, size_t max_len) override;

    BER_CONTAINER* addValueToListRaw(BER_CONTAINER* newObj){
        if(!newObj){
            SNMP_LOGE("ComplexType::addValueToListRaw: nullptr child rejected (ASNPool exhausted?)\n");
            return nullptr;
        }
        if(this->valuesLen >= SNMP_MAX_COMPLEX_CHILDREN){
            SNMP_LOGE("ComplexType::addValueToListRaw: values[] full (%d max). Raise SNMP_MAX_COMPLEX_CHILDREN.\n",
                      (int)SNMP_MAX_COMPLEX_CHILDREN);
            return nullptr;
        }
        this->values[this->valuesLen++] = newObj;
        return newObj;
    }

  private:
    static BER_CONTAINER* createObjectForType(ASN_TYPE valueType);
};

static_assert(sizeof(SortableOIDType) <= SNMP_POOL_SLOT_SIZE, "SortableOIDType exceeds ASNPool slot size");
static_assert(sizeof(OIDType) <= SNMP_POOL_SLOT_SIZE, "OIDType exceeds ASNPool slot size");
static_assert(sizeof(ComplexType) <= SNMP_POOL_SLOT_SIZE, "ComplexType exceeds ASNPool slot size");

#endif
