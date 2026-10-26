#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_oled_bus.h"
/* Call once BEFORE scheduler, AFTER App_DHT11_Create and App_ADC_Create.
 * 0=success, 1=task allocation failure, 2=already created, 3=configuration.
 */
int App_Display_Create(void);
extern volatile uint32_t g_display_status;
extern volatile uint32_t g_display_frames;
extern volatile uint32_t g_display_errors;
extern volatile UBaseType_t g_display_stack_free;
#endif
