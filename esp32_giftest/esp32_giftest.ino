#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SD.h>

#include "anim_impl.h";

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

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

#define FILENAME "/128_commscafedragon_by_metallicumbrage-dcv6bo5.anim"
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

uint16_t screen[16384];

// Setup method runs once, when the sketch starts
void setup() {
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
}


void loop() {
    File fp;
    fp = SD.open(FILENAME);
    if (!fp) {
        die("Failed to open file");
    }

    Animation anim = Animation(&tft, &fp, screen);

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
    }
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
