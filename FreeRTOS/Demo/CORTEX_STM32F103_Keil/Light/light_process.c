#include "light_process.h"

void Light_Reset(LightContext *ctx)
{
    if (ctx != 0)
    {
        ctx->state = LIGHT_UNKNOWN;
        ctx->candidate = LIGHT_UNKNOWN;
        ctx->count = 0U;
    }
}

LightResult Light_Update(LightContext *ctx, const LightConfig *cfg,
                         uint16_t filtered, uint8_t adc_valid,
                         uint8_t filter_ready)
{
    LightResult out;
    LightState desired;
    int32_t span;
    int32_t distance;

    out.percent = 0U;
    out.state = LIGHT_UNKNOWN;
    out.status = LIGHT_CONFIG_ERROR;
    if ((ctx == 0) || (cfg == 0))
    {
        Light_Reset(ctx);
        return out;
    }
    span = (int32_t)cfg->bright_adc - (int32_t)cfg->dark_adc;
    if (span < 0) { span = -span; }
    if ((cfg->dark_adc > 4095U) || (cfg->bright_adc > 4095U) ||
        (cfg->min_span == 0U) || (span < (int32_t)cfg->min_span) ||
        (cfg->on_pct >= cfg->off_pct) || (cfg->off_pct > 100U) ||
        (cfg->confirm_samples == 0U) || (cfg->calibrated > 1U))
    {
        Light_Reset(ctx);
        return out;
    }
    if ((adc_valid == 0U) || (filtered > 4095U))
    {
        out.status = LIGHT_ADC_ERROR;
    }
    else if (cfg->calibrated == 0U)
    {
        out.status = LIGHT_UNCALIBRATED;
    }
    else if (filter_ready == 0U)
    {
        out.status = LIGHT_WARMUP;
    }
    else
    {
        out.status = LIGHT_OK;
    }
    if (out.status != LIGHT_OK)
    {
        Light_Reset(ctx);
        return out;
    }

    /* Both sensor polarities are supported. Clamp before division. */
    if (cfg->bright_adc > cfg->dark_adc)
    {
        distance = (int32_t)filtered - (int32_t)cfg->dark_adc;
    }
    else
    {
        distance = (int32_t)cfg->dark_adc - (int32_t)filtered;
    }
    if (distance < 0) { distance = 0; }
    if (distance > span) { distance = span; }
    out.percent = (uint8_t)((distance * 100 + span / 2) / span);

    desired = ctx->state;
    if (out.percent <= cfg->on_pct) { desired = LIGHT_DARK; }
    else if (out.percent >= cfg->off_pct) { desired = LIGHT_NORMAL; }

    if (desired == ctx->state)
    {
        ctx->candidate = ctx->state;
        ctx->count = 0U;
    }
    else
    {
        if (ctx->candidate != desired)
        {
            ctx->candidate = desired;
            ctx->count = 1U;
        }
        else if (ctx->count < cfg->confirm_samples)
        {
            ++ctx->count;
        }
        if (ctx->count >= cfg->confirm_samples)
        {
            ctx->state = desired;
            ctx->count = 0U;
        }
    }
    out.state = ctx->state;
    return out;
}

const char *Light_StateName(LightState state)
{
    switch (state)
    {
        case LIGHT_NORMAL: return "NORMAL";
        case LIGHT_DARK: return "DARK";
        default: return "UNKNOWN";
    }
}

const char *Light_StatusName(LightStatus status)
{
    switch (status)
    {
        case LIGHT_OK: return "OK";
        case LIGHT_UNCALIBRATED: return "UNCAL";
        case LIGHT_ADC_ERROR: return "ADC_ERROR";
        case LIGHT_WARMUP: return "WARMUP";
        default: return "CONFIG_ERROR";
    }
}
