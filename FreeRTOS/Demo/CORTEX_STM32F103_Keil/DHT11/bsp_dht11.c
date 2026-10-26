#include "bsp_dht11.h"
#include "dht11_config.h"
#include "stm32f10x_lib.h"
#include "stm32f10x_tim.h"

#if !defined(_EXTI) || !defined(_TIM2) || !defined(_GPIOB) || !defined(_AFIO)
#error Enable _EXTI, _TIM2, _GPIOB and _AFIO in stm32f10x_conf.h
#endif

#define DHT_LINE 0x00000001UL
static DHT11_Capture s_capture;
static uint8_t s_ready = 0U;
static uint8_t s_low = 0U;
static volatile uint8_t s_active = 0U;

static void StopCapture(void)
{
    EXTI->IMR &= ~DHT_LINE;
    s_active = 0U;
    EXTI->PR = DHT_LINE;
    NVIC_ClearIRQChannelPendingBit(EXTI0_IRQChannel);
}

DHT11_Status BSP_DHT11_Init(void)
{
    RCC_ClocksTypeDef clocks;
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef timer;
    NVIC_InitTypeDef irq;
    uint32_t timer_hz;

    s_ready = 0U;
    s_low = 0U;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    StopCapture();
    GPIOB->BSRR = GPIO_Pin_0; /* OD high = release, external pull-up needed. */
    gpio.GPIO_Pin = GPIO_Pin_0;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);

    RCC_GetClocksFreq(&clocks);
    timer_hz = clocks.PCLK1_Frequency;
    if (clocks.PCLK1_Frequency != clocks.HCLK_Frequency) { timer_hz *= 2U; }
    if ((timer_hz < 1000000UL) || ((timer_hz % 1000000UL) != 0U) ||
        ((timer_hz / 1000000UL) > 65536UL)) { return DHT11_CLOCK; }

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_DeInit(TIM2);
    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = (uint16_t)(timer_hz / 1000000UL - 1U);
    timer.TIM_Period = 0xFFFFU;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &timer);
    TIM2->DIER = 0U; /* No timer interrupt or DMA request. */
    TIM2->CNT = 0U;
    TIM2->SR = 0U;
    TIM_Cmd(TIM2, ENABLE);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource0);
    /* Only EXTI0 is owned here; no need to add stm32f10x_exti.c. */
    EXTI->EMR &= ~DHT_LINE;
    EXTI->RTSR |= DHT_LINE;
    EXTI->FTSR |= DHT_LINE;
    StopCapture();
    irq.NVIC_IRQChannel = EXTI0_IRQChannel;
    irq.NVIC_IRQChannelPreemptionPriority = DHT11_IRQ_PRIORITY;
    irq.NVIC_IRQChannelSubPriority = 0U;
    irq.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&irq); /* main must already have selected PriorityGroup_4. */
    s_capture.count = 0U;
    s_capture.status = (uint8_t)DHT11_NOT_READY;
    s_ready = 1U;
    return DHT11_OK;
}

DHT11_Status BSP_DHT11_StartLow(void)
{
    if (s_ready == 0U) { return DHT11_NOT_READY; }
    if ((s_active != 0U) || (s_low != 0U)) { return DHT11_BUSY; }
    StopCapture();
    s_capture.count = 0U;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) == Bit_RESET)
    { return DHT11_BUS_LOW; }
    GPIOB->BRR = GPIO_Pin_0;
    s_low = 1U;
    return DHT11_OK;
}

DHT11_Status BSP_DHT11_Release(void)
{
    if ((s_ready == 0U) || (s_low == 0U)) { return DHT11_NOT_READY; }
    StopCapture();
    DHT11_CaptureBegin(&s_capture, TIM2->CNT);
    s_low = 0U;
    s_active = 1U;
    EXTI->IMR |= DHT_LINE;
    GPIOB->BSRR = GPIO_Pin_0;
    return DHT11_OK;
}

DHT11_Status BSP_DHT11_Poll(DHT11_Reading *out)
{
    DHT11_Status result;
    if (out == 0) { return DHT11_ARGUMENT; }
    if (s_ready == 0U) { return DHT11_NOT_READY; }
    result = (DHT11_Status)s_capture.status;
    if (result == DHT11_BUSY) { return result; }
    StopCapture();
    if (result != DHT11_OK) { return result; }
    return DHT11_Decode(s_capture.edge, s_capture.count, out);
}

DHT11_Status BSP_DHT11_Abort(void)
{
    DHT11_Status result;
    if (s_ready == 0U) { return DHT11_NOT_READY; }
    StopCapture();
    s_low = 0U;
    GPIOB->BSRR = GPIO_Pin_0;
    result = (s_capture.count == 0U) ? DHT11_NO_RESPONSE : DHT11_TIMEOUT;
    s_capture.status = (uint8_t)result;
    return result;
}

uint8_t BSP_DHT11_EdgeCount(void) { return s_capture.count; }

void BSP_DHT11_EXTI0_IRQHandler(void)
{
    uint16_t now;
    uint8_t high;
    if ((EXTI->PR & DHT_LINE) == 0U) { return; }
    now = TIM2->CNT;
    high = (GPIOB->IDR & GPIO_Pin_0) ? 1U : 0U;
    EXTI->PR = DHT_LINE;
    if (s_active == 0U) { return; }
    /* No RTOS, printf, delay, allocation or busy-wait in this high-priority ISR. */
    DHT11_CaptureEdge(&s_capture, now, high);
    if (s_capture.status != (uint8_t)DHT11_BUSY)
    {
        EXTI->IMR &= ~DHT_LINE;
        s_active = 0U;
    }
}

#if DHT11_DEFINE_EXTI0_HANDLER
void EXTI0_IRQHandler(void) { BSP_DHT11_EXTI0_IRQHandler(); }
#endif
