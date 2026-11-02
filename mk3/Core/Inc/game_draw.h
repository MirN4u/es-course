#ifndef GAME_DRAW_H_
#define GAME_DRAW_H_

#include "tanks.h"

/* Рисует кадр игры в буфер дисплея (по I2C ничего не передаёт - это делает oled_UpdateScreen). */
void Draw_Game(const Game *g);

#endif /* GAME_DRAW_H_ */
