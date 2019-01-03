/*
 * Animated GIFs Display Code for SmartMatrix and HUB75 RGB LED Panels
 *
 * Uses SmartMatrix Library written by Louis Beaudoin at pixelmatix.com
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
 * This SmartMatrix Library example displays GIF animations loaded from a SD Card connected to the Teensy 3
 *
 * The example can be modified to drive displays other than SmartMatrix by replacing SmartMatrix Library calls in setup() and
 * the *Callback() functions with calls to a different library
 *
 * This code has been tested with many size GIFs including 128x32, 64x64, 32x32, and 16x16 pixel GIFs, but is optimized for 32x32 pixel GIFs.
 *
 * Wiring is on the default Teensy 3.2 SPI pins, and chip select can be on any GPIO,
 * set by defining SD_CS in the code below.  For Teensy 3.5/3.6 with the onboard SDIO, change SD_CS to BUILTIN_SDCARD
 * Function     | Pin
 * DOUT         |  11
 * DIN          |  12
 * CLK          |  13
 * CS (default) |  15
 *
 * Wiring for ESP32 follows the default for the ESP32 SD Library, see: https://github.com/espressif/arduino-esp32/tree/master/libraries/SD
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
 *  - If you're not using SmartLED Shield V4 (or above), comment out the line that includes <SmartMatrixShieldV4.h>
 *  - update the "SmartMatrix configuration and memory allocation" section to match the width and height and other configuration of your display
 *  - Note for 128x32 and 64x64 displays with Teensy 3.2 - need to reduce RAM:
 *    set kRefreshDepth=24 and kDmaBufferRows=2 or set USB Type: "None" in Arduino,
 *    decrease refreshRate in setup() to 90 or lower to get good an accurate GIF frame rate
 *  - Set the chip select pin for your board.  On Teensy 3.5/3.6, the onboard microSD CS pin is "BUILTIN_SDCARD"
 *  - For ESP32 and large panels, you don't need to lower the refreshRate, but you can lower the frameRate (number of times the refresh buffer
 *    is updaed with new data per second), giving more time for the CPU to decode the GIF.
 *    Use matrix.setMaxCalculationCpuPercentage() or matrix.setCalcRefreshRateDivider()
 */

#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SD.h>
#include "GifDecoder.h"
#include "FilenameFunctions.h"
#include "Buttons_impl.h"
#include "Menu_impl.h"

// Definitions of pin numbers for the TFT
// SPI pins are hardware and do not need to be defined
#define TFT_CS  13  // TFT chip select line
#define TFT_RST 27  // TFT reset line
#define TFT_DC  33  // TFT data/command line
#define SD_CS   15  // SD chip select line
// Unused for now
// #define TFT_BL  99  // TFT backlight PWM line

// Buttons - will use these later
#define BTN_L   4
#define BTN_M   25
#define BTN_R   26

#define DISPLAY_TIME_SECONDS 10

/* template parameters are maxGifWidth, maxGifHeight, lzwMaxBits
 * 
 * The lzwMaxBits value of 12 supports all GIFs, but uses 16kB RAM
 * lzwMaxBits can be set to 10 or 11 for smaller displays to save RAM, but use 12 for large displays
 * All 32x32-pixel GIFs tested so far work with 11, most work with 10
 */
// GifDecoder<128, 128, 12> decoder = GifDecoder<128, 128, 12>();
GifDecoder<128, 128, 12> decoder;
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);
Buttons buttons = Buttons(BTN_L, BTN_M, BTN_R);

#define GIF_DIRECTORY "/gifs"

int num_files;
uint16_t screen[16384];

void screenClearCallback(void) {
    // Serial.println("clear");
    tft.fillScreen(ST77XX_BLACK);
}

void updateScreenCallback(void) {
    // Serial.println("update");
    tft.startWrite();
    tft.writePixels(screen, 16384);
    tft.endWrite();
}

void drawPixelCallback(int16_t x, int16_t y, uint16_t color) {
    screen[(y * 128) + x] = color;
}

// Setup method runs once, when the sketch starts
void setup() {
    decoder.setScreenClearCallback(screenClearCallback);
    decoder.setUpdateScreenCallback(updateScreenCallback);
    decoder.setDrawPixelCallback(drawPixelCallback);

    Serial.begin(115200);
    Serial.println("Starting AnimatedGIFs Sketch");

    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    // For 0.96", 1.44" and 1.8" TFT with ST7735 use
    tft.initR(INITR_144GREENTAB);

    tft.fillScreen(ST77XX_BLUE);

    Serial.print("Initializing SD card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("failed!");
        die("Failed to initialize SD card");
    }
    Serial.println("OK!");


    // Determine how many animated GIF files exist
    num_files = enumerateGIFFiles(GIF_DIRECTORY, false);

    if(num_files < 0) {
        Serial.println("No gifs directory");
        die("No /gifs directory");
    }

    if(!num_files) {
        Serial.println("Empty gifs directory");
        die("No files in /gifs");
    }
}

int gifindex = 0;

// void loop() {
//     static unsigned long futureTime;
//     static int change_gif = 0;

//     change_gif = change_gif ?: (futureTime < millis() ? 1 : 0);
//     if(change_gif != 0) {
//         gifindex += change_gif;
//         if (gifindex < 0) gifindex = num_files - 1;
//         if (gifindex >= num_files) gifindex = 0;
//         change_gif = 0;

//         char pathname[128];

//         getGIFFilenameByIndex(GIF_DIRECTORY, gifindex, pathname);

//         if (decoder.openGifFilename(pathname) == 0) {
//             decoder.startDecoding();
//             futureTime = millis() + (DISPLAY_TIME_SECONDS * 1000);
//         }

//         // if (openGifFilenameByIndex(GIF_DIRECTORY, gifindex) >= 0) {
//         //     // Can clear screen for new animation here, but this might cause flicker with short animations
//         //     // matrix.fillScreen(COLOR_BLACK);
//         //     // matrix.swapBuffers();

//         //     decoder.startDecoding();

//         //     // Calculate time in the future to terminate animation
//         //     futureTime = millis() + (DISPLAY_TIME_SECONDS * 1000);
//         // }
//     }

//     decoder.decodeFrame();

//     buttons.check();
//     if (buttons.l_btn()) {
//         change_gif = -1;
//         while (buttons.l_btn());
//     }
//     if (buttons.r_btn()) {
//         change_gif = 1;
//         while (buttons.r_btn());
//     }
// }

void loop() {
    MenuRenderer m = MenuRenderer(&tft, &buttons);
    const char * text[] = {
        "First entry",
        "Second entry",
        "Third entry",
        "Fourth really super long entry that won't fit",
    };
    m.render((const char **)text, 4);
    delay(10000);
}


void die(const char *message) {
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextWrap(true);

    tft.setCursor(10, 15);
    tft.setTextColor(tft.color565(255, 0, 0));
    tft.setTextSize(3);
    tft.println("ERROR");

    tft.setCursor(0, 50);
    tft.setTextColor(0xffff);
    tft.setTextSize(1);
    tft.println(message);
    while (true) delay(1000);
}