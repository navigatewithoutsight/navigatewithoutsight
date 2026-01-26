#include "button.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static QueueHandle_t s_btn_queue = NULL;
static gpio_num_t s_gpio = GPIO_NUM_NC;
static uint32_t s_debounce_ms = 50;

static int64_t s_last_edge_us = 0;
static int64_t s_press_start_us = 0;

static void IRAM_ATTR button_isr(void *arg) {
  (void)arg;

  const int64_t now_us = esp_timer_get_time();

  /* debounce */
  if (now_us - s_last_edge_us < (int64_t)s_debounce_ms * 1000) {
    return;
  }
  s_last_edge_us = now_us;

  const int level =
      gpio_get_level(s_gpio); /* pull-up => released=1, pressed=0 */

  if (level == 0) {
    /* pressed */
    s_press_start_us = now_us;
  } else {
    /* released => register 1 press */
    (void)s_press_start_us;

    bool pressed_evt = true;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_btn_queue, &pressed_evt, &woken);
    if (woken == pdTRUE) {
      portYIELD_FROM_ISR();
    }
  }
}

void button_init(const button_config_t *cfg) {
  if (!cfg)
    return;

  s_gpio = cfg->gpio;
  s_debounce_ms = cfg->debounce_ms;

  if (!s_btn_queue) {
    s_btn_queue = xQueueCreate(8, sizeof(bool));
  }

  gpio_config_t io = {0};
  io.pin_bit_mask = 1ULL << s_gpio;
  io.mode = GPIO_MODE_INPUT;
  io.pull_up_en = GPIO_PULLUP_ENABLE;
  io.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io.intr_type = GPIO_INTR_ANYEDGE;
  gpio_config(&io);

  static bool isr_installed = false;
  if (!isr_installed) {
    gpio_install_isr_service(0);
    isr_installed = true;
  }

  gpio_isr_handler_add(s_gpio, button_isr, NULL);
}

bool button_was_pressed(void) {
  if (!s_btn_queue)
    return false;

  bool evt = false;
  if (xQueueReceive(s_btn_queue, &evt, 0) == pdTRUE) {
    return true;
  }
  return false;
}
