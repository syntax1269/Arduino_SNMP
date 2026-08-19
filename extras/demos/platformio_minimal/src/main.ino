/*
 * SNMP_Agent minimal demo — dual-build (esp8266 + esp32 via PlatformIO)
 * Target:    pio run --environment esp8266 / pio run --environment esp32
 * Purpose:   DoD I — confirm PlatformIO strict build (with -Werror) passes
 *            both ESP targets cleanly.
 *
 * Uses #if macro dispatch so a single .ino file compiles against both
 * ESP8266WiFi.h (Espressif 8266 core) and WiFi.h (Espressif 32 core).
 *
 * Before flashing: fill in WIFI_SSID / WIFI_PASSWORD below, then
 *   pio run -t upload -e esp8266   OR   pio run -t upload -e esp32
 *   pio device monitor -b 115200
 */

#if defined(ESP8266)
#  include <ESP8266WiFi.h>
#else
#  include <WiFi.h>
#endif

#include <SNMP_Agent.h>

#define WIFI_SSID     "your-ssid-here"
#define WIFI_PASSWORD "your-pass-here"

static SNMPAgent agent;

static int          myInteger   = 42;
static char         sensorName[] = "PlatformIO-Sensor";
static uint32_t     counter32   = 0;

static uint32_t getUptimeSeconds(void) { return (uint32_t)(millis() / 1000U); }

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println();
#if defined(ESP8266)
    Serial.println(F("SNMP_Agent v3.x minimal demo (ESP8266 / PlatformIO)"));
#else
    Serial.println("SNMP_Agent v3.x minimal demo (ESP32 / PlatformIO)");
#endif

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
        Serial.print('.');
    }
    Serial.println();
    Serial.print("IP: "); Serial.println(WiFi.localIP());

    agent.begin();

    agent.addIntegerHandler(".8.0", &myInteger, true);
    agent.addReadOnlyStaticStringHandler(".5.0", sensorName);
    agent.addDynamicReadOnlyTimestampHandler(".3.0", getUptimeSeconds);
    agent.addCounter32Handler(".6.1.2.1.0", &counter32);

    Serial.println("SNMP agent started.");
}

void loop()
{
    agent.loop();

    static unsigned long lastTick = 0;
    if (millis() - lastTick > 1000UL) {
        lastTick = millis();
        counter32++;
    }
}
