#include "buzzer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>

// Active buzzer: (obstacle/direction/error)
// Passive buzzer: (startup/mode)

// saved values
static int distance_cm = 999; // last distance we got
static int distance_ok = 0;

static int direction = 0;
static int mode_event = 0;
static int error_on = 0; // 1 = error, 0 = normal

// on off
static void active_on(void) { gpio_set_level(ACTIVE_BUZZER_PIN, 1); }

static void active_off(void) { gpio_set_level(ACTIVE_BUZZER_PIN, 0); }

static void passive_on(void) { gpio_set_level(PASSIVE_BUZZER_PIN, 1); }

static void passive_off(void) { gpio_set_level(PASSIVE_BUZZER_PIN, 0); }

/* -------------------------
   Public functions
   ------------------------- */
void buzzer_init(void) {
  // Set the buzzer pins as outputs
  gpio_set_direction(ACTIVE_BUZZER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(PASSIVE_BUZZER_PIN, GPIO_MODE_OUTPUT);

  // Make sure both buzzers start OFF
  active_off();
  passive_off();
}

void buzzer_set_distance_cm(int new_distance_cm, int is_valid) {
  // Save the latest distance reading
  distance_cm = new_distance_cm;
  distance_ok = is_valid;
}

void buzzer_set_direction(int new_direction) {

  // -1 = left, 0 = ok, 1 = right
  direction = new_direction;
}

void buzzer_set_mode_event(int event_code) {
  // Save a mode/startup event to play once
  mode_event = event_code;
}

void buzzer_set_error(int has_error) {

  // 1 = error, 0 = normal
  error_on = has_error;
}

static void play_error_pattern(void) {
  // 3x beep, then a pause
  for (int i = 0; i < 3; i++) {
    active_on();
    vTaskDelay(pdMS_TO_TICKS(100));
    active_off();
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Longer pause
  vTaskDelay(pdMS_TO_TICKS(400));
}

static void play_mode_event_pattern(void) {
  // Passive buzzer short double beep for startup/mode change
  passive_on();
  vTaskDelay(pdMS_TO_TICKS(80));
  passive_off();
  vTaskDelay(pdMS_TO_TICKS(80));
  passive_on();
  vTaskDelay(pdMS_TO_TICKS(80));
  passive_off();

  //  played it once, clear
  mode_event = 0;
}

/// warnings
// left (-1)  = two short beeps
// right (1)  = one long beep
static void direction_pattern(int direction) {

  if (direction < 0) {
    // left: two short beeps
    for (int i = 0; i < 2; i++) {
      active_on();
      vTaskDelay(pdMS_TO_TICKS(80));
      active_off();
      vTaskDelay(pdMS_TO_TICKS(80));
    }
  } else if (direction > 0) {
    // right: one long beep
    active_on();
    vTaskDelay(pdMS_TO_TICKS(200));
    active_off();
  }

  // Clear direction so it doesn't keep repeating forever
  direction = 0;
}

static void play_direction_pattern(void) {
  /// warnings
  // left (-1)  = two short beeps
  // right (1)  = one long beep

  if (direction < 0) {
    // left: two short beeps
    for (int i = 0; i < 2; i++) {
      active_on();
      vTaskDelay(pdMS_TO_TICKS(80));
      active_off();
      vTaskDelay(pdMS_TO_TICKS(80));
    }
  } else if (direction > 0) {
    // right: one long beep
    active_on();
    vTaskDelay(pdMS_TO_TICKS(200));
    active_off();
  }

  // Clear direction so it doesn't keep repeating forever
  direction = 0;
}

static void play_distance_pattern(void) {
  // Distance warnings:
  // - closer distance -> faster beeps
  // - below 30cm -> continuous tone

  if (distance_ok == 0) {

    play_error_pattern();
    return;
  }

  if (distance_cm < 30) {
    // Danger: continuous tone
    active_on();
    vTaskDelay(pdMS_TO_TICKS(200));
    return; // keep it on
  }

  // If not danger zone, ensure it's off before beeping
  active_off();

  // How long to wait between beeps
  int wait_ms = 1000;

  if (distance_cm > 150)
    wait_ms = 1000; // far
  else if (distance_cm > 80)
    wait_ms = 500;
  else if (distance_cm > 50)
    wait_ms = 250;
  else
    wait_ms = 150; // close

  //  one short beep
  active_on();
  vTaskDelay(pdMS_TO_TICKS(60));
  active_off();

  vTaskDelay(pdMS_TO_TICKS(wait_ms));
}

// void buzzer_iteration() {
//
//   if (error_on == 1) {
//     play_error_pattern();
//     return;
//   }
//
//   if (mode_event != 0) {
//     play_mode_event_pattern();
//     return;
//   }
//
//   if (direction != 0) {
//     play_direction_pattern();
//     return;
//   }
//
//   play_distance_pattern();
// }

// Distance warnings:
// - closer distance -> faster beeps
// - below 30cm -> continuous tone
static void play_distance_pattern_main(int distance_cm_arg) {

  if (distance_cm_arg == 0) {
    play_error_pattern();
    return;
  }

  if (distance_cm_arg < 30) {
    // Danger: continuous tone
    active_on();
    vTaskDelay(pdMS_TO_TICKS(200));
    return; // keep it on
  }

  // If not danger zone, ensure it's off before beeping
  active_off();

  // How long to wait between beeps
  int wait_ms = 1000; // 1 second.

  if (distance_cm_arg > 150)
    wait_ms = 1000; // far
  else if (distance_cm_arg > 80)
    wait_ms = 500;
  else if (distance_cm_arg > 50)
    wait_ms = 250;
  else
    wait_ms = 150; // close

  //  one short beep
  active_on();
  vTaskDelay(pdMS_TO_TICKS(60));
  active_off();

  // vTaskDelay(pdMS_TO_TICKS(wait_ms));
}
void buzzer_iteration_main(int err, int event, int direction_arg,
                           int distance_cm) {

  if (err == 1) {
    play_error_pattern();
    return;
  }

  if (event != 0) {
    play_mode_event_pattern();
    return;
  }

  if (direction_arg != 0) {
    direction_pattern(direction_arg);
    return;
  }

  play_distance_pattern_main(distance_cm);
}
