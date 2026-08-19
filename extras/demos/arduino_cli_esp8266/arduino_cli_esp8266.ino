/*
 * SNMP_Agent minimal demo — ESP8266 (ESP-01)
 * Target:    arduino-cli compile --fqbn esp8266:esp8266:generic
 * Purpose:   DoD H — confirm Arduino-CLI ESP8266 build is zero-error,
 *            zero-warning against the current SNMP_Agent HEAD checkout.
 *
 * Wiring:    Serial 115200 baud, GPIO1 TX, GPIO3 RX (ESP-01 pins).
 *
 * Before run:
 *   1. Fill in WIFI_SSID / WIFI_PASSWORD below.
 *   2. Install esp8266:esp8266 core via Arduino CLI board manager.
 *   3. Symlink / copy this repo into your <sketchbook>/libraries/SNMP_Agent
 *      (Arduino-CLI scans for libraries here at compile time).
 *      e.g. ln -s <repo> ~/Arduino/libraries/SNMP_Agent
 */

#include <ESP8266WiFi.h>
#include <SNMP_Agent.h>

#define WIFI_SSID     "your-ssid-here"
#define WIFI_PASSWORD "your-pass-here"

static SNMPAgent agent;

static int          myInteger   = 42;
static char         sensorName[] = "ESP-01-Sensor";
static uint32_t     counter32   = 0;

static uint32_t getUptimeSeconds(void) { return (uint32_t)(millis() / 1000U); }

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println(F("SNMP_Agent v3.x minimal demo (ESP8266)"));

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
        Serial.print('.');
    }
    Serial.println();
    Serial.print(F("IP: ")); Serial.println(WiFi.localIP());

    agent.begin();   // Default community: "public", OID prefix: ".1.3.6.1.2.1.1"

    /* Three representative handlers to stress-test different ValueCallback
     * code paths (integer / static string / dynamic-timestamp). */
    agent.addIntegerHandler(".8.0", &myInteger, true);
    agent.addReadOnlyStaticStringHandler(".5.0", sensorName);
    agent.addDynamicReadOnlyTimestampHandler(".3.0", getUptimeSeconds);

    agent.addCounter32Handler(".6.1.2.1.0", &counter32);

    Serial.println(F("SNMP agent started. try: snmpget -v 2c -c public <ip> .1.3.6.1.2.1.1.8.0"));
}

void loop()
{
    agent.loop();

    static unsigned long lastTick = 0;
    if (millis() - lastTick > 1000UL) {
        lastTick = millis();
        counter32++;     /* Exercise the counter32 handler on each tick so
                            subsequent SNMP walks see changing data. */
    }
}
