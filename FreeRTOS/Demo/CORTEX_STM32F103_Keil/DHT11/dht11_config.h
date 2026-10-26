#ifndef DHT11_CONFIG_H
#define DHT11_CONFIG_H

#define DHT11_DEFINE_EXTI0_HANDLER 1
/* Logical NVIC priority. Current MAX_SYSCALL logical priority is 11.
 * This IRQ is above the RTOS threshold and MUST NOT call ANY RTOS API.
 */
#define DHT11_IRQ_PRIORITY 10U
#define DHT11_STARTUP_MS 2000U
#define DHT11_INTERVAL_MS 2500U
#define DHT11_START_LOW_MS 21U
#define DHT11_CAPTURE_TIMEOUT_MS 12U
#define DHT11_STALE_MS 6000U

#endif
