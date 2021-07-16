/*
    Author: Robert Alexa
    Github: https://github.com/robertalexa
*/

#define FASTLED_INTERRUPT_RETRY_COUNT 0

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "FastLED.h"
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "secrets.h"


/*********************************  MQTT Topics ****************************************/
const char* light_state_topic = "esp/stairs/state";
const char* light_set_topic = "esp/stairs/set";
const char* lwtTopic = "esp/stairs/lwt";


/*******************************  MQTT Definitions *************************************/
const char* on_cmd = "ON";
const char* off_cmd = "OFF";
const char* effect = "solid";
String effectString = "solid";


/*****************************  MQTT JSON Definitions **********************************/
const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);
#define MQTT_MAX_PACKET_SIZE 512


/******************************  FastLED Defintions ************************************/
#define NUM_LEDS    50          // Total number of LEDs
#define DATA_PIN    0           // pin for the NEOPIXEL = D3
#define CHIPSET     WS2812
#define COLOR_ORDER GRB

byte realRed = 0;
byte realGreen = 0;
byte realBlue = 0;

byte red = 255;
byte green = 255;
byte blue = 255;

byte motionBrightness = 25;               // Adjust LEDs brightness. 0 to 255
byte brightness = 127;
const int steps = 5;                        // Total number of steps
const int ledsPerStep = NUM_LEDS / steps;   // Total LEDs per step


/*****************************  Sensors Definitions ************************************/
const int pirTop = 5;           // PIR at the top of the staircase = D1
const int pirBottom = 4;        // PIR at the bottom of the staircase = D2
const int ldrSensor = A0;       // LDR Pin = A0

int ledStatus = 0;
int pirTopValue = LOW;          // Variable to hold the PIR status
int pirBottomValue = LOW;       // Variable to hold the PIR status
int ldrSensorValue = 0;         // LDR startup value
int direction = 0;              // Direction of travel; init 0, 1 top to bottom, 2 bottom to top

unsigned long pirTimeout = 60000;  // timestamp to remember when the PIR was triggered.


/*******************************  GLOBALS for FADE *************************************/
bool stateOn = false;
bool startFade = false;
unsigned long lastLoop = 0;
int transitionTime = 0;
int delayMultiplier = 1;
int effectSpeed = 0;
bool inFade = false;
int loopCount = 0;
int stepR, stepG, stepB;
int redVal, grnVal, bluVal;


/*****************************  GLOBALS for EFFECTS ************************************/
//RAINBOW
uint8_t thishue = 0;            // Starting hue value.
uint8_t deltahue = 10;

//CANDYCANE
CRGBPalette16 currentPalettestriped;    //for Candy Cane
CRGBPalette16 HJPalettestriped;         //for Christmas Cane

//NOISE
static uint16_t dist;         // A random number for our noise generator.
uint16_t scale = 30;          // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
uint8_t maxChanges = 48;      // Value for blending between palettes.
CRGBPalette16 targetPalette(OceanColors_p);
CRGBPalette16 currentPalette(CRGB::Black);

//TWINKLE
#define DENSITY     80
int twinklecounter = 0;

//DOTS
uint8_t   count =   0;                                        // Count up to 255 and then reverts to 0
uint8_t fadeval = 224;                                        // Trail behind the LED's. Lower => faster fade.
uint8_t bpm = 30;

//LIGHTNING
uint8_t frequency = 50;                                       // controls the interval between strikes
uint8_t flashes = 8;                                          //the upper limit of flashes per strike
unsigned int dimmer = 1;
uint8_t ledstart;                                             // Starting location of a flash
uint8_t ledlen;
int lightningcounter = 0;

// POLICE
int idex = 0;                //-LED INDEX (0 to NUM_LEDS-1
int TOP_INDEX = int(NUM_LEDS / 2);
int thissat = 255;           //-FX LOOPS DELAY VAR
int antipodal_index(int i) {
    int iN = i + TOP_INDEX;
    if (i >= TOP_INDEX) {
        iN = ( i + TOP_INDEX ) % NUM_LEDS;
    }
    return iN;
}
uint8_t thishuepolice = 0;

//BPM
uint8_t gHue = 0;

//CHRISTMAS ALTERNATE
int toggle = 0;

WiFiClient espClient;
PubSubClient client(espClient);
struct CRGB leds[NUM_LEDS];

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.


/********************************** START SETUP *****************************************/
void setup()
{
    Serial.begin(115200);
    Serial.println("Starting up...");

    FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

    setupStripedPalette(CRGB::Red, CRGB::Red, CRGB::White, CRGB::White); //for CANDY CANE
    setupHJPalette(CRGB::Red, CRGB::Red, CRGB::Green, CRGB::Green); //for Holly Jolly

    setup_wifi();
    setup_mqtt();
    setup_ota();

    pinMode(pirTop, INPUT_PULLUP);          // for PIR at top of staircase initialise the input pin and use the internal resistor
    pinMode(pirBottom, INPUT_PULLUP);       // for PIR at bottom of staircase initialise the input pin and use the internal resistor

    Serial.println("Ready");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    delay(2000); // it takes the sensor 2 seconds to scan the area around it before it can detect infrared presence.
}


/********************************** START SETUP WIFI *****************************************/
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
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}


/********************************** START SETUP MQTT *****************************************/
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
            client.subscribe(light_set_topic);
            client.publish(lwtTopic, "online", true);
            client.publish(light_state_topic, "off");
            setColor(0, 0, 0);
            sendState();
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}


/********************************** START SETUP OAT *****************************************/
void setup_ota()
{
    ArduinoOTA.setPort(SECRET_OTA_PORT);
    // Hostname defaults to esp8266-[ChipID]
    ArduinoOTA.setHostname(SECRET_DEVICE);

    // No authentication by default
    ArduinoOTA.setPassword((const char *)SECRET_OTA_PASS);

    ArduinoOTA.onStart([]() {
        Serial.println("Starting");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            Serial.println("End Failed");
        }
    });
    ArduinoOTA.begin();
}

/*
    SAMPLE PAYLOAD:
    {
    "brightness": 120,
    "color": {
      "r": 255,
      "g": 100,
      "b": 100
    },
    "transition": 5,
    "state": "ON"
    }
*/


/********************************** START CALLBACK*****************************************/
void callback(char* topic, byte* payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    char message[length + 1];
    for (int i = 0; i < length; i++) {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    Serial.println(message);

    if (!processJson(message)) {
        return;
    }

    if (stateOn) {
        realRed = map(red, 0, 255, 0, brightness);
        realGreen = map(green, 0, 255, 0, brightness);
        realBlue = map(blue, 0, 255, 0, brightness);
    } else {
        realRed = 0;
        realGreen = 0;
        realBlue = 0;
    }

    Serial.println(effect);

    startFade = true;
    inFade = false; // Kill the current fade

    sendState();
}


/********************************** START PROCESS JSON*****************************************/
bool processJson(char* message)
{
    StaticJsonDocument<BUFFER_SIZE> doc;

    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.print("deserializeJson() failed with code ");
        Serial.println(error.c_str());
        return false;
    }

    if (doc.containsKey("state")) {
        if (strcmp(doc["state"], on_cmd) == 0) {
            stateOn = true;
        } else if (strcmp(doc["state"], off_cmd) == 0) {
            stateOn = false;
        }
    }

    if (doc.containsKey("color")) {
        red = doc["color"]["r"];
        green = doc["color"]["g"];
        blue = doc["color"]["b"];
    }

    if (doc.containsKey("color_temp")) {
        //temp comes in as mireds, need to convert to kelvin then to RGB
        int color_temp = doc["color_temp"];
        unsigned int kelvin  = 1000000 / color_temp; //MILLION / color_temp;

        temp2rgb(kelvin);

    }

    if (doc.containsKey("brightness")) {
        brightness = doc["brightness"];
    }

    if (doc.containsKey("effect")) {
        effect = doc["effect"];
        effectString = effect;
        twinklecounter = 0; //manage twinklecounter
    }

    if (doc.containsKey("transition")) {
        transitionTime = doc["transition"];
    } else if ( effectString == "solid") {
        transitionTime = 0;
    }

    return true;
}


/********************************** START SEND STATE*****************************************/
void sendState()
{
    StaticJsonDocument<BUFFER_SIZE> doc;

    doc["state"] = (stateOn) ? on_cmd : off_cmd;
    JsonObject color = doc.createNestedObject("color");
    color["r"] = red;
    color["g"] = green;
    color["b"] = blue;

    doc["brightness"] = brightness;
    doc["effect"] = effectString.c_str();
    doc["transition"] = transitionTime;

    size_t docSize = measureJson(doc)+1;
    char buffer[docSize];

    serializeJson(doc, buffer, docSize);

    client.publish(light_state_topic, buffer, true);
}


/********************************** START Set Color*****************************************/
void setColor(int inR, int inG, int inB)
{
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].red   = inR;
        leds[i].green = inG;
        leds[i].blue  = inB;
    }

    FastLED.show();

    Serial.println("Setting LEDs:");
    Serial.print("r: ");
    Serial.print(inR);
    Serial.print(", g: ");
    Serial.print(inG);
    Serial.print(", b: ");
    Serial.println(inB);
}


/********************************** START MAIN LOOP*****************************************/
void loop()
{
    if (WiFi.status() != WL_CONNECTED) {
        delay(1);
        Serial.print("WIFI Disconnected. Attempting reconnection.");
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

    // Manual LEDs control via MQTT
    if (stateOn) {
        FastLED.setBrightness(brightness);
        effects();
        delay(10);
        return;
    }

    // Motion Lights - only get here if stateOn is false (e.g no manual colour/effect)
    showleds();
    FastLED.setBrightness(motionBrightness);

    // Constantly poll the sensors
    pirTopValue = digitalRead(pirTop);
    pirBottomValue = digitalRead(pirBottom);
    ldrSensorValue = analogRead(ldrSensor);

    if ((pirTopValue == HIGH || pirBottomValue == HIGH) && ldrSensorValue > 200) { // Motion and Darkness
        pirTimeout = millis(); // Timestamp when the PIR was triggered.
        if (pirTopValue == HIGH && direction != 2) { // the 2nd term allows pirTimeout to be constantly reset if one lingers at the top of the staircase before decending but will not allow the bottom PIR to reset pirTimeout as you descend past it.
            direction = 1;
            if (ledStatus == 0) {
                // lights up the strip from top to bottom
                Serial.println("Top PIR motion detected");
                colourTopToBottom(255, 255, 250, 100);    // Warm White
                ledStatus = 1;
            }
        }
        if (pirBottomValue == HIGH && direction != 1) { // the 2nd term allows pirTimeout to be constantly reset if one lingers at the bottom of the staircase before ascending but will not allow the top PIR to reset pirTimeout as you ascent past it.
            direction = 2;
            if (ledStatus == 0) {
                // lights up the strip from bottom to top
                Serial.println ("Bottom PIR motion detected");
                colourBottomToTop(255, 255, 250, 100);      // Warm White
                ledStatus = 1;
            }
        }
    }

    if (pirTimeout + 4000 < millis() && direction > 0) { //switch off LEDs in the direction of travel.
        if (direction == 1) {
            colourTopToBottom(0, 0, 0, 100); // Off
        }
        if (direction == 2) {
            colourBottomToTop(0, 0, 0, 100); // Off
        }
        direction = 0;
        ledStatus = 0;
    }

    delay(10);
}

// Fade light each step strip
void colourBottomToTop(int inR, int inG, int inB, uint16_t wait)
{
    for (uint16_t j = steps; j > 0; j--) {
        int start = ledsPerStep * j;                                // starting LED per step basis
        for (uint16_t i = start; i > start - ledsPerStep; i--) {    // loop LEDs on this step
            leds[i - 1].red   = inR;
            leds[i - 1].green = inG;
            leds[i - 1].blue  = inB;
        }
        FastLED.show();
        delay(wait);
    }
}

void colourTopToBottom(int inR, int inG, int inB, uint16_t wait)
{
    for (uint16_t j = 0; j < steps; j++) {
        int start = ledsPerStep * j;                                // starting LED per step basis
        for (uint16_t i = start; i < start + ledsPerStep; i++) {    // loop LEDs on this step
            leds[i - 1].red   = inR;
            leds[i - 1].green = inG;
            leds[i - 1].blue  = inB;
        }
        FastLED.show();
        delay(wait);
    }
}

void effects()
{
    //EFFECT BPM
    if (effectString == "bpm") {
        fadeToBlackBy(leds, NUM_LEDS, 20);
        uint8_t BeatsPerMinute = 62;
        CRGBPalette16 palette = PartyColors_p;
        uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
        for ( int i = 0; i < NUM_LEDS; i++) { //9948
            leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
        }
        if (transitionTime == 0 or transitionTime == NULL) {
            transitionTime = 30;
        }
        delayMultiplier = 1;
        showleds();
    }


    //EFFECT Candy Cane
    if (effectString == "candy cane") {
        static uint8_t startIndex = 0;
        startIndex = startIndex + 1; /* higher = faster motion */
        fill_palette( leds, NUM_LEDS,
                      startIndex, 16, /* higher = narrower stripes */
                      currentPalettestriped, 255, LINEARBLEND);
        if (transitionTime == 0 or transitionTime == NULL) {
            transitionTime = 0;
        }
        delayMultiplier = 1;
        showleds();
    }

    // EFFECT CHRISTMAS CANE
    if (effectString == "christmas cane") {
        static uint8_t startIndex = 0;
        startIndex = startIndex + 1; /* higher = faster motion */

        fill_palette( leds, NUM_LEDS,
                      startIndex, 16, /* higher = narrower stripes */
                      HJPalettestriped, 255, LINEARBLEND);
        if (transitionTime == 0 or transitionTime == NULL) {
            transitionTime = 30;
        }
        delayMultiplier = 1;
        showleds();
    }

    //EFFECT CHRISTMAS ALTERNATE
    if (effectString == "christmas alternate") {
        for (int i = 0; i < NUM_LEDS; i++) {
            if ((toggle + i) % 2 == 0) {
                leds[i] = CRGB::Crimson;
            } else {
                leds[i] = CRGB::DarkGreen;
            }
        }
        toggle = (toggle + 1) % 2;
        if (transitionTime == 0 or transitionTime == NULL) {
            transitionTime = 130;
        }
        delayMultiplier = 30;
        showleds();
        fadeall();
    }

    //EFFECT CONFETTI
    if (effectString == "confetti" ) {
        fadeToBlackBy(leds, NUM_LEDS, 20);
        int pos = random16(NUM_LEDS);
        leds[pos] += CRGB(realRed + random8(64), realGreen, realBlue);
        if (transitionTime == 0 or transitionTime == NULL) {
            transitionTime = 30;
        }
        delayMultiplier = 1;
        showleds();
    }

    //EFFECT CYCLON RAINBOW
    if (effectString == "cyclon rainbow") {                    //Single Dot Down
        fadeToBlackBy(leds, NUM_LEDS, 20);
        static uint8_t hue = 0;
        // First slide the led in one direction
        for (int i = 0; i < NUM_LEDS; i++) {
            // Set the i'th led to red
            leds[i] = CHSV(hue++, 255, brightness);
            // Show the leds
            delayMultiplier = 1;
            showleds();
            // now that we've shown the leds, reset the i'th led to black
            // leds[i] = CRGB::Black;
            fadeall();
            // Wait a little bit before we loop around and do it again
            delay(10);
        }
        for (int i = (NUM_LEDS) - 1; i >= 0; i--) {
            // Set the i'th led to red
            leds[i] = CHSV(hue++, 255, brightness);
            // Show the leds
            delayMultiplier = 1;
            showleds();
            // now that we've shown the leds, reset the i'th led to black
            // leds[i] = CRGB::Black;
            fadeall();
            // Wait a little bit before we loop around and do it again
            delay(10);
        }
    }

    //EFFECT DOTS
    if (effectString == "dots") {
        fadeToBlackBy(leds, NUM_LEDS, 20);
        uint8_t inner = beatsin8(bpm, NUM_LEDS / 4, NUM_LEDS / 4 * 3);
        uint8_t outer = beatsin8(bpm, 0, NUM_LEDS - 1);
        uint8_t middle = beatsin8(bpm, NUM_LEDS / 3, NUM_LEDS / 3 * 2);
        leds[middle] = CRGB::Purple;
        leds[inner] = CRGB::Blue;
        leds[outer] = CRGB::Aqua;
        nscale8(leds, NUM_LEDS, fadeval);

        if (transitionTime == 0 or transitionTime == NULL) {
            transitionTime = 30;
        }
        delayMultiplier = 1;
        showleds();
    }

    random16_add_entropy( random8());

    //EFFECT JUGGLE
    if (effectString == "juggle" ) {                           // eight colored dots, weaving in and out of sync with each other
        fadeToBlackBy(leds, NUM_LEDS, 20);
        for (int i = 0; i < 8; i++) {
            leds[beatsin16(i + 7, 0, NUM_LEDS - 1  )] |= CRGB(realRed, realGreen, realBlue);
        }
        if (transitionTime == 0 or transitionTime == NULL) {
            transitionTime = 130;
        }
        delayMultiplier = 1;
        showleds();
    }

    //EFFECT LIGHTNING
    if (effectString == "lightning") {
        fadeToBlackBy(leds, NUM_LEDS, 20);
        twinklecounter = twinklecounter + 1;                     //Resets strip if previous animation was running
        if (twinklecounter < 2) {
            FastLED.clear();
            FastLED.show();
        }
        ledstart = random16(NUM_LEDS);           // Determine starting location of flash
        ledlen = random16(NUM_LEDS - ledstart);  // Determine length of flash (not to go beyond NUM_LEDS-1)
        for (int flashCounter = 0; flashCounter < random8(3, flashes); flashCounter++) {
            if (flashCounter == 0) {
                dimmer = 5;    // the brightness of the leader is scaled down by a factor of 5
            } else {
                dimmer = random8(1, 3);          // return strokes are brighter than the leader
            }

            fill_solid(leds + ledstart, ledlen, CHSV(255, 0, brightness / dimmer));
            showleds();    // Show a section of LED's
            delay(random8(4, 10));              // each flash only lasts 4-10 milliseconds

            fill_solid(leds + ledstart, ledlen, CHSV(255, 0, 0)); // Clear the section of LED's
            showleds();
            if (flashCounter == 0) delay (130);   // longer delay until next flash after the leader
            delay(50 + random8(100));             // shorter delay between strokes
        }
        delay(random8(frequency) * 100);        // delay between strikes
        if (transitionTime == 0 or transitionTime == NULL) {
            transitionTime = 0;
        }
        delayMultiplier = 1;
        showleds();
    }

    //EFFECT POLICE
    if (effectString == "police") {                 //POLICE LIGHTS (TWO COLOR SOLID)
        idex++;
        if (idex >= NUM_LEDS) {
            idex = 0;
        }
        int idexR = idex;
        int idexB = antipodal_index(idexR);
        int thathue = (thishuepolice + 160) % 255;
        leds[idexR] = CHSV(thishuepolice, thissat, brightness);
        leds[idexB] = CHSV(thathue, thissat, brightness);
        if (transitionTime == 0 or transitionTime == NULL) {
            transitionTime = 30;
        }
        delayMultiplier = 1;
        showleds();
    }

    //EFFECT RAINBOW
    if (effectString == "rainbow") {
        // FastLED's built-in rainbow generator
        static uint8_t starthue = 0;    thishue++;
        fill_rainbow(leds, NUM_LEDS, thishue, deltahue);
        if (transitionTime == 0 or transitionTime == NULL) {
            transitionTime = 130;
        }
        delayMultiplier = 1;
        showleds();
    }

    //EFFECT SINELON
    if (effectString == "sinelon") {
        fadeToBlackBy(leds, NUM_LEDS, 20);
        int pos = beatsin16(13, 0, NUM_LEDS - 1);
        leds[pos] += CRGB(realRed, realGreen, realBlue);
        if (transitionTime == 0 or transitionTime == NULL) {
            transitionTime = 150;
        }
        delayMultiplier = 1;
        showleds();
    }

    //EFFECT TWINKLE
    if (effectString == "twinkle") {
        twinklecounter = twinklecounter + 1;
        if (twinklecounter < 2) {                               //Resets strip if previous animation was running
            FastLED.clear();
            FastLED.show();
        }
        const CRGB lightcolor(8, 7, 1);
        for ( int i = 0; i < NUM_LEDS; i++) {
            if ( !leds[i]) continue; // skip black pixels
            if ( leds[i].r & 1) { // is red odd?
                leds[i] -= lightcolor; // darken if red is odd
            } else {
                leds[i] += lightcolor; // brighten if red is even
            }
        }
        if ( random8() < DENSITY) {
            int j = random16(NUM_LEDS);
            if ( !leds[j] ) leds[j] = lightcolor;
        }

        if (transitionTime == 0 or transitionTime == NULL) {
            transitionTime = 0;
        }
        delayMultiplier = 1;
        showleds();
    }

    //EFFECT NOISE
    if (effectString == "noise") {
        fadeToBlackBy(leds, NUM_LEDS, 20);
        for (int i = 0; i < NUM_LEDS; i++) {                                     // Just onE loop to fill up the LED array as all of the pixels change.
            uint8_t index = inoise8(i * scale, dist + i * scale) % 255;            // Get a value from the noise function. I'm using both x and y axis.
            leds[i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);   // With that value, look up the 8 bit colour palette value and assign it to the current LED.
        }
        dist += beatsin8(10, 1, 4);                                              // Moving along the distance (that random number we started out with). Vary it a bit with a sine wave.
        // In some sketches, I've used millis() instead of an incremented counter. Works a treat.
        if (transitionTime == 0 or transitionTime == NULL) {
            transitionTime = 0;
        }
        delayMultiplier = 1;
        showleds();
    }

    EVERY_N_MILLISECONDS(10)
    {

        nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);  // FOR NOISE ANIMATIon
        {
            gHue++;
        }
    }

    EVERY_N_SECONDS(5)
    {
        targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128, 255)), CHSV(random8(), 255, random8(128, 255)), CHSV(random8(), 192, random8(128, 255)), CHSV(random8(), 255, random8(128, 255)));
    }

    //FADE SUPPORT
    if (startFade && effectString == "solid") {
        // If we don't want to fade, skip it.
        if (transitionTime == 0) {
            setColor(realRed, realGreen, realBlue);

            redVal = realRed;
            grnVal = realGreen;
            bluVal = realBlue;

            startFade = false;
        } else {
            loopCount = 0;
            stepR = calculateStep(redVal, realRed);
            stepG = calculateStep(grnVal, realGreen);
            stepB = calculateStep(bluVal, realBlue);

            inFade = true;
        }
    }

    if (inFade) {
        startFade = false;
        unsigned long now = millis();
        if (now - lastLoop > transitionTime) {
            if (loopCount <= 1020) {
                lastLoop = now;

                redVal = calculateVal(stepR, redVal, loopCount);
                grnVal = calculateVal(stepG, grnVal, loopCount);
                bluVal = calculateVal(stepB, bluVal, loopCount);

                if (effectString == "solid") {
                    setColor(redVal, grnVal, bluVal); // Write current values to LED pins
                }
                loopCount++;
            } else {
                inFade = false;
            }
        }
    }
}


/**************************** START TRANSITION FADER *****************************************/
// From https://www.arduino.cc/en/Tutorial/ColorCrossfader
/*  BELOW THIS LINE IS THE MATH -- YOU SHOULDN'T NEED TO CHANGE THIS FOR THE BASICS
    The program works like this:
    Imagine a crossfade that moves the red LED from 0-10,
    the green from 0-5, and the blue from 10 to 7, in
    ten steps.
    We'd want to count the 10 steps and increase or
    decrease color values in evenly stepped increments.
    Imagine a + indicates raising a value by 1, and a -
    equals lowering it. Our 10 step fade would look like:
    1 2 3 4 5 6 7 8 9 10
    R + + + + + + + + + +
    G   +   +   +   +   +
    B     -     -     -
    The red rises from 0 to 10 in ten steps, the green from
    0-5 in 5 steps, and the blue falls from 10 to 7 in three steps.
    In the real program, the color percentages are converted to
    0-255 values, and there are 1020 steps (255*4).
    To figure out how big a step there should be between one up- or
    down-tick of one of the LED values, we call calculateStep(),
    which calculates the absolute gap between the start and end values,
    and then divides that gap by 1020 to determine the size of the step
    between adjustments in the value.
*/
int calculateStep(int prevValue, int endValue)
{
    int step = endValue - prevValue; // What's the overall gap?
    if (step) {                      // If its non-zero,
        step = 1020 / step;          //   divide by 1020
    }

    return step;
}
/*  The next function is calculateVal. When the loop value, i,
    reaches the step size appropriate for one of the
    colors, it increases or decreases the value of that color by 1.
    (R, G, and B are each calculated separately.)
*/
int calculateVal(int step, int val, int i)
{
    if ((step) && i % step == 0) { // If step is non-zero and its time to change a value,
        if (step > 0) {              //   increment the value if step is positive...
            val += 1;
        } else if (step < 0) {         //   ...or decrement it if step is negative
            val -= 1;
        }
    }

    // Defensive driving: make sure val stays in the range 0-255
    if (val > 255) {
        val = 255;
    } else if (val < 0) {
        val = 0;
    }

    return val;
}


/**************************** START STRIPLED PALETTE *****************************************/
void setupStripedPalette( CRGB A, CRGB AB, CRGB B, CRGB BA)
{
    currentPalettestriped = CRGBPalette16(
                                A, A, A, A, A, A, A, A, B, B, B, B, B, B, B, B
                                //    A, A, A, A, A, A, A, A, B, B, B, B, B, B, B, B
                            );
}

void setupHJPalette( CRGB A, CRGB AB, CRGB B, CRGB BA)
{
    HJPalettestriped = CRGBPalette16(
                           A, A, A, A, A, A, A, A, B, B, B, B, B, B, B, B
                       );
}


/********************************** START FADE ************************************************/
void fadeall()
{
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].nscale8(250);  //for CYCLon
    }
}

/********************************** START SHOW LEDS ***********************************************/
void showleds()
{
    delay(1);

    if (stateOn) {
        FastLED.setBrightness(brightness);
        FastLED.show();                                     //EXECUTE EFFECT COLOR
        if (transitionTime > 0 && transitionTime < 250) {   //Sets animation speed based on received value
            FastLED.delay(transitionTime / 10 * delayMultiplier); //1000 / transitionTime);
            //delay(10*transitionTime);
        }
    } else if (startFade) {
        setColor(0, 0, 0);
        startFade = false;
    }
}

/*************************************** Converter ***********************************************/
void temp2rgb(unsigned int kelvin)
{
    int tmp_internal = kelvin / 100.0;

    // red
    if (tmp_internal <= 66) {
        red = 255;
    } else {
        float tmp_red = 329.698727446 * pow(tmp_internal - 60, -0.1332047592);
        if (tmp_red < 0) {
            red = 0;
        } else if (tmp_red > 255) {
            red = 255;
        } else {
            red = tmp_red;
        }
    }

    // green
    if (tmp_internal <= 66) {
        float tmp_green = 99.4708025861 * log(tmp_internal) - 161.1195681661;
        if (tmp_green < 0) {
            green = 0;
        } else if (tmp_green > 255) {
            green = 255;
        } else {
            green = tmp_green;
        }
    } else {
        float tmp_green = 288.1221695283 * pow(tmp_internal - 60, -0.0755148492);
        if (tmp_green < 0) {
            green = 0;
        } else if (tmp_green > 255) {
            green = 255;
        } else {
            green = tmp_green;
        }
    }

    // blue
    if (tmp_internal >= 66) {
        blue = 255;
    } else if (tmp_internal <= 19) {
        blue = 0;
    } else {
        float tmp_blue = 138.5177312231 * log(tmp_internal - 10) - 305.0447927307;
        if (tmp_blue < 0) {
            blue = 0;
        } else if (tmp_blue > 255) {
            blue = 255;
        } else {
            blue = tmp_blue;
        }
    }
}
