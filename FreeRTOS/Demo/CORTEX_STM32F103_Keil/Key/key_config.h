#ifndef KEY_CONFIG_H
#define KEY_CONFIG_H
/* One external normally-open button: PA1 to GND. Internal pull-up.
 * Times are measured with 1 ms RTOS ticks, not by counting task loops.
 */
#define KEY_SCAN_MS 10U
#define KEY_DEBOUNCE_MS 30U
#define KEY_LONG_MS 800U
#define KEY_MAX_SCAN_GAP_MS 50U
#define KEY_QUEUE_LENGTH 8U
#endif
