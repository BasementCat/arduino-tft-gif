#ifndef GIFDEC_H
#define GIFDEC_H

// #include <stdint.h>
// #include <sys/types.h>
#include <SD.h>

typedef struct gd_RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} gd_RGBColor;

typedef struct gd_Palette {
    int size;
    uint16_t colors[256];
} gd_Palette;

typedef struct gd_GCE {
    uint16_t delay;
    uint8_t tindex;
    uint8_t disposal;
    int input;
    int transparency;
} gd_GCE;

typedef struct gd_Entry {
    uint16_t length;
    uint16_t prefix;
    uint8_t  suffix;
} gd_Entry;

typedef struct gd_Table {
    int bulk;
    int nentries;
    gd_Entry *entries;
} gd_Table;

typedef struct gd_GIF {
    File* fd;
    off_t anim_start;
    uint16_t width, height;
    uint16_t depth;
    uint16_t loop_count;
    gd_GCE gce;
    gd_Palette *palette;
    gd_Palette lct, gct;
    void (*plain_text)(
        struct gd_GIF *gif, uint16_t tx, uint16_t ty,
        uint16_t tw, uint16_t th, uint8_t cw, uint8_t ch,
        uint8_t fg, uint8_t bg
    );
    void (*comment)(struct gd_GIF *gif);
    void (*application)(struct gd_GIF *gif, char id[8], char auth[3]);
    uint16_t fx, fy, fw, fh;
    uint8_t bgindex;
    uint16_t *canvas;
    uint8_t *frame;
    gd_Table* table;
} gd_GIF;

gd_GIF *gd_open_gif(File* fd);
static gd_Table * new_table();
static void reset_table(gd_Table* table, int key_size);
// int add_entry(gd_Table* table, uint16_t length, uint16_t prefix, uint8_t suffix)
int gd_get_frame(gd_GIF *gif);
void gd_render_frame(gd_GIF *gif, uint16_t *buffer);
void gd_rewind(gd_GIF *gif);
void gd_close_gif(gd_GIF *gif);

#endif /* GIFDEC_H */
