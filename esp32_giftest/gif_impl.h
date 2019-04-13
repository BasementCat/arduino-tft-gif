#ifndef GIF_IMPL_H
#define GIF_IMPL_H

#include <stdint.h>
#include <sys/types.h>
#include <SD.h>

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define R_NONE 0

#define E_CANTOPEN  -1
#define E_BADSIG    -2
#define E_BADVER    -3
#define E_BADSIZE   -4
#define E_NO_GCT    -5
#define E_BADSEP    -6
#define E_BADEXT    -7

#define R_TRAILER   1

typedef struct LZWTableEntry {
    uint16_t length;
    uint16_t prefix;
    uint8_t  suffix;
};

typedef struct LZWTable {
    int bulk;
    int nentries;
    LZWTableEntry entries[0x1000];
};

typedef struct Palette565 {
    int size;
    uint16_t colors[256];
};

typedef struct GCE {
    uint16_t delay;
    uint8_t tindex;
    uint8_t disposal;
    int input;
    int transparency;
};

class GIF
{
    public:
        uint16_t *canvas;

        GIF(const char *fname)
        {
            Serial.print("Open file: ");Serial.println(fname);delay(50);
            this->fd = SD.open(fname, FILE_READ);
        }

        ~GIF() {
            this->close();
        }

        int read_header()
        {
            uint8_t buf[3], flags, aspect;
            int gct_sz;

            Serial.println("Test fd");delay(50);
            if (!this->fd) return E_CANTOPEN;

            /* Header */
            Serial.println("Read and test signature");delay(50);
            this->fd.read(buf, 3);
            if (memcmp(buf, "GIF", 3) != 0) {
                return E_BADSIG;
            }
            /* Version */
            Serial.println("Read and test version");delay(50);
            this->fd.read(buf, 3);
            if (memcmp(buf, "89a", 3) != 0) {
                return E_BADVER;
            }

            /* Width and Height */
            Serial.println("Read w/h");delay(50);
            this->width = this->read_num();
            this->height = this->read_num();
            Serial.print("w=");Serial.println(this->width);Serial.print("h=");Serial.println(this->height);delay(50);
            /* Sanity check */
            if (this->width != 128 | this->height != 128) {
                return E_BADSIZE;
            }

            /* FDSZ */
            Serial.println("Read flags");delay(50);
            this->fd.read(&flags, 1);
            /* Presence of GCT */
            if (!(flags & 0x80)) {
                return E_NO_GCT;
            }

            /* Color Space's Depth */
            this->depth = ((flags >> 4) & 7) + 1;
            /* Ignore Sort Flag. */
            /* GCT Size */
            gct_sz = 1 << ((flags & 0x07) + 1);
            /* Background Color Index */
            Serial.println("Read bgindex");delay(50);
            this->fd.read(&this->bgindex, 1);
            /* Aspect Ratio, ignored */
            Serial.println("Read aspect");delay(50);
            this->fd.read(&aspect, 1);

            /* Allocate palette, canvas, frame */
            Serial.println("Set ptr to palette");delay(50);
            this->palette = &this->gct;
            this->palette->size = gct_sz;

            Serial.println("Read GCT");delay(50);
            for (int i = 0; i < gct_sz; i++) {
                this->fd.read(buf, 3);
                this->palette->colors[i] = this->color565(buf[0], buf[1], buf[2]);
            }

            Serial.println("Alloc canvas and frame");delay(50);
            this->canvas = (uint16_t*) calloc(1, this->width * this->height * 2);
            this->frame = (uint8_t*) calloc(1, this->width * this->height);
            if (this->bgindex) {
                memset(this->frame, this->bgindex, this->width * this->height);
            }

            Serial.println("Set anim start position");delay(50);
            this->anim_start = this->fd.position();

            // /* Create LZW table */
            // Serial.println("Initialize lzw table");delay(50);
            // this->init_lzw_table();

            return R_NONE;
        }

        void _debug_gif()
        {
            if (!this->fd) {
                Serial.println("ERROR: could not open gif");
            }
            Serial.print("Name: ");Serial.println(this->fd.name());
            Serial.print("Size: ");
                Serial.print(this->width);
                Serial.print("x");Serial.print(this->height);
                Serial.print("@");Serial.print(this->depth);
                Serial.println(" BPP");
        }

        int read_frame()
        {
            char sep;
            int res;

            Serial.println("rf:dispose");delay(50);
            this->dispose();
            Serial.println("rf:read sep 1");delay(50);
            this->fd.read((uint8_t*) &sep, 1);
            while (sep != ',') {
                if (sep == ';') {
                    return R_TRAILER;
                }
                if (sep == '!') {
                    Serial.println("rf:read ext");delay(50);
                    if (this->read_ext() == E_BADEXT) {
                        Serial.println("rf: bad ext");delay(50);
                        return E_BADEXT;
                    }
                } else {
                    return E_BADSEP;
                }
                Serial.println("rf:read sep 2");delay(50);
                this->fd.read((uint8_t*) &sep, 1);
            }
            Serial.println("rf:read image");delay(50);
            return this->read_image();
        }

        int render_frame() {
            // Is this needed?
            this->render_frame_rect();
        }

        int rewind();

        int close() {
            if (this->fd) {
                this->fd.close();
            }
            free(this->canvas);
            free(this->frame);
        }

    private:
        File fd;
        off_t anim_start;
        uint16_t width, height;
        uint16_t depth;
        uint16_t loop_count;
        GCE gce;
        Palette565 *palette;
        Palette565 lct, gct;
        // void (*plain_text)(
        //     struct gd_GIF *gif, uint16_t tx, uint16_t ty,
        //     uint16_t tw, uint16_t th, uint8_t cw, uint8_t ch,
        //     uint8_t fg, uint8_t bg
        // );
        // void (*comment)(struct gd_GIF *gif);
        // void (*application)(struct gd_GIF *gif, char id[8], char auth[3]);
        uint16_t fx = 0, fy = 0, fw = 0, fh = 0;

        // LZW stuff
        LZWTable lzw_table;

        uint8_t bgindex;
        // uint16_t *canvas;
        uint8_t *frame;

        /*
            Helper Functions
        */

        uint16_t
        read_num()
        {
            uint8_t bytes[2];

            this->fd.read(bytes, 2);
            return bytes[0] + (((uint16_t) bytes[1]) << 8);
        }

        uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
            return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }

        void
        discard_sub_blocks()
        {
            uint8_t size;

            do {
                this->fd.read(&size, 1);
                this->fd.seek(size, SeekCur);
            } while (size);
        }

        int
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

        /*
            LZW decoding functions
        */

        void
        init_lzw_table()
        {
            this->lzw_table.bulk = 0x1000;
            this->lzw_table.nentries = (1 << 0x100) + 2;
            for (int i = 0; i < 0x100; i++) {
                this->lzw_table.entries[i].length = 1;
                this->lzw_table.entries[i].prefix = 0xFFF;
                this->lzw_table.entries[i].suffix = i;
            }
        }

        uint16_t
        lzw_get_key(int key_size, uint8_t *sub_len, uint8_t *shift, uint8_t *byte)
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
                        this->fd.read(sub_len, 1); /* Must be nonzero! */
                    this->fd.read(byte, 1);
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

        int
        lzw_add_entry(uint16_t length, uint16_t prefix, uint8_t suffix)
        {
            // if (this->lzw_table.nentries == this->lzw_table.bulk) {
            //     this->lzw_table.bulk *= 2;
            //     this->lzw_table.entries = (LZWTableEntry *) &this->lzw_table[1];
            // }
            this->lzw_table.entries[this->lzw_table.nentries] = (LZWTableEntry) {length, prefix, suffix};
            this->lzw_table.nentries++;
            if ((this->lzw_table.nentries & (this->lzw_table.nentries - 1)) == 0)
                return 1;
            return 0;
        }

        /*
            Image Decoding Functions
        */

        int
        read_image()
        {
            uint8_t fisrz;
            int interlace;
            uint8_t buf[3];
            Serial.println("Read frame");delay(50);

            /* Image Descriptor. */
            this->fx = this->read_num();
            this->fy = this->read_num();
            this->fw = this->read_num();
            this->fh = this->read_num();
            this->fd.read(&fisrz, 1);
            interlace = fisrz & 0x40;
            Serial.print("Frame sz ");
                Serial.print(this->fw);Serial.print("x");Serial.print(this->fh);
                Serial.print(" @ ");
                Serial.print(this->fx);Serial.print("x");Serial.print(this->fy);
                Serial.print(", interlace=");Serial.println(interlace);
            /* Ignore Sort Flag. */
            /* Local Color Table? */
            if (fisrz & 0x80) {
                /* Read LCT */
                Serial.println("Read LCT");delay(50);
                this->lct.size = 1 << ((fisrz & 0x07) + 1);
                // this->fd.read(this->lct.colors, 3 * this->lct.size);
                this->palette = &this->lct;
                for (int i = 0; i < this->lct.size; i++) {
                    this->fd.read(buf, 3);
                    this->palette->colors[i] = this->color565(buf[0], buf[1], buf[2]);
                }
            } else {
                this->palette = &this->gct;
            }
            /* Image Data. */
            return this->read_image_data(interlace);
        }

        int
        read_image_data(int interlace)
        {
            uint8_t sub_len, shift, byte;
            int init_key_size, key_size, table_is_full;
            int frm_off, str_len, p, x, y;
            uint16_t key, clear, stop;
            int ret;
            // Table *table;
            LZWTableEntry entry;
            off_t start, end;

            this->init_lzw_table();

            this->fd.read(&byte, 1);
            key_size = (int) byte;
            start = this->fd.position();
            this->discard_sub_blocks();
            end = this->fd.position();
            this->fd.seek(start, SeekSet);
            clear = 1 << key_size;
            stop = clear + 1;
            key_size++;
            init_key_size = key_size;
            sub_len = shift = 0;
            key = this->lzw_get_key(key_size, &sub_len, &shift, &byte); /* clear code */
            frm_off = 0;
            ret = 0;
            while (1) {
                if (key == clear) {
                    key_size = init_key_size;
                    this->lzw_table.nentries = (1 << (key_size - 1)) + 2;
                    table_is_full = 0;
                } else if (!table_is_full) {
                    ret = this->lzw_add_entry(str_len + 1, key, entry.suffix);
                    if (ret == -1) {
                        // free(table);
                        return -1;
                    }
                    if (this->lzw_table.nentries == 0x1000) {
                        ret = 0;
                        table_is_full = 1;
                    }
                }
                key = this->lzw_get_key(key_size, &sub_len, &shift, &byte);
                if (key == clear) continue;
                if (key == stop) break;
                if (ret == 1) key_size++;
                entry = this->lzw_table.entries[key];
                str_len = entry.length;
                while (1) {
                    p = frm_off + entry.length - 1;
                    x = p % this->fw;
                    y = p / this->fw;
                    if (interlace) {
                        y = this->interlaced_line_index((int) this->fh, y);
                    }
                    this->frame[(this->fy + y) * this->width + this->fx + x] = entry.suffix;
                    if (entry.prefix == 0xFFF)
                        break;
                    else
                        entry = this->lzw_table.entries[entry.prefix];
                }
                frm_off += str_len;
                if (key < this->lzw_table.nentries - 1 && !table_is_full)
                    this->lzw_table.entries[this->lzw_table.nentries - 1].suffix = entry.suffix;
            }
            // free(table);
            this->fd.read(&sub_len, 1); /* Must be zero! */
            this->fd.seek(end, SeekSet);
            return 0;
        }

        /*
            Rendering functions
        */

        void
        dispose()
        {
            int y, x;
            // int i, j, k;
            // uint8_t *bgcolor;
            switch (this->gce.disposal) {
                case 2: /* Restore to background color. */
                    Serial.println("disp:bg");delay(50);
                    for (y = this->fy; y < this->fy + this->fh; y++) {
                        for (x = this->fx; x < this->fx + this->fw; x++) {
                            this->canvas[(y * this->width) + x] = this->palette->colors[this->bgindex];
                        }
                        // memcpy(
                        //     this->canvas + this->fx + y,
                        //     (uint16_t) this->palette->colors[this->bgindex],
                        //     this->fw
                        // )
                    }
                    // bgcolor = &this->palette->colors[this->bgindex*3];
                    // i = this->fy * this->width + this->fx;
                    // for (j = 0; j < this->fh; j++) {
                    //     for (k = 0; k < this->fw; k++)
                    //         memcpy(&this->canvas[(i+k)*3], bgcolor, 3);
                    //     i += this->width;
                    // }
                    break;
                case 3: /* Restore to previous, i.e., don't update canvas.*/
                    Serial.println("disp:prev");delay(50);
                    break;
                default:
                    /* Add frame non-transparent pixels to canvas. */
                    Serial.println("disp:render");delay(50);
                    this->render_frame_rect();
            }
        }

        void
        render_frame_rect()
        {
            Serial.println("render frame rect");delay(50);
            int y, x;
            for (y = 0; y < this->fh; y++) {
                for (x = 0; x < this->fw; x++) {
                    this->canvas[((this->fy + y) * this->width) + (this->fx + x)] =
                        (uint16_t) this->palette->colors[(y * this->fw) + x];
                }
            }
            // int i, j, k;
            // uint8_t index, *color;
            // i = this->fy * this->width + this->fx;
            // for (j = 0; j < this->fh; j++) {
            //     for (k = 0; k < this->fw; k++) {
            //         index = this->frame[(this->fy + j) * this->width + this->fx + k];
            //         color = &this->palette->colors[index*3];
            //         if (!this->gce.transparency || index != this->gce.tindex)
            //             memcpy(&this->canvas[(i+k)*3], color, 3);
            //     }
            //     i += this->width;
            // }
        }

        /*
            Extension Functions
        */

        int
        read_ext()
        {
            uint8_t label;

            this->fd.read(&label, 1);
            switch (label) {
                case 0x01:
                    this->read_plain_text_ext();
                    break;
                case 0xF9:
                    this->read_graphic_control_ext();
                    break;
                case 0xFE:
                    this->read_comment_ext();
                    break;
                case 0xFF:
                    this->read_application_ext();
                    break;
                default:
                    return E_BADEXT;
            }

            return R_NONE;
        }

        void
        read_plain_text_ext()
        {
            // Ignore plain text extension
            this->fd.seek(13, SeekCur);
            /* Discard plain text sub-blocks. */
            this->discard_sub_blocks();
        }

        void
        read_graphic_control_ext()
        {
            uint8_t rdit;

            /* Discard block size (always 0x04). */
            this->fd.seek(1, SeekCur);
            this->fd.read(&rdit, 1);
            this->gce.disposal = (rdit >> 2) & 3;
            this->gce.input = rdit & 2;
            this->gce.transparency = rdit & 1;
            this->gce.delay = this->read_num();
            this->fd.read(&this->gce.tindex, 1);
            /* Skip block terminator. */
            this->fd.seek(1, SeekCur);

            Serial.print("GCE disp=");Serial.println(this->gce.disposal);
            Serial.print("GCE input=");Serial.println(this->gce.input);
            Serial.print("GCE trans=");Serial.println(this->gce.transparency);
            Serial.print("GCE delay=");Serial.println(this->gce.delay);
            Serial.print("GCE tindex=");Serial.println(this->gce.tindex);
        }

        void
        read_comment_ext()
        {
            /* Discard comment sub-blocks. */
            this->discard_sub_blocks();
        }

        void
        read_application_ext()
        {
            char app_id[8];
            char app_auth_code[3];

            /* Discard block size (always 0x0B). */
            this->fd.seek(1, SeekCur);
            /* Application Identifier. */
            this->fd.read((uint8_t*) app_id, 8);
            /* Application Authentication Code. */
            this->fd.read((uint8_t*) app_auth_code, 3);
            if (!strncmp(app_id, "NETSCAPE", sizeof(app_id))) {
                /* Discard block size (0x03) and constant byte (0x01). */
                this->fd.seek(2, SeekCur);
                this->loop_count = this->read_num();
                Serial.print("NSapp loop count=");Serial.println(this->loop_count);
                /* Skip block terminator. */
                this->fd.seek(1, SeekCur);
            } else {
                this->discard_sub_blocks();
            }
        }
};

#endif