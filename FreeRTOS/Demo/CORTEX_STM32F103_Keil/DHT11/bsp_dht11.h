#ifndef BSP_DHT11_H
#define BSP_DHT11_H

#include "dht11_protocol.h"

/* Fixed resources: PB0, EXTI0, exclusive free-running TIM2 (1 MHz).
 * Foreground calls belong to one DHT task only.
 */
DHT11_Status BSP_DHT11_Init(void);
DHT11_Status BSP_DHT11_StartLow(void);
DHT11_Status BSP_DHT11_Release(void);
DHT11_Status BSP_DHT11_Poll(DHT11_Reading *out);
DHT11_Status BSP_DHT11_Abort(void);
uint8_t BSP_DHT11_EdgeCount(void);
void BSP_DHT11_EXTI0_IRQHandler(void);

#endif
