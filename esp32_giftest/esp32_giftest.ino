#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SD.h>

// Definitions of pin numbers for the TFT
// SPI pins are hardware and do not need to be defined
#define TFT_CS  13  // TFT chip select line
#define TFT_RST 27  // TFT reset line
#define TFT_DC  33  // TFT data/command line
#define SD_CS   15  // SD chip select line
// Unused for now
// #define TFT_BL  99  // TFT backlight PWM line

// Buttons - will use these later
#define BTN_L   4
#define BTN_M   25
#define BTN_R   26

#define DISPLAY_TIME_SECONDS 10

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

#define FILENAME "/128_commscafedragon_by_metallicumbrage-dcv6bo5.anim"
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

uint16_t screen[16384];

// Setup method runs once, when the sketch starts
void setup() {
    Serial.begin(115200);
    Serial.println("Starting AnimatedGIFs Sketch");

    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    // For 0.96", 1.44" and 1.8" TFT with ST7735 use
    tft.initR(INITR_144GREENTAB);

    tft.fillScreen(ST77XX_BLUE);

    Serial.print("Initializing SD card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("failed!");
        die("Failed to initialize SD card");
    }
    Serial.println("OK!");
}

typedef struct {
    uint32_t magic;
    uint16_t version;

    uint16_t width;
    uint16_t height;
    uint32_t frame_count;
    uint8_t px_format;
} __attribute__ ((packed)) Header;

typedef struct {
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

void loop() {
    File fp;
    Header header;
    Frame frame;
    Chunk chunk;
    int anim_start, pixels_to_read, read_pixels, fx, fy, frame_start, frame_delay;
    // For timing
    int t_start, t_reading, t_unpacking, t_rendering;
    uint16_t buffer[1024], trans_color;
    bool has_trans = false;

    fp = SD.open(FILENAME);
    if (!fp) {
        die("Failed to open file");
    }

    fp.read((uint8_t*) &header, 15);

    // Serial.print("file:magic=");Serial.println(header.magic);
    // Serial.print("file:version=");Serial.println(header.version);
    // Serial.print("file:width=");Serial.println(header.width);
    // Serial.print("file:height=");Serial.println(header.height);
    // Serial.print("file:frame_count=");Serial.println(header.frame_count, HEX);
    // Serial.print("file:px_format=");Serial.println(header.px_format, HEX);

    if (header.magic != 0x676d4941) {
        die("Bad magic");
    }
    if (header.version != 4) {
        die("Bad version");
    }
    if (header.width != 128 || header.height != 128) {
        die("Bad size");
    }
    if (header.px_format != 1) {
        die("Bad pixel format");
    }

    anim_start = fp.position();

    for (int i = 0; i < header.frame_count; i++) {
        t_reading = t_unpacking = t_rendering = 0;
        frame_start = millis();
        t_start = frame_start;
        // Serial.print("Frame ");Serial.println(i + 1);
        has_trans = false;
        fx = 0;
        fy = 0;
        fp.read((uint8_t*) &frame, 11);
        // Serial.print("frame:width=");Serial.println(frame.width);
        // Serial.print("frame:height=");Serial.println(frame.height);
        // Serial.print("frame:x_off=");Serial.println(frame.x_off);
        // Serial.print("frame:y_off=");Serial.println(frame.y_off);
        // Serial.print("frame:delay=");Serial.println(frame.delay);
        // Serial.print("frame:flags=");Serial.println(frame.flags, HEX);
        if (frame.flags & 0x80) {
            has_trans = true;
            fp.read((uint8_t*) &trans_color, 2);
            // Serial.print("frame:trans_color=");Serial.println(trans_color, HEX);
        }
        while (1) {
            read_pixels = 0;
            fp.read((uint8_t*) &chunk, 3);
            if (chunk.flags & 0x40) {
                break;
            }
            // Serial.print("chunk:pixel_count=");Serial.println(chunk.pixel_count);
            // Serial.print("chunk:flags=");Serial.println(chunk.flags, HEX);
            if (chunk.flags & 0x80) {
                // Chunk is RLE-encoded, read 1 pixel value
                fp.read((uint8_t*) &buffer, 2);
                t_reading += millis() - t_start;t_start = millis();
                // Serial.print("chunk:rle color=");Serial.println(buffer[0], HEX);
                // if (has_trans && buffer[0] == trans_color) {
                //     // Serial.println("chunk:rle color is trans");
                //     // Short circuit here, all we have to do is bump the pixel pointer
                //     fx += chunk.pixel_count;
                //     if (fx >= frame.width) {
                //         fy = (fx / frame.width) + 1;
                //         fx = fx % frame.width;
                //     }
                // } else {
                for (int j = 0; j < chunk.pixel_count; j++) {
                    // Serial.print("chunk:set rle pixel=");Serial.println(j);
                    if (!has_trans || buffer[0] != trans_color) {
                        // No transparency, or the pixel is not transparent
                        screen[((frame.y_off + fy) * header.width) + (frame.x_off + fx)] = buffer[0];
                    }
                    if (++fx >= frame.width) {
                        fx = 0;
                        fy++;
                    }
                }
                t_unpacking += millis() - t_start;t_start = millis();
            } else {
                while (read_pixels < chunk.pixel_count) {
                    pixels_to_read = MIN(chunk.pixel_count - read_pixels, 1024);
                    // Serial.print("Reading pixels: ");Serial.println(pixels_to_read);
                    fp.read((uint8_t*) &buffer, pixels_to_read * 2);
                    t_reading += millis() - t_start;t_start = millis();
                    read_pixels += pixels_to_read;
                    /* Loop through the pixels we read, and either put them directly
                    in the buffer, or if they're the transparent color, skip them */
                    for (int j = 0; j < pixels_to_read; j++) {
                        if (!has_trans || buffer[j] != trans_color) {
                            // No transparency, or the pixel is not transparent
                            screen[((frame.y_off + fy) * header.width) + (frame.x_off + fx)] = buffer[j];
                        }
                        if (++fx >= frame.width) {
                            fx = 0;
                            fy++;
                        }
                    }
                    t_unpacking += millis() - t_start;t_start = millis();
                }
            }
        }

        tft.startWrite();
        tft.writePixels(screen, 16384);
        tft.endWrite();
        t_rendering += millis() - t_start;

        Serial.print("t_reading=");Serial.println(t_reading);
        Serial.print("t_unpacking=");Serial.println(t_unpacking);
        Serial.print("t_rendering=");Serial.println(t_rendering);

        frame_delay = frame.delay - (millis() - frame_start);
        Serial.print("Delay for ");Serial.print(frame_delay);Serial.print(" of ");Serial.println(frame.delay);
        if (frame_delay > 0) {
            delay(frame_delay);
        }
    }




    // Serial.println("Instantiate gif");delay(50);
    // GIF *gif = new GIF(GIFFILENAME);
    // gif->read_header();
    // gif->_debug_gif();
    // gif->read_frame();
    // gif->render_frame();
    // // memcpy(gif->canvas, screen, 16384);
    // tft.startWrite();
    // tft.writePixels(gif->canvas, 16384);
    // tft.endWrite();
    // delay(5000);
    // gif->close();
    // free(gif);
}


void die(const char *message) {
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextWrap(true);

    tft.setCursor(10, 15);
    tft.setTextColor(tft.color565(255, 0, 0));
    tft.setTextSize(3);
    tft.println("ERROR");

    tft.setCursor(0, 50);
    tft.setTextColor(0xffff);
    tft.setTextSize(1);
    tft.println(message);
    while (true) delay(1000);
}
