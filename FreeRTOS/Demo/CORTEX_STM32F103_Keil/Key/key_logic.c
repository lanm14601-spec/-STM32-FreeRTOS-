#include "key_logic.h"
#include "key_config.h"

void KeyLogic_Init(KeyContext *ctx, uint8_t raw_pressed, uint32_t now_ms)
{
    if (ctx == 0) return;
    raw_pressed = (raw_pressed != 0U) ? 1U : 0U;
    ctx->candidate_since = now_ms;
    ctx->pressed_since = now_ms;
    ctx->last_sample = now_ms;
    ctx->stable = raw_pressed;
    ctx->candidate = raw_pressed;
    ctx->armed = 0U;
    ctx->long_sent = 0U;
}

KeyAction KeyLogic_Update(KeyContext *ctx, uint8_t raw_pressed, uint32_t now_ms)
{
    uint32_t held_ms;
    if (ctx == 0) return KEY_NONE;
    raw_pressed = (raw_pressed != 0U) ? 1U : 0U;
    if ((uint32_t)(now_ms - ctx->last_sample) > KEY_MAX_SCAN_GAP_MS)
    {
        /* An unobserved interval is not evidence of a continuous press. */
        KeyLogic_Init(ctx, raw_pressed, now_ms);
        return KEY_NONE;
    }
    ctx->last_sample = now_ms;
    if (raw_pressed != ctx->candidate)
    {
        ctx->candidate = raw_pressed;
        ctx->candidate_since = now_ms;
    }
    if (ctx->armed == 0U)
    {
        if ((ctx->candidate == 0U) &&
            ((uint32_t)(now_ms - ctx->candidate_since) >= KEY_DEBOUNCE_MS))
        {
            ctx->stable = 0U;
            ctx->armed = 1U;
        }
        return KEY_NONE;
    }
    if ((ctx->candidate != ctx->stable) &&
        ((uint32_t)(now_ms - ctx->candidate_since) >= KEY_DEBOUNCE_MS))
    {
        ctx->stable = ctx->candidate;
        if (ctx->stable != 0U)
        {
            ctx->pressed_since = now_ms; /* Long duration starts after debounce. */
            ctx->long_sent = 0U;
        }
        else if (ctx->long_sent == 0U)
        {
            /* Use the start of the confirmed release, excluding release debounce. */
            held_ms = (uint32_t)(ctx->candidate_since - ctx->pressed_since);
            return (held_ms >= KEY_LONG_MS) ? KEY_LONG : KEY_SHORT;
        }
    }
    /* Do not turn a near-threshold short press into a long during release debounce. */
    if ((ctx->stable != 0U) && (ctx->candidate != 0U) &&
        (ctx->long_sent == 0U) &&
        ((uint32_t)(now_ms - ctx->pressed_since) >= KEY_LONG_MS))
    {
        ctx->long_sent = 1U;
        return KEY_LONG;
    }
    return KEY_NONE;
}
