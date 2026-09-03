#ifndef SNMPTrap_h
#define SNMPTrap_h

#include "include/ValueCallbacks.h"
#include "include/defs.h"
#include "include/SNMPPacket.h"

#include <stdlib.h>

#ifdef COMPILING_TESTS
	#include "tests/required/IPAddress.h"
	#include "tests/required/UDP.h"
#else
	#include <Arduino.h>
	#include "IPAddress.h"
	
	#if defined(ESP8266) || defined(ESP32)
		#include <WiFiUdp.h>
	#else
		#include "Udp.h"
	#endif
#endif

class SNMPTrap : public SNMPPacket {
  public:
    SNMPTrap(const char* community, SNMP_VERSION version){
        this->setVersion(version);
        this->setPDUType(Trapv2PDU);
        this->setCommunityString(community);
    };
    virtual ~SNMPTrap();
    
    IPAddress agentIP;
    OIDType* trapOID = nullptr;

    TimestampCallback* uptimeCallback = nullptr;
    short genericTrap = 6;
    short specificTrap = 1;
    
    short trapUDPport = 162;
    
    bool inform = false;

    bool setInform(bool inf){
        this->inform = inf;
        ASN_TYPE pduType = this->packetPDUType;
        switch(this->snmpVersion){
            case SNMP_VERSION_1:
                pduType = TrapPDU;
            break;
            case SNMP_VERSION_2C:
                pduType = this->inform ? InformRequestPDU : Trapv2PDU;
            break;
            default:
            break;
        }
        this->setPDUType(pduType);
        return true;
    }
    
    void setTrapOID(OIDType* oid){
        if(_trapOIDOwned) asn_delete(trapOID);
        trapOID = oid;
        _trapOIDOwned = false;
    }

    void setTrapOID(const char* oid_str){
        if(_trapOIDOwned) asn_delete(trapOID);
        trapOID = asn_new<OIDType>(oid_str);
        _trapOIDOwned = true;
    }

    void setTrapOID(const OIDType& oid_ref){
        if(_trapOIDOwned) asn_delete(trapOID);
        trapOID = oid_ref.cloneRaw();
        _trapOIDOwned = true;
    }
    
    void setSpecificTrap(short num){
        specificTrap = num;
    }

    void setIP(IPAddress ip){
        agentIP = ip;
    }
    
    void setUDPport(short port){
        trapUDPport = port;
    }
    
    void setUDP(UDP* udp){
        _udp = udp;
    }
    
    void stop(){
        _udp = nullptr;
    }
    
    void setUptimeCallback(TimestampCallback* uptime){
        uptimeCallback = uptime;
    }
    
    bool addOIDPointer(ValueCallback* callback);
    
    UDP* _udp = nullptr;

    bool buildForSending(){
        this->setRequestID(SNMPPacket::generate_request_id());

        if(this->snmpVersion == SNMP_VERSION_1){
            return this->build();
        } else {
            return SNMPPacket::build();
        }
    }

    bool sendTo(const IPAddress& ip, bool skipBuild = false){
        ASNPool::resetAll();

        bool buildStatus = true;
        if(!skipBuild) {
            buildStatus = this->buildForSending();
        }

        if(!_udp){
            return false;
        }

        if(!this->packet){
            return false;
        }

        if(!buildStatus){
            SNMP_LOGW("Failed Building packet..");
            return false;
        }

        uint8_t _packetBuffer[MAX_SNMP_PACKET_LENGTH] = {0};
        int length = packet->serialise(_packetBuffer, MAX_SNMP_PACKET_LENGTH);

        if(length <= 0) return false;

        _udp->beginPacket(ip, trapUDPport);
        _udp->write(_packetBuffer, length);
        return _udp->endPacket();
    }

  protected:
    ValueCallback* callbacks[SNMP_MAX_CALLBACKS_PER_TRAP] = {nullptr};
    int callbacksCount = 0;
    bool _trapOIDOwned = false;

    std::shared_ptr<ComplexType> generateVarBindList() override;
    ComplexType* generateVarBindListRaw() override;

    static OIDType s_timestampOID;
    static OIDType s_snmpTrapOID;
    OIDType* timestampOID = &s_timestampOID;
    OIDType* snmpTrapOID  = &s_snmpTrapOID;

    bool build() override;
};

#endif
