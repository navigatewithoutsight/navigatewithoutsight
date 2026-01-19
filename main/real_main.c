#include "real_main.h"
#include "buzzer.h"
#include "esp_log.h"
#include "gy87.h"
#include "ssd1306_simple.h"
#include "tof.h"
#include "vl53l1x.h"

static const char *ALL_MODULES_MAIN = "TOF";

void all_modules_main() {
  // STEPS:
  // 1. Init bus. VL lib does it
  // 2. Init mcu
  // 3. loop
  vl53l1x_t *dev = NULL;

  ESP_LOGI(ALL_MODULES_MAIN, "Start...");
  int err;
  err = tof_init(dev);
  if (err) {
    ESP_LOGI(ALL_MODULES_MAIN, "TOF init failed...");
    return;
  }
  err = GY87_init_no_i2c_bus();
  if (err) {
    ESP_LOGI(ALL_MODULES_MAIN, "TOF init failed...");
    return;
  }

  ESP_LOGI(ALL_MODULES_MAIN, "Init phase passed...");
  ssd1306_init();
  buzzer_init();
  buzzer_set_mode_event(1); // startup beep

  float gz; // store z-axis degree/sec readings, updates each loop
  int turn_dir = 0;
  int turn_deg = 0;
  int spm = 0;
  int speed_x100 = 0;
  int iteration_error = 0; // 1 is error, 0 is success
  int event = 0;           // 1 is error, 0 is success
  int direction = 0;       // 1 is error, 0 is success
  int distance = 999;
  while (1) {
    ESP_LOGI(ALL_MODULES_MAIN, "Iteration starts...");
    gy87_loop_iteration(&gz);
    distance = tof_loop_iteration(dev);
    if (gz > 20.0f) turn_dir =1;
    else if (gz < -20.0f) turn_dir = -1;
    else turn_dir = 0;
    /*// Testing: cahnge with real distance from ToF
    buzzer_set_distance_cm(999, 1);  // 999cm = far away, valid reading
    buzzer_set_error(0);            // no error for now
  */
    turn_deg = (int)gz;

    ssd1306_render_dashboard(turn_dir, // turn_dir (LEFT / RIGHT / NONE) → placeholder
                             turn_deg, // turn_deg
                             spm, // steps per minute
                             speed_x100, // speed * 100
                             NULL, // depth
                             0);
    buzzer_iteration_main(iteration_error, event, direction, distance);
    ESP_LOGI(ALL_MODULES_MAIN, "Iteration ends...");
  }
}
