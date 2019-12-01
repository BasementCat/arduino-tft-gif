#ifndef _PREFS_H_
#define _PREFS_H_

#include <SD.h>

#define PREFS_VERSION 2
#define PREFS_FILENAME "/preferences.bin"

typedef struct {
    uint16_t version;
    uint16_t display_time_s;
    char last_filename[128];
    uint8_t brightness;
} __attribute__ ((packed)) Prefs;

void set_pref_last_filename(Prefs* prefs, const char* filename);
void write_prefs(Prefs* prefs);
void read_prefs(Prefs* prefs);

#endif