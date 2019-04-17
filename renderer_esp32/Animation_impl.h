#ifndef ANIM_IMPL_H
#define ANIM_IMPL_H

#include <SD.h>
#include <Adafruit_ST7735.h>

#ifndef MIN
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif
#ifndef MAX
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif

typedef struct {
    uint32_t magic;
    uint16_t version;

    uint16_t width;
    uint16_t height;
    uint32_t frame_count;
    uint8_t px_format;
} __attribute__ ((packed)) Header;

typedef struct {
    uint32_t data_sz;
    uint16_t width;
    uint16_t height;
    uint16_t x_off;
    uint16_t y_off;
    uint16_t delay;
    uint8_t flags;
} __attribute__ ((packed)) Frame;

typedef struct {
    uint16_t pixel_count;
    uint8_t flags;
} __attribute__ ((packed)) Chunk;

class Animation {
    public:
        char* errstr;

        Animation(Adafruit_ST7735* tft, File* fp, uint16_t* screen) {
            this->tft = tft;
            this->fp = fp;
            this->screen = screen;

            this->fp->read((uint8_t*) &this->header, sizeof(Header));

            Serial.print("file:magic=");Serial.println(header.magic);
            Serial.print("file:version=");Serial.println(header.version);
            Serial.print("file:width=");Serial.println(header.width);
            Serial.print("file:height=");Serial.println(header.height);
            Serial.print("file:frame_count=");Serial.println(header.frame_count, HEX);
            Serial.print("file:px_format=");Serial.println(header.px_format, HEX);

            if (this->header.magic != 0x676d4941) {
                this->errstr = "Bad magic";
            }
            if (this->header.version != 4) {
                this->errstr = "Bad version";
            }
            if (this->header.width != 128 || this->header.height != 128) {
                this->errstr = "Bad size";
            }
            if (this->header.px_format != 1) {
                this->errstr = "Bad pixel format";
            }

            anim_start = this->fp->position();
        }

        // ~Animation();

        int read_frame() {
            if (this->frame_num >= this->header.frame_count) return -1;

            this->t_reading = this->t_unpacking = this->t_rendering = 0;
            this->frame_start = millis();
            this->t_start = this->frame_start;
            // Serial.print("Frame ");Serial.println(i + 1);
            this->has_trans = false;
            this->fx = 0;
            this->fy = 0;
            this->fp->read((uint8_t*) &this->frame, sizeof(Frame));
            // Serial.print("frame:width=");Serial.println(this->frame.width);
            // Serial.print("frame:height=");Serial.println(this->frame.height);
            // Serial.print("frame:x_off=");Serial.println(this->frame.x_off);
            // Serial.print("frame:y_off=");Serial.println(this->frame.y_off);
            // Serial.print("frame:delay=");Serial.println(this->frame.delay);
            // Serial.print("frame:flags=");Serial.println(this->frame.flags, HEX);
            if (this->frame.flags & 0x80) {
                this->has_trans = true;
                this->fp->read((uint8_t*) &this->trans_color, 2);
                // Serial.print("frame:trans_color=");Serial.println(this->trans_color, HEX);
            }
            while (1) {
                this->read_pixels = 0;
                this->fp->read((uint8_t*) &this->chunk, sizeof(Chunk));
                if (this->chunk.flags & 0x40) {
                    break;
                }
                // Serial.print("chunk:pixel_count=");Serial.println(this->chunk.pixel_count);
                // Serial.print("chunk:flags=");Serial.println(this->chunk.flags, HEX);
                if (this->chunk.flags & 0x80) {
                    // Chunk is RLE-encoded, read 1 pixel value
                    this->fp->read((uint8_t*) &this->buffer, 2);
                    this->t_reading += millis() - this->t_start;this->t_start = millis();
                    // Serial.print("chunk:rle color=");Serial.println(this->buffer[0], HEX);
                    // if (this->has_trans && buffer[0] == this->trans_color) {
                    //     // Serial.println("chunk:rle color is trans");
                    //     // Short circuit here, all we have to do is bump the pixel pointer
                    //     fx += this->chunk.pixel_count;
                    //     if (fx >= this->frame.width) {
                    //         this->fy = (fx / this->frame.width) + 1;
                    //         fx = fx % this->frame.width;
                    //     }
                    // } else {
                    for (int j = 0; j < this->chunk.pixel_count; j++) {
                        // Serial.print("chunk:set rle pixel=");Serial.println(j);
                        if (!this->has_trans || this->buffer[0] != this->trans_color) {
                            // No transparency, or the pixel is not transparent
                            this->screen[((this->frame.y_off + this->fy) * this->header.width) + (this->frame.x_off + this->fx)] = this->buffer[0];
                        }
                        if (++this->fx >= this->frame.width) {
                            this->fx = 0;
                            this->fy++;
                        }
                    }
                    this->t_unpacking += millis() - this->t_start;this->t_start = millis();
                } else {
                    while (this->read_pixels < this->chunk.pixel_count) {
                        this->pixels_to_read = MIN(this->chunk.pixel_count - this->read_pixels, 1024);
                        // Serial.print("Reading pixels: ");Serial.println(this->pixels_to_read);
                        this->fp->read((uint8_t*) &this->buffer, this->pixels_to_read * 2);
                        this->t_reading += millis() - this->t_start;this->t_start = millis();
                        this->read_pixels += this->pixels_to_read;
                        /* Loop through the pixels we read, and either put them directly
                        in the buffer, or if they're the transparent color, skip them */
                        for (int j = 0; j < this->pixels_to_read; j++) {
                            if (!this->has_trans || this->buffer[j] != this->trans_color) {
                                // No transparency, or the pixel is not transparent
                                this->screen[((this->frame.y_off + this->fy) * this->header.width) + (this->frame.x_off + this->fx)] = this->buffer[j];
                            }
                            if (++this->fx >= this->frame.width) {
                                this->fx = 0;
                                this->fy++;
                            }
                        }
                        this->t_unpacking += millis() - this->t_start;this->t_start = millis();
                    }
                }
            }

            this->frame_num++;

            return 0;
        }

        void render_frame() {
            this->tft->startWrite();
            this->tft->writePixels(this->screen, 16384);
            this->tft->endWrite();
            this->t_rendering += millis() - this->t_start;
        }

        void delay_frame() {
            Serial.print("t_reading=");Serial.println(this->t_reading);
            Serial.print("t_unpacking=");Serial.println(this->t_unpacking);
            Serial.print("t_rendering=");Serial.println(this->t_rendering);
            this->frame_delay = this->frame.delay - (millis() - this->frame_start);
            Serial.print("Delay for ");Serial.print(this->frame_delay);Serial.print(" of ");Serial.println(this->frame.delay);
            if (this->frame_delay > 0) {
                delay(this->frame_delay);
            }
        }

        void rewind() {
            this->fp->seek(this->anim_start);
            this->frame_num = 0;
        }

    private:
        Adafruit_ST7735* tft;
        File* fp;
        uint16_t* screen;
        Header header;
        Frame frame;
        Chunk chunk;
        int anim_start, pixels_to_read, read_pixels, fx, fy, frame_start, frame_delay, frame_num = 0;
        // For timing
        int t_start, t_reading, t_unpacking, t_rendering;
        uint16_t buffer[1024], trans_color;
        bool has_trans = false;
};

#endif