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
#define SNMP_POOL_SLOT_SIZE 768
#endif

    struct Slot {
        alignas(8) char storage[SNMP_POOL_SLOT_SIZE];
        bool occupied;
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
        if(sz > SNMP_POOL_SLOT_SIZE) return nullptr;
#ifndef SNMP_POOLS_IN_BSS
        if(!_poolsReady) _ensurePools();
#endif
        for(int i = 0; i < SNMP_POOL_ASN_OBJECTS; i++){
            if(!slots[i].occupied){
                slots[i].occupied = true;
                usedCount++;
                return slots[i].storage;
            }
        }
        return nullptr;
    }

    static void release(BER_CONTAINER* p);
};

template<typename T, typename... Args>
static inline T* asn_new(Args&&... args){
    void* slot = ASNPool::rawAlloc(sizeof(T));
    if(slot){
        T* obj = ::new (slot) T(std::forward<Args>(args)...);
        return obj;
    }
    return ::new T(std::forward<Args>(args)...);
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
    BER_CONTAINER(ASN_TYPE type) : _type(type){};
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
    NetworkAddress(): BER_CONTAINER(NETWORK_ADDRESS) {};
    explicit NetworkAddress(const IPAddress& ip): NetworkAddress(){
        _value = ip;
    };

    IPAddress _value = INADDR_NONE;

protected:
    int serialise(uint8_t* buf, size_t max_len) override;
    int fromBuffer(const uint8_t *buf, size_t max_len) override;
};


class IntegerType: public BER_CONTAINER {
  public:
    IntegerType(): BER_CONTAINER(INTEGER) {};
    explicit IntegerType(int value): IntegerType(){
        _value = value;
    };

    int _value = 0;

protected:
    int serialise(uint8_t* buf, size_t max_len) override;
    int fromBuffer(const uint8_t *buf, size_t max_len) override;
};

class TimestampType: public IntegerType {
  public:
    TimestampType(): IntegerType(){
        _type = TIMESTAMP;
    };
    explicit TimestampType(unsigned long value): IntegerType(value){
        _type = TIMESTAMP;
    };
};

class OctetType: public BER_CONTAINER {
  public:
    explicit OctetType(const char* value): BER_CONTAINER(STRING) {
        size_t len = strlen(value);
        if(len > SNMP_MAX_STRING_LEN) len = SNMP_MAX_STRING_LEN;
        memcpy(_value, value, len);
        _value[len] = 0;
        _valueLen = len;
    };
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

    OctetType(): BER_CONTAINER(STRING) { _value[0] = 0; };
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
    };
    friend class ComplexType;
    template<typename U, typename... Args> friend U* asn_new(Args&&... args);
};


class OIDType: public BER_CONTAINER {
  public:
    explicit OIDType(const char* value): BER_CONTAINER(OID) {
        size_t len = strlen(value);
        if(len > SNMP_MAX_OID_STR_LEN) len = SNMP_MAX_OID_STR_LEN;
        memcpy(_valueStr, value, len);
        _valueStr[len] = 0;
        this->dataLen = 0;
        this->valid = this->generateInternalData();
    };

    std::shared_ptr<OIDType> cloneOID() const {
        return std::shared_ptr<OIDType>(asn_new<OIDType>(this->_valueStr, this->data, this->dataLen, this->valid));
    };

    OIDType* cloneRaw() const {
        return asn_new<OIDType>(this->_valueStr, this->data, this->dataLen, this->valid);
    };

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
        /* oid must be an exact prefix of this data (front of array).
           Old code used reverse-equal (from the tail) which works only for
           equal-sized trailing bytes, but semantically OID subtree is a
           leading-prefix check, which is the same as:
              compare first oid->dataLen bytes of this->data == oid->data
           Reverse-equal worked because the `std::equal(rbegin, rend, rbegin + off)`
           form verified that the suffix matched offset-tail; mathematically
           equivalent to prefix match when off = this->size - oid->size. For a
           zero-allocation version we just do the direct prefix check. */
        if(oid->dataLen == 0) return true;
        return memcmp(this->data, oid->data, (size_t)oid->dataLen) == 0;
    }

  protected:
    int serialise(uint8_t* buf, size_t max_len) override;
    int fromBuffer(const uint8_t *buf, size_t max_len) override;

    friend class ComplexType;
    template<typename U, typename... Args> friend U* asn_new(Args&&... args);
    OIDType(): BER_CONTAINER(OID) { _valueStr[0] = 0; dataLen = 0; };

    char _valueStr[SNMP_MAX_OID_STR_LEN + 1];
    uint8_t data[SNMP_MAX_OID_SUBIDENTIFIERS + 1];
    int dataLen = 0;

  private:
    explicit OIDType(const char* value, const uint8_t* srcData, int srcLen, bool valid): BER_CONTAINER(OID), valid(valid), dataLen(srcLen) {
        size_t len = strlen(value);
        if(len > SNMP_MAX_OID_STR_LEN) len = SNMP_MAX_OID_STR_LEN;
        memcpy(_valueStr, value, len);
        _valueStr[len] = 0;
        if(srcLen > 0 && srcData) {
            if(srcLen > (int)sizeof(this->data)) srcLen = (int)sizeof(this->data);
            this->dataLen = srcLen;
            memcpy(this->data, srcData, (size_t)srcLen);
        } else {
            this->dataLen = 0;
        }
    };

    bool generateInternalData();
};

class SortableOIDType: public OIDType {
  public:
    explicit SortableOIDType(const char* value): OIDType(value), sortingMapLen(0) {
        generateSortingMap(this->sortingMap, &this->sortingMapLen);
    }

    static bool sort_oids(SortableOIDType* oid1, SortableOIDType* oid2);

    bool operator < (SortableOIDType& other){
        return SortableOIDType::sort_oids(this, &other);
    }

    uint32_t sortingMap[SNMP_MAX_OID_SUBIDENTIFIERS];
    int sortingMapLen;

  private:
    void generateSortingMap(uint32_t outMap[SNMP_MAX_OID_SUBIDENTIFIERS], int* outLen) const;
};

class NullType: public BER_CONTAINER {
  public:
    NullType(): BER_CONTAINER(NULLTYPE) {};

protected:
    int serialise(uint8_t* buf, size_t max_len) override;
    int fromBuffer(const uint8_t *buf, size_t max_len) override;
};

class ImplicitNullType: public NullType {
  public:
    explicit ImplicitNullType(ASN_TYPE type): NullType(){
        //TODO: check that we're one of the implicit null types
        _type = type;
    };
};

class Counter64: public BER_CONTAINER {
  public:
    Counter64(): BER_CONTAINER(COUNTER64) {};
    explicit Counter64(uint64_t value): Counter64(){
        _value = value;
    };

    uint64_t _value = 0;

protected:
    int serialise(uint8_t* buf, size_t max_len) override;
    int fromBuffer(const uint8_t *buf, size_t max_len) override;
};

class Counter32: public IntegerType {
  public:
    Counter32(): IntegerType(){
        _type = COUNTER32;
    };
    explicit Counter32(unsigned int value): IntegerType(value){
        _type = COUNTER32;
    };

};

class Gauge: public IntegerType { // Unsigned int
  public:
    Gauge(): IntegerType(){
        _type = GAUGE32;
    };
    explicit Gauge(unsigned int value): IntegerType(value){
        _type = GAUGE32;
    };

};

class ComplexType: public BER_CONTAINER {
  public:
    explicit ComplexType(ASN_TYPE type): BER_CONTAINER(type), valuesLen(0), _ownsChildren(false) {};
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
        if(this->valuesLen >= SNMP_MAX_COMPLEX_CHILDREN) return nullptr;
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
