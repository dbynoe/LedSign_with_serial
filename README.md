# LedSign_with_serial
LED display with Animated GIF Playback, Conway's Game of Life, and incoming serial data scrolling. 
This is the code used to drive the blinky light display at Calgary Protospace. 

Principal modifications to the SmartMatrix and Conway code include: 
 - Removing delay() functions and replacing them with a timer, allowing repeated trips through loop()
 - Adding in hooks to escape and return from the animated GIF player mid playback on reciving incoming serial data
 - Adding in a hardware serial recieve port on pin 0 for connecting to an external wifi bridge

For hardware you will need:
 - the SmartMatrix library from: https://github.com/pixelmatix/SmartMatrix 
 - a teensy 3.1
 - an microsd card breakout board
 - a pair of 16x32 RGB led matrix panels
 - a beefy 5v 4 amp+ power supply
 - optional: SmartMatrix breakout board
 - optional: esp8266 board or similar to pushing serial events to the display over wifi
