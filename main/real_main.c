#include "real_main.h"
#include "button.h"
#include "buzzer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "gy87.h"
#include "ssd1306_simple.h"
#include "tof.h"
#include <stdbool.h>

#define I2C_SDA_NUM 21
#define I2C_SCL_NUM 22

static const char *ALL_MODULES_MAIN = "REAL_MAIN";

/* System ON/OFF (button toggles this) - делаем static, но не внутри функции */
static bool system_active = false; // start OFF

void all_modules_main() {
  /* Shared I2C lock for ToF + IMU + OLED */
  SemaphoreHandle_t i2c_mutex = xSemaphoreCreateMutex();
  /* Configure ToF device on I2C */
  vl53l1x_t *dev = NULL;
  vTaskDelay(pdMS_TO_TICKS(100));
  dev = vl53l1x_config(0,           // port
                       I2C_SCL_NUM, // scl
                       I2C_SDA_NUM, // sda
                       -1,          // xshut (not used)
                       0x29,        // I2C address
                       0            // io_2v8
  );
  ESP_LOGI(ALL_MODULES_MAIN, "Start...");
  int err;
  err = tof_init(dev);
  if (err) {
    ESP_LOGI(ALL_MODULES_MAIN, "TOF init failed...");
    return;
  }
  vTaskDelay(pdMS_TO_TICKS(100));
  err = GY87_init_no_i2c_bus();
  if (err) {
    ESP_LOGI(ALL_MODULES_MAIN, "GY87 init failed...");
    return;
  }
  ESP_LOGI(ALL_MODULES_MAIN, "Init phase passed...");
  vTaskDelay(pdMS_TO_TICKS(100));
  ssd1306_init();
  buzzer_init();

  /* Button init: GPIO32 -> button -> GND (internal pull-up in button.c) */
  button_config_t btn = {.gpio = 4, .debounce_ms = 50};
  button_init(&btn);

  // int64_t start = esp_timer_get_time();
  float gz = 0.0f;
  float speed = 0.0f;
  float cadence = 0.0f;
  int turn_dir = 0;
  int turn_deg = 0;
  int spm = 0;
  int speed_x100 = 0;

  ESP_LOGI(ALL_MODULES_MAIN, "Init phase passed2...");

  while (1) {
    /* Обработка кнопки ВНУТРИ цикла */
    if (button_was_pressed()) {
      system_active = !system_active;
      buzzer_set_mode_event(system_active ? 1 : 2); // ON beep / OFF beep
      ESP_LOGI(ALL_MODULES_MAIN, "System %s",
               system_active ? "ACTIVATED" : "DEACTIVATED");
    }

    if (!system_active) {
      buzzer_set_error(0);
      buzzer_set_distance(3000, 1);
      buzzer_set_speed(0);
      buzzer_iteration();
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    /* IMU loop */
    gy87_loop_iteration(&cadence, &speed, &gz, i2c_mutex);
    spm = (int)(cadence + 0.5f);
    speed_x100 = (int)(speed * 100.0f + 0.5f);

    /* ToF loop */
    tof_loop_iteration(dev, i2c_mutex);

    /* Turn direction from gyro z-rate */
    analize_gz(&gz, &turn_dir, &turn_deg);

    /* Логирование */
    // ESP_LOGI(ALL_MODULES_MAIN,
    //          "Dist: %u mm | Speed: %.2f m/s | Gz: %.2f °/s | Cadence: %.1f
    //          spm", dev->ranging_data.range_mm, speed, gz, cadence);

    /* Установка параметров для бузера - КЛЮЧЕВОЙ МОМЕНТ! */
    buzzer_set_speed(speed);        // Устанавливаем скорость
    buzzer_set_direction(turn_dir); // Устанавливаем направление поворота
    buzzer_set_distance(dev->ranging_data.range_mm,
                        1); // Устанавливаем расстояние
    buzzer_set_error(0);    // Сбрасываем ошибку

    /* OLED обновление */
    ssd1306_render_dashboard(turn_dir, turn_deg, spm, speed_x100,
                             dev->ranging_data.range_mm, i2c_mutex);

    buzzer_iteration();

    // int actual_duration = (int)(end - start);
    // start = end;

    // ESP_LOGI(ALL_MODULES_MAIN, "Iteration took: %.1f ms",
    //          (float)actual_duration / 1000);
    vTaskDelay(pdMS_TO_TICKS(30));
  }
}
