#ifndef SSD1306_SIMPLE_H
#define SSD1306_SIMPLE_H

#include <stdint.h>

void ssd1306_init(void);
void ssd1306_fill(uint8_t color);
void ssd1306_draw_text(uint8_t x, uint8_t page, const char *text);

#endif
