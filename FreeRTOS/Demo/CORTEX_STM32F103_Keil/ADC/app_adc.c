#include <stdio.h>
#include <string.h>
#include "app_adc.h"
#include "queue.h"
#include "bsp_adc.h"
#include "adc_filter.h"

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
    AdcSample_t sample;
    BSP_ADC_Status status = BSP_ADC_ERR_NOT_READY;
    uint32_t sequence = 0U;
    uint8_t ready = 0U;
    uint8_t retry = 0U;

    (void)parameter;
    ADC_FilterReset(&filter);
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

static void MonitorTask(void *parameter)
{
    TickType_t last_wake;
    AdcSample_t sample;

    (void)parameter;
    last_wake = xTaskGetTickCount();

    for (;;)
    {
        if (xQueueReceive(s_queue, &sample, 0) == pdPASS)
        {
            if (sample.valid != 0U)
            {
                printf("seq=%lu tick=%lu raw=%u avg=%u mv=%lu n=%u\r\n",
                       (unsigned long)sample.sequence,
                       (unsigned long)sample.tick,
                       (unsigned int)sample.raw,
                       (unsigned int)sample.filtered,
                       (unsigned long)sample.voltage_mv,
                       (unsigned int)sample.sample_count);
            }
            else
            {
                printf("seq=%lu tick=%lu ADC_ERROR=%u\r\n",
                       (unsigned long)sample.sequence,
                       (unsigned long)sample.tick,
                       (unsigned int)sample.status);
            }
        }
        else
        {
            ++g_adc_no_new_data;
            printf("ADC_NO_NEW_DATA\r\n");
        }

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
