#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SD.h>

#include "gifdec.h";

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

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} colortype;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

#define FILENAME "/128_commscafedragon_by_metallicumbrage-dcv6bo5.gif"
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


uint16_t color565(colortype c) {
    return ((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | ((c.b & 0xF8) >> 3);
}


void loop() {
    File fp;
    fp = SD.open(FILENAME);
    if (!fp) {
        die("Failed to open file");
    }

    // Serial.println("Open gif");
    gd_GIF *gif = gd_open_gif(&fp);
    int res;
    // Serial.println("Alloc buffer");
    colortype *buffer = (colortype*) malloc(gif->width * gif->height * 3);
    // Serial.println("Loop start");
    for (unsigned looped = 1;; looped++) {
        // Serial.println("Loop frames");
        while (1) {
            res = gd_get_frame(gif);
            if (res < 0) {
                die("failure");
            } else if (res == 0) {
                break;
            }
            // Serial.println("Got frame, rendering");
            gd_render_frame(gif, (uint8_t*) buffer);
            // Serial.println("Copy to screen");
            for (int i = 0; i < gif->width * gif->height; i++) {
                screen[i] = color565(buffer[i]);
            }
            // Serial.println("Render to tft");
            tft.startWrite();
            tft.writePixels(screen, 16384);
            tft.endWrite();
        }
        if (looped == gif->loop_count)
            break;
        // Serial.println("Rewind");
        gd_rewind(gif);
    }
    // Serial.println("Clear buffer, close file");
    free(buffer);
    gd_close_gif(gif);
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
