#include "app_display.h"
#include "display_view.h"
#include "oled_text.h"
#include "ssd1306.h"

static TaskHandle_t s_task = NULL;
/* Only DisplayTask accesses these static buffers; do not use in interrupts. */
static DisplayRows s_rows;
static uint8_t s_page[OLED_WIDTH];
volatile uint32_t g_display_status = OLED_NOT_READY;
volatile uint32_t g_display_frames = 0U;
volatile uint32_t g_display_errors = 0U;
volatile UBaseType_t g_display_stack_free = 0U;

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
        status = OLED_OK;
        if (ready == 0U) status = SSD1306_Init();
        if (status == OLED_OK)
        {
            adc_ptr = (App_ADC_GetLatest(&adc) == pdPASS) ? &adc : NULL;
            dht_ptr = (App_DHT11_GetLatest(&dht) == pdPASS) ? &dht : NULL;
            DisplayView_Build(adc_ptr, dht_ptr, xTaskGetTickCount(), s_rows);
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
        /* Relative pause: no backlog/catch-up transfers after a slow frame. */
        vTaskDelay(pdMS_TO_TICKS((ready != 0U) ? OLED_REFRESH_PAUSE_MS : OLED_RETRY_MS));
    }
}

int App_Display_Create(void)
{
    if (s_task != NULL) return 2;
    if ((configTICK_RATE_HZ != 1000) || (configMAX_PRIORITIES < 4)) return 3;
    if (xTaskCreate(DisplayTask, "Display", 384, NULL, 1, &s_task) != pdPASS)
    {
        s_task = NULL;
        return 1;
    }
    return 0;
}
