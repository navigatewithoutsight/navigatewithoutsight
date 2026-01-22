// developed by Anastasia Badrishvili
// Dec 12, 2025

#include "gy87.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "ssd1306_simple.h"
#include <math.h>

/* ================= MPU6050 REGISTERS ================= */

#define MPU6050_ADDR 0x68

#define MPU6050_RA_PWR_MGMT_1 0x6B
#define MPU6050_RA_GYRO_ZOUT_H 0x47
#define MPU6050_RA_ACCEL_XOUT_H 0x3B

/* ================= SENSOR SCALING ================= */

#define GYRO_SENSITIVITY 131.0f       // ±250 dps
#define ACCEL_SENSITIVITY_2G 16384.0f // ±2g

/* ================= TURN DETECTION ================= */

#define TURN_THRESHOLD 20.0f // deg/s

/* ================= STEP / GAIT MODEL ================= */

#define STEP_THRESHOLD_G 0.15f // torso-friendly
#define MIN_STEP_INTERVAL_MS 300

#define CADENCE_WINDOW_MS 3000      // 3-second window
#define ASSUMED_STEP_LENGTH_M 0.75f // average adult

/* ================= I2C ================= */

static const i2c_port_t I2C_PORT = I2C_NUM_0;
#define TAG "GY87"

/* ================= INTERNAL STATE ================= */

static int step_count = 0;
static int64_t last_step_time_us = 0;
static int64_t cadence_window_start_us = 0;

/* =================================================== */
/* ================== CORE FUNCTIONS ================= */
/* =================================================== */

int GY87_init(void) {
  i2c_config_t conf = {.mode = I2C_MODE_MASTER,
                       .sda_io_num = 21,
                       .scl_io_num = 22,
                       .sda_pullup_en = GPIO_PULLUP_ENABLE,
                       .scl_pullup_en = GPIO_PULLUP_ENABLE,
                       .master.clk_speed = 400000};

  if (i2c_param_config(I2C_PORT, &conf) != ESP_OK)
    return -1;

  if (i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0) != ESP_OK)
    return -2;

  uint8_t wake_cmd[2] = {MPU6050_RA_PWR_MGMT_1, 0x00};
  if (i2c_master_write_to_device(I2C_PORT, MPU6050_ADDR, wake_cmd, 2,
                                 pdMS_TO_TICKS(100)) != ESP_OK)
    return -3;

  ESP_LOGI(TAG, "GY-87 initialized");
  return 0;
}

/* ================= GYRO ================= */

int GY87_read_gyro_z(float *gyro_z, SemaphoreHandle_t mutex) {
  uint8_t reg = MPU6050_RA_GYRO_ZOUT_H;
  uint8_t data[2];

  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    if (i2c_master_write_read_device(I2C_PORT, MPU6050_ADDR, &reg, 1, data, 2,
                                     pdMS_TO_TICKS(100)) != ESP_OK)
      return -1;

    xSemaphoreGive(mutex);
    // ESP_LOGI(TAG, "Releasing lock");
  } else {

    // ESP_LOGI(TAG, "Can't aquire lock");
    return 0;
  }
  int16_t raw = (data[0] << 8) | data[1];
  *gyro_z = (float)raw / GYRO_SENSITIVITY;
  return 0;
}

turn_direction_t GY87_detect_turn(float gyro_z) {
  if (gyro_z > TURN_THRESHOLD)
    return TURN_RIGHT;
  if (gyro_z < -TURN_THRESHOLD)
    return TURN_LEFT;
  return TURN_NONE;
}

/* ================= ACCELEROMETER ================= */

int GY87_read_accel(float *ax, float *ay, float *az) {
  uint8_t reg = MPU6050_RA_ACCEL_XOUT_H;
  uint8_t data[6];

  if (i2c_master_write_read_device(I2C_PORT, MPU6050_ADDR, &reg, 1, data, 6,
                                   pdMS_TO_TICKS(100)) != ESP_OK)
    return -1;

  int16_t rx = (data[0] << 8) | data[1];
  int16_t ry = (data[2] << 8) | data[3];
  int16_t rz = (data[4] << 8) | data[5];

  *ax = rx / ACCEL_SENSITIVITY_2G;
  *ay = ry / ACCEL_SENSITIVITY_2G;
  *az = rz / ACCEL_SENSITIVITY_2G;

  return 0;
}

/* ================= STEP DETECTION ================= */

static bool detect_step(float ax, float ay, float az) {
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  float dynamic = mag - 1.0f;

  int64_t now = esp_timer_get_time();

  if (dynamic > STEP_THRESHOLD_G) {
    if ((now - last_step_time_us) > (MIN_STEP_INTERVAL_MS * 1000)) {
      last_step_time_us = now;
      return true;
    }
  }
  return false;
}

/* ================= CADENCE & SPEED ================= */

void GY87_update_cadence_and_speed(SemaphoreHandle_t mutex) {
  float ax, ay, az;

  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    if (GY87_read_accel(&ax, &ay, &az) != 0)
      return;

    xSemaphoreGive(mutex);
    // ESP_LOGI("GAIT", "Released lock");
  } else {
    // ESP_LOGI("GAIT", "Can't aquire lock");
    return;
  }
  if (detect_step(ax, ay, az))
    step_count++;

  int64_t now = esp_timer_get_time();

  if (cadence_window_start_us == 0)
    cadence_window_start_us = now;

  if ((now - cadence_window_start_us) > (CADENCE_WINDOW_MS * 1000)) {
    float window_sec = CADENCE_WINDOW_MS / 1000.0f;
    float cadence_hz = step_count / window_sec;
    float cadence_spm = cadence_hz * 60.0f;
    float speed_mps = cadence_hz * ASSUMED_STEP_LENGTH_M;

    // ESP_LOGI("GAIT", "Cadence: %.1f spm | Estimated speed: %.2f m/s",
    //          cadence_spm, speed_mps);

    step_count = 0;
    cadence_window_start_us = now;
  }
}

/* ================= MAIN LOOP ================= */

void gy87_loop_iteration(float *gz, SemaphoreHandle_t mutex) {
  if (GY87_read_gyro_z(gz, mutex) == 0) {
    // turn_direction_t dir = GY87_detect_turn(*gz);
    // ssd1306_clear();
    //
    // if (dir == TURN_LEFT) {
    //   // ESP_LOGI(TAG, "Turning LEFT (%.2f °/s)", *gz);
    //   ssd1306_draw_text_xy(20, 3, "LEFT");
    // } else if (dir == TURN_RIGHT) {
    //   // ESP_LOGI(TAG, "Turning RIGHT (%.2f °/s)", *gz);
    //   ssd1306_draw_text_xy(20, 3, "RIGHT");
    // } else {
    //   // ESP_LOGI(TAG, "No turn (%.2f °/s)", *gz);
    //   ssd1306_draw_text_xy(10, 3, "NO TURN");
    // }
  }

  GY87_update_cadence_and_speed(mutex);
}

void gy87_main(void) {
  SemaphoreHandle_t i2c_mutex;
  i2c_mutex = xSemaphoreCreateMutex();

  GY87_init();

  float gz;

  while (1) {
    if (GY87_read_gyro_z(&gz, i2c_mutex) == 0) {
      turn_direction_t dir = GY87_detect_turn(gz);
      ssd1306_clear();

      if (dir == TURN_LEFT) {
        ESP_LOGI(TAG, "Turning LEFT (%.2f °/s)", gz);
        ssd1306_draw_text_xy(20, 3, "LEFT");
      } else if (dir == TURN_RIGHT) {
        ESP_LOGI(TAG, "Turning RIGHT (%.2f °/s)", gz);
        ssd1306_draw_text_xy(20, 3, "RIGHT");
      } else {
        ESP_LOGI(TAG, "No turn (%.2f °/s)", gz);
        ssd1306_draw_text_xy(10, 3, "NO TURN");
      }
    }

    GY87_update_cadence_and_speed(i2c_mutex);

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

int GY87_init_no_i2c_bus(void) {
  uint8_t wake_cmd[2] = {MPU6050_RA_PWR_MGMT_1, 0x00};
  // byte 0 - register address (0x6B)
  // BYTE 1 - VALUE TO WRITE (0X00)
  esp_err_t err = i2c_master_write_to_device(I2C_PORT, MPU6050_ADDR, wake_cmd,
                                             2, 1000 / portTICK_PERIOD_MS);
  if (err != ESP_OK)
    return -3;
  // ESP sends start, sends devide address 0x68 + WRITE, sends 0x6B, 0x00, sends
  // STOP result -> MPU exits sleep mode

  ESP_LOGI(TAG, "GY-87 initialized");
  return 0;
}
