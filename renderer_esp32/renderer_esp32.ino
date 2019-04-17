#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SD.h>
// #include "GifDecoder.h"
// #include "FilenameFunctions.h"
#include "Buttons_impl.h"
#include "Menu_impl.h"
#include "FileList_impl.h"
#include "Animation_impl.h"

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
#define ANIM_DIRECTORY "/anim"


Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);
Buttons buttons = Buttons(BTN_L, BTN_M, BTN_R);
FileList files = FileList(ANIM_DIRECTORY);

uint16_t screen[16384];


// Setup method runs once, when the sketch starts
void setup() {
    Serial.begin(115200);
    Serial.println("Starting AnimatedGIFs Sketch");

    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
    SPI.setClockDivider(2);

    // For 0.96", 1.44" and 1.8" TFT with ST7735 use
    tft.initR(INITR_144GREENTAB);

    tft.fillScreen(ST77XX_BLUE);

    Serial.print("Initializing SD card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("failed!");
        die("Failed to initialize SD card");
    }
    Serial.println("OK!");

    files.init();
}

void loop() {
    File fp;
    fp = SD.open(files.get_cur_file());
    if (!fp) {
        die("Failed to open file");
    }

    Animation anim = Animation(&tft, &fp, screen);
    if (anim.errstr) {
        // die(anim.errstr);
        files.next_file();
        return;
    }

    int next_time = millis() + (DISPLAY_TIME_SECONDS * 1000);
    while (1) {
        // Serial.println("read frame");
        if (anim.read_frame()) {
            // Serial.println("end of frames, rewind, continue");
            anim.rewind();
            continue;
        }
        // Serial.println("render frame");
        anim.render_frame();
        // Serial.println("delay frame");
        anim.delay_frame();

        buttons.check();
        if (buttons.l_btn()) {
            files.prev_file();
            break;
        }
        if (buttons.r_btn()) {
            files.next_file();
            break;
        }
        // if (buttons.m_btn()) {
        //     main_menu();
        // }
        if (millis() >= next_time) {
            files.next_file();
            break;
        }
    }

    fp.close();
    // static unsigned long futureTime;
    // static int change_gif = 0;

    // change_gif = change_gif ?: (futureTime < millis() ? 1 : 0);
    // if(change_gif != 0) {
    //     gifindex += change_gif;
    //     if (gifindex < 0) gifindex = num_files - 1;
    //     if (gifindex >= num_files) gifindex = 0;
    //     change_gif = 0;

    //     char pathname[128];

    //     getGIFFilenameByIndex(GIF_DIRECTORY, gifindex, pathname);

    //     if (decoder.openGifFilename(pathname) == 0) {
    //         decoder.startDecoding();
    //         futureTime = millis() + (DISPLAY_TIME_SECONDS * 1000);
    //     }

    //     // if (openGifFilenameByIndex(GIF_DIRECTORY, gifindex) >= 0) {
    //     //     // Can clear screen for new animation here, but this might cause flicker with short animations
    //     //     // matrix.fillScreen(COLOR_BLACK);
    //     //     // matrix.swapBuffers();

    //     //     decoder.startDecoding();

    //     //     // Calculate time in the future to terminate animation
    //     //     futureTime = millis() + (DISPLAY_TIME_SECONDS * 1000);
    //     // }
    // }

    // decoder.decodeFrame();

}

void main_menu() {
    MenuRenderer m = MenuRenderer(&tft, &buttons);
    const char * text[] = {
        "First entry",
        "Second entry",
        "Third entry",
        "Fourth really super long entry that won't fit",
    };
    Serial.println(m.render((const char **)text, 4));
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