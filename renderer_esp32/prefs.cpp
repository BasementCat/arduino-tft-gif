#include <SD.h>
#include "prefs.h"

void set_pref_last_filename(Prefs* prefs, const char* filename) {
    if (prefs->last_filename != NULL) {
        prefs->last_filename_len = 0;
        free(prefs->last_filename);
        prefs->last_filename = NULL;
    }

    if (filename == NULL)
        return;

    prefs->last_filename_len = strlen(filename);
    prefs->last_filename = (char*)malloc(prefs->last_filename_len + 1);
    strncpy(prefs->last_filename, filename, prefs->last_filename_len);
    prefs->last_filename[prefs->last_filename_len] = '\0';
}

void write_prefs(Prefs* prefs) {
    File file;
    file = SD.open(PREFS_FILENAME, FILE_WRITE);
    if (!file) {
        Serial.print("Can't write to ");
        Serial.println(PREFS_FILENAME);
        return;
    }

    prefs->version = PREFS_VERSION;
    file.write((uint8_t*)&prefs->version, 2);
    file.write((uint8_t*)&prefs->display_time_s, 2);
    file.write((uint8_t*)&prefs->last_filename_len, 2);
    if (prefs->last_filename_len > 0) {
        file.write((uint8_t*)&prefs->last_filename, prefs->last_filename_len);
    }

    file.close();
}

void read_prefs(Prefs* prefs) {
    File file;

    prefs->version = PREFS_VERSION;
    prefs->display_time_s = 10;
    prefs->last_filename_len = 0;
    prefs->last_filename = NULL;

    file = SD.open(PREFS_FILENAME);
    if (!file) {
        write_prefs(prefs);
        return;
    }

    file.read((uint8_t*)&prefs->version, 2);
    if (prefs->version >= 1) {
        file.read((uint8_t*)&prefs->display_time_s, 2);
        file.read((uint8_t*)&prefs->last_filename_len, 2);
        if (prefs->last_filename_len > 0) {
            prefs->last_filename = (char*)malloc(prefs->last_filename_len + 1);
            file.read((uint8_t*)prefs->last_filename, prefs->last_filename_len);
            prefs->last_filename[prefs->last_filename_len] = '\0';
        }
    }
    file.close();
}
