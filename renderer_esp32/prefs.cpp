#include <SD.h>
#include "prefs.h"

void set_pref_last_filename(Prefs* prefs, const char* filename) {
    prefs->last_filename[0] = 0;

    if (filename == NULL)
        return;

    strcpy(prefs->last_filename, filename);
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
    file.write((uint8_t*)prefs, sizeof(Prefs));
    file.close();
}

void read_prefs(Prefs* prefs) {
    File file;
    uint16_t version;

    prefs->version = PREFS_VERSION;
    prefs->display_time_s = 10;
    prefs->last_filename[0] = 0;
    prefs->brightness = 255;

    file = SD.open(PREFS_FILENAME);
    if (!file) {
        write_prefs(prefs);
        return;
    }

    file.read((uint8_t*) &version, 2);
    if (version < 1 || version > 2) {
        Serial.print("Invalid prefs version, expected ");
        Serial.print(PREFS_VERSION);
        Serial.print(", got ");
        Serial.println(version);
        file.close();
        return;
    }
    file.seek(0);

    if (version == 1)
        file.read((uint8_t*)prefs, 132);
    else if (version == 2)
        file.read((uint8_t*)prefs, 133);
    file.close();
}
