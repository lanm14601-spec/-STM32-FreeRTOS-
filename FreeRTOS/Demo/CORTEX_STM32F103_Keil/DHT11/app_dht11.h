#ifndef APP_DHT11_H
#define APP_DHT11_H

#include "FreeRTOS.h"
#include "task.h"
#include "dht11_protocol.h"

typedef struct {
    uint32_t sequence;
    TickType_t tick; /* Acquisition/attempt completion tick, not sensor-internal time. */
    uint32_t failures;
    DHT11_Reading reading;
    uint8_t valid;
    uint8_t status;
    uint8_t edges;
} DHT11_Sample;

/* Call once before scheduler. 0=OK, 1=queue, 2=task, 3=already created,
 * 4=unsupported tick/IRQ configuration. Does not print.
 */
int App_DHT11_Create(void);
/* Task context only. Copies the latest complete sample; does not consume it.
 * Returned valid=0 on errors, warmup, priming or stale data.
 */
BaseType_t App_DHT11_GetLatest(DHT11_Sample *out);

extern volatile uint32_t g_dht_sequence;
extern volatile uint32_t g_dht_failures;
extern volatile uint32_t g_dht_status;
extern volatile uint32_t g_dht_edges;
extern volatile UBaseType_t g_dht_stack_free;

#endif
