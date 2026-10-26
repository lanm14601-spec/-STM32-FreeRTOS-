#ifndef APP_ADC_H
#define APP_ADC_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

typedef struct
{
    uint32_t sequence;
    TickType_t tick;
    uint16_t raw;
    uint16_t filtered;
    uint32_t voltage_mv;
    uint8_t valid;
    uint8_t status;
    uint8_t sample_count;
    uint8_t light_percent;
    uint8_t light_state;
    uint8_t light_status;
} AdcSample_t;

/* Call ONCE, before vTaskStartScheduler().
 * 0 = success, 1 = queue failure, 2 = ADC task failure,
 * 3 = monitor task failure, 4 = already created.
 */
int App_ADC_Create(void);

/* Watch variables: diagnostics only, not a synchronized multi-field snapshot. */
extern volatile uint32_t g_adc_sequence;
extern volatile uint32_t g_adc_error_frames;
extern volatile uint32_t g_adc_status;
extern volatile uint32_t g_adc_valid;
extern volatile uint32_t g_adc_raw;
extern volatile uint32_t g_adc_filtered;
extern volatile uint32_t g_adc_mv;
extern volatile uint32_t g_adc_no_new_data;
extern volatile UBaseType_t g_adc_stack_free;
extern volatile UBaseType_t g_monitor_stack_free;
extern volatile size_t g_heap_free;
/* Percent is valid only when status == LIGHT_OK and data are fresh. */
extern volatile uint32_t g_light_percent;
extern volatile uint32_t g_light_state;
extern volatile uint32_t g_light_status;

#endif

