#include <stdio.h>
#include <string.h>
#include "app_adc.h"
#include "app_dht11.h"
#include "app_display.h"
#include "app_key.h"
#include "app_console.h"
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

static uint8_t s_log_enabled = 1U;
static uint32_t s_log_period_ms = CONSOLE_DEFAULT_PERIOD_MS;

static void PrintStatus(void)
{
    AdcSample_t sample;
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
    printf("page=%u key=%lu dropped=%lu gaps=%lu\r\n",
           (unsigned int)(g_display_page + 1U),
           (unsigned long)g_key_events,
           (unsigned long)g_key_dropped,
           (unsigned long)g_key_scan_gaps);
    printf("console=V0.9 ready=%lu rx=%lu rxdrop=%lu hwerr=%lu cmd=%lu cmddrop=%lu\r\n",
           (unsigned long)g_console_ready, (unsigned long)g_console_rx_bytes,
           (unsigned long)g_console_rx_dropped, (unsigned long)g_console_uart_errors,
           (unsigned long)g_console_commands, (unsigned long)g_console_command_dropped);
    printf("log=%u period=%lums req=%lu applied=%lu up=%lus\r\n",
           (unsigned int)s_log_enabled, (unsigned long)s_log_period_ms,
           (unsigned long)g_display_requests, (unsigned long)g_display_applied,
           (unsigned long)(xTaskGetTickCount() / configTICK_RATE_HZ));
    printf("heap=%lu stack:adc=%lu dht=%lu key=%lu oled=%lu console=%lu monitor=%lu\r\n",
           (unsigned long)xPortGetFreeHeapSize(),
           (unsigned long)g_adc_stack_free, (unsigned long)g_dht_stack_free,
           (unsigned long)g_key_stack_free, (unsigned long)g_display_stack_free,
           (unsigned long)g_console_stack_free, (unsigned long)g_monitor_stack_free);
}

static void HandleCommand(const ConsoleCommand *cmd)
{
    switch (cmd->kind)
    {
    case CMD_HELP:
        printf("BEGIN HELP %lu\r\n", (unsigned long)cmd->id);
        printf("HELP | STATUS | PAGE 1..3 | HOME\r\n");
        printf("LOG ON/OFF | PERIOD 200..10000 (ms)\r\n");
        printf("ASCII, end with CR/LF. Settings reset at reboot.\r\n");
        printf("END HELP %lu\r\n", (unsigned long)cmd->id);
        break;
    case CMD_STATUS:
        printf("BEGIN STATUS %lu\r\n", (unsigned long)cmd->id);
        PrintStatus();
        printf("END STATUS %lu\r\n", (unsigned long)cmd->id);
        break;
    case CMD_PAGE:
        if (App_Display_RequestPage((DisplayPage)(cmd->value - 1U)) == pdPASS)
            printf("OK %lu PAGE %lu QUEUED\r\n", (unsigned long)cmd->id,
                   (unsigned long)cmd->value);
        else
            printf("ERR %lu DISPLAY_BUSY\r\n", (unsigned long)cmd->id);
        break;
    case CMD_LOG:
        s_log_enabled = (uint8_t)cmd->value;
        printf("OK %lu LOG %s\r\n", (unsigned long)cmd->id,
               s_log_enabled ? "ON" : "OFF");
        break;
    case CMD_PERIOD:
        s_log_period_ms = cmd->value;
        printf("OK %lu PERIOD %lu\r\n", (unsigned long)cmd->id,
               (unsigned long)s_log_period_ms);
        break;
    case CMD_ERROR:
        printf("ERR %lu %s\r\n", (unsigned long)cmd->id,
               Console_ErrorName(cmd->error));
        break;
    default:
        printf("ERR %lu INTERNAL\r\n", (unsigned long)cmd->id);
        break;
    }
}

static void MonitorTask(void *parameter)
{
    ConsoleCommand command;
    TickType_t last_report;
    TickType_t elapsed;
    TickType_t period;
    TickType_t wait;
    (void)parameter;
    printf("BOOT V0.9 UART 115200 8N1; send HELP with CR/LF\r\n");
    last_report = xTaskGetTickCount();
    for (;;)
    {
        period = pdMS_TO_TICKS(s_log_period_ms);
        elapsed = (TickType_t)(xTaskGetTickCount() - last_report);
        if ((s_log_enabled != 0U) && (elapsed >= period))
        {
            printf("BEGIN TELEMETRY\r\n");
            PrintStatus();
            printf("END TELEMETRY\r\n");
            /* No catch-up flood after a slow output or a command burst. */
            last_report = xTaskGetTickCount();
        }
        g_monitor_stack_free = uxTaskGetStackHighWaterMark(NULL);
        g_heap_free = xPortGetFreeHeapSize();
        elapsed = (TickType_t)(xTaskGetTickCount() - last_report);
        wait = (elapsed >= period) ? 0U : (TickType_t)(period - elapsed);
        if (s_log_enabled == 0U) wait = pdMS_TO_TICKS(500U);
        if (App_Console_Receive(&command, wait) == pdPASS)
        {
            HandleCommand(&command);
            if ((command.kind == CMD_LOG) || (command.kind == CMD_PERIOD))
                last_report = xTaskGetTickCount();
        }
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


