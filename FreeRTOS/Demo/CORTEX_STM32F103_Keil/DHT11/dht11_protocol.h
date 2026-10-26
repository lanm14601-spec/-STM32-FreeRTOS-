#ifndef DHT11_PROTOCOL_H
#define DHT11_PROTOCOL_H

#include <stdint.h>

#define DHT11_EDGE_COUNT 83U

typedef enum {
    DHT11_OK = 0, DHT11_BUSY, DHT11_WARMUP, DHT11_PRIMING,
    DHT11_NO_RESPONSE, DHT11_BUS_LOW, DHT11_TIMEOUT,
    DHT11_TIMING, DHT11_CHECKSUM, DHT11_FORMAT,
    DHT11_CLOCK, DHT11_ARGUMENT, DHT11_START_LATE,
    DHT11_NOT_READY, DHT11_STALE
} DHT11_Status;

typedef struct {
    uint16_t humidity_x10;
    int16_t temperature_x10;
    uint8_t raw[5];
} DHT11_Reading;

/* Single writer: EXTI0 ISR after Begin; foreground reads after completion.
 * Disable the owning interrupt source before cancelling/reinitializing.
 */
typedef struct {
    volatile uint16_t edge[DHT11_EDGE_COUNT];
    volatile uint16_t release_us;
    volatile uint8_t count;
    volatile uint8_t status;
} DHT11_Capture;

void DHT11_CaptureBegin(DHT11_Capture *capture, uint16_t release_us);
void DHT11_CaptureEdge(DHT11_Capture *capture, uint16_t now_us, uint8_t high);
DHT11_Status DHT11_Decode(const volatile uint16_t *edges, uint8_t count,
                          DHT11_Reading *out);
const char *DHT11_StatusName(DHT11_Status status);

#endif
