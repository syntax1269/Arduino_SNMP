#include "SNMPTrap.h"
#include "include/SNMPParser.h"
#include "include/defs.h"

OIDType SNMPTrap::s_timestampOID(RFC1213_OID_sysUpTime);
OIDType SNMPTrap::s_snmpTrapOID(SNMPv2_SNMPTRAP_OID_0);

SNMPTrap::~SNMPTrap(){
    asn_delete(packet);
}

bool SNMPTrap::build(){
    asn_delete(packet);

    if(!this->trapOID) return false;

    ComplexType* root = asn_new<ComplexType>(STRUCTURE);
    root->_ownsChildren = true;
    packet = root;

    root->addValueToListRaw(asn_new<IntegerType>((int)this->snmpVersion));
    root->addValueToListRaw(asn_new<OctetType>(this->communityString));

    ComplexType* trapPDU = asn_new<ComplexType>(TrapPDU);
    trapPDU->_ownsChildren = true;

    trapPDU->addValueToListRaw(trapOID->cloneRaw());
    trapPDU->addValueToListRaw(asn_new<NetworkAddress>(agentIP));
    trapPDU->addValueToListRaw(asn_new<IntegerType>(genericTrap));
    trapPDU->addValueToListRaw(asn_new<IntegerType>(specificTrap));

    if(uptimeCallback){
        auto sp = std::static_pointer_cast<TimestampType>(ValueCallback::getValueForCallback(uptimeCallback));
        if(sp) trapPDU->addValueToListRaw(asn_new<TimestampType>(sp->_value));
        else   trapPDU->addValueToListRaw(asn_new<TimestampType>(0));
    } else {
        trapPDU->addValueToListRaw(asn_new<TimestampType>(0));
    }

    ComplexType* ourVBList = this->generateVarBindListRaw();
    if(!ourVBList) return false;

    trapPDU->addValueToListRaw(ourVBList);
    root->addValueToListRaw(trapPDU);
    return true;
}

ComplexType* SNMPTrap::generateVarBindListRaw(){
    SNMP_LOGD("generateVarBindListRaw from SNMPTrap");
    ComplexType* ourVBList = asn_new<ComplexType>(STRUCTURE);
    ourVBList->_ownsChildren = true;

    if(this->snmpVersion == SNMP_VERSION_2C){
        if(!this->trapOID){
            asn_delete(ourVBList);
            return nullptr;
        }
        ComplexType* timestampVarBind = asn_new<ComplexType>(STRUCTURE);
        timestampVarBind->_ownsChildren = true;
        timestampVarBind->addValueToListRaw(timestampOID->cloneRaw());

        if(uptimeCallback){
            auto sp = std::static_pointer_cast<TimestampType>(ValueCallback::getValueForCallback(uptimeCallback));
            if(sp) timestampVarBind->addValueToListRaw(asn_new<TimestampType>(sp->_value));
            else   timestampVarBind->addValueToListRaw(asn_new<TimestampType>(0));
        } else {
            timestampVarBind->addValueToListRaw(asn_new<TimestampType>(0));
        }
        ourVBList->addValueToListRaw(timestampVarBind);

        ComplexType* oidVarBind = asn_new<ComplexType>(STRUCTURE);
        oidVarBind->_ownsChildren = true;
        oidVarBind->addValueToListRaw(snmpTrapOID->cloneRaw());
        oidVarBind->addValueToListRaw(trapOID->cloneRaw());
        ourVBList->addValueToListRaw(oidVarBind);
    }

    for(int i = 0; i < callbacksCount; i++){
        ValueCallback* value = callbacks[i];
        if(!value) continue;
        ComplexType* varBind = asn_new<ComplexType>(STRUCTURE);
        varBind->_ownsChildren = true;

        varBind->addValueToListRaw(value->OID->cloneRaw());

        auto valueSP = ValueCallback::getValueForCallback(value);
        BER_CONTAINER* src = valueSP.get();
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

        ourVBList->addValueToListRaw(varBind);
    }

    return ourVBList;
}

std::shared_ptr<ComplexType> SNMPTrap::generateVarBindList(){
    return std::shared_ptr<ComplexType>(generateVarBindListRaw());
}

void SNMPTrap::addOIDPointer(ValueCallback* callback){
    if(callbacksCount >= SNMP_MAX_CALLBACKS_PER_TRAP) return;
    callbacks[callbacksCount++] = callback;
}
