#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SD.h>
#include "Buttons_impl.h"
#include "Menu_impl.h"
#include "FileList_impl.h"
#include "prefs.h"
#include "gifdec.h"
#include "menus.h"
#include "version.h"

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

#define GIFS_DIRECTORY "/"


Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);
Buttons buttons = Buttons(BTN_L, BTN_M, BTN_R);
FileList files = FileList(GIFS_DIRECTORY);
Prefs prefs;

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

    read_prefs(&prefs);
    files.init(&prefs);
}

void loop() {
    File fp;
    int t_fstart=0, t_delay=0, t_real_delay, res, next_time, delay_until;
    next_time = millis() + (prefs.display_time_s * 1000);

    fp = SD.open(files.get_cur_file());
    if (!fp) {
        files.next_file(&prefs);
        return;
    }

    gd_GIF *gif = gd_open_gif(&fp);
    if (!gif) {
        files.next_file(&prefs);
        return;
    }

    while (1) {
        t_delay = gif->gce.delay * 10;
        res = gd_get_frame(gif);
        if (res < 0) {
            die("failure");
        } else if (res == 0) {
            gd_rewind(gif);
            t_fstart = 0;
            continue;
        }
        gd_render_frame(gif, screen);
        t_real_delay = t_delay - (millis() - t_fstart);
        delay_until = millis() + t_real_delay;
        do {
            buttons.check();
            if (buttons.l_btn()) {
                files.prev_file(&prefs);
                goto end_loop;
            }
            if (buttons.r_btn()) {
                files.next_file(&prefs);
                goto end_loop;
            }
            if (buttons.m_btn()) {
                main_menu(&tft, &buttons, &prefs);
            }
        } while (millis() < delay_until);

        tft.startWrite();
        tft.writePixels(screen, 16384);
        tft.endWrite();
        t_fstart = millis();

        if (prefs.display_time_s < 1000 && millis() >= next_time) {
            files.next_file(&prefs);
            goto end_loop;
        }
    }

end_loop:

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