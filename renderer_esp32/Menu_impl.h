#include <Adafruit_ST7735.h>
#include "Buttons_impl.h"

// Needed here too
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))


typedef struct {
    const char* text;
    bool selected;
    uint16_t width, height;

    uint16_t schar, lchar;
} MenuItem;


class MenuRenderer {
    public:
        MenuRenderer(Adafruit_ST7735* tft_ptr, Buttons* buttons_ptr) {
            tft = tft_ptr;
            buttons = buttons_ptr;
        }

        int render(const char** items_text, int item_count) {
            if (item_count < 1)
                return -1;

            MenuItem items[item_count];
            int i, y_offset, max_items_rendered, item_offset=0;
            uint32_t next_render;
            bool selection_changed = true;

            tft->fillScreen(ST77XX_BLACK);
            tft->drawRoundRect(1, 1 + top_offset, width - 1, (height - btm_offset) - 1, 4, color);
            tft->setTextWrap(false);
            // setTextColor(uint16_t c),
            // setTextColor(uint16_t c, uint16_t bg),
            // setTextSize(uint8_t s),
            tft->setTextColor(0xffff, 0x0000);
            tft->setTextSize(1);

            for (i = 0; i < item_count; i++) {
                items[i].text = items_text[i];
                items[i].selected = (i == 0 ? true : false);
                items[i].schar = 0;
                items[i].lchar = strlen(items_text[i]);
                calc_size(&items[i]);
            }

            max_items_rendered = ((this->height - ((this->margin * 2) + this->top_offset + this->btm_offset)) / (items[0].height + 2));
            Serial.println(max_items_rendered);

            while (true) {
                y_offset = margin + top_offset;
                for (i = item_offset; i < item_count; i++) {
                    if (i >= (max_items_rendered + item_offset))
                        break;

                    if (selection_changed) {
                        tft->fillRect(margin - 1, y_offset, (width - margin) - 1, items[i].height, items[i].selected ? color : 0);
                    }
                    y_offset = render_text(y_offset, &items[i]);
                }
                selection_changed = false;
                next_render = millis() + 250;
                while (millis() < next_render) {
                    buttons->check();
                    if (buttons->l_btn()) {
                        for (i = item_count - 1; i >= 0; i--) {
                            if (items[i].selected) {
                                if (i > 0) {
                                    items[i].selected = false;
                                    items[i - 1].selected = true;
                                    if (i - 1 < item_offset)
                                        item_offset = MAX(0, item_offset - 1);
                                    break;
                                }
                            }
                        }
                        next_render = 0;
                        selection_changed = true;
                    }
                    if (buttons->r_btn()) {
                        for (i = 0; i < item_count; i++) {
                            if (items[i].selected) {
                                if (i + 1 < item_count) {
                                    items[i].selected = false;
                                    items[i + 1].selected = true;
                                    if (i + 1 >= max_items_rendered + item_offset)
                                        item_offset = MIN(item_count, item_offset + 1);
                                    break;
                                }
                            }
                        }
                        next_render = 0;
                        selection_changed = true;
                    }
                    if (buttons->m_btn()) {
                        for (i = 0; i < item_count; i++) {
                            if (items[i].selected) {
                                // For some reason simply resetting the address window doesn't work here
                                // This does appear to "reset" though, and because the image is decoded into a buffer first
                                // there's no drawing of partial frames after returning from the menu
                                tft->fillScreen(ST77XX_BLACK);
                                return i;
                            }
                        }
                    }
                }
            }
        }

    private:
        Adafruit_ST7735* tft;
        Buttons* buttons;
        // TODO: get color, w, h, etc elsewhere
        uint16_t color = 0xC81F;
        uint16_t width = 128, height = 128;
        uint16_t top_offset = 2, btm_offset = 2, margin = 5;
        char text_buf[128];

        void calc_size(MenuItem* item) {
            int16_t x1, y1;
            tft->getTextBounds(item->text, 0, 0, &x1, &y1, &item->width, &item->height);
        }

        int render_text(int y_offset, MenuItem* item) {
            int16_t x1, y1;
            uint16_t w, h;
            int max_w = width - (margin * 2);
            bool resized = false;
            while (true) {
                strncpy(text_buf, item->text + item->schar, item->lchar);
                text_buf[item->lchar] = '\0';
                tft->getTextBounds(text_buf, 0, 0, &x1, &y1, &w, &h);
                if (w > max_w) {
                    item->lchar--;
                    resized = true;
                } else if (item->lchar < strlen(item->text)) {
                    if (!resized) {
                        item->schar++;
                        if (item->schar + item->lchar > strlen(item->text)) {
                            item->schar = 0;
                        }
                    }
                    break;
                } else {
                    break;
                }
            }

            tft->setTextColor(0xffff, item->selected ? color : 0);
            tft->setCursor(margin, y_offset);
            tft->println(text_buf);
            return y_offset + item->height + 2;
        }
};