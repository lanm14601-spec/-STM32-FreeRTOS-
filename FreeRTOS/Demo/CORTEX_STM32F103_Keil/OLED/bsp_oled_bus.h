#ifndef BSP_OLED_BUS_H
#define BSP_OLED_BUS_H
#include <stdint.h>
typedef enum {
    OLED_OK = 0, OLED_NOT_READY, OLED_CLOCK_ERROR,
    OLED_SCL_TIMEOUT, OLED_BUS_LOW, OLED_NACK, OLED_ARGUMENT
} OLED_Status;
/* Single owner: DisplayTask. No ISR calls, no other I2C bus users. */
OLED_Status BSP_OLED_BusInit(void);
OLED_Status BSP_OLED_Write(uint8_t control, const uint8_t *data, uint16_t count);
const char *OLED_StatusName(OLED_Status status);
#endif
