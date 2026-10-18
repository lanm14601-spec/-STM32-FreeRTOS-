#ifndef ADC_FILTER_H
#define ADC_FILTER_H

#include <stdint.h>

#define ADC_FILTER_WINDOW 8U
#define ADC_REFERENCE_MV 3300UL

typedef struct
{
    uint16_t values[ADC_FILTER_WINDOW];
    uint32_t sum;
    uint8_t next;
    uint8_t count;
} ADC_Filter;

void ADC_FilterReset(ADC_Filter *filter);
uint16_t ADC_FilterPush(ADC_Filter *filter, uint16_t raw);
uint32_t ADC_ToMillivolts(uint16_t raw);

#endif
