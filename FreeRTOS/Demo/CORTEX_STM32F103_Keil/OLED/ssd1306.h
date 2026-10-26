#ifndef SSD1306_H
#define SSD1306_H
#include "bsp_oled_bus.h"
OLED_Status SSD1306_Init(void); /* Leaves panel OFF until first complete frame. */
OLED_Status SSD1306_WritePage(uint8_t page, const uint8_t *pixels);
OLED_Status SSD1306_DisplayOn(void);
#endif
