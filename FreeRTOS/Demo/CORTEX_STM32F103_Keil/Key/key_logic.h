#ifndef KEY_LOGIC_H
#define KEY_LOGIC_H
#include <stdint.h>
typedef enum { KEY_NONE = 0, KEY_SHORT, KEY_LONG } KeyAction;
typedef struct {
    uint32_t candidate_since;
    uint32_t pressed_since;
    uint32_t last_sample;
    uint8_t stable;
    uint8_t candidate;
    uint8_t armed;
    uint8_t long_sent;
} KeyContext;
/* raw_pressed: 1=pressed, 0=released. now_ms may wrap at UINT32_MAX.
 * At boot or after a scan gap, require a debounced release to arm.
 */
void KeyLogic_Init(KeyContext *ctx, uint8_t raw_pressed, uint32_t now_ms);
KeyAction KeyLogic_Update(KeyContext *ctx, uint8_t raw_pressed, uint32_t now_ms);
#endif
