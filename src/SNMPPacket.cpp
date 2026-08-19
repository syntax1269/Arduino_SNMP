#include "include/SNMPPacket.h"

#define SNMP_PARSE_ERROR_AT_STATE(STATE) ((int)STATE * -1) - 10 + SNMP_PACKET_PARSE_ERROR_OFFSET

#define ASN_TYPE_FOR_STATE_SNMPVERSION  INTEGER
#define ASN_TYPE_FOR_STATE_COMMUNITY    STRING
#define ASN_TYPE_FOR_STATE_REQUESTID    INTEGER
#define ASN_TYPE_FOR_STATE_ERRORSTATUS  INTEGER
#define ASN_TYPE_FOR_STATE_ERRORID      INTEGER
#define ASN_TYPE_FOR_STATE_VARBINDS     STRUCTURE
#define ASN_TYPE_FOR_STATE_VARBIND      STRUCTURE

#define STR_IMPL_(x) #x      //stringify argument
#define STR(x) STR_IMPL_(x)  //indirection to expand argument macros

#define ASSERT_ASN_TYPE_AT_STATE(value, TYPE, STATE) \
    if(!value || value->_type != TYPE) { \
        SNMP_LOGW("Expecting value to be " STR(TYPE) " for " #STATE); \
        return SNMP_PARSE_ERROR_GENERIC; \
    }

#define ASSERT_ASN_STATE_TYPE(value, STATE) \
    if(!value || value->_type != ASN_TYPE_FOR_STATE_##STATE) { \
        SNMP_LOGW("Expecting " STR(ASN_TYPE_FOR_STATE_##STATE) " for " #STATE " failed: %d\n", value->_type); \
        return SNMP_PARSE_ERROR_AT_STATE(STATE); \
    }

#define ASSERT_ASN_PARSING_TYPE_RANGE(value, LOW_TYPE, HIGH_TYPE) \
    if(!value || !(value->_type >= LOW_TYPE && value->_type <= HIGH_TYPE)){ \
        SNMP_LOGW("Expecting vartype for PDU failed: %d\n", value->_type); \
        return SNMP_PARSE_ERROR_GENERIC; \
    }

SNMPPacket::~SNMPPacket(){
    asn_delete(this->packet);
}

SNMP_PACKET_PARSE_ERROR SNMPPacket::parsePacket(ComplexType *structure, enum SNMPParsingState state) {
    for(int n = 0; n < structure->valuesLen && state != DONE; n++){
        BER_CONTAINER* value = structure->values[n];

        switch(state) {

            case SNMPVERSION:
                ASSERT_ASN_STATE_TYPE(value, SNMPVERSION);
                this->snmpVersionPtr = std::shared_ptr<IntegerType>(asn_new<IntegerType>(static_cast<IntegerType*>(value)->_value));
                this->snmpVersion = (SNMP_VERSION) this->snmpVersionPtr.get()->_value;
                if (this->snmpVersion >= SNMP_VERSION_MAX) {
                    SNMP_LOGW("Invalid SNMP Version: %d\n", this->snmpVersion);
                    return SNMP_PARSE_ERROR_AT_STATE(SNMPVERSION);
                };
                state = COMMUNITY;
            break;

            case COMMUNITY:
                ASSERT_ASN_STATE_TYPE(value, COMMUNITY);
                {
                    OctetType* src = static_cast<OctetType*>(value);
                    this->communityStringPtr = std::shared_ptr<OctetType>(asn_new<OctetType>(src->_value, src->_valueLen));
                }
                {
                    size_t len = this->communityStringPtr.get()->_valueLen;
                    if(len > SNMP_MAX_COMMUNITY_LEN) len = SNMP_MAX_COMMUNITY_LEN;
                    memcpy(this->communityString, this->communityStringPtr.get()->_value, len);
                    this->communityString[len] = 0;
                }
                state = PDU;
            break;

            case PDU:
                ASSERT_ASN_PARSING_TYPE_RANGE(value, ASN_PDU_TYPE_MIN_VALUE, ASN_PDU_TYPE_MAX_VALUE)
                this->packetPDUType = value->_type;
                return this->parsePacket(static_cast<ComplexType*>(value), REQUESTID);

            case REQUESTID:
                ASSERT_ASN_STATE_TYPE(value, REQUESTID);
                this->requestIDPtr = std::shared_ptr<IntegerType>(asn_new<IntegerType>(static_cast<IntegerType*>(value)->_value));
                this->requestID = this->requestIDPtr.get()->_value;
                state = ERRORSTATUS;
            break;

            case ERRORSTATUS:
                ASSERT_ASN_STATE_TYPE(value, ERRORSTATUS);
                this->errorStatus.errorStatus = (SNMP_ERROR_STATUS) static_cast<IntegerType *>(value)->_value;
                state = ERRORID;
            break;

            case ERRORID:
                ASSERT_ASN_STATE_TYPE(value, ERRORID);
                this->errorIndex.errorIndex = static_cast<IntegerType*>(value)->_value;
                state = VARBINDS;
            break;

            case VARBINDS:
                ASSERT_ASN_STATE_TYPE(value, VARBINDS);
                // we have a varbind structure, lets dive into it.
                return this->parsePacket(static_cast<ComplexType*>(value), VARBIND);

            case VARBIND:
            {
                ASSERT_ASN_STATE_TYPE(value, VARBIND);
                // we are in a single varbind

                ComplexType* varbindValues = static_cast<ComplexType*>(value);

                if (varbindValues->valuesLen != 2) {
                    SNMP_LOGW("Expecting VARBIND TO CONTAIN 2 OBEJCTS; %d\n",
                              varbindValues ? varbindValues->valuesLen : 0);
                    return SNMP_PARSE_ERROR_AT_STATE(VARBIND);
                };

                BER_CONTAINER* vbOid = varbindValues->values[0];
                ASSERT_ASN_TYPE_AT_STATE(vbOid, OID, VARBIND);

                BER_CONTAINER* vbValue = varbindValues->values[1];

                /* Clone the OID so the VarBind holds an owning reference independent
                 * of the incoming ComplexType tree (which will be delete'd when the
                 * SNMPPacket object is reused or destroyed).  For value we wrap with
                 * a no-op deleter: the caller's lifetime (packet processing + reply
                 * serialise) always happens before packet destruction. */
                std::shared_ptr<OIDType> oidClone = static_cast<OIDType*>(vbOid)->cloneOID();
                std::shared_ptr<BER_CONTAINER> valueView(vbValue, [](BER_CONTAINER*){});

                this->emplace_back(oidClone, valueView);
            }
            break;

            case DONE:
                return true;
        }
    }
    return SNMP_ERROR_OK;
}

SNMP_PACKET_PARSE_ERROR SNMPPacket::parseFrom(unsigned char* buf, size_t max_len){
    SNMP_LOGD("Parsing %ld bytes\n", max_len);
    if(buf[0] != 0x30) {
        SNMP_LOGD("First byte error\n");
        return SNMP_PARSE_ERROR_MAGIC_BYTE;
    }

    packet = asn_new<ComplexType>(STRUCTURE);

    SNMP_BUFFER_PARSE_ERROR decodePacket = packet->fromBuffer(buf, max_len);
    if(decodePacket <= 0){
        SNMP_LOGD("failed to fromBuffer\n");
        return decodePacket;
    }

    // we now have a full ASN.1 packet in SNMPPacket
    return parsePacket(packet, SNMPVERSION);
}

int SNMPPacket::serialiseInto(uint8_t* buf, size_t max_len){
    if(this->build()){
        return this->packet->serialise(buf, max_len);
    }
    return 0;
}

bool SNMPPacket::build(){
    asn_delete(this->packet);

    ComplexType* root = asn_new<ComplexType>(STRUCTURE);
    root->_ownsChildren = true;
    this->packet = root;

    if(this->snmpVersionPtr)
        root->addValueToListRaw(asn_new<IntegerType>(this->snmpVersionPtr->_value));
    else
        root->addValueToListRaw(asn_new<IntegerType>(this->snmpVersion));

    if(this->communityStringPtr)
        root->addValueToListRaw(asn_new<OctetType>(this->communityStringPtr->_value, this->communityStringPtr->_valueLen));
    else
        root->addValueToListRaw(asn_new<OctetType>(this->communityString));

    ComplexType* snmpPDU = asn_new<ComplexType>(this->packetPDUType);
    snmpPDU->_ownsChildren = true;

    if(this->requestIDPtr)
        snmpPDU->addValueToListRaw(asn_new<IntegerType>(this->requestIDPtr->_value));
    else
        snmpPDU->addValueToListRaw(asn_new<IntegerType>(this->requestID));


    snmpPDU->addValueToListRaw(asn_new<IntegerType>(this->errorStatus.errorStatus));
    snmpPDU->addValueToListRaw(asn_new<IntegerType>(this->errorIndex.errorIndex));

    ComplexType* varBindList = this->generateVarBindListRaw();
    if(!varBindList) return false;

    snmpPDU->addValueToListRaw(varBindList);

    root->addValueToListRaw(snmpPDU);

    return true;
}

void SNMPPacket::setCommunityString(const char *CommunityString){
    this->communityStringPtr = nullptr;
    size_t len = strlen(CommunityString);
    if(len > SNMP_MAX_COMMUNITY_LEN) len = SNMP_MAX_COMMUNITY_LEN;
    memcpy(this->communityString, CommunityString, len);
    this->communityString[len] = 0;
}

void SNMPPacket::setRequestID(snmp_request_id_t RequestId){
    this->requestIDPtr = nullptr;
    this->requestID = RequestId;
}

bool SNMPPacket::setPDUType(ASN_TYPE responseType){
    if(responseType >= ASN_PDU_TYPE_MIN_VALUE && responseType <= ASN_PDU_TYPE_MAX_VALUE){
        //TODO: check that we're a valid response type
        this->packetPDUType = responseType;
        return true;
    }
    return false;
}

void SNMPPacket::setVersion(SNMP_VERSION SnmpVersion){
    this->snmpVersionPtr = nullptr;
    this->snmpVersion = SnmpVersion;
}

ComplexType* SNMPPacket::generateVarBindListRaw(){
    SNMP_LOGD("generateVarBindListRaw from SNMPPacket");
    ComplexType* list = asn_new<ComplexType>(STRUCTURE);
    list->_ownsChildren = true;

    for(int vbIdx = 0; vbIdx < this->varbindCount; vbIdx++){
        const VarBind& varBindItem = this->varbindList[vbIdx];
        ComplexType* varBind = asn_new<ComplexType>(STRUCTURE);
        varBind->_ownsChildren = true;

        varBind->addValueToListRaw(varBindItem.oid->cloneRaw());

        BER_CONTAINER* src = varBindItem.value;
        BER_CONTAINER* clonedValue = nullptr;
        if(!src){
            clonedValue = asn_new<NullType>();
        } else switch(src->_type){
            case INTEGER:        clonedValue = asn_new<IntegerType>(static_cast<IntegerType*>(src)->_value); break;
            case STRING:
            {
                OctetType* so = static_cast<OctetType*>(src);
                clonedValue = asn_new<OctetType>(so->_value, so->_valueLen);
            } break;
            case OID:            clonedValue = static_cast<OIDType*>(src)->cloneRaw(); break;
            case NULLTYPE:       clonedValue = asn_new<NullType>(); break;
            case NOSUCHOBJECT:   clonedValue = asn_new<ImplicitNullType>(NOSUCHOBJECT); break;
            case NOSUCHINSTANCE: clonedValue = asn_new<ImplicitNullType>(NOSUCHINSTANCE); break;
            case ENDOFMIBVIEW:   clonedValue = asn_new<ImplicitNullType>(ENDOFMIBVIEW); break;
            case NETWORK_ADDRESS:
            {
                NetworkAddress* so = static_cast<NetworkAddress*>(src);
                clonedValue = asn_new<NetworkAddress>(so->_value);
            } break;
            case TIMESTAMP:      clonedValue = asn_new<TimestampType>(static_cast<TimestampType*>(src)->_value); break;
            case COUNTER32:      clonedValue = asn_new<Counter32>(static_cast<Counter32*>(src)->_value); break;
            case GAUGE32:        clonedValue = asn_new<Gauge>(static_cast<Gauge*>(src)->_value); break;
            case COUNTER64:      clonedValue = asn_new<Counter64>(static_cast<Counter64*>(src)->_value); break;
            case OPAQUE:
            {
                OpaqueType* so = static_cast<OpaqueType*>(src);
                clonedValue = asn_new<OpaqueType>(so->_value, so->_dataLength);
            } break;
            default:
                clonedValue = asn_new<NullType>(); break;
        }
        varBind->addValueToListRaw(clonedValue);

        list->addValueToListRaw(varBind);
    }

    return list;
}

std::shared_ptr<ComplexType> SNMPPacket::generateVarBindList(){
    return std::shared_ptr<ComplexType>(generateVarBindListRaw());
}

snmp_request_id_t SNMPPacket::generate_request_id(){
    //NOTE: do not generate 0
    snmp_request_id_t request_id = 0;
    while(request_id == 0){
        request_id |= rand();
        request_id <<= 8;
        request_id |= rand();
    }
    return request_id;
}
