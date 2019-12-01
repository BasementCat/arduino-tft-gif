#include <Adafruit_ST7735.h>
#include "Buttons_impl.h"
#include "Menu_impl.h"
#include "prefs.h"
#include "version.h"
#include "battery.h"
#include "menus.h"


void battery_menu(Adafruit_ST7735* tft, Buttons* buttons) {
    MenuRenderer m = MenuRenderer(tft, buttons);
    char voltage[7], percent[4];
    dtostrf(read_battery_voltage(), 5, 3, voltage);
    strcat(voltage, "v");
    itoa(read_battery_percent(), percent, 10);
    strcat(percent, "%");
    const char * text[3] = {
        "Back",
        voltage,
        percent
    };
    m.render((const char **)text, 3);
}

void version_menu(Adafruit_ST7735* tft, Buttons* buttons) {
    MenuRenderer m = MenuRenderer(tft, buttons);
    const char * text[] = {
        "Back",
        VERSION
    };
    m.render((const char **)text, 2);
}

void system_menu(Adafruit_ST7735* tft, Buttons* buttons) {
    MenuRenderer m = MenuRenderer(tft, buttons);
    const char * text[] = {
        "Back",
        "Battery",
        "Version",
        "Update From SD"
    };
    while (1) {
        switch (m.render((const char **)text, 3)) {
            case 0:
                return;
            case 1:
                battery_menu(tft, buttons);
                break;
            case 2:
                version_menu(tft, buttons);
                break;
            case 3:
                break;
        }
    }
}

uint16_t prefs_disp_time_menu(Adafruit_ST7735* tft, Buttons* buttons) {
    MenuRenderer m = MenuRenderer(tft, buttons);
    const char * text[] = {
        "Back",
        "Forever",
        "5s",
        "10s",
        "15s",
        "20s",
        "30s",
        "1m"
    };
    while (1) {
        switch (m.render((const char **)text, 8)) {
            case 0:
                return 0;
            case 1:
                return 1234;
            case 2:
                return 5;
            case 3:
                return 10;
            case 4:
                return 15;
            case 5:
                return 20;
            case 6:
                return 30;
            case 7:
                return 60;
        }
    }
}

uint8_t prefs_bri_menu(Adafruit_ST7735* tft, Buttons* buttons) {
    MenuRenderer m = MenuRenderer(tft, buttons);
    const char * text[] = {
        "Back",
        "10%",
        "20%",
        "30%",
        "40%",
        "50%",
        "60%",
        "70%",
        "80%",
        "90%",
        "100%"
    };
    while (1) {
        switch (m.render((const char **)text, 11)) {
            case 0:
                return 0;
            case 1:
                return 25;
            case 2:
                return 50;
            case 3:
                return 75;
            case 4:
                return 100;
            case 5:
                return 125;
            case 6:
                return 150;
            case 7:
                return 175;
            case 8:
                return 200;
            case 9:
                return 225;
            case 10:
                return 255;
        }
    }
}


void preferences_menu(Adafruit_ST7735* tft, Buttons* buttons, Prefs* prefs) {
    MenuRenderer m = MenuRenderer(tft, buttons);
    uint16_t disp_time;
    uint8_t bri;
    const char * text[] = {
        "Back",
        "Display Time",
        "Brightness"
    };
    while (1) {
        switch (m.render((const char **)text, 3)) {
            case 0:
                return;
            case 1:
                disp_time = prefs_disp_time_menu(tft, buttons);
                if (disp_time > 0) {
                    prefs->display_time_s = disp_time;
                    write_prefs(prefs);
                }
                break;
            case 2:
                bri = prefs_bri_menu(tft, buttons);
                if (bri > 0) {
                    prefs->brightness = bri;
                    write_prefs(prefs);
                    // ledcWrite(TFT_BL_CHAN, prefs->brightness);
                }
                break;
        }
    }
}

void main_menu(Adafruit_ST7735* tft, Buttons* buttons, Prefs* prefs) {
    MenuRenderer m = MenuRenderer(tft, buttons);
    const char * text[] = {
        "Back",
        // "Pick GIF",
        "Preferences",
        "System",
        // "Too",
        // "Many",
        // "Items",
        // "To",
        // "Fit",
        // "On",
        // "One",
        // "Screen",
        // "For",
        // "real",
        // "this",
        // "time",
        // "too long asldkfjsk alwekjjl;a aaaaaaaaaaaaaaaaaaa",
    };
    while (1) {
        switch (m.render((const char **)text, 3)) {
            case 0:
                return;
            case 1:
                preferences_menu(tft, buttons, prefs);
                break;
            case 2:
                system_menu(tft, buttons);
                break;
        }
    }
}