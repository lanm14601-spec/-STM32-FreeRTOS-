#ifndef OLED_CONFIG_H
#define OLED_CONFIG_H

/* Assumption: SSD1306 128x64, I2C, internal charge-pump module.
 * Address is SEVEN bits: use 0x3C or 0x3D, never 0x78 here.
 * Fixed wiring: PB6=SCL, PB7=SDA; TIM4 dedicated 1 MHz timebase.
 */
#define OLED_ADDRESS_7BIT 0x3CU
#define OLED_HALF_PERIOD_US 5U
#define OLED_SCL_TIMEOUT_US 100U
#define OLED_REFRESH_PAUSE_MS 250U
#define OLED_RETRY_MS 1000U
#define OLED_POWER_WAIT_MS 100U
#define OLED_WIDTH 128U
#define OLED_PAGES 8U
#define OLED_TEXT_COLS 21U
#if (OLED_ADDRESS_7BIT != 0x3C) && (OLED_ADDRESS_7BIT != 0x3D)
#error OLED_ADDRESS_7BIT must be 0x3C or 0x3D
#endif
#endif
