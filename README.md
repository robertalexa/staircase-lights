# staircase-lights

Original project by @snikogos https://www.instructables.com/LED-Stair-Lighting/

Further adapted for nodeMCU by @daromer2 https://github.com/daromer2/StairLights

The code base is heavily modified. Code and variables have been cleaned.

For my use case, the project allows for:
- MQTT commands to set fixed lighting effects
- OTA updates

## Hardware
- nodeMCU v3
- 2 x AM312 PIR
- 1 x LDR
- 1 x 100 kOhm resistor for the LDR
- 1 x 500 Ohm resistor for the LED Data wire
- WS2812B LEDs
- 5V power supply - make sure you get a power supply able to supply the required Amps, plus 20% to allow for good performance. The closer to the limit you use a PSU, the more heat it will generate and the worst performance it will have.
- 18 awg cable

## Before you start
Rename `secrets.example.h` to `secrets.h` and fill in your credentials


This is **NOT** a tutorial, this is the code used in my project. Feel free to use it for  non-commercial purposes but remember that I take no responsibility for any issues you might encounter, damaged electronics, house electrics etc.

Always be careful when working with electricity, especially him voltage from the mains, if you are uncertain, consult an electrician. Do not endanger yourself or your property. 