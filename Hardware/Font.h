#ifndef FONT_H
#define FONT_H

#include "DS3231.h"
#include <stdint.h>

void Font_Draw7SegTime(Time_TypeDef *time, uint8_t x, uint8_t y);
void Font_DrawBinaryTime(Time_TypeDef *time);

#endif
