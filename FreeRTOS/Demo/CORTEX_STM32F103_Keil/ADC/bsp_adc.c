#include "bsp_adc.h"
#include "stm32f10x_lib.h"
#include "stm32f10x_adc.h"

#if !defined(_ADC) || !defined(_ADC1)
#error Enable _ADC and _ADC1 in the legacy stm32f10x_conf.h
#endif

/* Iteration budget, NOT a calibrated time in microseconds or milliseconds. */
#define ADC_WAIT_LIMIT 100000UL

static uint8_t s_ready = 0U;

static void ADC_PowerUpDelay(void)
{
    volatile uint32_t i;

    /* One-time conservative settling delay at <=72 MHz.
     * Volatile keeps the loop. No SysTick reconfiguration, no IRQ masking.
     * Periodic sampling delays are handled by FreeRTOS in app_adc.c.
     */
    for (i = 0U; i < 1024U; ++i)
    {
    }
}

static BSP_ADC_Status ADC_Fail(BSP_ADC_Status status)
{
    ADC_Cmd(ADC1, DISABLE);
    s_ready = 0U;
    return status;
}

BSP_ADC_Status BSP_ADC_Init(void)
{
    GPIO_InitTypeDef gpio;
    ADC_InitTypeDef adc;
    uint32_t count;

    s_ready = 0U;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                          RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6); /* 72 MHz / 6 = 12 MHz. */

    gpio.GPIO_Pin = GPIO_Pin_0;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &gpio);

    ADC_DeInit(ADC1);
    ADC_StructInit(&adc);
    adc.ADC_Mode = ADC_Mode_Independent;
    adc.ADC_ScanConvMode = DISABLE;
    adc.ADC_ContinuousConvMode = DISABLE;
    adc.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    adc.ADC_DataAlign = ADC_DataAlign_Right;
    adc.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &adc);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1,
                             ADC_SampleTime_239Cycles5);

    ADC_Cmd(ADC1, ENABLE);
    ADC_PowerUpDelay();

    ADC_ResetCalibration(ADC1);
    count = ADC_WAIT_LIMIT;
    while (ADC_GetResetCalibrationStatus(ADC1) != RESET)
    {
        if (count == 0U)
        {
            return ADC_Fail(BSP_ADC_ERR_RESET_CAL);
        }
        --count;
    }

    ADC_StartCalibration(ADC1);
    count = ADC_WAIT_LIMIT;
    while (ADC_GetCalibrationStatus(ADC1) != RESET)
    {
        if (count == 0U)
        {
            return ADC_Fail(BSP_ADC_ERR_CAL);
        }
        --count;
    }

    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    s_ready = 1U;
    return BSP_ADC_OK;
}

BSP_ADC_Status BSP_ADC_ReadRaw(uint16_t *value)
{
    uint32_t count;

    if (value == 0)
    {
        return BSP_ADC_ERR_ARGUMENT;
    }
    if (s_ready == 0U)
    {
        return BSP_ADC_ERR_NOT_READY;
    }

    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    count = ADC_WAIT_LIMIT;
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET)
    {
        if (count == 0U)
        {
            return ADC_Fail(BSP_ADC_ERR_CONVERSION);
        }
        --count;
    }

    *value = ADC_GetConversionValue(ADC1);
    return BSP_ADC_OK;
}
