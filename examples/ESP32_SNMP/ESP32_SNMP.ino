#if defined (ESP8266)
    #include <ESP8266WiFi.h>        // ESP8266 Core WiFi Library
#else
    #include <WiFi.h>               // ESP32 Core WiFi Library
#endif

/* -------------------------------------------------------------------------- *
 * COMPILE-TIME TUNING (optional, BEFORE #include <SNMP_Agent.h>)
 *
 *   All SNMP size constants in src/include/defs.h are wrapped with
 *   `#ifndef ... #endif` so a sketch-side #define placed here wins over the
 *   library defaults. Good targets for ESP-01 1 MB / 80 KB RAM sensors:
 *
 *   #define SNMP_MAX_COMPLEX_CHILDREN       8
 *   #define SNMP_MAX_VARBINDS               4
 *   #define SNMP_MAX_CALLBACKS_PER_AGENT   16
 *   #define SNMP_MAX_TRAPS_INFLIGHT         4
 *   #define SNMP_POOL_ASN_OBJECTS          32
 *   #define SNMP_POOL_VARBIND_OBJECTS      12
 *
 *   Saves ~25 KB BSS (ASNPool halves: (64 - 32) * 768 B = 24,576 B) and a
 *   few KB more from smaller fixed callback/varbind arrays.
 * -------------------------------------------------------------------------- */

#include <WiFiUdp.h>
#include <SNMP_Agent.h>
#include <SNMPTrap.h>

const char* ssid = "SSID";
const char* password = "password";

WiFiUDP udp;
// Starts an SMMPAgent instance with the read-only community string 'public', and read-write community string 'private
SNMPAgent snmp = SNMPAgent("public", "private");  

// Numbers used to response to Get requests
int changingNumber = 1;
int settableNumber = 0;
uint32_t tensOfMillisCounter = 0;

// arbitrary data will be stored here to act as an OPAQUE data-type
uint8_t stuff[4];


// If we want to change the functionaality of an OID callback later, store them here.
ValueCallback* changingNumberOID;
ValueCallback* settableNumberOID;
TimestampCallback* timestampCallbackOID;

char staticString[] = "This value will never change";

// Setup an SNMPTrap for later use
SNMPTrap* settableNumberTrap = new SNMPTrap("public", SNMP_VERSION_2C);
char _changingStringBuf[25];
char* changingString = _changingStringBuf;

void setup(){
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    // WiFi.begin(ssid);
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    } 
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // give snmp a pointer to the UDP object
    snmp.setUDP(&udp);
    snmp.begin();

    // setup our OPAQUE data-type
    stuff[0] = 1;
    stuff[1] = 2;
    stuff[2] = 24;
    stuff[3] = 67;
    
    // add 'callback' for an OID - pointer to an integer
    changingNumberOID = snmp.addIntegerHandler(".1.3.6.1.4.1.5.0", &changingNumber);
    
    // Using your favourite snmp tool:
    // snmpget -v 1 -c public <IP> 1.3.6.1.4.1.5.0
    
    // you can accept SET commands with a pointer to an integer 
    settableNumberOID = snmp.addIntegerHandler(".1.3.6.1.4.1.5.1", &settableNumber, true);
    // snmpset -v 1 -c public <IP> 1.3.6.1.4.1.5.0 i 99


    // More examples:
    snmp.addIntegerHandler(".1.3.6.1.4.1.4.0", &changingNumber);
    snmp.addOpaqueHandler(".1.3.6.1.4.1.5.9", stuff, 4, true);
    snmp.addReadOnlyStaticStringHandler(".1.3.6.1.4.1.5.11", staticString);
    
    // Setup read/write string
    snprintf(changingString, 25, "This is changeable");
    snmp.addReadWriteStringHandler(".1.3.6.1.4.1.5.12", &changingString, 25, true);


    // Setup SNMP TRAP
    // The SNMP Trap spec requires an uptime counter to be sent along with the trap.
    timestampCallbackOID = (TimestampCallback*)snmp.addTimestampHandler(".1.3.6.1.2.1.1.3.0", &tensOfMillisCounter);

    settableNumberTrap->setUDP(&udp); // give a pointer to our UDP object
    settableNumberTrap->setTrapOID(new OIDType(".1.3.6.1.2.1.33.2")); // OID of the trap
    settableNumberTrap->setSpecificTrap(1); 

    // Set the uptime counter to use in the trap (required)
    settableNumberTrap->setUptimeCallback(timestampCallbackOID);

    // Set some previously set OID Callbacks to send these values with the trap (optional)
    settableNumberTrap->addOIDPointer(changingNumberOID);
    settableNumberTrap->addOIDPointer(settableNumberOID);

    settableNumberTrap->setIP(WiFi.localIP()); // Set our Source IP

    // Ensure to sortHandlers after adding/removing and OID callbacks - this makes snmpwalk work
    snmp.sortHandlers();
} 

void loop(){
    snmp.loop(); // must be called as often as possible
    if(settableNumberOID->setOccurred){
        
        Serial.printf("Number has been set to value: %i\n", settableNumber);
        if(settableNumber%2 == 0){
            // Sending an SNMPv2 INFORM (trap will be kept and re-sent until it is acknowledged by the IP address it was sent to)
            settableNumberTrap->setVersion(SNMP_VERSION_2C);
            settableNumberTrap->setInform(true); // set this to false and send using `settableNumberTrap->sendTo` to send it without the INFORM request
        } else {
            // Sending regular SNMPv1 trap
            settableNumberTrap->setVersion(SNMP_VERSION_1);
            settableNumberTrap->setInform(false);
        }
        // Serial.println("Lets remove the changingNumber reference");
        // snmp.sortHandlers();
        // if(snmp.removeHandler(settableNumberOID)){
        //     Serial.println("Remove succesful");
        // }
        settableNumberOID->resetSetOccurred();

        // Send the trap to the specified IP address
        // If INFORM is set, snmp.loop(); needs to be called in order for the acknowledge mechanism to work.
        IPAddress destinationIP = IPAddress(192, 168, 1, 243);
        if(snmp.sendTrapTo(settableNumberTrap, destinationIP, true, 2, 5000) != INVALID_SNMP_REQUEST_ID){ 
            Serial.println("Sent SNMP Trap");
        } else {
            Serial.println("Couldn't send SNMP Trap");
        }
    }
    changingNumber++;
    tensOfMillisCounter = millis()/10;
}
