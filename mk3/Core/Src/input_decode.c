#include "keypad.h"
#include "input_decode.h"

/* Раскладка телефона: 2 вверх, 8 вниз, 4 влево, 6 вправо, 5 огонь. Если на стенде иначе - поправить здесь. */
#define KEY_UP      KEYPAD_BIT(0, 1)
#define KEY_LEFT    KEYPAD_BIT(1, 0)
#define KEY_FIRE    KEYPAD_BIT(1, 1)
#define KEY_RIGHT   KEYPAD_BIT(1, 2)
#define KEY_DOWN    KEYPAD_BIT(2, 1)

#define ARROW_COUNT 4

static const struct { uint16_t key; Dir dir; } ARROWS[ARROW_COUNT] = {
    { KEY_UP, DIR_UP }, { KEY_DOWN, DIR_DOWN }, { KEY_LEFT, DIR_LEFT }, { KEY_RIGHT, DIR_RIGHT }
};

Input Input_Decode(uint16_t mask, uint16_t previous, Dir *held)
{
    uint16_t pressed_now = mask & (uint16_t)~previous;
    Input in;

    for (int i = 0; i < ARROW_COUNT; i++) {
        if (pressed_now & ARROWS[i].key) {
            *held = ARROWS[i].dir;
        }
    }

    uint8_t held_still_down = 0;
    for (int i = 0; i < ARROW_COUNT; i++) {
        if (ARROWS[i].dir == *held && (mask & ARROWS[i].key)) {
            held_still_down = 1;
        }
    }
    if (!held_still_down) {
        *held = DIR_NONE;
        for (int i = 0; i < ARROW_COUNT; i++) {
            if (mask & ARROWS[i].key) {
                *held = ARROWS[i].dir;
            }
        }
    }

    in.dir = *held;
    in.fire = (mask & KEY_FIRE) ? 1 : 0;
    return in;
}
