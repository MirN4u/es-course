#include <stdio.h>
#include "keypad.h"
#include "input_decode.h"
#include "../Core/Src/input_decode.c"
#include "../Core/Src/tanks.c"

static int errors = 0;
#define CHECK(cond, name) do { if (!(cond)) { errors++; printf("FAIL: %s\n", name); } else { printf("ok   %s\n", name); } } while (0)

#define UP    KEYPAD_BIT(0, 1)
#define LEFT  KEYPAD_BIT(1, 0)
#define FIRE  KEYPAD_BIT(1, 1)
#define RIGHT KEYPAD_BIT(1, 2)
#define DOWN  KEYPAD_BIT(2, 1)

int main(void)
{
    Dir held = DIR_NONE;
    uint16_t prev = 0;
    Input in;

    in = Input_Decode(0, prev, &held);
    CHECK(in.dir == DIR_NONE && in.fire == 0, "no keys -> idle");

    in = Input_Decode(UP, prev, &held); prev = UP;
    CHECK(in.dir == DIR_UP && !in.fire, "key 2 -> up");

    in = Input_Decode(UP | FIRE, prev, &held); prev = UP | FIRE;
    CHECK(in.dir == DIR_UP && in.fire, "up + fire together (different rows)");

    in = Input_Decode(UP | FIRE | LEFT, prev, &held); prev = UP | FIRE | LEFT;
    CHECK(in.dir == DIR_LEFT && in.fire, "newly pressed left wins over held up; fire in the same row still seen");

    in = Input_Decode(UP | FIRE, prev, &held); prev = UP | FIRE;
    CHECK(in.dir == DIR_UP && in.fire, "left released -> back to still-held up");

    in = Input_Decode(FIRE, prev, &held); prev = FIRE;
    CHECK(in.dir == DIR_NONE && in.fire, "arrows released, fire held");

    in = Input_Decode(RIGHT | FIRE, prev, &held); prev = RIGHT | FIRE;
    CHECK(in.dir == DIR_RIGHT && in.fire, "5 and 6 in the same row work together");

    in = Input_Decode(0, prev, &held); prev = 0;
    CHECK(in.dir == DIR_NONE && !in.fire && held == DIR_NONE, "all released");

    in = Input_Decode(DOWN, prev, &held); prev = DOWN;
    CHECK(in.dir == DIR_DOWN, "key 8 -> down");

    in = Input_Decode(DOWN | UP, DOWN, &held);
    CHECK(in.dir == DIR_UP, "second arrow pressed later takes over");

    printf("\n%s\n", errors ? "FAILED" : "ALL PASSED");
    return errors != 0;
}
