#include "ui_navigation.h"
DisplayPage UI_Navigate(DisplayPage current, KeyAction action)
{
    if ((current < DISPLAY_OVERVIEW) || (current >= DISPLAY_PAGE_COUNT))
        current = DISPLAY_OVERVIEW;
    if (action == KEY_LONG) return DISPLAY_OVERVIEW;
    if (action == KEY_SHORT)
        return (DisplayPage)(((unsigned int)current + 1U) % DISPLAY_PAGE_COUNT);
    return current;
}
