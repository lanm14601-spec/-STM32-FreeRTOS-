#ifndef OLED_TEXT_H
#define OLED_TEXT_H
#include <stdint.h>
#include "oled_config.h"
/* One 5x7 uppercase glyph plus one blank column; clears the complete page. */
void OLED_TextPage(uint8_t pixels[OLED_WIDTH], const char *text);
#endif
