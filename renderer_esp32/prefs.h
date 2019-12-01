#ifndef _PREFS_H_
#define _PREFS_H_

#include <SD.h>

#define PREFS_VERSION 1
#define PREFS_FILENAME "/preferences.bin"

typedef struct {
    uint16_t version;
    uint16_t display_time_s;
    uint16_t last_filename_len;
    char* last_filename;
} Prefs;

void set_pref_last_filename(Prefs* prefs, const char* filename);
void write_prefs(Prefs* prefs);
void read_prefs(Prefs* prefs);

#endif