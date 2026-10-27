#include <stdio.h>
#include <string.h>
#include "display_view.h"
#include "light_process.h"
#include "dht11_config.h"

void DisplayView_Build(const AdcSample_t *adc, const DHT11_Sample *dht,
                       TickType_t now, DisplayRows rows)
{
    const char *dht_state = "NOT_CREATED";
    const char *light_state = "NO_DATA";
    uint8_t dht_valid = 0U;
    uint8_t light_valid = 0U;
    int temperature;
    unsigned int magnitude;
    if (rows == 0) return;
    memset(rows, 0, sizeof(DisplayRows));
    if (dht != 0)
    {
        dht_state = DHT11_StatusName((DHT11_Status)dht->status);
        if ((TickType_t)(now - dht->tick) >= pdMS_TO_TICKS(DHT11_STALE_MS))
            dht_state = "STALE";
        else if ((dht->valid != 0U) && (dht->status == DHT11_OK))
            dht_valid = 1U;
    }
    if (adc != 0)
    {
        light_state = Light_StatusName((LightStatus)adc->light_status);
        if ((TickType_t)(now - adc->tick) >= pdMS_TO_TICKS(1000U))
            light_state = "STALE";
        else if ((adc->valid != 0U) && (adc->light_status == LIGHT_OK))
        {
            light_valid = 1U;
            light_state = Light_StateName((LightState)adc->light_state);
        }
        else if (adc->valid == 0U) light_state = "ADC_ERROR";
    }
    (void)snprintf(rows[0], sizeof(rows[0]), "ENV MONITOR V0.6");
    if (dht_valid != 0U)
    {
        temperature = (int)dht->reading.temperature_x10;
        magnitude = (unsigned int)((temperature < 0) ? -temperature : temperature);
        (void)snprintf(rows[1], sizeof(rows[1]), "TEMP:%s%u.%u C",
                       (temperature < 0) ? "-" : "", magnitude / 10U, magnitude % 10U);
        (void)snprintf(rows[2], sizeof(rows[2]), "RH:%u.%u %%",
                       (unsigned int)(dht->reading.humidity_x10 / 10U),
                       (unsigned int)(dht->reading.humidity_x10 % 10U));
    }
    else
    {
        (void)snprintf(rows[1], sizeof(rows[1]), "TEMP:--.- C");
        (void)snprintf(rows[2], sizeof(rows[2]), "RH:--.- %%");
    }
    if (light_valid != 0U)
        (void)snprintf(rows[3], sizeof(rows[3]), "LIGHT:%u %%", (unsigned int)adc->light_percent);
    else
        (void)snprintf(rows[3], sizeof(rows[3]), "LIGHT:-- %%");
    (void)snprintf(rows[4], sizeof(rows[4]), "L:%.19s", light_state);
    (void)snprintf(rows[5], sizeof(rows[5]), "D:%.19s", dht_state);
    (void)snprintf(rows[6], sizeof(rows[6]), "UP:%lu S",
                   (unsigned long)(now / configTICK_RATE_HZ));
    (void)snprintf(rows[7], sizeof(rows[7]), "1-3 SHORT:NEXT");
}

void DisplayView_BuildPage(DisplayPage page, const AdcSample_t *adc,
                           const DHT11_Sample *dht, TickType_t now, DisplayRows rows)
{
    uint8_t adc_valid;
    if (rows == 0) return;
    DisplayView_Build(adc, dht, now, rows);
    if (page == DISPLAY_LIGHT)
    {
        /* Preserve formatted percentage and status before replacing their rows. */
        memcpy(rows[5], rows[4], sizeof(rows[5]));
        memcpy(rows[4], rows[3], sizeof(rows[4]));
        (void)snprintf(rows[0], sizeof(rows[0]), "LIGHT DETAILS");
        adc_valid = (uint8_t)((adc != 0) && (adc->valid != 0U) &&
                     ((TickType_t)(now - adc->tick) < pdMS_TO_TICKS(1000U)));
        if (adc_valid != 0U)
        {
            /* Raw ADC remains useful for calibration even if LIGHT is uncalibrated. */
            (void)snprintf(rows[1], sizeof(rows[1]), "RAW:%u", (unsigned int)adc->raw);
            (void)snprintf(rows[2], sizeof(rows[2]), "AVG:%u", (unsigned int)adc->filtered);
            (void)snprintf(rows[3], sizeof(rows[3]), "MV:%lu", (unsigned long)adc->voltage_mv);
        }
        else
        {
            (void)snprintf(rows[1], sizeof(rows[1]), "RAW:--");
            (void)snprintf(rows[2], sizeof(rows[2]), "AVG:--");
            (void)snprintf(rows[3], sizeof(rows[3]), "MV:--");
        }
        if (adc != 0)
            (void)snprintf(rows[6], sizeof(rows[6]), "AGE:%lu MS",
                           (unsigned long)((TickType_t)(now - adc->tick)));
        else
            (void)snprintf(rows[6], sizeof(rows[6]), "AGE:--");
        (void)snprintf(rows[7], sizeof(rows[7]), "2-3 SHORT:NEXT");
    }
    else if (page == DISPLAY_DHT)
    {
        memcpy(rows[3], rows[5], sizeof(rows[3]));
        (void)snprintf(rows[0], sizeof(rows[0]), "DHT11 DETAILS");
        if (dht != 0)
        {
            (void)snprintf(rows[4], sizeof(rows[4]), "SEQ:%lu", (unsigned long)dht->sequence);
            (void)snprintf(rows[5], sizeof(rows[5]), "ERR:%lu", (unsigned long)dht->failures);
            (void)snprintf(rows[6], sizeof(rows[6]), "AGE:%lu MS",
                           (unsigned long)((TickType_t)(now - dht->tick)));
        }
        else
        {
            (void)snprintf(rows[4], sizeof(rows[4]), "SEQ:--");
            (void)snprintf(rows[5], sizeof(rows[5]), "ERR:--");
            (void)snprintf(rows[6], sizeof(rows[6]), "AGE:--");
        }
        (void)snprintf(rows[7], sizeof(rows[7]), "3-3 HOLD:HOME");
    }
}
