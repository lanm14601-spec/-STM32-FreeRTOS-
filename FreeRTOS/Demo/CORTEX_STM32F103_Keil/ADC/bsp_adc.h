#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdint.h>

typedef enum
{
    BSP_ADC_OK = 0,
    BSP_ADC_ERR_ARGUMENT = 1,
    BSP_ADC_ERR_RESET_CAL = 2,
    BSP_ADC_ERR_CAL = 3,
    BSP_ADC_ERR_CONVERSION = 4,
    BSP_ADC_ERR_NOT_READY = 5
} BSP_ADC_Status;

/* ADC1 channel 0 / PA0. Exclusive ownership by AdcTask. */
BSP_ADC_Status BSP_ADC_Init(void);
BSP_ADC_Status BSP_ADC_ReadRaw(uint16_t *value);

#endif
