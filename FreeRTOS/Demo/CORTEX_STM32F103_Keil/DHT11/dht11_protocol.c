#include "dht11_protocol.h"

void DHT11_CaptureBegin(DHT11_Capture *c, uint16_t release_us)
{
    c->release_us = release_us;
    c->count = 0U;
    c->status = (uint8_t)DHT11_BUSY;
}

void DHT11_CaptureEdge(DHT11_Capture *c, uint16_t now_us, uint8_t high)
{
    uint8_t n;
    uint16_t delta;
    if (c->status != (uint8_t)DHT11_BUSY) { return; }
    n = c->count;
    if (n == 0U)
    {
        /* Ignore the host's release rising edge. First sensor edge is falling. */
        if (high != 0U) { return; }
        delta = (uint16_t)(now_us - c->release_us);
        if ((delta < 10U) || (delta > 200U))
        {
            c->status = (uint8_t)DHT11_TIMING;
            return;
        }
    }
    else
    {
        if (high != (uint8_t)(n & 1U))
        {
            c->status = (uint8_t)DHT11_TIMING;
            return;
        }
        delta = (uint16_t)(now_us - c->edge[n - 1U]);
        if ((delta == 0U) || (delta > 200U))
        {
            c->status = (uint8_t)DHT11_TIMING;
            return;
        }
    }
    c->edge[n] = now_us;
    c->count = (uint8_t)(n + 1U);
    if (c->count == DHT11_EDGE_COUNT) { c->status = (uint8_t)DHT11_OK; }
}

DHT11_Status DHT11_Decode(const volatile uint16_t *edges, uint8_t count,
                          DHT11_Reading *out)
{
    uint8_t data[5] = {0U, 0U, 0U, 0U, 0U};
    uint8_t i;
    uint8_t one;
    uint16_t low_us;
    uint16_t high_us;
    uint16_t humidity;
    uint16_t temperature;
    if ((edges == 0) || (out == 0)) { return DHT11_ARGUMENT; }
    if (count != DHT11_EDGE_COUNT) { return DHT11_TIMEOUT; }
    low_us = (uint16_t)(edges[1] - edges[0]);
    high_us = (uint16_t)(edges[2] - edges[1]);
    if ((low_us < 55U) || (low_us > 110U) ||
        (high_us < 55U) || (high_us > 110U)) { return DHT11_TIMING; }
    for (i = 0U; i < 40U; ++i)
    {
        low_us = (uint16_t)(edges[3U + 2U*i] - edges[2U + 2U*i]);
        high_us = (uint16_t)(edges[4U + 2U*i] - edges[3U + 2U*i]);
        if ((low_us < 35U) || (low_us > 80U)) { return DHT11_TIMING; }
        if ((high_us >= 15U) && (high_us <= 40U)) { one = 0U; }
        else if ((high_us >= 55U) && (high_us <= 95U)) { one = 1U; }
        else { return DHT11_TIMING; }
        data[i / 8U] = (uint8_t)((data[i / 8U] << 1) | one);
    }
    for (i = 0U; i < 5U; ++i) { out->raw[i] = data[i]; }
    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
    { return DHT11_CHECKSUM; }
    /* Positive-temperature DHT11 format, with decimal digits if supplied.
     * Reject negative/variant formats explicitly; never parse them as DHT22.
     */
    if ((data[1] > 9U) || (data[3] > 9U)) { return DHT11_FORMAT; }
    humidity = (uint16_t)(data[0] * 10U + data[1]);
    temperature = (uint16_t)(data[2] * 10U + data[3]);
    /* Broad software plausibility limits, NOT an accuracy specification. */
    if ((humidity > 1000U) || (temperature > 600U)) { return DHT11_FORMAT; }
    out->humidity_x10 = humidity;
    out->temperature_x10 = (int16_t)temperature;
    return DHT11_OK;
}

const char *DHT11_StatusName(DHT11_Status s)
{
    switch (s)
    {
        case DHT11_OK: return "OK";
        case DHT11_BUSY: return "BUSY";
        case DHT11_WARMUP: return "WARMUP";
        case DHT11_PRIMING: return "PRIMING";
        case DHT11_NO_RESPONSE: return "NO_RESPONSE";
        case DHT11_BUS_LOW: return "BUS_LOW";
        case DHT11_TIMEOUT: return "TIMEOUT";
        case DHT11_TIMING: return "TIMING";
        case DHT11_CHECKSUM: return "CHECKSUM";
        case DHT11_FORMAT: return "FORMAT";
        case DHT11_CLOCK: return "CLOCK";
        case DHT11_START_LATE: return "START_LATE";
        case DHT11_NOT_READY: return "NOT_READY";
        case DHT11_STALE: return "STALE";
        default: return "ARGUMENT";
    }
}
