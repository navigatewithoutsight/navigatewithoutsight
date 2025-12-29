#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "vl53l1x.h"

#define I2C_SDA_NUM 21
#define I2C_SCL_NUM 22
#define I2C_CLK_SPEED 400000
#define I2C_TIMEOUT 400000
#define I2C_TX_BUF 0
#define I2C_RX_BUF 0
#define I2C_MASTER_NUM I2C_NUM_0

static const char *TAG = "TOF";

static void scan_i2c() {
  ESP_LOGI(TAG, "Starting I2C scan...");

  static i2c_cmd_handle_t cmd; // Статическая переменная для экономии стека

  for (short current_addr = 1; current_addr < 127; current_addr++) {
    vTaskDelay(pdMS_TO_TICKS(100)); // Дать время на освобождение стека
    cmd = i2c_cmd_link_create();
    if (!cmd)
      continue;

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (current_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);

    esp_err_t ret =
        i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 20 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "I2C device found at 0x%02X", current_addr);
    }
  }

  ESP_LOGI(TAG, "Scan complete");
}

int tof_main(void) {
  ESP_LOGI(TAG, "Configuring VL53L1X");

  vTaskDelay(pdMS_TO_TICKS(100));
  vl53l1x_t *dev = vl53l1x_config(0,           // port
                                  I2C_SCL_NUM, // scl
                                  I2C_SDA_NUM, // sda
                                  -1,          // xshut (not used)
                                  0x29,        // I2C address
                                  0            // io_2v8
  );

  // scan_i2c();

  if (!dev) {
    ESP_LOGE(TAG, "vl53l1x_config failed");
    return -1;
  }

  vTaskDelay(pdMS_TO_TICKS(100));
  ESP_LOGI(TAG, "Calling vl53l1x_init()");
  const char *err = vl53l1x_init(dev);
  if (err) {
    ESP_LOGE(TAG, "vl53l1x_init failed: %s", err);
    return -1;
  }

  vl53l1x_setDistanceMode(dev, VL53L1X_Long);
  vTaskDelay(pdMS_TO_TICKS(100));

  /* ---- Sanity: read basic configuration ---- */

  // vl53l1x_DistanceMode mode = vl53l1x_getDistanceMode(dev);
  uint32_t budget_us = vl53l1x_getMeasurementTimingBudget(dev);

  // ESP_LOGI(TAG, "Distance mode: %d", mode);
  ESP_LOGI(TAG, "Timing budget: %lu us", (unsigned long)budget_us);

  uint8_t roi_w, roi_h;
  vl53l1x_getROISize(dev, &roi_w, &roi_h);
  ESP_LOGI(TAG, "ROI size: %ux%u", roi_w, roi_h);

  uint8_t roi_center = vl53l1x_getROICenter(dev);
  ESP_LOGI(TAG, "ROI center SPAD: %u", roi_center);

  /* ---- Single-shot measurement (blocking) ---- */

  ESP_LOGI(TAG, "Single-shot measurement");

  uint16_t distance = vl53l1x_readSingle(dev, 1);

  vl53l1x_RangeStatus status =
      (vl53l1x_RangeStatus)vl53l1x_readReg(dev, 0x0089); // RESULT__RANGE_STATUS

  ESP_LOGI(TAG, "Single-shot result: %u mm, status=%u (%s)", distance, status,
           vl53l1x_rangeStatusToString(dev, status));

  /* ---- Continuous ranging ---- */

  ESP_LOGI(TAG, "Starting continuous ranging");
  vl53l1x_startContinuous(dev, 50); // 50 ms period

  while (1) {

    if (vl53l1x_dataReady(dev)) {

      uint16_t dist = vl53l1x_read(dev, 1);

      vl53l1x_RangeStatus s = (vl53l1x_RangeStatus)vl53l1x_readReg(dev, 0x0089);

      ESP_LOGI(TAG, "Range: %5u mm | status=%u (%s)", 10, dist,
               vl53l1x_rangeStatusToString(dev, s));
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  /* not reached */
  vl53l1x_stopContinuous(dev);
  vl53l1x_end(dev);
  return 0;
}
