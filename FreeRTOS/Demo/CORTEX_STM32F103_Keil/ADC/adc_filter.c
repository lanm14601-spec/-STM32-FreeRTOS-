#include "adc_filter.h"

void ADC_FilterReset(ADC_Filter *filter)
{
    uint8_t i;

    filter->sum = 0U;
    filter->next = 0U;
    filter->count = 0U;
    for (i = 0U; i < ADC_FILTER_WINDOW; ++i)
    {
        filter->values[i] = 0U;
    }
}

uint16_t ADC_FilterPush(ADC_Filter *filter, uint16_t raw)
{
    if (filter->count == ADC_FILTER_WINDOW)
    {
        filter->sum -= filter->values[filter->next];
    }
    else
    {
        ++filter->count;
    }

    filter->values[filter->next] = raw;
    filter->sum += raw;
    filter->next = (uint8_t)((filter->next + 1U) % ADC_FILTER_WINDOW);

    /* Average only valid samples during startup; round to nearest integer. */
    return (uint16_t)((filter->sum + filter->count / 2U) / filter->count);
}

uint32_t ADC_ToMillivolts(uint16_t raw)
{
    return ((uint32_t)raw * ADC_REFERENCE_MV + 2047UL) / 4095UL;
}
