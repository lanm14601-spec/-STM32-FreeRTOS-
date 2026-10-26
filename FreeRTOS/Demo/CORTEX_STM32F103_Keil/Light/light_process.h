#ifndef LIGHT_PROCESS_H
#define LIGHT_PROCESS_H

#include <stdint.h>

typedef enum {
    LIGHT_OK = 0,
    LIGHT_UNCALIBRATED = 1,
    LIGHT_ADC_ERROR = 2,
    LIGHT_WARMUP = 3,
    LIGHT_CONFIG_ERROR = 4
} LightStatus;

typedef enum {
    LIGHT_UNKNOWN = 0,
    LIGHT_NORMAL = 1,
    LIGHT_DARK = 2
} LightState;

typedef struct {
    uint16_t dark_adc;
    uint16_t bright_adc;
    uint16_t min_span;
    uint8_t calibrated;
    uint8_t on_pct;
    uint8_t off_pct;
    uint8_t confirm_samples;
} LightConfig;

typedef struct {
    LightState state;
    LightState candidate;
    uint8_t count;
} LightContext;

typedef struct {
    uint8_t percent; /* Meaningful ONLY when status == LIGHT_OK. */
    LightState state;
    LightStatus status;
} LightResult;

void Light_Reset(LightContext *ctx);
LightResult Light_Update(LightContext *ctx, const LightConfig *cfg,
                         uint16_t filtered, uint8_t adc_valid,
                         uint8_t filter_ready);
const char *Light_StateName(LightState state);
const char *Light_StatusName(LightStatus status);

#endif
