#include "real_main.h"
#include "buzzer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "gy87.h"
#include "ssd1306_simple.h"
#include "tof.h"

#define I2C_SDA_NUM 21
#define I2C_SCL_NUM 22
static const char *ALL_MODULES_MAIN = "REAL_MAIN";

// This is main_main function
// STEPS:
// 1. Init bus. VL lib does it
// 2. Init mcu
// 3. loop
void all_modules_main() {
  SemaphoreHandle_t i2c_mutex;
  i2c_mutex = xSemaphoreCreateMutex();
  vl53l1x_t *dev = NULL;

  vTaskDelay(pdMS_TO_TICKS(100));
  dev = vl53l1x_config(0,           // port
                       I2C_SCL_NUM, // scl
                       I2C_SDA_NUM, // sda
                       -1,          // xshut (not used)
                       0x29,        // I2C address
                       0            // io_2v8
  );

  // vTaskDelay(pdMS_TO_TICKS(400));
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
    ESP_LOGI(ALL_MODULES_MAIN, "TOF init failed...");
    return;
  }

  ESP_LOGI(ALL_MODULES_MAIN, "Init phase passed...");
  vTaskDelay(pdMS_TO_TICKS(100));
  ssd1306_init();
  buzzer_init();
  buzzer_set_mode_event(1); // startup beep

  int64_t start = esp_timer_get_time();
  vTaskDelay(pdMS_TO_TICKS(100));
  float gz; // store z-axis degree/sec readings, updates each loop
  int turn_dir = 0;
  int turn_deg = 0;
  int spm = 0;
  int speed_x100 = 0;
  float speed = 0;
  float cadence = 0;
  int iteration_error = 0; // 1 is error, 0 is success
  int event = 0;           // 1 is error, 0 is success
  int direction = 0;       // 1 is error, 0 is success

  while (1) {
    int64_t end = esp_timer_get_time();
    ESP_LOGI(ALL_MODULES_MAIN, "Iteration took microsecs: %d",
             (uint)(end - start));
    gy87_loop_iteration(&cadence, &speed, &gz, i2c_mutex);
    spm = (int)(cadence + 0.5f);
    speed_x100 = (int)(speed * 100.0f + 0.5f);

    tof_loop_iteration(dev, i2c_mutex);
    ESP_LOGI(ALL_MODULES_MAIN, "Single-shot result: %u mm",
             dev->ranging_data.range_mm);
    ESP_LOGI(ALL_MODULES_MAIN, "(%.2f °/s)", gz);
    ESP_LOGI(ALL_MODULES_MAIN, "Cadence: %.1f spm | Estimated speed: %.2f m/s",
             cadence, speed);

    if (gz > 20.0f)
      turn_dir = 1;
    else if (gz < -20.0f)
      turn_dir = -1;
    else
      turn_dir = 0;
    buzzer_set_distance_cm(dev->ranging_data.range_mm, 1);
    buzzer_set_error(0);

    turn_deg = (int)gz;

    ssd1306_render_dashboard(
        turn_dir,   // turn_dir (LEFT / RIGHT / NONE) → placeholder
        turn_deg,   // turn_deg
        spm,        // steps per minute
        speed_x100, // speed * 100
        dev->ranging_data.range_mm,
        i2c_mutex);
    buzzer_iteration_main(iteration_error, event, direction,
                          dev->ranging_data.range_mm);
    start = end;
  }
}
