#ifndef APP_CONSOLE_H
#define APP_CONSOLE_H
#include "FreeRTOS.h"
#include "task.h"
#include "console_protocol.h"
/* Call once before scheduler, after xSerialPortInit.
 * 0=OK, 1=RX queue, 2=command queue, 3=task, 4=duplicate, 5=IRQ/tick config.
 */
int App_Console_Create(void);
/* Sole consumer: MonitorTask; all printf stays in that task. */
BaseType_t App_Console_Receive(ConsoleCommand *out, TickType_t wait);
extern volatile uint32_t g_console_ready;
extern volatile uint32_t g_console_rx_bytes;
extern volatile uint32_t g_console_rx_dropped;
extern volatile uint32_t g_console_uart_errors;
extern volatile uint32_t g_console_commands;
extern volatile uint32_t g_console_command_dropped;
extern volatile UBaseType_t g_console_stack_free;
#endif
