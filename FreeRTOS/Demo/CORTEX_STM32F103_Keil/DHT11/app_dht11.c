#include <string.h>
#include "app_dht11.h"
#include "bsp_dht11.h"
#include "dht11_config.h"
#include "queue.h"

#if (INCLUDE_vTaskDelay != 1) || (INCLUDE_uxTaskGetStackHighWaterMark != 1)
#error Enable vTaskDelay and uxTaskGetStackHighWaterMark
#endif
#if configMAX_PRIORITIES < 4
#error DHT task uses priority 3; configMAX_PRIORITIES must be >= 4
#endif

static QueueHandle_t s_dht_queue = NULL;
volatile uint32_t g_dht_sequence = 0U;
volatile uint32_t g_dht_failures = 0U;
volatile uint32_t g_dht_status = DHT11_WARMUP;
volatile uint32_t g_dht_edges = 0U;
volatile UBaseType_t g_dht_stack_free = 0U;

static DHT11_Status ReadOnce(DHT11_Reading *out)
{
    DHT11_Status result;
    TickType_t start;
    start = xTaskGetTickCount();
    result = BSP_DHT11_StartLow();
    if (result != DHT11_OK) { return result; }
    vTaskDelay(pdMS_TO_TICKS(DHT11_START_LOW_MS));
    if ((TickType_t)(xTaskGetTickCount() - start) > pdMS_TO_TICKS(28U))
    {
        (void)BSP_DHT11_Abort();
        return DHT11_START_LATE;
    }
    result = BSP_DHT11_Release();
    if (result != DHT11_OK)
    {
        (void)BSP_DHT11_Abort();
        return result;
    }
    start = xTaskGetTickCount();
    for (;;)
    {
        if ((TickType_t)(xTaskGetTickCount() - start) >=
            pdMS_TO_TICKS(DHT11_CAPTURE_TIMEOUT_MS))
        { return BSP_DHT11_Abort(); }
        result = BSP_DHT11_Poll(out);
        if (result != DHT11_BUSY) { return result; }
        /* EXTI captures microsecond edges while this task is blocked. */
        vTaskDelay(1);
    }
}

static void DhtTask(void *parameter)
{
    DHT11_Status result;
    DHT11_Sample sample;
    uint8_t initialized = 0U;
    uint8_t primed = 0U;
    uint32_t sequence = 0U;
    uint32_t failures = 0U;
    (void)parameter;
    /* Also allows OD release and the sensor's power-up stabilization. */
    result = BSP_DHT11_Init();
    if (result == DHT11_OK) { initialized = 1U; }
    vTaskDelay(pdMS_TO_TICKS(DHT11_STARTUP_MS));
    for (;;)
    {
        memset(&sample, 0, sizeof(sample));
        if (initialized == 0U)
        {
            result = BSP_DHT11_Init();
            if (result == DHT11_OK) { initialized = 1U; }
        }
        if (initialized != 0U) { result = ReadOnce(&sample.reading); }
        if (result == DHT11_OK)
        {
            if (primed == 0U)
            {
                primed = 1U;
                result = DHT11_PRIMING; /* Sensor may return its previous measurement. */
            }
            else { sample.valid = 1U; }
        }
        else
        {
            ++failures;
            primed = 0U;
        }
        sample.sequence = ++sequence;
        sample.tick = xTaskGetTickCount();
        sample.failures = failures;
        sample.status = (uint8_t)result;
        sample.edges = (initialized != 0U) ? BSP_DHT11_EdgeCount() : 0U;
        g_dht_sequence = sequence;
        g_dht_failures = failures;
        g_dht_status = sample.status;
        g_dht_edges = sample.edges;
        (void)xQueueOverwrite(s_dht_queue, &sample);
        g_dht_stack_free = uxTaskGetStackHighWaterMark(NULL);
        /* Relative wait avoids rapid catch-up reads after a scheduling overrun. */
        vTaskDelay(pdMS_TO_TICKS(DHT11_INTERVAL_MS));
    }
}

int App_DHT11_Create(void)
{
    DHT11_Sample sample;
    if (s_dht_queue != NULL) { return 3; }
    if ((configTICK_RATE_HZ != 1000U) ||
        ((DHT11_IRQ_PRIORITY << 4) >= configMAX_SYSCALL_INTERRUPT_PRIORITY) ||
        (DHT11_IRQ_PRIORITY > 15U)) { return 4; }
    s_dht_queue = xQueueCreate(1, sizeof(DHT11_Sample));
    if (s_dht_queue == NULL) { return 1; }
    memset(&sample, 0, sizeof(sample));
    sample.status = (uint8_t)DHT11_WARMUP;
    sample.tick = xTaskGetTickCount();
    (void)xQueueOverwrite(s_dht_queue, &sample);
    if (xTaskCreate(DhtTask, "DHT11", 384, NULL, 3, NULL) != pdPASS)
    {
        vQueueDelete(s_dht_queue);
        s_dht_queue = NULL;
        return 2;
    }
    return 0;
}

BaseType_t App_DHT11_GetLatest(DHT11_Sample *out)
{
    if ((out == NULL) || (s_dht_queue == NULL)) { return pdFAIL; }
    if (xQueuePeek(s_dht_queue, out, 0) != pdPASS) { return pdFAIL; }
    if ((TickType_t)(xTaskGetTickCount() - out->tick) >= pdMS_TO_TICKS(DHT11_STALE_MS))
    {
        out->valid = 0U;
        out->status = (uint8_t)DHT11_STALE;
    }
    return pdPASS;
}
