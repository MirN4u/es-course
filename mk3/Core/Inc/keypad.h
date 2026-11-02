#ifndef KEYPAD_H_
#define KEYPAD_H_

#include <stdint.h>

#define KEYPAD_ROWS 4
#define KEYPAD_COLS 3

/* Номер бита клавиши в маске: строка * KEYPAD_COLS + столбец (строки сверху вниз, столбцы слева направо). */
#define KEYPAD_BIT(row, col) ((uint16_t)(1u << ((row) * KEYPAD_COLS + (col))))

/* Опрашивает всю клавиатуру по I2C. Единица в бите - клавиша нажата.
   В отличие от Check_Row из SDK, видит несколько клавиш в одной строке (например 4 и 5). */
uint16_t Keypad_Read(void);

#endif /* KEYPAD_H_ */
