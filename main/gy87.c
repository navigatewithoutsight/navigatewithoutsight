// developed by Anastasia Badrishvili
// Dec 12, 2025
#include "gy87.h"
#include "driver/i2c.h" // ESP-IDF's I2C driver
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // timing macros

#define MPU6050_ADDR 0x68 // I^2C address of the MPU chip, 0x68 is correct when the AD0 pin is low
#define MPU6050_RA_PWR_MGMT_1 0x6B // address of power management register, MPU starts in sleep mode
// writing 0x00 wakes it up
#define MPU6050_RA_GYRO_ZOUT_H 0x47 // address of the high byte of gyroscope Z-axis data
#define GYRO_SENSITIVITY 131.0 // conversion factor from raw units → degrees/second
#define TURN_THRESHOLD 20.0 // minimum angular velocity to consider turn
#define TAG "GY87"

static const i2c_port_t I2C_PORT = I2C_NUM_0; // using the default I2C0 port on the ESP

// configuration of ESP's I2C hardware
// installation of the I2C driver
// waking up the MPU6050
int GY87_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER, // ESP32 is the I2C master
        .sda_io_num = 21,
        .scl_io_num = 22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, // internal pull-ups enabled
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000 // clock speed 400kHz, FAST MODE
    };
    esp_err_t err = i2c_param_config(I2C_PORT, &conf); // applying configurations on I2C
    if (err != ESP_OK) return -1;

    err = i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
    // installing the I2C driver
    // no RX or TX buffer, because im using blocking master mode
    if (err != ESP_OK) return -2;

    uint8_t wake_cmd[2] = {MPU6050_RA_PWR_MGMT_1, 0x00};
    // byte 0 - register address (0x6B)
    // BYTE 1 - VALUE TO WRITE (0X00)
    err = i2c_master_write_to_device(I2C_PORT, MPU6050_ADDR, wake_cmd, 2, 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK) return -3;
    // ESP sends start, sends devide address 0x68 + WRITE, sends 0x6B, 0x00, sends STOP
    // result -> MPU exits sleep mode

    ESP_LOGI(TAG, "GY-87 initialized");
    return 0;
}

// reads the current z-axis rotation rate
int GY87_read_gyro_z(float *gyro_z) {
    uint8_t reg = MPU6050_RA_GYRO_ZOUT_H; // read starting from register 0x47
    uint8_t data[2]; // data buffer, 0 -> high byte, 1 -> low byte
    esp_err_t err = i2c_master_write_read_device(I2C_PORT, MPU6050_ADDR, &reg, 1, data, 2, 100 / portTICK_PERIOD_MS);
    // on the I2C bus, START, address + WRITE, send register address 0x47
    // REPEATED START, address + read, read TWO BYTES, STOP
    if (err != ESP_OK) return -1;

    // combination of high and low bytes into a 16-bit num, MPU6050 uses big-endian
    int16_t raw = (data[0] << 8) | data[1]; // Result is a signed 16-bit number
    *gyro_z = (float)raw / GYRO_SENSITIVITY; // convert raw value to degrees per sec, write the result to the callers var
    return 0;
}

// interpreting the readings as left/right/none
turn_direction_t GY87_detect_turn(float gyro_z) {
    if (gyro_z > TURN_THRESHOLD) return TURN_RIGHT; // rotating clockwise
    if (gyro_z < -TURN_THRESHOLD) return TURN_LEFT; // rotating counter-clockwise
    return TURN_NONE;
}