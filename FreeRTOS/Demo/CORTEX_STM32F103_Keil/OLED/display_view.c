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
    (void)snprintf(rows[0], sizeof(rows[0]), "ENV MONITOR V0.5");
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
    if (dht != 0)
        (void)snprintf(rows[6], sizeof(rows[6]), "DSEQ:%lu", (unsigned long)dht->sequence);
    else
        (void)snprintf(rows[6], sizeof(rows[6]), "DSEQ:--");
    (void)snprintf(rows[7], sizeof(rows[7]), "UP:%lu S",
                   (unsigned long)(now / configTICK_RATE_HZ));
}
