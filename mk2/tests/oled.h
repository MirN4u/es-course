#ifndef OLED_H
#define OLED_H
#include <stdint.h>
#include "fonts.h"
typedef enum { Black = 0, White = 1 } OLED_COLOR;
uint8_t oled_Init(void);
void oled_Fill(OLED_COLOR c);
void oled_UpdateScreen(void);
char oled_WriteString(char *s, FontDef f, OLED_COLOR c);
void oled_SetCursor(uint8_t x, uint8_t y);
#endif
