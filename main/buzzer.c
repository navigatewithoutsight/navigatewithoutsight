#include "buzzer.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <stdint.h>

static const char *TAG = "buzzer";

/* System state */
static int   s_distance_cm = 999;
static bool  s_distance_ok = false;
static int   s_direction   = 0;
static int   s_mode_event  = 0;
static bool  s_error_on    = false;

/* Buzzer state */
static bool    s_active_is_on  = false;
static bool    s_passive_is_on = false;
static int64_t s_passive_event_end_us = 0;

/* Time helper */
static inline int64_t now_us(void)
{
    return esp_timer_get_time();
}

/* Active buzzer control */
static inline void active_set(bool on)
{
    gpio_set_level(ACTIVE_BUZZER_PIN, on ? 1 : 0);
    s_active_is_on = on;
}

/* Passive buzzer PWM config */
#define PASSIVE_LEDC_MODE        LEDC_LOW_SPEED_MODE
#define PASSIVE_LEDC_TIMER       LEDC_TIMER_0
#define PASSIVE_LEDC_CHANNEL     LEDC_CHANNEL_0
#define PASSIVE_LEDC_DUTY_RES    LEDC_TIMER_10_BIT
#define PASSIVE_LEDC_DUTY_ON     512
#define PASSIVE_DEFAULT_FREQ_HZ  2000

static void passive_pwm_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = PASSIVE_LEDC_MODE,
        .duty_resolution = PASSIVE_LEDC_DUTY_RES,
        .timer_num       = PASSIVE_LEDC_TIMER,
        .freq_hz         = PASSIVE_DEFAULT_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .gpio_num   = PASSIVE_BUZZER_PIN,
        .speed_mode = PASSIVE_LEDC_MODE,
        .channel    = PASSIVE_LEDC_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = PASSIVE_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&ch);
}

static void passive_set_tone(bool on, uint32_t freq_hz)
{
    ledc_set_freq(PASSIVE_LEDC_MODE, PASSIVE_LEDC_TIMER, freq_hz);
    ledc_set_duty(PASSIVE_LEDC_MODE, PASSIVE_LEDC_CHANNEL,
                  on ? PASSIVE_LEDC_DUTY_ON : 0);
    ledc_update_duty(PASSIVE_LEDC_MODE, PASSIVE_LEDC_CHANNEL);

    s_passive_is_on = on;
}

/* Public API */

void buzzer_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << ACTIVE_BUZZER_PIN,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };
    gpio_config(&io);
    active_set(false);

    passive_pwm_init();
    passive_set_tone(false, PASSIVE_DEFAULT_FREQ_HZ);

    s_distance_cm = 999;
    s_distance_ok = false;
    s_direction   = 0;
    s_mode_event  = 0;
    s_error_on    = false;
    s_passive_event_end_us = 0;

    ESP_LOGI(TAG, "Buzzer initialized");
}

void buzzer_set_distance_cm(int distance_value, int is_valid)
{
    int cm = distance_value;

    if (distance_value > 300) {
        cm = distance_value / 10; // assume mm
    }

    if (cm < 0) cm = 0;
    if (cm > 999) cm = 999;

    s_distance_cm = cm;
    s_distance_ok = (is_valid != 0);
}

void buzzer_set_direction(int direction)
{
    s_direction = direction;
}

void buzzer_set_mode_event(int event_code)
{
    s_mode_event = event_code;
}

void buzzer_set_error(int has_error)
{
    s_error_on = (has_error != 0);
}

/* Distance to beep timing */
static uint32_t distance_to_period_ms(int distance_cm)
{
    if (distance_cm >= 150) return 1000;
    if (distance_cm <= 30)  return 150;

    return 150 + (uint32_t)((1000 - 150) * (distance_cm - 30) / 120);
}

/* Passive buzzer one-shot tones */
static void service_passive_mode_event(int64_t tnow)
{
    if (s_mode_event == 0) return;
    if (s_passive_event_end_us > tnow) return;

    uint32_t freq = 2000;
    uint32_t dur_ms = 120;

    if (s_mode_event == 1)      { freq = 2500; dur_ms = 150; }
    else if (s_mode_event == 2) { freq = 1200; dur_ms = 180; }
    else if (s_mode_event == 3) { freq = 1800; dur_ms = 120; }

    passive_set_tone(true, freq);
    s_passive_event_end_us = tnow + (int64_t)dur_ms * 1000;
    s_mode_event = 0;
}

static void service_passive_stop(int64_t tnow)
{
    if (s_passive_is_on &&
        s_passive_event_end_us > 0 &&
        tnow >= s_passive_event_end_us)
    {
        passive_set_tone(false, PASSIVE_DEFAULT_FREQ_HZ);
        s_passive_event_end_us = 0;
    }
}

/* Error pattern on active buzzer */
static void service_active_error(int64_t tnow)
{
    int64_t t = tnow % (1200 * 1000);

    bool on = (t < 100000) ||
              (t >= 200000 && t < 300000) ||
              (t >= 400000 && t < 500000);

    if (on != s_active_is_on) active_set(on);
}

/* Direction cue (optional) */
static void service_active_direction(int64_t tnow)
{
    if (s_direction == 0) return;

    int64_t t = tnow % (600 * 1000);
    bool on = false;

    if (s_direction < 0) {
        on = (t < 80000) || (t >= 160000 && t < 240000);
    } else {
        on = (t < 200000);
    }

    if (on != s_active_is_on) active_set(on);
    if (t >= 599000) s_direction = 0;
}

/* Main distance-based warning */
static void service_active_distance(int64_t tnow)
{
    if (!s_distance_ok) {
        service_active_error(tnow);
        return;
    }

    if (s_distance_cm > 0 && s_distance_cm < 30) {
        if (!s_active_is_on) active_set(true);
        return;
    }

    int64_t period_us =
        (int64_t)distance_to_period_ms(s_distance_cm) * 1000;

    bool on = (tnow % period_us) < (60 * 1000);
    if (on != s_active_is_on) active_set(on);
}

void buzzer_iteration(void)
{
    int64_t tnow = now_us();

    service_passive_mode_event(tnow);
    service_passive_stop(tnow);

    if (s_error_on) {
        service_active_error(tnow);
        return;
    }

    if (s_direction != 0) {
        service_active_direction(tnow);
        return;
    }

    service_active_distance(tnow);
}

void buzzer_iteration_main(int err, int event, int direction_arg, int distance_mm)
{
    buzzer_set_error(err);
    if (event) buzzer_set_mode_event(event);
    if (direction_arg) buzzer_set_direction(direction_arg);

    buzzer_set_distance_cm(distance_mm, 1);
    buzzer_iteration();
}
