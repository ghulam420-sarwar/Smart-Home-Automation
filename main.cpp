/**
 * Smart Home Automation System
 * ----------------------------
 * Target MCU : ESP32
 * Sensors    : DHT22 (temp/humidity), LDR (light), PIR (motion)
 * Actuators  : 4-channel relay board (controls lights, fans, AC)
 * Protocol   : MQTT over WiFi — compatible with Home Assistant / Node-RED
 * Author     : Ghulam Sarwar
 *
 * Automation rules (run locally, no cloud needed for basic operation):
 *   - If temp > TEMP_FAN_ON  AND fan relay OFF  → turn fan ON
 *   - If temp < TEMP_FAN_OFF AND fan relay ON   → turn fan OFF
 *   - If lux < LUX_LIGHT_ON AND motion detected → turn light ON
 *   - Light stays ON for LIGHT_TIMEOUT_MS after last motion
 *
 * Remote control: publish to ghulam/home/relay/<1-4>/set with "ON" or "OFF"
 * Telemetry:      subscribes nothing, publishes to ghulam/home/telemetry every 5s
 *
 * MIT License
 */

#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ── user config ──────────────────────────────────────────────────────────────
const char* WIFI_SSID  = "YOUR_SSID";
const char* WIFI_PASS  = "YOUR_PASSWORD";
const char* MQTT_HOST  = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;
const char* CLIENT_ID  = "esp32-home-ctrl";

// ── pins ──────────────────────────────────────────────────────────────────────
#define DHT_PIN      4
#define LDR_PIN     34       // ADC1 channel 6
#define PIR_PIN     35
#define RELAY1_PIN  26       // light (active LOW)
#define RELAY2_PIN  27       // fan
#define RELAY3_PIN  14       // spare
#define RELAY4_PIN  12       // spare

// ── thresholds ────────────────────────────────────────────────────────────────
constexpr float    TEMP_FAN_ON        = 28.0f;   // °C
constexpr float    TEMP_FAN_OFF       = 26.0f;
constexpr uint16_t LUX_LIGHT_ON       = 200;     // raw ADC (0-4095)
constexpr uint32_t LIGHT_TIMEOUT_MS   = 120000;  // 2 min

// ── globals ───────────────────────────────────────────────────────────────────
DHT         dht(DHT_PIN, DHT22);
WiFiClient  wifiClient;
PubSubClient mqtt(wifiClient);

uint8_t relayPins[4] = {RELAY1_PIN, RELAY2_PIN, RELAY3_PIN, RELAY4_PIN};
bool    relayState[4] = {false, false, false, false};
uint32_t lastMotionMs = 0;

void setRelay(uint8_t idx, bool on) {
    if (idx > 3) return;
    relayState[idx] = on;
    digitalWrite(relayPins[idx], on ? LOW : HIGH);  // active LOW board
    Serial.printf("Relay %d → %s\n", idx + 1, on ? "ON" : "OFF");
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
    String t(topic);
    String val((char*)payload, len);
    val.trim();

    for (uint8_t i = 0; i < 4; ++i) {
        if (t == String("ghulam/home/relay/") + (i+1) + "/set") {
            setRelay(i, val == "ON");
            return;
        }
    }
}

void connectAll() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("WiFi");
    for (uint8_t i = 0; i < 40 && WiFi.status() != WL_CONNECTED; ++i) {
        delay(250); Serial.print(".");
    }
    Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " FAIL");

    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
}

void mqttEnsure() {
    if (mqtt.connected()) return;
    while (!mqtt.connect(CLIENT_ID)) delay(1000);
    for (uint8_t i = 0; i < 4; ++i) {
        char topic[48];
        snprintf(topic, sizeof(topic), "ghulam/home/relay/%d/set", i+1);
        mqtt.subscribe(topic);
    }
    Serial.println("MQTT connected & subscribed");
}

void runAutomation(float temp, uint16_t lux, bool motion) {
    // Fan control
    if (!relayState[1] && temp > TEMP_FAN_ON)  setRelay(1, true);
    if ( relayState[1] && temp < TEMP_FAN_OFF) setRelay(1, false);

    // Light control with timeout
    if (motion) {
        lastMotionMs = millis();
        if (!relayState[0] && lux < LUX_LIGHT_ON) setRelay(0, true);
    }
    if (relayState[0] && (millis() - lastMotionMs > LIGHT_TIMEOUT_MS)) {
        setRelay(0, false);
    }
}

void publishTelemetry(float temp, float hum, uint16_t lux, bool motion) {
    char buf[200];
    snprintf(buf, sizeof(buf),
        "{\"temp\":%.2f,\"hum\":%.2f,\"lux\":%u,\"motion\":%s,"
        "\"r1\":%s,\"r2\":%s,\"r3\":%s,\"r4\":%s}",
        temp, hum, lux, motion ? "true" : "false",
        relayState[0]?"true":"false", relayState[1]?"true":"false",
        relayState[2]?"true":"false", relayState[3]?"true":"false");
    mqtt.publish("ghulam/home/telemetry", buf, true);
    Serial.println(buf);
}

void setup() {
    Serial.begin(115200);
    dht.begin();
    for (uint8_t i = 0; i < 4; ++i) {
        pinMode(relayPins[i], OUTPUT);
        setRelay(i, false);
    }
    pinMode(PIR_PIN, INPUT);
    connectAll();
}

void loop() {
    mqttEnsure();
    mqtt.loop();

    static uint32_t tNext = 0;
    if (millis() < tNext) return;
    tNext = millis() + 5000;

    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();
    if (isnan(temp) || isnan(hum)) { Serial.println("DHT fail"); return; }

    uint16_t lux  = analogRead(LDR_PIN);
    bool  motion  = digitalRead(PIR_PIN);

    runAutomation(temp, lux, motion);
    publishTelemetry(temp, hum, lux, motion);
}
