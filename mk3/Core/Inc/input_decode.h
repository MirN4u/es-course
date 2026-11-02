#ifndef INPUT_DECODE_H_
#define INPUT_DECODE_H_

#include <stdint.h>
#include "tanks.h"

/* Из маски нажатых клавиш (Keypad_Read) получает направление и «огонь».
   previous - маска прошлого опроса, held - запомненное направление (хранит вызывающий).
   Новая нажатая стрелка важнее удерживаемой; отпустили удерживаемую - берётся любая другая нажатая. */
Input Input_Decode(uint16_t mask, uint16_t previous, Dir *held);

#endif /* INPUT_DECODE_H_ */
