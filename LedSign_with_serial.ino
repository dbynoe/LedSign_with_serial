/*
 * Animated GIFs Display Code for SmartMatrix and 32x32 RGB LED Panels
 *
 * Uses SmartMatrix Library for Teensy 3.1 written by Louis Beaudoin at pixelmatix.com
 *
 * Written by: Craig A. Lindley
 *
 * Copyright (c) 2014 Craig A. Lindley
 * Refactoring by Louis Beaudoin (Pixelmatix)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * This example displays 32x32 GIF animations loaded from a SD Card connected to the Teensy 3.1
 * The GIFs can be up to 32 pixels in width and height.
 * This code has been tested with 32x32 pixel and 16x16 pixel GIFs, but is optimized for 32x32 pixel GIFs.
 *
 * Wiring is on the default Teensy 3.1 SPI pins, and chip select can be on any GPIO,
 * set by defining SD_CS in the code below
 * Function     | Pin
 * DOUT         |  11
 * DIN          |  12
 * CLK          |  13
 * CS (default) |  15
 *
 * This code first looks for .gif files in the /gifs/ directory
 * (customize below with the GIF_DIRECTORY definition) then plays random GIFs in the directory,
 * looping each GIF for DISPLAY_TIME_SECONDS
 *
 * This example is meant to give you an idea of how to add GIF playback to your own sketch.
 * For a project that adds GIF playback with other features, take a look at
 * Light Appliance and Aurora:
 * https://github.com/CraigLindley/LightAppliance
 * https://github.com/pixelmatix/aurora
 *
 * If you find any GIFs that won't play properly, please attach them to a new
 * Issue post in the GitHub repo here:
 * https://github.com/pixelmatix/AnimatedGIFs/issues
 */

/*
 * CONFIGURATION:
 *  - update the "SmartMatrix configuration and memory allocation" section to match the width and height and other configuration of your display
 *  - Note for 128x32 and 64x64 displays - need to reduce RAM:
 *    set kRefreshDepth=24 and kDmaBufferRows=2 or set USB Type: "None" in Arduino,
 *    decrease refreshRate in setup() to 90 or lower to get good an accurate GIF frame rate
 *  - WIDTH and HEIGHT are defined in GIFParseFunctions.cpp, update to match the size of your GIFs
 *    only play GIFs that are size WIDTHxHEIGHT or smaller
 */

#include <SmartMatrix3.h>
#include <FastLED.h>
#include <SPI.h>
#include <SD.h>
#include "GIFDecoder.h"

#define HWSERIAL Serial1 //hardware serial for accepting data from external esp8266 wifi board. 

#define DISPLAY_TIME_SECONDS 10 //how long to display an animated GIF

#define CONWAY_UPDATE 70 //how long between generations for conways game of life. 

//set of slogans to scroll on the board randomly, progmem drops the data into flash rather than ram
const char* slogans[] PROGMEM = {"Protospace: No longer a complete embarrassment",
                                 "Protospace: Our website doesn't suck anymore",
                                 "Protospace: I am pretty sure it was on fire when I got here",
                                 "Protospace: When this baby gets up to 88 miles an hour, you're going to see some serious shit.",
                                 "Hack the planet.        Hack the planet.        Hack the planet.        Hack the planet.",
                                 "Protospace: It's amazing how productive doing nothing can be.",
                                 "Protospace: A new life awaits you in the off-world colonies",
                                 "Protospace: Now 17% less half assed",
                                 "Protospace: We make space, you make things",
                                 "Protospace: BIKESHED BIKESHED BIKESHED BIKESHED ..... DO-OCRACY",
                                 "Protospace: Now with 17% more awesome",
                                 "First Law of Kipple: Kipple drives out nonkipple.",
                                 "Protospace: Listen! Do you smell something?",
                                 "Protospace: In retrospect, we probally should have gone with the blue pill",
                                 "Protospace: Future home of the 2023 robot uprising",
                                 "KILL ALL HUMANS, except David, he's cool."
                                };

// range 0-255

const rgb24 defaultBackgroundColor = {0x40, 0, 0};

const rgb24 COLOR_BLACK = {0, 0, 0};


char pathname[30];

/* SmartMatrix configuration and memory allocation */
#define COLOR_DEPTH 24                  // known working: 24, 48 - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24
const uint8_t kMatrixWidth = 64;        // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 16;       // known working: 16, 32, 48, 64
const uint8_t kRefreshDepth = 36;       // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 2;       // known working: 2-4
const uint8_t kPanelType = SMARTMATRIX_HUB75_16ROW_MOD8SCAN;
const uint8_t kMatrixOptions = (SMARTMATRIX_OPTIONS_NONE);    // see http://docs.pixelmatix.com/SmartMatrix for options
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);

// Chip select for SD card on the SmartMatrix Shield
#define SD_CS 1

#define GIF_DIRECTORY "/gifs/"

elapsedMillis sinceUpdate;
float nextUpdate;
boolean changeDisplay = true;
byte modeSwitch = 0;
byte alertCounter = 0;
boolean flasher = false;

String inputString = "";   // a string to hold incoming data
String printString = "";  // a string for the screen so it can recieve while printing
boolean stringComplete = false;  // whether the string is complete


class Cell {
  public:
    byte alive =  1;
    byte prev =  1;
    byte hue = 6;
    byte brightness;
};

Cell world[kMatrixWidth][kMatrixHeight];
long density = 10;
int generation = 0;
// adjust the amount of blur
float blurAmount = 1;


CRGBPalette16 currentPalette = PartyColors_p;


int num_files;

void screenClearCallback(void) {
  backgroundLayer.fillScreen({0, 0, 0});
}

void updateScreenCallback(void) {
  backgroundLayer.swapBuffers();
}

void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue) {
  backgroundLayer.drawPixel(x, y, {red, green, blue});
}

void setup() {

  pinMode(0, INPUT);

  setScreenClearCallback(screenClearCallback);
  setUpdateScreenCallback(updateScreenCallback);
  setDrawPixelCallback(drawPixelCallback);

  // Seed the random number generator
  randomSeed(analogRead(14));

  HWSERIAL.begin(115200);


  inputString.reserve(200);
  // Initialize matrix
  matrix.addLayer(&backgroundLayer);

  matrix.addLayer(&scrollingLayer);


  matrix.begin();

  // matrix.setRefreshRate(90);
  // for large panels, set the refresh rate lower to leave more CPU time to decoding GIFs (needed if GIFs are playing back slowly)

  // Clear screen
  backgroundLayer.fillScreen(COLOR_BLACK);
  backgroundLayer.swapBuffers();

  // initialize the SD card at full speed
  pinMode(SD_CS, OUTPUT);
  if (!SD.begin(SD_CS)) {

    scrollingLayer.start("No SD card", -1);

    Serial.println("No SD card");
    while (1);
  }

  // Determine how many animated GIF files exist
  num_files = enumerateGIFFiles(GIF_DIRECTORY, false);

  if (num_files < 0) {

    scrollingLayer.start("No gifs directory", -1);

    Serial.println("No gifs directory");
    while (1);
  }

  if (!num_files) {

    scrollingLayer.start("Empty gifs directory", -1);

    Serial.println("Empty gifs directory");
    while (1);
  }
}


void loop() {

  if (HWSERIAL.available()) { //data coming in on the hardware serial port (pin 0)

    while (HWSERIAL.available()) {
      // get the new byte:
      char inChar = (char)HWSERIAL.read();
      // add it to the inputString:
      inputString += inChar;
      // if the incoming character is a newline, set a flag
      // so the main loop can do something about it:
      if (inChar == '\n') {
        printString = inputString;
        inputString = "";
        stringComplete = true;
      }
    }
  }

  if  (stringComplete) { //incoming message from the big giant head
    modeSwitch = 0;
    alertCounter = 0;
    flasher = true;
    changeDisplay = false;
    scrollingLayer.setColor({255, 255, 255});
    scrollingLayer.setMode(wrapForward);
    scrollingLayer.setSpeed(60);
    scrollingLayer.setFont(font8x13);
    scrollingLayer.setOffsetFromTop(1);
    stringComplete = false;
    scrollingLayer.stop();
    nextUpdate = 0;
  }


  if (changeDisplay) { //changes which display is running. 
    int rndEvent = random(0, 10); //randomize the next display
    if (rndEvent > 8) { //play an animated gif
      modeSwitch = 1;
      getGIFFilenameByIndex(GIF_DIRECTORY, random(num_files), pathname);
      nextUpdate = DISPLAY_TIME_SECONDS * 1000;

      //nextUpdate =0;
      sinceUpdate = 0;
    }
    else if (rndEvent < 2)//scroll some text while displaying conway in the back

    { modeSwitch = 2; 
      generation = 0;
      scrollingLayer.setColor({random(120, 254), random(120, 254), random(120, 254)});
      scrollingLayer.setMode(wrapForward);
      scrollingLayer.setSpeed(60);
      scrollingLayer.setFont(font5x7);
      scrollingLayer.setOffsetFromTop(4);
      int rndSlogan = random (0, (sizeof(slogans) / sizeof(char*)));
      scrollingLayer.start(slogans[rndSlogan], 1);
      nextUpdate = CONWAY_UPDATE ;
      sinceUpdate = 0;
    }
    else {//straight up conway for all other cases 
      modeSwitch = 3;
      generation = 0;
      nextUpdate = CONWAY_UPDATE;
      sinceUpdate = 0;
      conway();
    }
    changeDisplay = false;
  }
  else { // no change, continue running
    switch (modeSwitch) {
      case 0: // serial data display
        if (sinceUpdate >= nextUpdate) { //blink 4 times before scrolling text
          if (flasher) {
            backgroundLayer.fillScreen(COLOR_BLACK);
            backgroundLayer.swapBuffers();
          }
          else {
            backgroundLayer.fillScreen({110, 0, 0});
            backgroundLayer.swapBuffers();
          }
          flasher = !flasher;
          nextUpdate = 130;
          sinceUpdate = 0;
          alertCounter++;
        }
        if (alertCounter == 5) {
          scrollingLayer.start(printString.c_str(), 2); //start scrolling text twice
          alertCounter++;
        }
        if (alertCounter > 6) {
          if (scrollingLayer.getStatus() == 0) { //cool we are done scrolling lets move along to the next thing
            scrollingLayer.stop();
            printString = "";
            changeDisplay = true;
          }
        }
        break;
      case 1: //playing an animated GIF
        processGIFFile(pathname);
        if (sinceUpdate > nextUpdate) {
          changeDisplay = true;
          //
        }

        break;
      case 2:
        conway();
        break;
      case 3:
        conway();
        break;

    }

  }



}

/*
 * Portions of the code below are adapted from Jason: https://github.com/pup05/SmartMatrixLife
 * which was in turn adapted from Andrew: http://pastebin.com/f22bfe94d
 * which, in turn, was "Adapted from the Life example on the Processing.org site"
 *
 * Made much more colorful by J.B. Langston: https://github.com/jblang/aurora/commit/6db5a884e3df5d686445c4f6b669f1668841929b
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


void conway() {

  if (sinceUpdate >= CONWAY_UPDATE) {


    if (generation == 0) { //start by randomly filling the screen. 
      density = random(30, 50);
      backgroundLayer.fillScreen(defaultBackgroundColor);
      randomFillWorld();
      chooseNewPalette();
      blurAmount = ((float)random(30, 60)) / 100.0;

    }

    // Display current generation
    for (int i = 0; i < kMatrixWidth; i++) {
      for (int j = 0; j < kMatrixHeight; j++) {

        backgroundLayer.drawPixel(i, j, ColorFromPalette(currentPalette, world[i][j].hue * 4, world[i][j].brightness));

      }
    }

    // Birth and death cycle
    for (int x = 0; x < kMatrixWidth; x++) {
      for (int y = 0; y < kMatrixHeight; y++) {
        // Default is for cell to stay the same
        if (world[x][y].brightness > 0 && world[x][y].prev == 0)
          world[x][y].brightness *= blurAmount;
        int count = neighbours(x, y);
        if (count == 3 && world[x][y].prev == 0) {
          // A new cell is born
          world[x][y].alive = 1;
          world[x][y].hue += 2;
          world[x][y].brightness = 255;
        }
        else if ((count < 2 || count > 3) && world[x][y].prev == 1) {
          // Cell dies
          world[x][y].alive = 0;
        }
      }
    }

    // Copy next generation into place
    for (int x = 0; x < kMatrixWidth; x++) {
      for (int y = 0; y < kMatrixHeight; y++) {
        world[x][y].prev = world[x][y].alive;
      }
    }

    generation++;
    if (generation >= 300) {
      generation = 0;
      scrollingLayer.stop();
      changeDisplay = true;
    }


    backgroundLayer.swapBuffers();
    sinceUpdate = 0;
  }


}

void randomFillWorld() {
  for (int i = 0; i < kMatrixWidth; i++) {
    for (int j = 0; j < kMatrixHeight; j++) {
      if (random(0, 100) < density) {
        world[i][j].alive = 1;
        world[i][j].brightness = 255;
      }
      else {
        world[i][j].alive = 0;
        world[i][j].brightness = 0;
      }
      world[i][j].prev = world[i][j].alive;
      world[i][j].hue = 0;
    }
  }
}

int neighbours(int x, int y) {
  return (world[(x + 1) % kMatrixWidth][y].prev) +
         (world[x][(y + 1) % kMatrixHeight].prev) +
         (world[(x + kMatrixWidth - 1) % kMatrixWidth][y].prev) +
         (world[x][(y + kMatrixHeight - 1) % kMatrixHeight].prev) +
         (world[(x + 1) % kMatrixWidth][(y + 1) % kMatrixHeight].prev) +
         (world[(x + kMatrixWidth - 1) % kMatrixWidth][(y + 1) % kMatrixHeight].prev) +
         (world[(x + kMatrixWidth - 1) % kMatrixWidth][(y + kMatrixHeight - 1) % kMatrixHeight].prev) +
         (world[(x + 1) % kMatrixWidth][(y + kMatrixHeight - 1) % kMatrixHeight].prev);
}

uint16_t XY( uint8_t x, uint8_t y) {
  return (y * kMatrixWidth) + x;
}

void chooseNewPalette() {
  switch (random(0, 8)) {
    case 0:
      currentPalette = CloudColors_p;

      break;

    case 1:
      currentPalette = ForestColors_p;

      break;

    case 2:
      currentPalette = HeatColors_p;

      break;

    case 3:
      currentPalette = LavaColors_p;

      break;

    case 4:
      currentPalette = OceanColors_p;

      break;

    case 5:
      currentPalette = PartyColors_p;

      break;

    case 6:
      currentPalette = RainbowColors_p;

      break;

    case 7:
    default:
      currentPalette = RainbowStripeColors_p;

      break;
  }
}




