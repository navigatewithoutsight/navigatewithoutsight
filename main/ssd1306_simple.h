#ifndef SSD1306_SIMPLE_H
#define SSD1306_SIMPLE_H
#include "freertos/idf_additions.h"
#include <stdint.h>

void ssd1306_init(void);
void ssd1306_clear(void);
void ssd1306_update(void);

void ssd1306_draw_pixel(int x, int y, int color);

void ssd1306_draw_hline(int x0, int x1, int y, int color);

void ssd1306_draw_vline(int x, int y0, int y1, int color);

void ssd1306_draw_rect(int x, int y, int w, int h, int color);

void ssd1306_draw_text_xy(int x, int y, const char *text);

/* ===================== DASHBOARD ===================== */
/*
 * turn_dir: -1 = left, 0 = none, +1 = right
 * turn_deg: turn angle in degrees
 * spm: steps per minute
 * speed_mps_x100: speed * 100 (e.g. 135 = 1.35 m/s)
 * depth: optional depth array (can be NULL)
 * depth_count: number of depth values
 */
void ssd1306_render_dashboard(int turn_dir, int turn_deg, int spm,
                              int speed_mps_x100, const uint8_t *depth,
                              int depth_count, SemaphoreHandle_t mutex);

#endif
