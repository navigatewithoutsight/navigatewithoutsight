// VL53L1X control
// Copyright ©2021 Adrian Kennard, Andrews & Arnold Ltd, and original autors.
// See LICENCE file for details .GPL 3.0

#ifndef VL53L1X_H
#define VL53L1X_H
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
// #include <stdio.h>
// #include <string.h>
// #include <time.h>
#include <unistd.h>

typedef struct vl53l1x_s vl53l1x_t;

typedef enum {
  VL53L1X_Short,
  VL53L1X_Medium,
  VL53L1X_Long,
  VL53L1X_Unknown
} vl53l1x_DistanceMode;

typedef enum {
  VL53L1X_RangeValid = 0,
  VL53L1X_SigmaFail = 1,
  VL53L1X_SignalFail = 2,
  VL53L1X_RangeValidMinRangeClipped = 3,
  VL53L1X_OutOfBoundsFail = 4,
  VL53L1X_HardwareFail = 5,
  VL53L1X_RangeValidNoWrapCheckFail = 6,
  VL53L1X_WrapTargetFail = 7,
  VL53L1X_ProcessingFail = 8,
  VL53L1X_XtalkSignalFail = 9,
  VL53L1X_SynchronizationInt = 10,
  VL53L1X_RangeValid_MergedPulse = 11,
  VL53L1X_TargetPresentLackOfSignal = 12,
  VL53L1X_MinRangeFail = 13,
  VL53L1X_RangeInvalid = 14,
  VL53L1X_None = 255,
} vl53l1x_RangeStatus;

struct vl53l1x_s {
  uint8_t port;
  uint8_t address;
  int8_t xshut;
  uint16_t io_timeout;
  uint16_t fast_osc_frequency;
  uint16_t osc_calibrate_val;
  uint16_t timeout_start_ms;
  esp_err_t err;
  uint8_t io_2v8 : 1;
  uint8_t did_timeout : 1;
  uint8_t i2c_fail : 1;
  uint8_t calibrated : 1;
  uint8_t saved_vhv_init;
  uint8_t saved_vhv_timeout;
  struct RangingData {
    uint16_t range_mm;
    vl53l1x_RangeStatus range_status;
    float peak_signal_count_rate_MCPS;
    float ambient_count_rate_MCPS;
  } ranging_data;
  struct ResultBuffer {
    uint8_t range_status;
    uint8_t stream_count;
    uint16_t dss_actual_effective_spads_sd0;
    uint16_t ambient_count_rate_mcps_sd0;
    uint16_t final_crosstalk_corrected_range_mm_sd0;
    uint16_t peak_signal_count_rate_crosstalk_corrected_mcps_sd0;
  } results;
};

vl53l1x_t *vl53l1x_config(int8_t port, int8_t scl, int8_t sda, int8_t xshut,
                          uint8_t address, uint8_t io_2v8);
const char *vl53l1x_init(vl53l1x_t *);
void vl53l1x_end(vl53l1x_t *);

void vl53l1x_setAddress(vl53l1x_t *, uint8_t new_addr);
uint8_t vl53l1x_getAddress(vl53l1x_t *v);

void vl53l1x_writeReg8Bit(vl53l1x_t *, uint16_t reg, uint8_t value);
void vl53l1x_writeReg16Bit(vl53l1x_t *, uint16_t reg, uint16_t value);
void vl53l1x_writeReg32Bit(vl53l1x_t *, uint16_t reg, uint32_t value);
uint8_t vl53l1x_readReg8Bit(vl53l1x_t *, uint16_t reg);
uint16_t vl53l1x_readReg16Bit(vl53l1x_t *, uint16_t reg);
uint32_t vl53l1x_readReg32Bit(vl53l1x_t *, uint16_t reg);

void vl53l1x_writeMulti(vl53l1x_t *, uint16_t reg, uint8_t const *src,
                        uint8_t count);
void vl53l1x_readMulti(vl53l1x_t *, uint16_t reg, uint8_t *dst, uint8_t count);

uint8_t vl53l1x_readReg(vl53l1x_t *v, uint16_t reg);
void vl53l1x_writeReg(vl53l1x_t *v, uint16_t reg, uint8_t val);
uint8_t vl53l1x_setDistanceMode(vl53l1x_t *, vl53l1x_DistanceMode mode);
vl53l1x_DistanceMode vl53l1x_getDistanceMode(vl53l1x_t *);

uint8_t vl53l1x_setMeasurementTimingBudget(vl53l1x_t *, uint32_t budget_us);
uint32_t vl53l1x_getMeasurementTimingBudget(vl53l1x_t *);

void vl53l1x_setROISize(vl53l1x_t *, uint8_t width, uint8_t height);
void vl53l1x_getROISize(vl53l1x_t *, uint8_t *width, uint8_t *height);
void vl53l1x_setROICenter(vl53l1x_t *, uint8_t spadNum);
uint8_t vl53l1x_getROICenter(vl53l1x_t *);

void vl53l1x_startContinuous(vl53l1x_t *, uint32_t period_ms);
void vl53l1x_stopContinuous(vl53l1x_t *);
uint16_t vl53l1x_read(vl53l1x_t *, uint8_t blocking);

uint16_t vl53l1x_readSingle(vl53l1x_t *, uint8_t blocking);

uint8_t vl53l1x_dataReady(vl53l1x_t *);

const char *vl53l1x_rangeStatusToString(vl53l1x_t *,
                                        vl53l1x_RangeStatus status);

void vl53l1x_setTimeout(vl53l1x_t *, uint16_t timeout);
uint16_t vl53l1x_getTimeout(vl53l1x_t *);
uint8_t vl53l1x_timeoutOccurred(vl53l1x_t *);

#endif
