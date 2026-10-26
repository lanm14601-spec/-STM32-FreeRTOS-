#include <stdio.h>
#include <string.h>
#include "app_adc.h"
#include "app_dht11.h"
#include "app_display.h"
#include "queue.h"
#include "bsp_adc.h"
#include "adc_filter.h"
#include "light_config.h"
#include "light_process.h"

#if (INCLUDE_vTaskDelayUntil != 1) || (INCLUDE_vTaskDelete != 1)
#error Enable INCLUDE_vTaskDelayUntil and INCLUDE_vTaskDelete
#endif
#if (INCLUDE_uxTaskGetStackHighWaterMark != 1)
#error Enable INCLUDE_uxTaskGetStackHighWaterMark
#endif
#if (configMAX_PRIORITIES < 3)
#error At least three FreeRTOS task priority levels are required
#endif

#define ADC_PERIOD_MS 100U
#define MONITOR_PERIOD_MS 500U
#define ADC_RETRY_CYCLES 10U

static const LightConfig s_light_config = {
    LIGHT_DARK_ADC, LIGHT_BRIGHT_ADC, LIGHT_MIN_SPAN,
    LIGHT_CALIBRATED, LIGHT_DARK_ON_PCT, LIGHT_DARK_OFF_PCT,
    LIGHT_CONFIRM_SAMPLES
};

volatile uint32_t g_light_percent = 0U;
volatile uint32_t g_light_state = LIGHT_UNKNOWN;
volatile uint32_t g_light_status = LIGHT_UNCALIBRATED;

static QueueHandle_t s_queue = NULL;
static TaskHandle_t s_adc_task = NULL;

volatile uint32_t g_adc_sequence = 0U;
volatile uint32_t g_adc_error_frames = 0U;
volatile uint32_t g_adc_status = BSP_ADC_ERR_NOT_READY;
volatile uint32_t g_adc_valid = 0U;
volatile uint32_t g_adc_raw = 0U;
volatile uint32_t g_adc_filtered = 0U;
volatile uint32_t g_adc_mv = 0U;
volatile uint32_t g_adc_no_new_data = 0U;
volatile UBaseType_t g_adc_stack_free = 0U;
volatile UBaseType_t g_monitor_stack_free = 0U;
volatile size_t g_heap_free = 0U;

static void AdcTask(void *parameter)
{
    TickType_t last_wake;
    ADC_Filter filter;
    LightContext light_context;
    LightResult light;
    AdcSample_t sample;
    BSP_ADC_Status status = BSP_ADC_ERR_NOT_READY;
    uint32_t sequence = 0U;
    uint8_t ready = 0U;
    uint8_t retry = 0U;

    (void)parameter;
    ADC_FilterReset(&filter);
    Light_Reset(&light_context);
    last_wake = xTaskGetTickCount();

    for (;;)
    {
        memset(&sample, 0, sizeof(sample));

        if (ready == 0U)
        {
            if (retry > 0U)
            {
                --retry;
            }
            if (retry == 0U)
            {
                status = BSP_ADC_Init();
                if (status == BSP_ADC_OK)
                {
                    ready = 1U;
                }
                else
                {
                    retry = ADC_RETRY_CYCLES;
                }
            }
        }

        if (ready != 0U)
        {
            status = BSP_ADC_ReadRaw(&sample.raw);
            if (status == BSP_ADC_OK)
            {
                sample.filtered = ADC_FilterPush(&filter, sample.raw);
                sample.voltage_mv = ADC_ToMillivolts(sample.filtered);
                sample.valid = 1U;
                sample.sample_count = filter.count;
            }
            else
            {
                ready = 0U;
                retry = ADC_RETRY_CYCLES;
            }
        }

        if (sample.valid == 0U)
        {
            /* Do not mix samples from before and after a conversion fault. */
            ADC_FilterReset(&filter);
            ++g_adc_error_frames;
        }

        /* Run on EVERY sample, before the latest-value queue can overwrite it. */
        light = Light_Update(&light_context, &s_light_config,
                             sample.filtered, sample.valid,
                             (uint8_t)(sample.sample_count >= ADC_FILTER_WINDOW));
        sample.light_percent = light.percent;
        sample.light_state = (uint8_t)light.state;
        sample.light_status = (uint8_t)light.status;
        g_light_percent = sample.light_percent;
        g_light_state = sample.light_state;
        g_light_status = sample.light_status;

        sample.sequence = ++sequence;
        sample.tick = xTaskGetTickCount();
        sample.status = (uint8_t)status;
        g_adc_sequence = sequence;
        g_adc_status = sample.status;
        g_adc_valid = sample.valid;
        g_adc_raw = sample.raw;
        g_adc_filtered = sample.filtered;
        g_adc_mv = sample.voltage_mv;

        /* Length MUST remain 1 when using xQueueOverwrite(). */
        (void)xQueueOverwrite(s_queue, &sample);
        g_adc_stack_free = uxTaskGetStackHighWaterMark(NULL);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(ADC_PERIOD_MS));
    }
}

static void PrintDhtLatest(void)
{
    DHT11_Sample sample;
    unsigned long age_ms;
    if (App_DHT11_GetLatest(&sample) != pdPASS)
    {
        printf("DHT11_NOT_CREATED\r\n");
        return;
    }
    age_ms = (unsigned long)((TickType_t)(xTaskGetTickCount() - sample.tick)) *
             (unsigned long)portTICK_PERIOD_MS;
    if (sample.valid != 0U)
    {
        printf("dhtseq=%lu T=%d.%uC RH=%u.%u%% age=%lums errors=%lu\r\n",
               (unsigned long)sample.sequence,
               (int)(sample.reading.temperature_x10 / 10),
               (unsigned int)(sample.reading.temperature_x10 % 10),
               (unsigned int)(sample.reading.humidity_x10 / 10U),
               (unsigned int)(sample.reading.humidity_x10 % 10U),
               age_ms, (unsigned long)sample.failures);
    }
    else
    {
        printf("dhtseq=%lu T=NA RH=NA status=%s edges=%u age=%lums errors=%lu\r\n",
               (unsigned long)sample.sequence,
               DHT11_StatusName((DHT11_Status)sample.status),
               (unsigned int)sample.edges, age_ms,
               (unsigned long)sample.failures);
    }
}

static void MonitorTask(void *parameter)
{
    TickType_t last_wake;
    AdcSample_t sample;

    (void)parameter;
    last_wake = xTaskGetTickCount();

    for (;;)
    {
        if (App_ADC_GetLatest(&sample) == pdPASS)
        {
            if ((TickType_t)(xTaskGetTickCount() - sample.tick) >=
                pdMS_TO_TICKS(1000U))
            {
                printf("LIGHT_STALE state=UNKNOWN\r\n");
            }
            else if (sample.light_status == (uint8_t)LIGHT_OK)
            {
                printf("seq=%lu raw=%u avg=%u mv=%lu light=%u%% state=%s\r\n",
                       (unsigned long)sample.sequence,
                       (unsigned int)sample.raw,
                       (unsigned int)sample.filtered,
                       (unsigned long)sample.voltage_mv,
                       (unsigned int)sample.light_percent,
                       Light_StateName((LightState)sample.light_state));
            }
            else
            {
                /* Never print an invalid/uncalibrated percentage as real data. */
                printf("seq=%lu raw=%u avg=%u n=%u light=NA status=%s adc=%u state=UNKNOWN\r\n",
                       (unsigned long)sample.sequence,
                       (unsigned int)sample.raw,
                       (unsigned int)sample.filtered,
                       (unsigned int)sample.sample_count,
                       Light_StatusName((LightStatus)sample.light_status),
                       (unsigned int)sample.status);
            }
        }
        else
        {
            ++g_adc_no_new_data;
            printf("ADC_NO_NEW_DATA state=UNKNOWN\r\n");
        }

        PrintDhtLatest();
        printf("oled=%s frames=%lu errors=%lu\r\n",
               OLED_StatusName((OLED_Status)g_display_status),
               (unsigned long)g_display_frames,
               (unsigned long)g_display_errors);
        g_monitor_stack_free = uxTaskGetStackHighWaterMark(NULL);
        g_heap_free = xPortGetFreeHeapSize();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(MONITOR_PERIOD_MS));
    }
}

int App_ADC_Create(void)
{
    BaseType_t result;

    if (s_queue != NULL)
    {
        return 4;
    }
    s_queue = xQueueCreate(1, sizeof(AdcSample_t));
    if (s_queue == NULL)
    {
        return 1;
    }

    result = xTaskCreate(AdcTask, "ADC", 256, NULL, 2, &s_adc_task);
    if (result != pdPASS)
    {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return 2;
    }

    result = xTaskCreate(MonitorTask, "Monitor", 384, NULL, 1, NULL);
    if (result != pdPASS)
    {
        /* Scheduler has NOT been started: task is not running. */
        vTaskDelete(s_adc_task);
        s_adc_task = NULL;
        vQueueDelete(s_queue);
        s_queue = NULL;
        return 3;
    }
    return 0;
}


BaseType_t App_ADC_GetLatest(AdcSample_t *out)
{
    if ((out == NULL) || (s_queue == NULL)) return pdFAIL;
    /* Multi-reader snapshot: never remove the sample from this queue.
     * Consumers check tick/valid/light_status before using numeric values.
     */
    return xQueuePeek(s_queue, out, 0);
}
