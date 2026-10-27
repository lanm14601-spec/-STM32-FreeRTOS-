#include "bsp_key.h"
#include "stm32f10x_lib.h"

void BSP_Key_Init(void)
{
    GPIO_InitTypeDef gpio;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIOA->BSRR = GPIO_Pin_1;
    gpio.GPIO_Pin = GPIO_Pin_1;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &gpio);
}

uint8_t BSP_Key_IsPressed(void)
{
    return ((GPIOA->IDR & GPIO_Pin_1) == 0U) ? 1U : 0U;
}
