#include "gy87.h" 
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h" 
#include "esp_log.h" 
 
#define TAG "MAIN" 

// ESP's entry point, called automatically after bootloader run and FreeRTOS scheduler runs
void app_main(void) { 
    GY87_init(); // call initialization function (configure I2C, install I2C, wake up MPU)
 
    float gz; // store z-axis degree/sec readings, updates each loop
    while (1) { 
        GY87_read_gyro_z(&gz); 
        turn_direction_t dir = GY87_detect_turn(gz); // classify rotation based on threshold
        // converts a continuous value (gz) into a discrete state (left, right, none)
 
        if (dir == TURN_LEFT) 
            ESP_LOGI(TAG, "Turning LEFT (%.2f °/s)", gz); 
        else if (dir == TURN_RIGHT) 
            ESP_LOGI(TAG, "Turning RIGHT (%.2f °/s)", gz); 
        else 
            ESP_LOGI(TAG, "No turn (%.2f °/s)", gz); 
 
        // pause loop for 0.1 seconds 
        vTaskDelay(pdMS_TO_TICKS(100)); 
    } 
} 