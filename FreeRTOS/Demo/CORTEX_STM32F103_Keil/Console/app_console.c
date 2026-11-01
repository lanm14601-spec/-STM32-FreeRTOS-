#include "app_console.h"
#include "queue.h"
#include "stm32f10x_lib.h"

typedef struct { uint32_t generation; uint8_t byte; } RxByte;
static QueueHandle_t s_rx_queue = NULL;
static QueueHandle_t s_command_queue = NULL;
static TaskHandle_t s_task = NULL;
/* These two fields are written only by USART1_IRQHandler. */
static uint32_t s_generation = 0U;
static uint8_t s_discard = 0U;
volatile uint32_t g_console_ready = 0U;
volatile uint32_t g_console_rx_bytes = 0U;
volatile uint32_t g_console_rx_dropped = 0U;
volatile uint32_t g_console_uart_errors = 0U;
volatile uint32_t g_console_commands = 0U;
volatile uint32_t g_console_command_dropped = 0U;
volatile UBaseType_t g_console_stack_free = 0U;

void USART1_IRQHandler(void)
{
    BaseType_t wake = pdFALSE;
    RxByte item = {0U, 0U};
    uint32_t status = USART1->SR;
    uint8_t byte;
    if ((status & (USART_FLAG_RXNE | USART_FLAG_ORE | USART_FLAG_NE |
                   USART_FLAG_FE | USART_FLAG_PE)) == 0U) return;
    /* SR then DR clears RXNE/ORE/FE/NE/PE. Do not manually clear RXNE later. */
    byte = (uint8_t)USART1->DR;
    ++g_console_rx_bytes;
    if ((status & (USART_FLAG_ORE | USART_FLAG_NE | USART_FLAG_FE | USART_FLAG_PE)) != 0U)
    {
        ++g_console_uart_errors;
        ++g_console_rx_dropped;
        if (s_discard == 0U) ++s_generation;
        s_discard = 1U;
        return;
    }
    if ((s_discard != 0U) && (byte != '\r') && (byte != '\n'))
    {
        ++g_console_rx_dropped;
        return;
    }
    item.generation = s_generation;
    item.byte = byte;
    if ((s_rx_queue == NULL) || (xQueueSendFromISR(s_rx_queue, &item, &wake) != pdPASS))
    {
        ++g_console_rx_dropped;
        if (s_discard == 0U) ++s_generation;
        s_discard = 1U;
    }
    else s_discard = 0U; /* A queued terminator ends discard mode. */
    portEND_SWITCHING_ISR(wake);
}

static void EnableReceive(void)
{
    NVIC_InitTypeDef nvic;
    volatile uint32_t discard;
    USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
    discard = USART1->SR;
    discard = USART1->DR;
    (void)discard;
    NVIC_ClearIRQChannelPendingBit(USART1_IRQChannel);
    nvic.NVIC_IRQChannel = USART1_IRQChannel;
    nvic.NVIC_IRQChannelPreemptionPriority = CONSOLE_IRQ_PRIORITY;
    nvic.NVIC_IRQChannelSubPriority = 0U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    g_console_ready = 1U;
}

static void ConsoleTask(void *parameter)
{
    ConsoleParser parser;
    ConsoleCommand command;
    RxByte item;
    uint32_t generation = 0U;
    unsigned int batch = 0U;
    (void)parameter;
    ConsoleParser_Init(&parser);
    /* Scheduler and both queues now exist before any FromISR call can occur. */
    EnableReceive();
    for (;;)
    {
        if (xQueueReceive(s_rx_queue, &item, portMAX_DELAY) == pdPASS)
        {
            if (item.generation != generation)
            {
                generation = item.generation;
                ConsoleParser_Lost(&parser);
            }
            if (ConsoleParser_Feed(&parser, item.byte, &command) != 0U)
            {
                command.id = ++g_console_commands;
                if (xQueueSend(s_command_queue, &command, 0) != pdPASS)
                    ++g_console_command_dropped;
            }
            if (++batch >= CONSOLE_RX_QUEUE_LENGTH)
            {
                batch = 0U;
                vTaskDelay(1); /* Bound work under sustained input. */
            }
        }
        g_console_stack_free = uxTaskGetStackHighWaterMark(NULL);
    }
}

int App_Console_Create(void)
{
    if ((s_rx_queue != NULL) || (s_command_queue != NULL) || (s_task != NULL)) return 4;
    if ((configTICK_RATE_HZ != 1000) || (configUSE_16_BIT_TICKS != 0) ||
        (CONSOLE_IRQ_PRIORITY > 15U) ||
        ((CONSOLE_IRQ_PRIORITY << 4) < configMAX_SYSCALL_INTERRUPT_PRIORITY)) return 5;
    s_rx_queue = xQueueCreate(CONSOLE_RX_QUEUE_LENGTH, sizeof(RxByte));
    if (s_rx_queue == NULL) return 1;
    s_command_queue = xQueueCreate(CONSOLE_COMMAND_QUEUE_LENGTH, sizeof(ConsoleCommand));
    if (s_command_queue == NULL)
    {
        vQueueDelete(s_rx_queue); s_rx_queue = NULL;
        return 2;
    }
    if (xTaskCreate(ConsoleTask, "Console", 256, NULL, 1, &s_task) != pdPASS)
    {
        vQueueDelete(s_command_queue); s_command_queue = NULL;
        vQueueDelete(s_rx_queue); s_rx_queue = NULL;
        s_task = NULL;
        return 3;
    }
    return 0;
}

BaseType_t App_Console_Receive(ConsoleCommand *out, TickType_t wait)
{
    if ((out == NULL) || (s_command_queue == NULL)) return pdFAIL;
    return xQueueReceive(s_command_queue, out, wait);
}
