#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_oled_bus.h"
#include "ui_navigation.h"
/* Call once BEFORE scheduler, AFTER DHT11, ADC and Key creation.
 * 0=success, 1=task allocation failure, 2=already created, 3=configuration, 4=Key task/queue not created.
 */
int App_Display_Create(void);
/* Task context. One-slot mailbox: pdFAIL if busy/invalid/not created.
 * pdPASS = accepted, NOT proof of a successful OLED transfer.
 */
BaseType_t App_Display_RequestPage(DisplayPage page);
extern volatile uint32_t g_display_requests;
extern volatile uint32_t g_display_applied;
/* 0=overview, 1=light, 2=DHT. Diagnostic only. */
extern volatile uint32_t g_display_page;
extern volatile uint32_t g_display_status;
extern volatile uint32_t g_display_frames;
extern volatile uint32_t g_display_errors;
extern volatile UBaseType_t g_display_stack_free;
#endif


