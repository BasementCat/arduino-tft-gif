#ifndef _PREFS_IMPL_H_
#define _PREFS_IMPL_H_

#include <SD.h>

#define PREFS_VERSION 1
#define PREFS_FILENAME "preferences.bin"

typedef struct {
    uint16_t version;
    uint16_t display_time_s;
    uint16_t last_filename_len;
    char* last_filename;
} Prefs;

Prefs prefs;

void set_pref_last_filename(const char* filename) {
    if (prefs.last_filename != NULL) {
        prefs.last_filename_len = 0;
        free(prefs.last_filename);
        prefs.last_filename = NULL;
    }

    if (filename == NULL)
        return;

    prefs.last_filename_len = strlen(filename);
    prefs.last_filename = (char*)malloc(prefs.last_filename_len + 1);
    strncpy(prefs.last_filename, filename, prefs.last_filename_len);
    prefs.last_filename[prefs.last_filename_len] = '\0';
}

void write_prefs() {
    File file;
    file = SD.open(PREFS_FILENAME, FILE_WRITE);
    if (!file)
        return;

    prefs.version = PREFS_VERSION;
    file.write((uint8_t*)&prefs.version, 2);
    file.write((uint8_t*)&prefs.display_time_s, 2);
    file.write((uint8_t*)&prefs.last_filename_len, 2);
    if (prefs.last_filename_len > 0) {
        file.write((uint8_t*)&prefs.last_filename, prefs.last_filename_len);
    }

    file.close();
}

void read_prefs() {
    File file;

    prefs.version = PREFS_VERSION
    prefs.display_time_s = 10;
    prefs.last_filename_len = 0;
    prefs.last_filename = NULL;

    file = SD.open(PREFS_FILENAME);
    if (!file) {
        write_prefs();
        return;
    }

    file.read((uint16_t*)&prefs.version, 2);
    if (prefs.version >= 1) {
        file.read((uint8_t*)&prefs.display_time_s, 2);
        file.read((uint8_t*)&prefs.last_filename_len, 2);
        if (prefs.last_filename_len > 0) {
            prefs.last_filename = (char*)malloc(prefs.last_filename_len + 1);
            file.read((uint8_t*)prefs.last_filename, prefs.last_filename_len);
            prefs.last_filename[prefs.last_filename_len] = '\0';
        }
    }
    file.close();
}

#endif