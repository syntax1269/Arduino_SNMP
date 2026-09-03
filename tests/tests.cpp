#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <cstring>

#include "include/SNMPPacket.h"
#include "include/ValueCallbacks.h"
#include "include/SNMPParser.h"

#include "SNMPTrap.h"

#include <list>

static SNMPPacket* GenerateTestSNMPRequestPacket(){
    SNMPPacket* packet = new SNMPPacket();

    packet->setPDUType(GetRequestPDU);
    packet->setCommunityString("public");
    packet->setRequestID(random());
    packet->setVersion(SNMP_VERSION_1);

    packet->push_back(VarBind(std::make_shared<SortableOIDType>(".1.3.6.1.4.1.5.1"),                  std::make_shared<IntegerType>(42)));
    packet->push_back(VarBind(std::make_shared<SortableOIDType>(".1.3.6.1.4.1.5.2"),                  std::make_shared<OctetType>("test 123")));
    packet->push_back(VarBind(std::make_shared<SortableOIDType>(".1.3.6.1.4.1.52420.9999999"),        std::make_shared<IntegerType>(0)));
    packet->push_back(VarBind(std::make_shared<SortableOIDType>(".1.3.6.1.4.1.5.3"),                  std::make_shared<IntegerType>(-42)));
    packet->push_back(VarBind(std::make_shared<SortableOIDType>(".1.3.6.1.4.1.5.4"),                  std::make_shared<IntegerType>(-420000)));

    return packet;
}

TEST_CASE( "Test handle failures when Encoding/Decoding", "[snmp]"){
    SNMPPacket *packet = GenerateTestSNMPRequestPacket();
    uint8_t buffer[500] = {0};
    int serialised_length = 0;

    serialised_length = packet->serialiseInto(buffer, 500);
    REQUIRE( serialised_length == 133 );

    SECTION( "Failed Serialisation" ){
        serialised_length = packet->serialiseInto(buffer, 132);
        REQUIRE( serialised_length <= 0 );
    }

    SECTION( "Suceed Serialisation" ){
        serialised_length = packet->serialiseInto(buffer, 133);
        REQUIRE( serialised_length == 133 );
    }

    uint8_t copyBuffer[500] = {0};

    memcpy(copyBuffer, buffer, 500);

    SECTION( "Should fail to parse a buffer too small"){
        SNMPPacket* readPack = new SNMPPacket();
        int rc = readPack->parseFrom(buffer, 130);
        REQUIRE( rc != SNMP_ERROR_OK );
    }

    SECTION( "Decoding should not modify the buffer"){
        REQUIRE( memcmp(copyBuffer, buffer, 500) == 0 );
    }

/*
    SECTION( "Should be able to reparse the buffer with correct max_size"){
        SNMPPacket* readPack = new SNMPPacket();
        REQUIRE( readPack->parseFrom(buffer, 133) == SNMP_ERROR_OK );
    }
*/

/*
    SECTION( "Should fail to parse a corrupt buffer "){
        SNMPPacket* readPacket = new SNMPPacket();
        for(int i = 25; i < 133; i+= 10){
            char old[10] = {0};
            memcpy(old, &buffer[i], 10);
            long randomLong = random();
            memcpy(&buffer[i], &randomLong, sizeof(randomLong));
            REQUIRE( readPacket->parseFrom(buffer, 200) != SNMP_ERROR_OK );

            memcpy(&buffer[i], old, 10);
            REQUIRE( readPacket->parseFrom(buffer, 200) == SNMP_ERROR_OK );
        }
    }
*/
}

TEST_CASE( "Test Encoding/Decoding packet", "[snmp]" ) {
    // Build Packet
    SNMPPacket *packet = GenerateTestSNMPRequestPacket();
    uint8_t buffer[500];
    int serialised_length = 0;

    SECTION( "Serialisation" ){
        serialised_length = packet->serialiseInto(buffer, 500);
        REQUIRE( serialised_length == 133 );
    }
    // Read packet
    SNMPPacket* readPacket = new SNMPPacket();
    REQUIRE( readPacket->parseFrom(buffer, serialised_length) == SNMP_ERROR_OK);

    // Check Meta
    REQUIRE( strcmp(packet->communityString, readPacket->communityString) == 0 );
    REQUIRE( packet->requestID == readPacket->requestID );
    REQUIRE( packet->snmpVersion == readPacket->snmpVersion );

    // Check Varbinds
    REQUIRE( packet->size() == 5 );

        // Integer
        REQUIRE( strcmp(packet->varbindList[0].oid->string(), ".1.3.6.1.4.1.5.1") == 0 );
        REQUIRE( packet->varbindList[0].type == ASN_TYPE::INTEGER );
        REQUIRE( static_cast<IntegerType*>(packet->varbindList[0].value)->_value == 42 );

        // String
        REQUIRE( strcmp(packet->varbindList[1].oid->string(), ".1.3.6.1.4.1.5.2") == 0 );
        REQUIRE( packet->varbindList[1].type == ASN_TYPE::STRING );
        REQUIRE( strcmp(static_cast<OctetType*>(packet->varbindList[1].value)->_value, "test 123") == 0 );

        // Long OID Integer
        REQUIRE( strcmp(packet->varbindList[2].oid->string(), ".1.3.6.1.4.1.52420.9999999") == 0 );
        REQUIRE( packet->varbindList[2].type == ASN_TYPE::INTEGER );
        REQUIRE( static_cast<IntegerType*>(packet->varbindList[2].value)->_value == 0 );

        REQUIRE( strcmp(packet->varbindList[3].oid->string(), ".1.3.6.1.4.1.5.3") == 0 );
        REQUIRE( packet->varbindList[3].type == ASN_TYPE::INTEGER );
        REQUIRE( static_cast<IntegerType*>(packet->varbindList[3].value)->_value == -42 );

        REQUIRE( strcmp(packet->varbindList[4].oid->string(), ".1.3.6.1.4.1.5.4") == 0 );
        REQUIRE( packet->varbindList[4].type == ASN_TYPE::INTEGER );
        REQUIRE( static_cast<IntegerType*>(packet->varbindList[4].value)->_value == -420000 );
}

TEST_CASE( "Test GetRequestPDU", "[snmp]" ){
    ValueCallback* callbacks[SNMP_MAX_CALLBACKS_PER_AGENT] = {nullptr};
    int callbacksCount = 0;

    int testInt = 23;
    ValueCallback* integer = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.5.1"), &testInt);
    callbacks[callbacksCount++] = integer;

    SNMPPacket *requestPacket = GenerateTestSNMPRequestPacket();
    uint8_t buffer[500];
    int buf_len = requestPacket->serialiseInto(buffer, 500);
    REQUIRE( buf_len > 0 );

    int responseLength = 0;
    REQUIRE( handlePacket(buffer, buf_len, &responseLength, 500, callbacks, callbacksCount, (char*)"public", (char*)"private") == SNMP_GET_OCCURRED );

    SNMPPacket* responsePacket = new SNMPPacket();
    REQUIRE( responsePacket->parseFrom(buffer, responseLength) == SNMP_ERROR_OK );

    REQUIRE( responsePacket->varbindList[0].type == INTEGER );
    REQUIRE( static_cast<IntegerType*>(responsePacket->varbindList[0].value)->_value == 23 );
}

TEST_CASE( "Test GetNextRequestPDU", "[snmp]" ){
    ValueCallback* callbacks[SNMP_MAX_CALLBACKS_PER_AGENT] = {nullptr};
    int callbacksCount = 0;

    int testInt = 23;
    IntegerCallback* integer = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.5.1"), &testInt);
    callbacks[callbacksCount++] = integer;

    IntegerCallback* integer2 = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.5.2"), &testInt);
    callbacks[callbacksCount++] = integer2;

    SNMPPacket *requestPacket = GenerateTestSNMPRequestPacket();
    requestPacket->setPDUType(GetNextRequestPDU);
    uint8_t buffer[500];
    int buf_len = requestPacket->serialiseInto(buffer, 500);
    REQUIRE( buf_len > 0 );

    int responseLength = 0;
    REQUIRE( handlePacket(buffer, buf_len, &responseLength, 500, callbacks, callbacksCount, "public", "private") == SNMP_GETNEXT_OCCURRED );

    SNMPPacket* responsePacket = new SNMPPacket();
    REQUIRE( responsePacket->parseFrom(buffer, responseLength) == SNMP_ERROR_OK );

    REQUIRE( responsePacket->varbindList[0].type == INTEGER );
    REQUIRE( strcmp(responsePacket->varbindList[0].oid->string(), ".1.3.6.1.4.1.5.2") == 0 );
}

TEST_CASE( "Test GetBulkRequestPDU", "[snmp]"){
    ValueCallback* callbacks[SNMP_MAX_CALLBACKS_PER_AGENT] = {nullptr};
    int callbacksCount = 0;

    int testInt = 23;
    IntegerCallback* integer = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.5.1"), &testInt);
    callbacks[callbacksCount++] = integer;

    IntegerCallback* integer2 = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.5.2"), &testInt);
    callbacks[callbacksCount++] = integer2;

    SNMPPacket *requestPacket = GenerateTestSNMPRequestPacket();
    requestPacket->pop_back();
    requestPacket->pop_back();
    requestPacket->pop_back();
    requestPacket->pop_back();

    requestPacket->setVersion(SNMP_VERSION_2C);
    requestPacket->setPDUType(GetBulkRequestPDU);
    requestPacket->errorIndex.maxRepititions = 2;
    requestPacket->errorStatus.nonRepeaters = 0;

    uint8_t buffer[500];
    int buf_len = requestPacket->serialiseInto(buffer, 500);
    REQUIRE( buf_len > 0 );

    int responseLength = 0;
    REQUIRE( handlePacket(buffer, buf_len, &responseLength, 500, callbacks, callbacksCount, (char*)"public", (char*)"private") == SNMP_GETBULK_OCCURRED );

    SNMPPacket* responsePacket = new SNMPPacket();
    REQUIRE( responsePacket->parseFrom(buffer, responseLength) == SNMP_ERROR_OK );

    REQUIRE( responsePacket->size() == 2 );

    REQUIRE( responsePacket->varbindList[0].type == INTEGER );
    REQUIRE( strcmp(responsePacket->varbindList[0].oid->string(), ".1.3.6.1.4.1.5.2") == 0 );

    REQUIRE( responsePacket->varbindList[1].type == ENDOFMIBVIEW );
}

TEST_CASE( "Test SetRequestPDU", "[snmp]" ){
    ValueCallback* callbacks[SNMP_MAX_CALLBACKS_PER_AGENT] = {nullptr};
    int callbacksCount = 0;

    int testInt = 23;
    IntegerCallback* integerCallback = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.5.1"), &testInt);
    integerCallback->isSettable = false;
    callbacks[callbacksCount++] = integerCallback;

    int testInt2 = 23;
    IntegerCallback* integerCallback2 = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.5.4"), &testInt2);
    integerCallback2->isSettable = true;
    callbacks[callbacksCount++] = integerCallback2;

    uint8_t opaqueBuf[5] = { 1, 2, 3, 4, 5 };
    OpaqueCallback* opaqueCallback = new OpaqueCallback(new SortableOIDType(".1.3.6.1.4.1.5.7"), opaqueBuf, 5);
    opaqueCallback->isSettable = true;
    callbacks[callbacksCount++] = opaqueCallback;

    SNMPPacket *requestPacket = GenerateTestSNMPRequestPacket();
    requestPacket->setPDUType(SetRequestPDU);

    uint8_t setOpaqueBuf[5] = { 5, 4, 3, 2, 1 };
    requestPacket->push_back(VarBind(std::make_shared<SortableOIDType>(".1.3.6.1.4.1.5.7"),                  std::make_shared<OpaqueType>(setOpaqueBuf, 5)));

    uint8_t buffer[500];

    int buf_len = requestPacket->serialiseInto(buffer, 500);
    REQUIRE( buf_len > 0 );

    int responseLength = 0;
    REQUIRE( handlePacket(buffer, buf_len, &responseLength, 500, callbacks, callbacksCount, (char*)"public", (char*)"public") == SNMP_SET_OCCURRED );

    SNMPPacket* responsePacket = new SNMPPacket();
    REQUIRE( responsePacket->parseFrom(buffer, responseLength) == SNMP_ERROR_OK );

    REQUIRE( integerCallback->setOccurred == false );
    REQUIRE( testInt == 23 );

    REQUIRE( integerCallback2->setOccurred == true );
    REQUIRE( testInt2 == -420000 );

    REQUIRE( opaqueCallback->setOccurred == true );
    REQUIRE( opaqueBuf[0] == 5 );
    REQUIRE( opaqueBuf[1] == 4 );
    REQUIRE( opaqueBuf[2] == 3 );
    REQUIRE( opaqueBuf[3] == 2 );
    REQUIRE( opaqueBuf[4] == 1 );

}


TEST_CASE( "sort/remove handlers ", "[snmp]"){
    ValueCallback* callbacks[SNMP_MAX_CALLBACKS_PER_AGENT] = {nullptr};
    int callbacksCount = 0;

    callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.51.2"), nullptr);
    ValueCallback* cb = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.510.2"), nullptr);
    callbacks[callbacksCount++] = cb;
    callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.5100.2"), nullptr);
    callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.5100.1"), nullptr);
    callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.51000.1"), nullptr);
    callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.510.1"), nullptr);
    callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.51.1"), nullptr);
    callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1200.5100000.1"), nullptr);
    callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.5.2"), nullptr);
    callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1200.5.2"), nullptr);


    sort_handlers(callbacks, callbacksCount);

    int idx = 0;

    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.5.2") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.51.1") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.51.2") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.510.1") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.510.2") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.5100.1") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.5100.2") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.51000.1") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1200.5.2") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1200.5100000.1") == 0 );
    idx++;

    REQUIRE( callbacksCount == 10 );

    remove_handler(callbacks, callbacksCount, cb);
    
    REQUIRE( callbacksCount == 9 );

    for(int i = 0; i < callbacksCount; i++){
        REQUIRE( callbacks[i] != cb );
    }

    REQUIRE( strcmp(cb->OID->string(), ".1.3.6.1.4.1.510.2") == 0 );

    idx = 0;

    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.5.2") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.51.1") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.51.2") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.510.1") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.5100.1") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.5100.2") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1.51000.1") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1200.5.2") == 0 );
    idx++;
    REQUIRE( strcmp(callbacks[idx]->OID->string(), ".1.3.6.1.4.1200.5100000.1") == 0 );
    idx++;

}

TEST_CASE( "SNMPTraps ", "[snmp]"){
    SNMPTrap* settableNumberTrap = new SNMPTrap("public", SNMP_VERSION_1);

    uint32_t tensOfMillisCounter = 10;
    int changingNumber = 12;
    int settableNumber = 78;

    IntegerCallback* changingNumberOID = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.23.0"), &changingNumber);
    IntegerCallback* settableNumberOID = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.24.0"), &settableNumber);
    TimestampCallback* timestampCallbackOID = new TimestampCallback(new SortableOIDType(".1.3.6.1.2.1.1.3.0"), &tensOfMillisCounter);

    settableNumberTrap->setTrapOID(new OIDType(".1.3.6.1.2.1.33.2")); // OID of the trap
    settableNumberTrap->setSpecificTrap(1); 

    // Set the uptime counter to use in the trap
    settableNumberTrap->setUptimeCallback(timestampCallbackOID);

    // Set some previously set OID Callbacks to send these values with the trap
    settableNumberTrap->addOIDPointer(changingNumberOID);
    settableNumberTrap->addOIDPointer(settableNumberOID);

    settableNumberTrap->setIP(IPAddress(192, 168, 0, 1)); // Set our Source IP

    REQUIRE( settableNumberTrap->buildForSending() == true );


    uint8_t buffer[500] = {0};

    REQUIRE( settableNumberTrap->packet->serialise(buffer, 500) > 0 );


     ComplexType* trapBuffer = new ComplexType(STRUCTURE);
     REQUIRE( trapBuffer->fromBuffer(buffer, 150) == SNMP_BUFFER_ERROR_UNKNOWN_TYPE );

    // Traps cannot be parsed as regular packets and we'll make sure parsing fails'
//    SNMPPacket* trapPacket = new SNMPPacket();
//    REQUIRE( trapPacket->parseFrom(buffer, 150) == SNMP_PARSE_ERROR_AT_STATE(REQUESTID) );

}

TEST_CASE( "SNMPInform ", "[snmp]"){
    SNMPTrap* settableNumberTrap = new SNMPTrap("public", SNMP_VERSION_2C);
    settableNumberTrap->setInform(true);

    uint32_t tensOfMillisCounter = 10;
    int changingNumber = 12;
    int settableNumber = 78;

    IntegerCallback* changingNumberOID = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.23.0"), &changingNumber);
    IntegerCallback* settableNumberOID = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.24.0"), &settableNumber);
    TimestampCallback* timestampCallbackOID = new TimestampCallback(new SortableOIDType(".1.3.6.1.2.1.1.3.0"), &tensOfMillisCounter);

    settableNumberTrap->setTrapOID(new OIDType(".1.3.6.1.2.1.33.2")); // OID of the trap

    // Set the uptime counter to use in the trap
    settableNumberTrap->setUptimeCallback(timestampCallbackOID);

    // Set some previously set OID Callbacks to send these values with the trap
    settableNumberTrap->addOIDPointer(changingNumberOID);
    settableNumberTrap->addOIDPointer(settableNumberOID);

    settableNumberTrap->setIP(IPAddress(192, 168, 0, 1)); // Set our Source IP

    REQUIRE( settableNumberTrap->buildForSending() == true );

    uint8_t buffer[500] = {0};

    REQUIRE( settableNumberTrap->packet->serialise(buffer, 500) > 0 );

    SNMPPacket* trapPacket = new SNMPPacket();
    REQUIRE(trapPacket->parseFrom(buffer, 150) == SNMP_ERROR_OK);

    REQUIRE( trapPacket->packetPDUType == InformRequestPDU );

}

TEST_CASE( "Test OID Validation ", "[snmp]"){
    REQUIRE( (new OIDType(".1.3.6.1.4.1.52420"))->valid );
    REQUIRE( (new OIDType(".1.3.6.1.4.1.52420."))->valid );
    REQUIRE( (new OIDType("1.3.6.1.4.1.52420"))->valid == false );
    REQUIRE( (new OIDType(".1.3.6.1.4.1..52420"))->valid == false );
}

TEST_CASE( "GetBulk exceeding SNMP_MAX_VARBINDS answers tooBig (no silent truncation)", "[snmp][v3124]"){
    ValueCallback* callbacks[SNMP_MAX_CALLBACKS_PER_AGENT] = {nullptr};
    int callbacksCount = 0;

    /* Register cap+1 OIDs so a one-repeater walk wants cap+1 appends
     * (cap values + endOfMibView) → guaranteed one-append overflow. */
    static int vals[SNMP_MAX_VARBINDS + 1];
    for(int i = 0; i < SNMP_MAX_VARBINDS + 1; i++){
        char oid[32];
        snprintf(oid, sizeof(oid), ".1.3.6.1.4.1.9.1.%d", i + 1);
        vals[i] = i;
        callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(oid), &vals[i]);
    }

    SNMPPacket *requestPacket = GenerateTestSNMPRequestPacket();
    while(requestPacket->size() > 1) requestPacket->pop_back();
    /* Aim the repeater at the PARENT OID .1.3.6.1.4.1.9 — findCallback's
     * subtree match resolves it to .9.1.1, then walks leaf-by-leaf through
     * all cap+1 registered OIDs → 17th append overflows the cap. */
    requestPacket->at(0) = VarBind(std::make_shared<SortableOIDType>(".1.3.6.1.4.1.9"), std::make_shared<IntegerType>(0));

    requestPacket->setVersion(SNMP_VERSION_2C);
    requestPacket->setPDUType(GetBulkRequestPDU);
    requestPacket->errorStatus.nonRepeaters = 0;
    requestPacket->errorIndex.maxRepititions = SNMP_MAX_VARBINDS + 4;

    uint8_t buffer[500];
    int buf_len = requestPacket->serialiseInto(buffer, 500);
    REQUIRE( buf_len > 0 );

    int responseLength = 0;
    /* Overflow is now loud: handlePacket sends a global tooBig error PDU
     * and reports SNMP_ERROR_PACKET_SENT (same convention as the other
     * error-PDU paths, e.g. GetBulk on a v1 request). */
    REQUIRE( handlePacket(buffer, buf_len, &responseLength, 500, callbacks, callbacksCount, (char*)"public", (char*)"private") == SNMP_ERROR_PACKET_SENT );

    SNMPPacket* responsePacket = new SNMPPacket();
    REQUIRE( responsePacket->parseFrom(buffer, responseLength) == SNMP_ERROR_OK );
    REQUIRE( responsePacket->errorStatus.errorStatus == TOO_BIG );
    REQUIRE( responsePacket->size() == 0 );
}

TEST_CASE( "Request with exactly SNMP_MAX_VARBINDS varbinds is served (no false reject)", "[snmp][v3124]"){
    ValueCallback* callbacks[SNMP_MAX_CALLBACKS_PER_AGENT] = {nullptr};
    int callbacksCount = 0;

    static int vals[SNMP_MAX_VARBINDS];
    for(int i = 0; i < SNMP_MAX_VARBINDS; i++){
        char oid[32];
        snprintf(oid, sizeof(oid), ".1.3.6.1.4.1.9.2.%d", i + 1);
        callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(oid), &vals[i]);
    }

    SNMPPacket *requestPacket = GenerateTestSNMPRequestPacket();
    while(requestPacket->size() > 0) requestPacket->pop_back();
    for(int i = 0; i < SNMP_MAX_VARBINDS; i++){
        char oid[32];
        snprintf(oid, sizeof(oid), ".1.3.6.1.4.1.9.2.%d", i + 1);
        requestPacket->push_back(VarBind(std::make_shared<SortableOIDType>(oid), std::make_shared<IntegerType>(i)));
    }
    REQUIRE( requestPacket->size() == SNMP_MAX_VARBINDS );

    uint8_t buffer[800];
    int buf_len = requestPacket->serialiseInto(buffer, 800);
    REQUIRE( buf_len > 0 );

    int responseLength = 0;
    REQUIRE( handlePacket(buffer, buf_len, &responseLength, 800, callbacks, callbacksCount, (char*)"public", (char*)"private") == SNMP_GET_OCCURRED );

    SNMPPacket* responsePacket = new SNMPPacket();
    REQUIRE( responsePacket->parseFrom(buffer, responseLength) == SNMP_ERROR_OK );
    REQUIRE( responsePacket->size() == SNMP_MAX_VARBINDS );
}

/* ---- CMP hardware-bug repro (fork-only ret=-2 / responseLength=-37) ----
 * On hardware the CMP sketch (12 handlers below) answers a Cr5 bulkwalk from
 * the sysDescr parent .1.3.6.1.2.1.1 with SNMP_FAILED_SERIALISATION and
 * serialiseInto() returning -37 (SNMP_BUFFER_ENCODE_ERROR_INVALID_OID),
 * while upstream v2.1.0 on the identical sketch serves the walk. This test
 * mirrors that roster exactly; it must reproduce until fixed. */
static int  cmp_heapK(void)  { return 30; }
static uint32_t cmp_ticks(void) { return 4216; }

TEST_CASE( "CMP repro: Cr5 bulkwalk from CMP-sketch roster serialises", "[snmp][cmp]" ){
    ValueCallback* callbacks[SNMP_MAX_CALLBACKS_PER_AGENT] = {nullptr};
    int callbacksCount = 0;

    static char cmpDescr[] = "CMP fork_3.1.24 | ESP-01 | 12 leaves";
    static uint32_t cmpUptime = 1234;
    static int32_t  cmpIntRO = 42, cmpIntRW = 7, cmpTrapCnt = 1;
    static char     cmpStrBuf[64] = "EditableString";
    static char*    cmpStr = cmpStrBuf;
    static uint32_t cmpC32 = 1, cmpG32 = 2;
    static uint8_t  cmpOpq[8] = {1,2,3,4,5,6,7,8};

    callbacks[callbacksCount++] = new ReadOnlyStringCallback(new SortableOIDType(".1.3.6.1.2.1.1.1.0"), cmpDescr);
    callbacks[callbacksCount++] = new TimestampCallback(new SortableOIDType(".1.3.6.1.2.1.1.3.0"), &cmpUptime);
    callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.99999.1.1.0"), &cmpIntRO);
    callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.99999.1.2.0"), &cmpIntRW);
    callbacks[callbacksCount++] = new DynamicIntegerCallback(new SortableOIDType(".1.3.6.1.4.1.99999.1.3.0"), &cmp_heapK);
    callbacks[callbacksCount++] = new ReadOnlyStringCallback(new SortableOIDType(".1.3.6.1.4.1.99999.1.4.0"), "Hello SNMP");
    callbacks[callbacksCount++] = new StringCallback(new SortableOIDType(".1.3.6.1.4.1.99999.1.5.0"), &cmpStr, 64);
    callbacks[callbacksCount++] = new DynamicTimestampCallback(new SortableOIDType(".1.3.6.1.4.1.99999.1.6.0"), &cmp_ticks);
    callbacks[callbacksCount++] = new Counter32Callback(new SortableOIDType(".1.3.6.1.4.1.99999.1.7.0"), &cmpC32);
    callbacks[callbacksCount++] = new Gauge32Callback(new SortableOIDType(".1.3.6.1.4.1.99999.1.8.0"), &cmpG32);
    callbacks[callbacksCount++] = new OpaqueCallback(new SortableOIDType(".1.3.6.1.4.1.99999.1.9.0"), cmpOpq, 8);
    callbacks[callbacksCount++] = new IntegerCallback(new SortableOIDType(".1.3.6.1.4.1.99999.1.10.0"), &cmpTrapCnt);

    SNMPPacket *requestPacket = GenerateTestSNMPRequestPacket();
    while(requestPacket->size() > 0) requestPacket->pop_back();
    requestPacket->setVersion(SNMP_VERSION_2C);
    requestPacket->setPDUType(GetBulkRequestPDU);
    requestPacket->errorIndex.maxRepititions = 5;
    requestPacket->errorStatus.nonRepeaters = 0;
    requestPacket->push_back(VarBind(std::make_shared<SortableOIDType>(".1.3.6.1.2.1.1"), std::make_shared<IntegerType>(0)));

    uint8_t buffer[800];
    int buf_len = requestPacket->serialiseInto(buffer, 800);
    REQUIRE( buf_len > 0 );

    int responseLength = 0;
    enum SNMP_ERROR_RESPONSE ret = handlePacket(buffer, buf_len, &responseLength, 800, callbacks, callbacksCount, (char*)"public", (char*)"private");
    if(ret != SNMP_GETBULK_OCCURRED){
        fprintf(stderr, "CMP repro: handlePacket ret=%d responseLength=%d\n", (int)ret, responseLength);
    }
    REQUIRE( ret == SNMP_GETBULK_OCCURRED );
    REQUIRE( responseLength > 0 );
}
