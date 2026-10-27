#include "app_display.h"
#include "display_view.h"
#include "oled_text.h"
#include "ssd1306.h"
#include "app_key.h"
#include "key_config.h"
#include "ui_navigation.h"

static TaskHandle_t s_task = NULL;
/* Only DisplayTask accesses these static buffers; do not use in interrupts. */
static DisplayRows s_rows;
static uint8_t s_page[OLED_WIDTH];
static DisplayPage s_current = DISPLAY_OVERVIEW;
volatile uint32_t g_display_page = DISPLAY_OVERVIEW;
volatile uint32_t g_display_status = OLED_NOT_READY;
volatile uint32_t g_display_frames = 0U;
volatile uint32_t g_display_errors = 0U;
volatile UBaseType_t g_display_stack_free = 0U;

static void ApplyEvent(const KeyEvent *event)
{
    s_current = UI_Navigate(s_current, event->action);
    g_display_page = (uint32_t)s_current;
}

static void DrainEvents(void)
{
    KeyEvent event;
    unsigned int i;
    /* Bounded drain: a future faster event source cannot monopolize display. */
    for (i = 0U; i < KEY_QUEUE_LENGTH; ++i)
    {
        if (App_Key_Receive(&event, 0) != pdPASS) break;
        ApplyEvent(&event);
    }
}

static void WaitForRefresh(uint32_t pause_ms, uint8_t wake_on_key)
{
    KeyEvent event;
    TickType_t start = xTaskGetTickCount();
    TickType_t budget = pdMS_TO_TICKS(pause_ms);
    TickType_t elapsed;
    for (;;)
    {
        elapsed = (TickType_t)(xTaskGetTickCount() - start);
        if (elapsed >= budget) break;
        if (App_Key_Receive(&event, (TickType_t)(budget - elapsed)) != pdPASS) break;
        ApplyEvent(&event);
        /* Normal: key wakes display early. Fault: preserve the retry delay
         * while consuming events, so button activity cannot hammer the bus.
         */
        if (wake_on_key != 0U) break;
    }
}

static void DisplayTask(void *parameter)
{
    AdcSample_t adc;
    DHT11_Sample dht;
    const AdcSample_t *adc_ptr;
    const DHT11_Sample *dht_ptr;
    OLED_Status status;
    uint8_t ready = 0U;
    uint8_t page;
    (void)parameter;
    vTaskDelay(pdMS_TO_TICKS(OLED_POWER_WAIT_MS));
    for (;;)
    {
        DrainEvents();
        status = OLED_OK;
        if (ready == 0U) status = SSD1306_Init();
        if (status == OLED_OK)
        {
            adc_ptr = (App_ADC_GetLatest(&adc) == pdPASS) ? &adc : NULL;
            dht_ptr = (App_DHT11_GetLatest(&dht) == pdPASS) ? &dht : NULL;
            DisplayView_BuildPage(s_current, adc_ptr, dht_ptr, xTaskGetTickCount(), s_rows);
            for (page = 0U; page < OLED_PAGES; ++page)
            {
                OLED_TextPage(s_page, s_rows[page]);
                status = SSD1306_WritePage(page, s_page);
                if (status != OLED_OK) break;
                /* Both bus transactions ended with STOP before yielding. */
                vTaskDelay(1);
            }
            if ((status == OLED_OK) && (ready == 0U))
                status = SSD1306_DisplayOn();
        }
        g_display_status = (uint32_t)status;
        if (status == OLED_OK)
        {
            ready = 1U;
            ++g_display_frames;
        }
        else
        {
            ready = 0U;
            ++g_display_errors;
        }
        g_display_stack_free = uxTaskGetStackHighWaterMark(NULL);
        WaitForRefresh((ready != 0U) ? OLED_REFRESH_PAUSE_MS : OLED_RETRY_MS, ready);
    }
}

int App_Display_Create(void)
{
    if (s_task != NULL) return 2;
    if ((configTICK_RATE_HZ != 1000) || (configMAX_PRIORITIES < 4)) return 3;
    if (App_Key_IsReady() != pdTRUE) return 4;
    if (xTaskCreate(DisplayTask, "Display", 384, NULL, 1, &s_task) != pdPASS)
    {
        s_task = NULL;
        return 1;
    }
    return 0;
}

