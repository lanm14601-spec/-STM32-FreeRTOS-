#ifndef DISPLAY_VIEW_H
#define DISPLAY_VIEW_H
#include "app_adc.h"
#include "app_dht11.h"
#include "oled_config.h"
#include "ui_navigation.h"
typedef char DisplayRows[OLED_PAGES][OLED_TEXT_COLS + 1U];
/* Pure formatting. NULL sample means unavailable. No hardware/RTOS calls.
 * now must be read AFTER both snapshots have been copied.
 */
void DisplayView_Build(const AdcSample_t *adc, const DHT11_Sample *dht,
                       TickType_t now, DisplayRows rows);
void DisplayView_BuildPage(DisplayPage page, const AdcSample_t *adc,
                           const DHT11_Sample *dht, TickType_t now, DisplayRows rows);
#endif

