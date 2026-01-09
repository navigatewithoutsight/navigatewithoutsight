#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <stdint.h>
#include "ssd1306_simple.h"

#define TAG "SSD1306"

// OLED settings
#define SSD1306_ADDR 0x3C
#define I2C_PORT     I2C_NUM_0

// OLED resolution
#define OLED_W 128
#define OLED_H 64
#define FB_SIZE (OLED_W * OLED_H / 8)

// Framebuffer
static uint8_t fb[FB_SIZE];


static const uint8_t font_space[5] = {0,0,0,0,0};
static const uint8_t font_dot[5]   = {0x00,0x60,0x60,0x00,0x00};
static const uint8_t font_dash[5]  = {0x08,0x08,0x08,0x08,0x08};
static const uint8_t font_colon[5] = {0x00,0x36,0x36,0x00,0x00};
static const uint8_t font_slash[5] = {0x20,0x10,0x08,0x04,0x02};

static const uint8_t font_digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E}
};

static const uint8_t font_upper[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46},
    {0x26,0x49,0x49,0x49,0x32},
    {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},
    {0x7F,0x20,0x18,0x20,0x7F},
    {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43}
};

static const uint8_t* glyph(char c)
{
    if (c == ' ') return font_space;
    if (c >= '0' && c <= '9') return font_digits[c - '0'];
    if (c >= 'A' && c <= 'Z') return font_upper[c - 'A'];
    if (c >= 'a' && c <= 'z') return font_upper[c - 'a'];
    if (c == '.') return font_dot;
    if (c == '-') return font_dash;
    if (c == ':') return font_colon;
    if (c == '/') return font_slash;
    return font_space;
}



static void ssd1306_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    i2c_master_write_to_device(I2C_PORT, SSD1306_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static void ssd1306_set_page(uint8_t page)
{
    ssd1306_cmd(0xB0 | (page & 0x07));
    ssd1306_cmd(0x00);
    ssd1306_cmd(0x10);
}



void ssd1306_init(void)
{
    const uint8_t cmds[] = {
        0xAE,0xD5,0x80,0xA8,0x3F,0xD3,0x00,0x40,
        0x8D,0x14,0x20,0x00,0xA1,0xC8,0xDA,0x12,
        0x81,0x7F,0xA4,0xA6,0xAF
    };

    for (int i = 0; i < sizeof(cmds); i++)
        ssd1306_cmd(cmds[i]);

    ESP_LOGI(TAG, "SSD1306 initialized");
    ssd1306_clear();
    ssd1306_update();
}

void ssd1306_clear(void)
{
    memset(fb, 0x00, sizeof(fb));
}

void ssd1306_update(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        ssd1306_set_page(page);

        uint8_t data[1 + OLED_W];
        data[0] = 0x40;
        memcpy(&data[1], &fb[page * OLED_W], OLED_W);

        i2c_master_write_to_device(I2C_PORT, SSD1306_ADDR, data, sizeof(data), pdMS_TO_TICKS(100));
    }
}

/* ===================== DRAWING ===================== */

void ssd1306_draw_pixel(int x, int y, int color)
{
    if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) return;

    int index = x + (y / 8) * OLED_W;
    uint8_t mask = 1 << (y % 8);

    if (color) fb[index] |= mask;
    else fb[index] &= ~mask;
}

void ssd1306_draw_hline(int x0, int x1, int y, int color)
{
    if (y < 0 || y >= OLED_H) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= OLED_W) x1 = OLED_W - 1;
    for (int x = x0; x <= x1; x++) ssd1306_draw_pixel(x, y, color);
}

void ssd1306_draw_vline(int x, int y0, int y1, int color)
{
    if (x < 0 || x >= OLED_W) return;
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (y0 < 0) y0 = 0;
    if (y1 >= OLED_H) y1 = OLED_H - 1;
    for (int y = y0; y <= y1; y++) ssd1306_draw_pixel(x, y, color);
}

void ssd1306_draw_rect(int x, int y, int w, int h, int color)
{
    if (w <= 0 || h <= 0) return;
    ssd1306_draw_hline(x, x + w - 1, y, color);
    ssd1306_draw_hline(x, x + w - 1, y + h - 1, color);
    ssd1306_draw_vline(x, y, y + h - 1, color);
    ssd1306_draw_vline(x + w - 1, y, y + h - 1, color);
}

/* ===================== TEXT ===================== */

static void draw_char_5x7(int x, int y, char c)
{
    const uint8_t *g = glyph(c);
    for (int col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (int row = 0; row < 7; row++) {
            if ((bits >> row) & 1)
                ssd1306_draw_pixel(x + col, y + row, 1);
        }
    }
}

void ssd1306_draw_text_xy(int x, int y, const char *text)
{
    while (*text) {
        draw_char_5x7(x, y, *text);
        x += 6;
        text++;
        if (x > OLED_W - 6) break;
    }
}

/* ===================== DASHBOARD ===================== */

void ssd1306_render_dashboard(int turn_dir,
                             int turn_deg,
                             int spm,
                             int speed_mps_x100,
                             const uint8_t *depth,
                             int depth_count)
{
    const int split_x = 48;
    const int top_h   = 26;

    ssd1306_clear();

    ssd1306_draw_vline(split_x, 0, OLED_H - 1, 1);
    ssd1306_draw_hline(0, split_x - 1, top_h, 1);

    char num[12];
    char spd[12];

    ssd1306_draw_text_xy(2, 2, "SPM:");
    itoa_simple(spm, num, sizeof(num));
    ssd1306_draw_text_xy(2 + 6*4, 2, num);

    ssd1306_draw_text_xy(2, 14, "SPD:");
    format_speed_x100(speed_mps_x100, spd, sizeof(spd));
    ssd1306_draw_text_xy(2 + 6*4, 14, spd);
    ssd1306_draw_text_xy(2 + 6*4 + 6*4, 14, "M/S");

    draw_arrow(24, 42, turn_dir);

    if (turn_dir < 0) ssd1306_draw_text_xy(2, top_h + 6, "LEFT");
    else if (turn_dir > 0) ssd1306_draw_text_xy(2, top_h + 6, "RIGHT");
    else ssd1306_draw_text_xy(2, top_h + 6, "NONE");

    ssd1306_draw_text_xy(2, OLED_H - 10, "DEG:");
    itoa_simple(turn_deg, num, sizeof(num));
    ssd1306_draw_text_xy(2 + 6*4, OLED_H - 10, num);

    int map_x = split_x + 2;
    int map_y = 2;
    int map_w = OLED_W - map_x - 2;
    int map_h = OLED_H - 4;

    ssd1306_draw_rect(map_x, map_y, map_w, map_h, 1);
    ssd1306_draw_text_xy(map_x + 6, map_y + 2, "DEPTH");

    if (depth && depth_count > 0) {
        int bar_y0 = map_y + 12;
        int bar_y1 = map_y + map_h - 2;
        int bar_h  = bar_y1 - bar_y0;

        int max_bars = map_w - 2;
        if (depth_count > max_bars) depth_count = max_bars;

        for (int i = 0; i < depth_count; i++) {
            int h = depth[i];
            if (h < 0) h = 0;
            if (h > bar_h) h = bar_h;

            int x = map_x + 1 + i;
            ssd1306_draw_vline(x, bar_y1 - h, bar_y1, 1);
        }
    } else {
        ssd1306_draw_text_xy(map_x + 6, map_y + 20, "NO DATA");
    }

    ssd1306_update();
}
