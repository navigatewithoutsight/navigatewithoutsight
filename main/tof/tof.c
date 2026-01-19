#include "tof.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include <stdint.h>

#define I2C_SDA_NUM 21
#define I2C_SCL_NUM 22
#define I2C_CLK_SPEED 400000
#define I2C_TIMEOUT 400000
#define I2C_TX_BUF 0
#define I2C_RX_BUF 0
#define I2C_MASTER_NUM I2C_NUM_0
#define SOFT_RESET 0x0000
#define SYSTEM__INTERRUPT_CLEAR 0x0086
#define RESULT__RANGE_STATUS 0x0089

static const char *TAG = "TOF";

// Функция для диагностики датчика
void vl53l1x_diagnostics(vl53l1x_t *v) {
  // Прочитайте ключевые регистры
  uint16_t model_id = vl53l1x_readReg16Bit(v, 0x010F);
  uint8_t revision_id = vl53l1x_readReg(v, 0x0111);
  uint8_t system_status = vl53l1x_readReg(v, 0x00E5);
  uint8_t mode_status = vl53l1x_readReg(v, 0x00E6);

  ESP_LOGI(TAG, "Diagnostics:");
  ESP_LOGI(TAG, "  Model ID: 0x%04X (expected 0xEACC)", model_id);
  ESP_LOGI(TAG, "  Revision ID: 0x%02X", revision_id);
  ESP_LOGI(TAG, "  System Status: 0x%02X", system_status);
  ESP_LOGI(TAG, "  Mode Status: 0x%02X", mode_status);

  // Проверьте VCSEL периоды
  uint8_t vcsel_period_a = vl53l1x_readReg(v, 0x0060);
  uint8_t vcsel_period_b = vl53l1x_readReg(v, 0x0063);
  ESP_LOGI(TAG, "  VCSEL Period A: 0x%02X", vcsel_period_a);
  ESP_LOGI(TAG, "  VCSEL Period B: 0x%02X", vcsel_period_b);

  // Проверьте таймауты
  uint16_t timeout_a = vl53l1x_readReg16Bit(v, 0x005E);
  uint16_t timeout_b = vl53l1x_readReg16Bit(v, 0x0061);
  ESP_LOGI(TAG, "  Timeout A: 0x%04X", timeout_a);
  ESP_LOGI(TAG, "  Timeout B: 0x%04X", timeout_b);
}

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

int tof_init(vl53l1x_t *dev) {
  ESP_LOGI(TAG, "tof_init called");

  vTaskDelay(pdMS_TO_TICKS(100));
  // dev = vl53l1x_config(0,           // port
  //                      I2C_SCL_NUM, // scl
  //                      I2C_SDA_NUM, // sda
  //                      -1,          // xshut (not used)
  //                      0x29,        // I2C address
  //                      0            // io_2v8
  // );

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

  // Complete reset. XSHUT doesnt work, this is software reset
  vl53l1x_writeReg(dev, SOFT_RESET, 0x00);
  vTaskDelay(pdMS_TO_TICKS(10));
  vl53l1x_writeReg(dev, SOFT_RESET, 0x01);
  vTaskDelay(pdMS_TO_TICKS(100));

  err = vl53l1x_init(dev);
  ESP_LOGI(TAG, "Setting up for single-shot measurements...");

  vl53l1x_setDistanceMode(dev, VL53L1X_Long);
  vTaskDelay(pdMS_TO_TICKS(100));

  vl53l1x_setMeasurementTimingBudget(dev, 100000); // 100 ms
  vTaskDelay(pdMS_TO_TICKS(100));

  vl53l1x_setROICenter(dev, 199); // Middle SPAD
  vl53l1x_setROISize(dev, 4, 4);  // Small ROI (4x4)
  vTaskDelay(pdMS_TO_TICKS(100));

  // 4. Wipe calibration
  vl53l1x_writeReg(dev, SYSTEM__INTERRUPT_CLEAR, 0x01);
  vTaskDelay(pdMS_TO_TICKS(100));

  /* ---- Sanity: read basic configuration ---- */
  uint32_t budget_us = vl53l1x_getMeasurementTimingBudget(dev);
  ESP_LOGI(TAG, "Timing budget: %lu us", (unsigned long)budget_us);

  uint8_t roi_w, roi_h;
  vl53l1x_getROISize(dev, &roi_w, &roi_h);
  ESP_LOGI(TAG, "ROI size: %ux%u", roi_w, roi_h);

  uint8_t roi_center = vl53l1x_getROICenter(dev);
  ESP_LOGI(TAG, "ROI center SPAD: %u", roi_center);

  vl53l1x_diagnostics(dev);
  // 5. Calibration
  ESP_LOGI(TAG, "Starting continuous mode for calibration...");
  vl53l1x_startContinuous(dev, 100); // 100 ms период
  vTaskDelay(pdMS_TO_TICKS(500));    // Дайте время на калибровку

  // 6. Why not, probably redundant tho
  for (int i = 0; i < 5; i++) {
    uint16_t distance = vl53l1x_read(dev, 1);
    ESP_LOGI(TAG, "Continuous reading %d: %u mm", i + 1, distance);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  vl53l1x_stopContinuous(dev);
  vTaskDelay(pdMS_TO_TICKS(100));

  /* ---- Single-shot ---- */
  ESP_LOGI(TAG, "Starting single-shot measurements...(tof_init)");
  return 0;
}

int tof_loop_iteration(vl53l1x_t *dev) {
  vl53l1x_writeReg(dev, SYSTEM__INTERRUPT_CLEAR, 0x01);

  uint16_t distance = vl53l1x_readSingle(dev, 1);

  //
  uint8_t raw_status =
      vl53l1x_readReg(dev, RESULT__RANGE_STATUS); // RESULT__RANGE_STATUS

  ESP_LOGI(TAG, "Single-shot result: %u mm, raw_status=0x%02X", distance,
           raw_status);

  // vl53l1x_rangeStatusToString copy-paste
  switch (raw_status) {
  case 0x09: // Range Complete
    ESP_LOGI(TAG, "Status: Range Valid");
    return distance;
  case 0x02: // Signal Fail
    ESP_LOGI(TAG, "Status: Signal Fail (no target)");
    break;
  case 0x04: // Sigma Fail
    ESP_LOGI(TAG, "Status: Sigma Fail");
    break;
  case 0x05: // Out of Bounds
    ESP_LOGI(TAG, "Status: Out of Bounds");
    break;
  case 0x07: // Wrap Target Fail
    ESP_LOGI(TAG, "Status: Wrap Target Fail");
    break;
  default:
    ESP_LOGI(TAG, "Status: Unknown (0x%02X)", raw_status);
    return -1;
  }
  return 0;
}

int tof_main(void) {
  ESP_LOGI(TAG, "we enter tof_main");

  vTaskDelay(pdMS_TO_TICKS(100));
  vl53l1x_t *dev = vl53l1x_config(0,           // port
                                  I2C_SCL_NUM, // scl
                                  I2C_SDA_NUM, // sda
                                  -1,          // xshut (not used)
                                  0x29,        // I2C address
                                  0            // io_2v8
  );

  tof_init(dev);

  if (!dev) {
    ESP_LOGI(TAG, "dev is NULL");
    return 1;
  }
  /* ---- Single-shot ---- */
  ESP_LOGI(TAG, "entering loop");

  int res;
  while (1) {
    res = tof_loop_iteration(dev);
    vTaskDelay(pdMS_TO_TICKS(30));
    if (res <= -1) {
      return -1;
    }
  }
  return 0;
}

int get_random_test_tof_values() {
  uint32_t value = esp_random();
  int cropped_value = (int)value;
  return cropped_value;
}
