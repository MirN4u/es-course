#include "main.h"
#include "pca9538.h"
#include "keypad.h"

#define KBRD_ADDR        0xE2
#define PCA_COLUMNS_MASK 0x70
#define PCA_COL_LEFT     0x10
#define PCA_COL_CENTER   0x20
#define PCA_COL_RIGHT    0x40

static const uint8_t ROW_SELECT[KEYPAD_ROWS] = { 0xFE, 0xFD, 0xFB, 0xF7 };   /* «0» на одной из линий P0..P3 */

uint16_t Keypad_Read(void)
{
    uint16_t mask = 0;

    for (int row = 0; row < KEYPAD_ROWS; row++) {
        uint8_t buf = 0;
        uint8_t inputs = PCA_COLUMNS_MASK;   /* при ошибке чтения считаем, что ничего не нажато */

        PCA9538_Write_Register(KBRD_ADDR, POLARITY_INVERSION, &buf);
        PCA9538_Write_Register(KBRD_ADDR, OUTPUT_PORT, &buf);
        buf = ROW_SELECT[row];
        PCA9538_Write_Register(KBRD_ADDR, CONFIG, &buf);
        if (PCA9538_Read_Inputs(KBRD_ADDR, &buf) == HAL_OK) {
            inputs = buf & PCA_COLUMNS_MASK;
        }

        /* нажатая клавиша тянет линию столбца к «0» */
        if (!(inputs & PCA_COL_LEFT))   mask |= KEYPAD_BIT(row, 0);
        if (!(inputs & PCA_COL_CENTER)) mask |= KEYPAD_BIT(row, 1);
        if (!(inputs & PCA_COL_RIGHT))  mask |= KEYPAD_BIT(row, 2);
    }
    return mask;
}
