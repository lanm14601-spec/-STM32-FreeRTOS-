#ifndef LIGHT_CONFIG_H
#define LIGHT_CONFIG_H

/* Set to 1 ONLY after measuring both endpoints on your actual sensor. */
#define LIGHT_CALIBRATED       1U
#define LIGHT_DARK_ADC         3100U
#define LIGHT_BRIGHT_ADC       1910U
/* Software acceptance criterion, not a sensor specification. */
#define LIGHT_MIN_SPAN         100U
#define LIGHT_DARK_ON_PCT      20U
#define LIGHT_DARK_OFF_PCT     30U
#define LIGHT_CONFIRM_SAMPLES  3U

#endif
