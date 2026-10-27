#include "app_key.h"
#include "key_config.h"
#include "bsp_key.h"
#include "queue.h"

static QueueHandle_t s_queue = NULL;
static TaskHandle_t s_task = NULL;
volatile uint32_t g_key_events = 0U;
volatile uint32_t g_key_dropped = 0U;
volatile uint32_t g_key_scan_gaps = 0U;
volatile uint32_t g_key_last_action = KEY_NONE;
volatile UBaseType_t g_key_stack_free = 0U;

static void KeyTask(void *parameter)
{
    KeyContext ctx;
    KeyEvent event;
    TickType_t now;
    uint8_t pressed;
    (void)parameter;
    BSP_Key_Init();
    pressed = BSP_Key_IsPressed();
    now = xTaskGetTickCount();
    KeyLogic_Init(&ctx, pressed, (uint32_t)now);
    for (;;)
    {
        /* Relative delay avoids back-to-back catch-up reads of one GPIO level. */
        vTaskDelay(pdMS_TO_TICKS(KEY_SCAN_MS));
        pressed = BSP_Key_IsPressed();
        now = xTaskGetTickCount();
        if ((uint32_t)((uint32_t)now - ctx.last_sample) > KEY_MAX_SCAN_GAP_MS)
            ++g_key_scan_gaps;
        event.action = KeyLogic_Update(&ctx, pressed, (uint32_t)now);
        if (event.action != KEY_NONE)
        {
            event.tick = now;
            event.sequence = ++g_key_events;
            g_key_last_action = (uint32_t)event.action;
            /* FIFO event history. Never overwrite; never block the scanner. */
            if (xQueueSend(s_queue, &event, 0) != pdPASS) ++g_key_dropped;
        }
        g_key_stack_free = uxTaskGetStackHighWaterMark(NULL);
    }
}

int App_Key_Create(void)
{
    if ((s_queue != NULL) || (s_task != NULL)) return 3;
    if ((configTICK_RATE_HZ != 1000) || (configUSE_16_BIT_TICKS != 0) ||
        (configMAX_PRIORITIES < 4)) return 4;
    s_queue = xQueueCreate(KEY_QUEUE_LENGTH, sizeof(KeyEvent));
    if (s_queue == NULL) return 1;
    if (xTaskCreate(KeyTask, "Key", 192, NULL, 2, &s_task) != pdPASS)
    {
        vQueueDelete(s_queue);
        s_queue = NULL;
        s_task = NULL;
        return 2;
    }
    return 0;
}

BaseType_t App_Key_IsReady(void)
{
    return ((s_queue != NULL) && (s_task != NULL)) ? pdTRUE : pdFALSE;
}

BaseType_t App_Key_Receive(KeyEvent *out, TickType_t wait_ticks)
{
    if ((out == NULL) || (s_queue == NULL)) return pdFAIL;
    return xQueueReceive(s_queue, out, wait_ticks);
}
