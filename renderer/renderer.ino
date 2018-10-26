#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>
#include <SD.h>

// TFT display and SD card will share the hardware SPI interface.
// Hardware SPI pins are specific to the Arduino board type and
// cannot be remapped to alternate pins.  For Arduino Uno,
// Duemilanove, etc., pin 11 = MOSI, pin 12 = MISO, pin 13 = SCK.
#define TFT_CS  10  // Chip select line for TFT display
#define TFT_RST  9  // Reset line for TFT (or see below...)
//Use this reset pin for the shield!
//#define TFT_RST  -1  // you can also connect this to the Arduino reset!
#define TFT_DC   8  // Data/command line for TFT

#define SD_CS    4  // Chip select line for SD card

// For 0.96", 1.44" and 1.8" TFT with ST7735 use
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

void setup(void) {
  Serial.begin(9600);

  while (!Serial) {
    delay(10);  // wait for serial console
  }

  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  tft.initR(INITR_144GREENTAB);

  tft.fillScreen(ST77XX_BLUE);

  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("failed!");
    return;
  }
  Serial.println("OK!");

  File root = SD.open("/");
  printDirectory(root, 0);
  root.close();

  render_anim();
}

void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void loop() {
// uncomment these lines to draw bitmaps in different locations/rotations!
/*
  tft.fillScreen(ST7735_BLACK); // Clear display
  for(uint8_t i=0; i<4; i++)    // Draw 4 parrots
    bmpDraw("parrot.bmp", tft.width() / 4 * i, tft.height() / 4 * i);
  delay(1000);
  tft.setRotation(tft.getRotation() + 1); // Inc rotation 90 degrees
*/
}


// // These read 16- and 32-bit types from the SD card file.
// // BMP data is stored little-endian, Arduino is little-endian too.
// // May need to reverse subscript order if porting elsewhere.

// uint16_t read16(File f) {
//   uint16_t result;
//   ((uint8_t *)&result)[1] = f.read(); // LSB
//   ((uint8_t *)&result)[0] = f.read(); // MSB
//   return result;
// }

// uint32_t read32(File f) {
//   uint32_t result;
//   ((uint8_t *)&result)[3] = f.read(); // MSB
//   ((uint8_t *)&result)[2] = f.read();
//   ((uint8_t *)&result)[1] = f.read();
//   ((uint8_t *)&result)[0] = f.read(); // LSB
//   return result;
// }


typedef struct {
  uint32_t magic;
  uint16_t version;
} Magic;

typedef struct {
  uint16_t w;
  uint16_t h;
} Header;

typedef struct {
  uint8_t cmd;
  uint8_t arg;
} Command;

#define CMD_START_SET 1
#define CMD_END_IMG 2
#define CMD_END_SET 3
#define CMD_PIX_RAW 4
#define CMD_PIX_RLE 5
#define CMD_SEEK 6
#define CMD_DELAY 7

void render_anim() {
  File fp;
  Magic magic;
  Header header;
  Command cmd;
  uint8_t loop_max;
  uint16_t buf[30], buf_pos = 30, buf_read = 0, px_i, loop_counter;
  uint32_t data_start, img_set_start = 0, img_start_time = 0;
  int32_t time_to_delay;

  if ((fp = SD.open("main~1.ani")) == NULL) {
    die("Can't open animation main.anim [main~1.ani]");
  }

  fp.read((uint8_t *)&magic, sizeof(magic));
  Serial.print("Magic: ");Serial.println(magic.magic, HEX);
  Serial.print("Version: ");Serial.println(magic.version);

  if (magic.magic != 0x676d4941) {
    die("Invalid sorcery (bad magic)");
  }

  if (magic.version != 2) {
    die("Bad file version");
  }

  fp.read((uint8_t *)&header, sizeof(header));
  Serial.print("Dimensions: ");
  Serial.print(header.w);Serial.print("x");Serial.println(header.h);

  if (header.w != 128 || header.h != 128) {
    die("Bad image dimensions");
  }

  // Start of image data
  data_start = fp.position();
  Serial.print("Start of data @ ");Serial.println(data_start);

  while(1) {
    // if (img_start_time == 0) img_start_time = millis();
    // if (img_set_start == 0) {
    //   // Get loop count
    //   img_set_start = fp.position();
    //   loop_max = fp.read();
    //   loop_counter = 0;
    //   Serial.print("Img set start, loop max ");Serial.println(loop_max);
    // }
    fp.read((uint8_t *)&cmd, sizeof(cmd));
    // Serial.print("Command: ");Serial.print(cmd.cmd);
    // Serial.print(", Arg: ");Serial.println(cmd.arg);
    switch (cmd.cmd) {
      case CMD_START_SET:
        // Start of image set
        img_set_start = fp.position() - sizeof(cmd);
        loop_max = cmd.arg;
        loop_counter = 0;
        img_start_time = millis();
        break;
      case CMD_END_IMG:
        // End of image in image set
        Serial.print("Wrote frame in ");
        Serial.print(millis() - img_start_time);
        Serial.println(" ms");
        img_start_time = millis();
        break;
      case CMD_END_SET:
        // End of image set
        if (++loop_counter > loop_max) {
          // End loop
          if (fp.position() >= fp.size()) {
            fp.seek(data_start);
          }
        } else {
          fp.seek(img_set_start);
        }
        // // img_start_time = 0;
        break;
      case CMD_PIX_RAW:
        // Raw pixel data - already 5-6-5 encoded
        px_i = 0;
        while (px_i < cmd.arg) {
          buf_read = min(sizeof(buf), (cmd.arg - px_i) * 2);
          fp.read(buf, buf_read);
          tft.startWrite();
          tft.writePixels(buf, buf_read / 2);
          tft.endWrite();
          buf_pos += buf_read;
          px_i += buf_read / 2;
        }
        break;
      case CMD_PIX_RLE:
        fp.read(buf, 2);
        tft.startWrite();
        tft.writeColor(buf[0], cmd.arg);
        tft.endWrite();
        break;
      case CMD_SEEK:
        // Seek (ignore arg, data 2b x, 2b y)
        // TODO: implement me
        break;
      case CMD_DELAY:
        // Delay MS
        time_to_delay = cmd.arg - (millis() - img_start_time);
        if (time_to_delay > 0) {
          delay(time_to_delay);
        }
        break;
      default:
        Serial.print("Invalid command at offset 0x");
        Serial.println(fp.position() - 2, HEX);
        die("Invalid command");
        break;
    }
  }
}

void die(char *msg) {
  Serial.println(msg);
  // TODO: draw something on the screen to indicate an error
  for(;;){}
}