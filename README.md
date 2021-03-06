# staircase-lights
#### ESP8266 NodeMCU v3 motion activated staircase lights dependant on ambient light and manual control with full RGB and pre-built effects via MQTT as JSON

For my use case, the project allows for:
- Set brightness, RGB colour and pre-built lighting effects via MQTT commands
- OTA updates - upload new firmware via WiFi. The device needs enough memory to store both versions of the firmware for this to work.
- Home Assistant integration - see the yaml file

## Hardware
- nodeMCU v3
- 2 x AM312 PIR
- 1 x Silonex Norps-12 LDR
- 1 x 510 Ohm resistor for the LDR
- 1 x 510 Ohm resistor for the LED Data wire
- 1 x 1000uF capacitor for the power supply, between negative and positive.
- WS2812B LEDs - 480 + 1 sacrificial - 40 per step - 12 steps
- 5V 30A power supply - make sure you get a power supply able to supply the required Amps, plus 20% to allow for good performance. The closer to the limit you use a PSU, the more heat it will generate and the worst performance it will have. In my case I should have gone with a bit bigger but since my LEDs will never run at 100% brightness, they should never consume that much. The formula is no_of_leds x 0.06 = total_amps. Then you need to take into the account the power consumption of the micro controller.
- 18 awg cable
- aluminium profiles with frosted diffusers

## Schematics
![schematics](https://raw.githubusercontent.com/robertalexa/staircase-lights/master/schematics/schematics.jpg)

## Before you start
1. Rename `secrets.example.h` to `secrets.h` and fill in your credentials
2. Check that the device PINs match your setup
3. Set up the total number of LEDs and number of steps
4. Set up desired starting brightness (can be adjusted via MQTT) as well as motion brightness (can't be adjusted)

## Libraries and versions
At the time of the build the software uses and is compatible with the following:
- ArduinoJson 6.18.0
- PubSubClient 2.8.0
- FastLED 3.4.0

And of course support for ESP8266 - add the following to your board manager
http://arduino.esp8266.com/stable/package_esp8266com_index.json

This will make the following available:
- ESP8266Wifi
- ESP8266mDNS
- WifiUdp
- ArduinoOTA

## Notes
The code contains relics of support for transition, and the HA config for it. I am not using it so I have not tested it. I have however decided to leave it in case I change my mind one day.

## Credits

Original project by @snikogos https://www.instructables.com/LED-Stair-Lighting/

Further adapted for nodeMCU by @daromer2 https://github.com/daromer2/StairLights

More features regarding Home Assistant and effects inspired from @bkpsu https://github.com/bkpsu/ESP-MQTT-JSON-Digital-LEDs which is a fork from @bruhautomation https://github.com/bruhautomation/ESP-MQTT-JSON-Digital-LEDs

## Disclaimer
This is **NOT** a tutorial, this is the code used in my project, as a way for me to store it and remind myself what I've done. Feel free to use it for non-commercial purposes but remember that I take no responsibility for any issues you might encounter, damaged electronics, house electrics etc.

Always be careful when working with electricity, especially with voltage from the mains, if you are uncertain, consult an electrician. Do not endanger yourself or your property. 
