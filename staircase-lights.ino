/*
    Author: Robert Alexa
    Github: https://github.com/robertalexa
*/

#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "secrets.h"

const int pirTop = 5;           // PIR at the top of the staircase = D1
const int pirBottom = 4;        // PIR at the bottom of the staircase = D2
const int ledPin = 0;           // pin for the NEOPIXEL = D3
const int ldrSensor = A0;       // LDR Pin = A0

const int steps = 5;           // Total number of steps
const int totalLeds = 40;      // Total number of LEDs
const int ledsPerStep = totalLeds / steps; // Total LEDs per step
const int brightness = 50;      // Adjust LEDs brightness

const char* stateTopic = "esp/stairs/state";
const char* setTopic = "esp/stairs/set";
const char* lwtTopic = "esp/stairs/lwt";

int ledStatus = 0;
int pirTopValue = LOW;          // Variable to hold the PIR status
int pirBottomValue = LOW;       // Variable to hold the PIR status
int ldrSensorValue = 0;         // LDR startup value
int direction = 0;              // Direction of travel; init 0, 1 top to bottom, 2 bottom to top

unsigned long pirTimeout = 60000;  // timestamp to remember when the PIR was triggered.
unsigned long debugTimeout = 0;

String currentPreset = "off";

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_NeoPixel strip(totalLeds, ledPin, NEO_GRB + NEO_KHZ800);

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

void setup_wifi()
{
    delay(10);

    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(SECRET_WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED) {
        delay(5000);
        Serial.println("Attempting WiFi connection...");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void setup_mqtt()
{
    client.setServer(SECRET_MQTT_IP, SECRET_MQTT_PORT);
    client.setCallback(callback);

    // Loop until we're connected
    while (!client.connected()) {
        Serial.println("");
        Serial.println("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect(SECRET_DEVICE, SECRET_MQTT_USER, SECRET_MQTT_PASS, lwtTopic, 0, 1, "offline")) {
            Serial.println("MQTT connected");
            client.subscribe(setTopic);
            client.publish(lwtTopic, "online", true);
            client.publish(stateTopic, "off");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void setup_ota()
{
    ArduinoOTA.setPort(SECRET_OTA_PORT);
    ArduinoOTA.setHostname(SECRET_DEVICE);
    ArduinoOTA.setPassword((const char *)SECRET_OTA_PASS);
    ArduinoOTA.begin();
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    char message[length + 1];
    for (int i = 0; i < length; i++) {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    Serial.println(message);

    String sMessage = (char*)message;

    if (sMessage != currentPreset) {
        if (sMessage == "red") {
            strip.fill(strip.Color(255, 0, 0));
            strip.show();
            currentPreset = "red";
        } else if (sMessage == "green") {
            strip.fill(strip.Color(0, 255, 0));
            strip.show();
            currentPreset = "green";
        } else if (sMessage == "blue") {
            strip.fill(strip.Color(0, 0, 255));
            strip.show();
            currentPreset = "blue";
        } else if (sMessage == "orange") {
            strip.fill(strip.Color(255, 165, 0));
            strip.show();
            currentPreset = "orange";
        } else if (sMessage == "pink") {
            strip.fill(strip.Color(255, 192, 203));
            strip.show();
            currentPreset = "pink";
        } else {
            strip.fill();
            strip.show();
            currentPreset = "off";
        }
    }
}

// Fade light each step strip
void colourBottomToTop(uint32_t c, uint16_t wait)
{
    for (uint16_t j = steps; j > 0; j--) {
        int start = ledsPerStep * j;                                // starting LED per step basis
        for (uint16_t i = start; i > start - ledsPerStep; i--) {    // loop LEDs on this step
            strip.setPixelColor(i - 1, c);
        }
        strip.show();
        delay(wait);
    }
}

void colourTopToBottom(uint32_t c, uint16_t wait)
{
    for (uint16_t j = 0; j < steps; j++) {
        int start = ledsPerStep * j;                                // starting LED per step basis
        for (uint16_t i = start; i < start + ledsPerStep; i++) {    // loop LEDs on this step
            strip.setPixelColor(i, c);
        }
        strip.show();
        delay(wait);
    }
}

void serialDebug()
{
    if (debugTimeout + 2000 < millis()) {
        Serial.println("");
        Serial.print("Pir top, Bot: ");
        Serial.print(pirTopValue);
        Serial.print(" ");
        Serial.println(pirBottomValue);
        Serial.print("LDR: ");
        Serial.print(ldrSensorValue);
        Serial.print(" Ledstatus: ");
        Serial.println(ledStatus);
        debugTimeout = millis();
    }
}

/********************************** START SETUP ********************************************/
void setup()
{
    Serial.begin(115200);
    Serial.println("Booting");

    setup_wifi();
    setup_mqtt();
    setup_ota();

    pinMode(pirTop, INPUT_PULLUP);          // for PIR at top of staircase initialise the input pin and use the internal resistor
    pinMode(pirBottom, INPUT_PULLUP);       // for PIR at bottom of staircase initialise the input pin and use the internal resistor

    strip.begin();
    strip.setBrightness(brightness);
    strip.fill();
    strip.show();

    delay(2000); // it takes the sensor 2 seconds to scan the area around it before it can detect infrared presence.
}

/********************************** START MAIN LOOP *****************************************/
void loop()
{
    if (WiFi.status() != WL_CONNECTED) {
        delay(1);
        Serial.print("WIFI Disconnected...");
        setup_wifi();
        return;
    }

    if (!client.connected()) {
        delay(1);
        Serial.print("MQTT Disconnected...");
        setup_mqtt();
        return;
    }

    client.loop();

    ArduinoOTA.handle();

    if (currentPreset != "off") {
        delay(10);
        return;
    }

    // Constantly poll the sensors
    pirTopValue = digitalRead(pirTop);
    pirBottomValue = digitalRead(pirBottom);
    ldrSensorValue = analogRead(ldrSensor);

    // Print serial debugging information
    serialDebug();

    if ((pirTopValue == HIGH || pirBottomValue == HIGH) && ldrSensorValue > 200) { // Motion and Darkness
        pirTimeout = millis(); // Timestamp when the PIR was triggered.
        if (pirTopValue == HIGH && direction != 2) { // the 2nd term allows pirTimeout to be constantly reset if one lingers at the top of the staircase before decending but will not allow the bottom PIR to reset pirTimeout as you descend past it.
            direction = 1;
            if (ledStatus == 0) {
                // lights up the strip from top to bottom
                Serial.println("Top PIR motion detected");
                colourTopToBottom(strip.Color(255, 255, 250), 100);    // Warm White
                ledStatus = 1;
            }
        }
        if (pirBottomValue == HIGH && direction != 1) { // the 2nd term allows pirTimeout to be constantly reset if one lingers at the bottom of the staircase before ascending but will not allow the top PIR to reset pirTimeout as you ascent past it.
            direction = 2;
            if (ledStatus == 0) {
                // lights up the strip from bottom to top
                Serial.println ("Bottom PIR motion detected");
                colourBottomToTop(strip.Color(255, 255, 250), 100);      // Warm White
                ledStatus = 1;
            }
        }
    }

    if (pirTimeout + 4000 < millis() && direction > 0) { //switch off LEDs in the direction of travel.
        if (direction == 1) {
            colourTopToBottom(strip.Color(0, 0, 0), 100); // Off
        }
        if (direction == 2) {
            colourBottomToTop(strip.Color(0, 0, 0), 100); // Off
        }
        direction = 0;
        ledStatus = 0;
    }

    delay(10);
}
