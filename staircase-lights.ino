/*
  Author: Robert Alexa
  Github: https://github.com/robertalexa

  Original project by @snikogos https://www.instructables.com/LED-Stair-Lighting/
  Further adapted for nodeMCU by @daromer2 https://github.com/daromer2/StairLights

  Refractored, code cleanup, variable cleanup, bespoke functionality via MQTT
*/

#include "EspMQTTClient.h"
#include <Adafruit_NeoPixel.h>

// Rename secrets.example to secrets.h and set your information
#include "secrets.h"

const int pirTop = 5;           // PIR at the top of the staircase = D1
const int pirBottom = 4;        // PIR at the bottom of the staircase = D2
const int ledPin = 0;           // pin for the NEOPIXEL = D3
const int ldrSensor = A0;       // LDR Pin = A0

const int steps = 5;           // Total number of steps
const int totalLeds = 40;      // Total number of LEDs
const int ledsPerStep = totalLeds / steps; // Total LEDs per step
const int brightness = 50;      // Adjust LEDs brightness

int ledStatus = 0;
int pirTopValue = LOW;          // Variable to hold the PIR status
int pirBottomValue = LOW;       // Variable to hold the PIR status
int ldrSensorValue = 0;         // LDR startup value
int direction = 0;              // Direction of travel; init 0, 1 top to bottom, 2 bottom to top

unsigned long pirTimeout = 60000;  // timestamp to remember when the PIR was triggered.
unsigned long debugTimeout = 0;

// Secrets
String topic = "esp/stairs";

Adafruit_NeoPixel strip(totalLeds, ledPin, NEO_GRB + NEO_KHZ800);

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

EspMQTTClient client(
    SECRET_WIFI_SSID,           // Wifi SSID
    SECRET_WIFI_PASS,           // Wifi Password
    SECRET_MQTT_IP,             // MQTT Broker server ip
    SECRET_MQTT_USER,           // Can be omitted if not needed
    SECRET_MQTT_PASS,           // Can be omitted if not needed
    SECRET_CLIENT,              // Client name that uniquely identifies your device
    SECRET_MQTT_PORT            // The MQTT port, default to 1883. this line can be omitted
    );

void setup()
{
    Serial.begin(115200);

    // Optional features of EspMQTTClient :
    client.enableDebuggingMessages();       // Enable debug messages sent to serial output
    client.enableHTTPWebUpdater();          // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overrited with enableHTTPWebUpdater("user", "password").
    client.enableLastWillMessage("esp/stairs/lastwill", "I am going offline"); // You can activate the retain flag by setting the third parameter to true

    pinMode(pirTop, INPUT_PULLUP);          // for PIR at top of staircase initialise the input pin and use the internal resistor
    pinMode(pirBottom, INPUT_PULLUP);       // for PIR at bottom of staircase initialise the input pin and use the internal resistor

    strip.begin();
    strip.setBrightness(brightness);
    strip.clear();

    delay(2000); // it takes the sensor 2 seconds to scan the area around it before it can detect infrared presence.
}

// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished()
{
    // Subscribe to topic and display received message to Serial
    client.subscribe(topic + "/commands", [](const String& payload) {
        Serial.println(payload);
    });
}

// Fade light each step strip
void colourBottomToTop(uint32_t c, uint16_t wait)
{
    for (uint16_t j = steps; j > 0; j--) {
        int start = strip.numPixels() / steps * j;                // starting LED per step basis
        for (uint16_t i = start; i > start - ledsPerStep; i--) {  // loop LEDs on this step
            strip.setPixelColor(i - 1, c);
        }
        strip.show();
        delay(wait);
    }
}

void colourTopToBottom(uint32_t c, uint16_t wait)
{
    for (uint16_t j = 0; j < steps; j++) {
        int start = strip.numPixels() / steps * j;                // starting LED per step basis
        for (uint16_t i = start; i < start + ledsPerStep; i++) {  // loop LEDs on this step
            strip.setPixelColor(i, c);
        }
        strip.show();
        delay(wait);
    }
}

// ######################### LOOP ############################
void loop()
{
    // Constantly poll the sensors
    pirTopValue = digitalRead(pirTop);
    pirBottomValue = digitalRead(pirBottom);
    ldrSensorValue = analogRead(ldrSensor);

    if (debugTimeout + 2000 < millis()) {
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
        client.publish(topic, "Turn off LEDs");     
    }

    client.loop();
    delay(10);
}
