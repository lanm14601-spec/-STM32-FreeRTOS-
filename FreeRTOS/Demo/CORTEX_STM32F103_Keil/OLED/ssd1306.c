#include "ssd1306.h"
#include "oled_config.h"

OLED_Status SSD1306_Init(void)
{
    OLED_Status status;
    static const uint8_t commands[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x02, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0x7F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0x2E
    };
    status = BSP_OLED_BusInit();
    if (status != OLED_OK) return status;
    return BSP_OLED_Write(0x00U, commands, (uint16_t)sizeof(commands));
}

OLED_Status SSD1306_WritePage(uint8_t page, const uint8_t *pixels)
{
    uint8_t commands[3];
    OLED_Status status;
    if ((page >= OLED_PAGES) || (pixels == 0)) return OLED_ARGUMENT;
    commands[0] = (uint8_t)(0xB0U | page);
    commands[1] = 0x00U;
    commands[2] = 0x10U;
    status = BSP_OLED_Write(0x00U, commands, 3U);
    if (status != OLED_OK) return status;
    return BSP_OLED_Write(0x40U, pixels, OLED_WIDTH);
}

OLED_Status SSD1306_DisplayOn(void)
{
    static const uint8_t command = 0xAFU;
    return BSP_OLED_Write(0x00U, &command, 1U);
}
