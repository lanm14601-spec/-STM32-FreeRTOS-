#include "bsp_oled_bus.h"
#include "oled_config.h"
#include "stm32f10x_lib.h"
#include "stm32f10x_tim.h"

#ifndef _TIM4
#error Enable _TIM4 in the OLD stm32f10x_conf.h
#endif

#define SCL GPIO_Pin_6
#define SDA GPIO_Pin_7
static uint8_t s_ready = 0U;

static void DelayUs(uint16_t us)
{
    uint16_t start = (uint16_t)TIM4->CNT;
    while ((uint16_t)((uint16_t)TIM4->CNT - start) < us) { }
}

static OLED_Status RaiseScl(void)
{
    uint16_t start = (uint16_t)TIM4->CNT;
    GPIOB->BSRR = SCL;
    while ((GPIOB->IDR & SCL) == 0U)
    {
        if ((uint16_t)((uint16_t)TIM4->CNT - start) >= OLED_SCL_TIMEOUT_US)
            return OLED_SCL_TIMEOUT;
    }
    DelayUs(OLED_HALF_PERIOD_US);
    return OLED_OK;
}

static OLED_Status Stop(void)
{
    OLED_Status status;
    GPIOB->BRR = SCL;
    DelayUs(1U);
    GPIOB->BRR = SDA;
    DelayUs(OLED_HALF_PERIOD_US);
    status = RaiseScl();
    GPIOB->BSRR = SDA;
    DelayUs(OLED_HALF_PERIOD_US);
    if (status != OLED_OK) return status;
    return ((GPIOB->IDR & SDA) != 0U) ? OLED_OK : OLED_BUS_LOW;
}

static OLED_Status Start(void)
{
    OLED_Status status;
    GPIOB->BSRR = SDA;
    DelayUs(OLED_HALF_PERIOD_US);
    status = RaiseScl();
    if (status != OLED_OK) return status;
    if ((GPIOB->IDR & SDA) == 0U) return OLED_BUS_LOW;
    GPIOB->BRR = SDA;
    DelayUs(OLED_HALF_PERIOD_US);
    GPIOB->BRR = SCL;
    return OLED_OK;
}

static OLED_Status WriteByte(uint8_t value)
{
    uint8_t bit;
    OLED_Status status;
    for (bit = 0U; bit < 8U; ++bit)
    {
        DelayUs(1U); /* Data hold after the previous SCL falling edge. */
        if ((value & 0x80U) != 0U) GPIOB->BSRR = SDA;
        else GPIOB->BRR = SDA;
        DelayUs(OLED_HALF_PERIOD_US);
        status = RaiseScl();
        if (status != OLED_OK) return status;
        GPIOB->BRR = SCL;
        value = (uint8_t)(value << 1);
    }
    DelayUs(1U);
    GPIOB->BSRR = SDA; /* Open-drain release for slave ACK. */
    DelayUs(OLED_HALF_PERIOD_US);
    status = RaiseScl();
    if (status != OLED_OK) return status;
    status = ((GPIOB->IDR & SDA) == 0U) ? OLED_OK : OLED_NACK;
    GPIOB->BRR = SCL;
    return status;
}

OLED_Status BSP_OLED_BusInit(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef timer;
    RCC_ClocksTypeDef clocks;
    uint32_t timer_hz;
    uint8_t i;
    OLED_Status status;
    s_ready = 0U;
    RCC_GetClocksFreq(&clocks);
    timer_hz = clocks.PCLK1_Frequency;
    if (clocks.PCLK1_Frequency != clocks.HCLK_Frequency) timer_hz *= 2U;
    if ((timer_hz < 1000000UL) || ((timer_hz % 1000000UL) != 0U) ||
        ((timer_hz / 1000000UL) > 65536UL)) return OLED_CLOCK_ERROR;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIOB->BSRR = SCL | SDA;
    gpio.GPIO_Pin = SCL | SDA;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(GPIOB, &gpio);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    TIM_DeInit(TIM4);
    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = (uint16_t)(timer_hz / 1000000UL - 1U);
    timer.TIM_Period = 0xFFFFU;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM4, &timer);
    TIM4->DIER = 0U;
    TIM4->CNT = 0U;
    TIM4->SR = 0U;
    TIM_Cmd(TIM4, ENABLE);

    /* At most nine recovery clocks; only this task owns the bus. */
    GPIOB->BSRR = SDA;
    for (i = 0U; (i < 9U) && ((GPIOB->IDR & SDA) == 0U); ++i)
    {
        GPIOB->BRR = SCL;
        DelayUs(OLED_HALF_PERIOD_US);
        status = RaiseScl();
        if (status != OLED_OK)
        {
            GPIOB->BSRR = SCL | SDA;
            return status;
        }
    }
    status = Stop();
    if (status == OLED_OK) s_ready = 1U;
    return status;
}

OLED_Status BSP_OLED_Write(uint8_t control, const uint8_t *data, uint16_t count)
{
    OLED_Status status;
    OLED_Status stop_status;
    uint16_t i;
    if ((data == 0) || (count == 0U)) return OLED_ARGUMENT;
    if (s_ready == 0U) return OLED_NOT_READY;
    status = Start();
    if (status == OLED_OK) status = WriteByte((uint8_t)(OLED_ADDRESS_7BIT << 1));
    if (status == OLED_OK) status = WriteByte(control);
    for (i = 0U; (i < count) && (status == OLED_OK); ++i)
        status = WriteByte(data[i]);
    stop_status = Stop();
    if (status == OLED_OK) status = stop_status;
    if (status != OLED_OK) s_ready = 0U;
    return status;
}

const char *OLED_StatusName(OLED_Status status)
{
    switch (status)
    {
    case OLED_OK: return "OK";
    case OLED_NOT_READY: return "NOT_READY";
    case OLED_CLOCK_ERROR: return "CLOCK";
    case OLED_SCL_TIMEOUT: return "SCL_TIMEOUT";
    case OLED_BUS_LOW: return "BUS_LOW";
    case OLED_NACK: return "NACK";
    case OLED_ARGUMENT: return "ARGUMENT";
    default: return "UNKNOWN";
    }
}
