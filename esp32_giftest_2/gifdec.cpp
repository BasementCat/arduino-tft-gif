#include "gifdec.h"

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

#include <sys/types.h>
// #include <sys/stat.h>
// #include <fcntl.h>
// #include <unistd.h>

#include <SD.h>

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))


static uint16_t
read_num(File* fd)
{
    uint8_t bytes[2];

    fd->read(bytes, 2);
    return bytes[0] + (((uint16_t) bytes[1]) << 8);
}

gd_GIF *
gd_open_gif(File* fd)
{
    uint8_t sigver[3];
    uint16_t width, height, depth;
    uint8_t fdsz, bgidx, aspect;
    int gct_sz;
    gd_GIF *gif;

    /* Header */
    fd->read(sigver, 3);
    if (memcmp(sigver, "GIF", 3) != 0) {
        Serial.println("invalid signature");
        goto fail;
    }
    /* Version */
    fd->read(sigver, 3);
    if (memcmp(sigver, "89a", 3) != 0) {
        Serial.println("invalid version");
        goto fail;
    }
    /* Width x Height */
    width  = read_num(fd);
    height = read_num(fd);
    /* FDSZ */
    fd->read(&fdsz, 1);
    /* Presence of GCT */
    if (!(fdsz & 0x80)) {
        Serial.println("no global color table");
        goto fail;
    }
    /* Color Space's Depth */
    depth = ((fdsz >> 4) & 7) + 1;
    /* Ignore Sort Flag. */
    /* GCT Size */
    gct_sz = 1 << ((fdsz & 0x07) + 1);
    /* Background Color Index */
    fd->read(&bgidx, 1);
    /* Aspect Ratio */
    fd->read(&aspect, 1);
    /* Create gd_GIF Structure. */
    gif = (gd_GIF*) calloc(1, sizeof(*gif) + 4 * width * height);
    if (!gif) goto fail;
    gif->fd = fd;
    gif->width  = width;
    gif->height = height;
    gif->depth  = depth;
    /* Read GCT */
    gif->gct.size = gct_sz;
    fd->read(gif->gct.colors, 3 * gif->gct.size);
    gif->palette = &gif->gct;
    gif->bgindex = bgidx;
    gif->canvas = (uint8_t *) &gif[1];
    gif->frame = &gif->canvas[3 * width * height];
    if (gif->bgindex)
        memset(gif->frame, gif->bgindex, gif->width * gif->height);
    gif->anim_start = fd->position();
    gif->table = new_table();
    goto ok;
fail:
ok:
    return gif;
}

static void
discard_sub_blocks(gd_GIF *gif)
{
    uint8_t size;

    do {
        gif->fd->read(&size, 1);
        gif->fd->seek(size, SeekCur);
    } while (size);
}

static void
read_plain_text_ext(gd_GIF *gif)
{
    if (gif->plain_text) {
        uint16_t tx, ty, tw, th;
        uint8_t cw, ch, fg, bg;
        off_t sub_block;
        gif->fd->seek(1, SeekCur); /* block size = 12 */
        tx = read_num(gif->fd);
        ty = read_num(gif->fd);
        tw = read_num(gif->fd);
        th = read_num(gif->fd);
        gif->fd->read(&cw, 1);
        gif->fd->read(&ch, 1);
        gif->fd->read(&fg, 1);
        gif->fd->read(&bg, 1);
        sub_block = gif->fd->seek(0, SeekCur);
        gif->plain_text(gif, tx, ty, tw, th, cw, ch, fg, bg);
        gif->fd->seek(sub_block, SeekSet);
    } else {
        /* Discard plain text metadata. */
        gif->fd->seek(13, SeekCur);
    }
    /* Discard plain text sub-blocks. */
    discard_sub_blocks(gif);
}

static void
read_graphic_control_ext(gd_GIF *gif)
{
    uint8_t rdit;

    /* Discard block size (always 0x04). */
    gif->fd->seek(1, SeekCur);
    gif->fd->read(&rdit, 1);
    gif->gce.disposal = (rdit >> 2) & 3;
    gif->gce.input = rdit & 2;
    gif->gce.transparency = rdit & 1;
    gif->gce.delay = read_num(gif->fd);
    gif->fd->read(&gif->gce.tindex, 1);
    /* Skip block terminator. */
    gif->fd->seek(1, SeekCur);
}

static void
read_comment_ext(gd_GIF *gif)
{
    if (gif->comment) {
        off_t sub_block = gif->fd->seek(0, SeekCur);
        gif->comment(gif);
        gif->fd->seek(sub_block, SeekSet);
    }
    /* Discard comment sub-blocks. */
    discard_sub_blocks(gif);
}

static void
read_application_ext(gd_GIF *gif)
{
    char app_id[8];
    char app_auth_code[3];

    /* Discard block size (always 0x0B). */
    gif->fd->seek(1, SeekCur);
    /* Application Identifier. */
    gif->fd->read((uint8_t*) app_id, 8);
    /* Application Authentication Code. */
    gif->fd->read((uint8_t*) app_auth_code, 3);
    if (!strncmp(app_id, "NETSCAPE", sizeof(app_id))) {
        /* Discard block size (0x03) and constant byte (0x01). */
        gif->fd->seek(2, SeekCur);
        gif->loop_count = read_num(gif->fd);
        /* Skip block terminator. */
        gif->fd->seek(1, SeekCur);
    } else if (gif->application) {
        off_t sub_block = gif->fd->seek(0, SeekCur);
        gif->application(gif, app_id, app_auth_code);
        gif->fd->seek(sub_block, SeekSet);
        discard_sub_blocks(gif);
    } else {
        discard_sub_blocks(gif);
    }
}

static void
read_ext(gd_GIF *gif)
{
    uint8_t label;

    gif->fd->read(&label, 1);
    switch (label) {
    case 0x01:
        read_plain_text_ext(gif);
        break;
    case 0xF9:
        read_graphic_control_ext(gif);
        break;
    case 0xFE:
        read_comment_ext(gif);
        break;
    case 0xFF:
        read_application_ext(gif);
        break;
    default:
        Serial.print("unknown extension: ");
        Serial.println(label, HEX);
    }
}

static gd_Table *
new_table()
{
    // int key;
    // int init_bulk = MAX(1 << (key_size + 1), 0x100);
    // Table *table = (Table*) malloc(sizeof(*table) + sizeof(Entry) * init_bulk);
    // if (table) {
    //     table->bulk = init_bulk;
    //     table->nentries = (1 << key_size) + 2;
    //     table->entries = (Entry *) &table[1];
    //     for (key = 0; key < (1 << key_size); key++)
    //         table->entries[key] = (Entry) {1, 0xFFF, key};
    // }
    // return table;
    gd_Table* table = (gd_Table*) malloc(sizeof(gd_Table) + (sizeof(gd_Entry) * 4096));
    table->entries = (gd_Entry*) &table[1];
    return table;
}

static void
reset_table(gd_Table* table, int key_size)
{
    table->nentries = (1 << key_size) + 2;
    for (int key = 0; key < (1 << key_size); key++) {
        table->entries[key] = (gd_Entry) {1, 0xFFF, key};
    }
}

/* Add table entry. Return value:
 *  0 on success
 *  +1 if key size must be incremented after this addition
 *  -1 if could not realloc table */
static int
add_entry(gd_Table* table, uint16_t length, uint16_t prefix, uint8_t suffix)
{
    // Table *table = *tablep;
    // if (table->nentries == table->bulk) {
    //     table->bulk *= 2;
    //     table = (Table*) realloc(table, sizeof(*table) + sizeof(Entry) * table->bulk);
    //     if (!table) return -1;
    //     table->entries = (Entry *) &table[1];
    //     *tablep = table;
    // }
    table->entries[table->nentries] = (gd_Entry) {length, prefix, suffix};
    table->nentries++;
    if ((table->nentries & (table->nentries - 1)) == 0)
        return 1;
    return 0;
}

static uint16_t
get_key(gd_GIF *gif, int key_size, uint8_t *sub_len, uint8_t *shift, uint8_t *byte)
{
    int bits_read;
    int rpad;
    int frag_size;
    uint16_t key;

    key = 0;
    for (bits_read = 0; bits_read < key_size; bits_read += frag_size) {
        rpad = (*shift + bits_read) % 8;
        if (rpad == 0) {
            /* Update byte. */
            if (*sub_len == 0)
                gif->fd->read(sub_len, 1); /* Must be nonzero! */
            gif->fd->read(byte, 1);
            (*sub_len)--;
        }
        frag_size = MIN(key_size - bits_read, 8 - rpad);
        key |= ((uint16_t) ((*byte) >> rpad)) << bits_read;
    }
    /* Clear extra bits to the left. */
    key &= (1 << key_size) - 1;
    *shift = (*shift + key_size) % 8;
    return key;
}

/* Compute output index of y-th input line, in frame of height h. */
static int
interlaced_line_index(int h, int y)
{
    int p; /* number of lines in current pass */

    p = (h - 1) / 8 + 1;
    if (y < p) /* pass 1 */
        return y * 8;
    y -= p;
    p = (h - 5) / 8 + 1;
    if (y < p) /* pass 2 */
        return y * 8 + 4;
    y -= p;
    p = (h - 3) / 4 + 1;
    if (y < p) /* pass 3 */
        return y * 4 + 2;
    y -= p;
    /* pass 4 */
    return y * 2 + 1;
}

/* Decompress image pixels.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table). */
static int
read_image_data(gd_GIF *gif, int interlace)
{
    uint8_t sub_len, shift, byte;
    int init_key_size, key_size, table_is_full;
    int frm_off, str_len, p, x, y;
    uint16_t key, clear, stop;
    int ret;
    gd_Entry entry;
    off_t start, end;

    // Serial.println("Read key size");
    gif->fd->read(&byte, 1);
    key_size = (int) byte;
    // Serial.println("Set pos, discard sub blocks");
    start = gif->fd->position();
    discard_sub_blocks(gif);
    end = gif->fd->position();
    gif->fd->seek(start, SeekSet);
    clear = 1 << key_size;
    stop = clear + 1;
    // Serial.println("New LZW table");
    // table = new_table(key_size);
    reset_table(gif->table, key_size);
    key_size++;
    init_key_size = key_size;
    sub_len = shift = 0;
    // Serial.println("Get init key");
    key = get_key(gif, key_size, &sub_len, &shift, &byte); /* clear code */
    frm_off = 0;
    ret = 0;
    while (1) {
        if (key == clear) {
            // Serial.println("Clear key, reset nentries");
            key_size = init_key_size;
            gif->table->nentries = (1 << (key_size - 1)) + 2;
            table_is_full = 0;
        } else if (!table_is_full) {
            // Serial.println("Add entry to table");
            ret = add_entry(gif->table, str_len + 1, key, entry.suffix);
            // if (ret == -1) {
            //     // Serial.println("Table entry add failure");
            //     free(table);
            //     return -1;
            // }
            if (gif->table->nentries == 0x1000) {
                // Serial.println("Table is full");
                ret = 0;
                table_is_full = 1;
            }
        }
        // Serial.println("Get key");
        key = get_key(gif, key_size, &sub_len, &shift, &byte);
        if (key == clear) continue;
        if (key == stop) break;
        if (ret == 1) key_size++;
        entry = gif->table->entries[key];
        str_len = entry.length;
        // Serial.println("Interpret key");
        while (1) {
            p = frm_off + entry.length - 1;
            x = p % gif->fw;
            y = p / gif->fw;
            if (interlace)
                y = interlaced_line_index((int) gif->fh, y);
            gif->frame[(gif->fy + y) * gif->width + gif->fx + x] = entry.suffix;
            if (entry.prefix == 0xFFF)
                break;
            else
                entry = gif->table->entries[entry.prefix];
        }
        frm_off += str_len;
        if (key < gif->table->nentries - 1 && !table_is_full)
            gif->table->entries[gif->table->nentries - 1].suffix = entry.suffix;
    }
    // Serial.println("Done w/ img data, free table and seek to end");
    // free(table);
    gif->fd->read(&sub_len, 1); /* Must be zero! */
    gif->fd->seek(end, SeekSet);
    return 0;
}

/* Read image.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table). */
static int
read_image(gd_GIF *gif)
{
    uint8_t fisrz;
    int interlace;

    /* Image Descriptor. */
    // Serial.println("Read image descriptor");
    gif->fx = read_num(gif->fd);
    gif->fy = read_num(gif->fd);
    gif->fw = read_num(gif->fd);
    gif->fh = read_num(gif->fd);
    // Serial.println("Read fisrz?");
    gif->fd->read(&fisrz, 1);
    interlace = fisrz & 0x40;
    /* Ignore Sort Flag. */
    /* Local Color Table? */
    if (fisrz & 0x80) {
        /* Read LCT */
        // Serial.println("Read LCT");
        gif->lct.size = 1 << ((fisrz & 0x07) + 1);
        gif->fd->read(gif->lct.colors, 3 * gif->lct.size);
        gif->palette = &gif->lct;
    } else
        gif->palette = &gif->gct;
    /* Image Data. */
    // Serial.println("Read image data");
    return read_image_data(gif, interlace);
}

static void
render_frame_rect(gd_GIF *gif, uint8_t *buffer)
{
    int i, j, k;
    uint8_t index, *color;
    i = gif->fy * gif->width + gif->fx;
    for (j = 0; j < gif->fh; j++) {
        for (k = 0; k < gif->fw; k++) {
            index = gif->frame[(gif->fy + j) * gif->width + gif->fx + k];
            color = &gif->palette->colors[index*3];
            if (!gif->gce.transparency || index != gif->gce.tindex)
                memcpy(&buffer[(i+k)*3], color, 3);
        }
        i += gif->width;
    }
}

static void
dispose(gd_GIF *gif)
{
    int i, j, k;
    uint8_t *bgcolor;
    switch (gif->gce.disposal) {
    case 2: /* Restore to background color. */
        bgcolor = &gif->palette->colors[gif->bgindex*3];
        i = gif->fy * gif->width + gif->fx;
        for (j = 0; j < gif->fh; j++) {
            for (k = 0; k < gif->fw; k++)
                memcpy(&gif->canvas[(i+k)*3], bgcolor, 3);
            i += gif->width;
        }
        break;
    case 3: /* Restore to previous, i.e., don't update canvas.*/
        break;
    default:
        /* Add frame non-transparent pixels to canvas. */
        render_frame_rect(gif, gif->canvas);
    }
}

/* Return 1 if got a frame; 0 if got GIF trailer; -1 if error. */
int
gd_get_frame(gd_GIF *gif)
{
    char sep;

    // Serial.println("Dispose frame");
    dispose(gif);
    while (1) {
        gif->fd->read((uint8_t*) &sep, 1);
        // Serial.print("Read sep: ");Serial.println(sep);
        if (sep == ',')
            break;
        if (sep == ';')
            return 0;
        if (sep == '!')
            read_ext(gif);
        else return -1;
    }
    // Serial.println("Do read image");
    if (read_image(gif) == -1)
        return -1;
    return 1;
}

void
gd_render_frame(gd_GIF *gif, uint8_t *buffer)
{
    memcpy(buffer, gif->canvas, gif->width * gif->height * 3);
    render_frame_rect(gif, buffer);
}

void
gd_rewind(gd_GIF *gif)
{
    gif->fd->seek(gif->anim_start, SeekSet);
}

void
gd_close_gif(gd_GIF *gif)
{
    gif->fd->close();
    free(gif->table);
    free(gif);
}
