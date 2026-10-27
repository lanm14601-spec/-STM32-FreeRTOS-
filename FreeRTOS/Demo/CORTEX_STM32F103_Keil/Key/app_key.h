#ifndef APP_KEY_H
#define APP_KEY_H
#include "FreeRTOS.h"
#include "task.h"
#include "key_logic.h"
typedef struct {
    KeyAction action;
    TickType_t tick;
    uint32_t sequence;
} KeyEvent;
/* Call once before scheduler, before App_Display_Create.
 * 0=OK, 1=queue allocation, 2=task allocation, 3=duplicate, 4=tick/config.
 */
int App_Key_Create(void);
BaseType_t App_Key_IsReady(void);
/* Single consumer: DisplayTask. Removes an event (unlike sensor Peek APIs). */
BaseType_t App_Key_Receive(KeyEvent *out, TickType_t wait_ticks);
extern volatile uint32_t g_key_events;
extern volatile uint32_t g_key_dropped;
extern volatile uint32_t g_key_scan_gaps;
extern volatile uint32_t g_key_last_action;
extern volatile UBaseType_t g_key_stack_free;
#endif
