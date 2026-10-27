#ifndef UI_NAVIGATION_H
#define UI_NAVIGATION_H
#include "key_logic.h"
typedef enum {
    DISPLAY_OVERVIEW = 0, DISPLAY_LIGHT, DISPLAY_DHT, DISPLAY_PAGE_COUNT
} DisplayPage;
DisplayPage UI_Navigate(DisplayPage current, KeyAction action);
#endif
