#ifndef _MENUS_H_
#define _MENUS_H_

#include <Adafruit_ST7735.h>
#include "Buttons_impl.h"
#include "prefs.h"

// void version_menu(Adafruit_ST7735* tft, Buttons* buttons);
// void system_menu(Adafruit_ST7735* tft, Buttons* buttons, Prefs* prefs);
void main_menu(Adafruit_ST7735* tft, Buttons* buttons, Prefs* prefs);

#endif