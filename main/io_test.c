#include "button.h"
#include "buzzer.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "gy87.h"
#include <stdbool.h>
#include <stdint.h>

static const char *TAG = "IO_TEST";
static bool go_up = true;

void generate_random_values(float *cadence_arg, float *gz_arg, float *speed_arg,
                            uint16_t *distance_mm_arg) {
  if (go_up)
    *distance_mm_arg = *distance_mm_arg + 4;
  else
    *distance_mm_arg = *distance_mm_arg - 4;

  if (*distance_mm_arg > 2500)
    go_up = false;

  if (*distance_mm_arg < 20)
    go_up = true;

  *speed_arg = 0;
  // *speed_arg = (float)esp_random() / (float)UINT32_MAX * 10.0f;
  *gz_arg = (float)esp_random() / (float)UINT32_MAX * 40.0f - 20.0f;
  *cadence_arg = (float)esp_random() / (float)UINT32_MAX * 200.0f;
};

int io_test_start() {
  ESP_LOGI(TAG, "We are in test");

  // Main vars
  uint16_t distance_mm = 0;
  float cadence = 0.0f, gz = 0.0f, speed = 0.0f;
  int turn_dir = 0, turn_deg = 0;

  /* System ON/OFF (button toggles this) */
  static bool system_active = false; // start OFF
  buzzer_init();

  button_config_t btn = {.gpio = 4, .debounce_ms = 50};
  button_init(&btn);
  int64_t start = esp_timer_get_time();

  ESP_LOGI(TAG, "Init phase passed2...");

  while (1) {

    if (button_was_pressed()) {
      system_active = !system_active;
      buzzer_set_mode_event(system_active ? 1 : 2); // ON beep / OFF beep
    }
    if (!system_active) {
      buzzer_set_error(0);
      buzzer_set_distance(3000, 1);
      buzzer_set_speed(0);
      buzzer_iteration();
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    int64_t end = esp_timer_get_time();

    generate_random_values(&cadence, &gz, &speed, &distance_mm);
    analize_gz(&gz, &turn_dir, &turn_deg);

    buzzer_set_speed(speed);
    buzzer_set_direction(turn_dir);
    buzzer_set_distance(distance_mm, 1);

    buzzer_set_error(0);
    buzzer_iteration();
    int actual_duration = (int)(end - start);
    start = end;
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG,
             "Distance: %u mm | Speed: %.2f m/s | "
             "Degrees: %.2f °/s | Cadence: %.1f | Hz: %.1f | Iteration took: "
             "%.1f ms",
             distance_mm, speed, gz, cadence, buzzer_get_hz(),
             (float)actual_duration / 1000);
  }
  return 0;
}
