#include "main.h"
#include "oled.h"
#include "fonts.h"
#include "game_draw.h"

#define SPRITE_SIZE      TILE_SIZE
#define HUD_X            (FIELD_W_PX + 2)
#define HUD_LINE_ENEMIES 2
#define HUD_LINE_LIVES   14
#define HUD_LINE_KILLS   26
#define BULLET_DRAW_SIZE 2
#define BODY_FIRST_COL   2                    /* середина танка рисуется с шашечкой у врагов */
#define BODY_LAST_COL    5

#define MSG_BOX_X1       8
#define MSG_BOX_X2       103
#define MSG_BOX_Y1       20
#define MSG_BOX_Y2       43

static const uint8_t SPRITE_TANK_UP[SPRITE_SIZE] = { 0x18, 0xDB, 0xFF, 0xE7, 0xE7, 0xFF, 0xFF, 0xC3 };
static const uint8_t SPRITE_BRICK[SPRITE_SIZE]   = { 0xEE, 0xEE, 0x00, 0x77, 0x77, 0x00, 0xEE, 0xEE };
static const uint8_t SPRITE_STEEL[SPRITE_SIZE]   = { 0xFF, 0x81, 0xBD, 0xA5, 0xA5, 0xBD, 0x81, 0xFF };
static const uint8_t SPRITE_BASE[SPRITE_SIZE]    = { 0x18, 0x3C, 0x7E, 0xFF, 0x7E, 0x3C, 0x24, 0x66 };

static uint8_t SpriteBit(const uint8_t *sprite, int col, int row)
{
    return (sprite[row] >> (SPRITE_SIZE - 1 - col)) & 1u;
}

static void DrawSprite(const uint8_t *sprite, int x, int y)
{
    for (int row = 0; row < SPRITE_SIZE; row++) {
        for (int col = 0; col < SPRITE_SIZE; col++) {
            if (SpriteBit(sprite, col, row)) {
                oled_DrawPixel((uint8_t)(x + col), (uint8_t)(y + row), White);
            }
        }
    }
}

/* Танк поворачиваем «на лету»: берём пиксель исходного спрайта (ствол вверх) с нужными координатами. */
static void DrawTank(const Tank *t, uint8_t is_enemy)
{
    int x = Tank_PixelX(t);
    int y = Tank_PixelY(t);

    for (int row = 0; row < SPRITE_SIZE; row++) {
        for (int col = 0; col < SPRITE_SIZE; col++) {
            int u = col, v = row;
            switch (t->dir) {
            case DIR_DOWN:  v = SPRITE_SIZE - 1 - row; break;
            case DIR_LEFT:  u = row; v = col; break;
            case DIR_RIGHT: u = row; v = SPRITE_SIZE - 1 - col; break;
            default: break;
            }
            if (!SpriteBit(SPRITE_TANK_UP, u, v)) {
                continue;
            }
            /* у врага середина «в шашечку», чтобы отличать от игрока */
            if (is_enemy && u >= BODY_FIRST_COL && u <= BODY_LAST_COL && ((col + row) & 1)) {
                continue;
            }
            oled_DrawPixel((uint8_t)(x + col), (uint8_t)(y + row), White);
        }
    }
}

static void DrawNumber2(int value, int x, int y)
{
    char text[3];
    if (value > 99) value = 99;
    text[0] = (char)('0' + value / 10);
    text[1] = (char)('0' + value % 10);
    text[2] = '\0';
    oled_SetCursor((uint8_t)x, (uint8_t)y);
    oled_WriteString(text, Font_7x10, White);
}

static void DrawHud(const Game *g)
{
    int enemies_left = g->enemies_to_spawn;
    for (int i = 1; i < TANK_COUNT; i++) {
        enemies_left += g->tanks[i].alive;
    }

    for (int y = 0; y < FIELD_H_PX; y++) {
        oled_DrawPixel(FIELD_W_PX, (uint8_t)y, White);
    }
    DrawNumber2(enemies_left, HUD_X, HUD_LINE_ENEMIES);
    DrawNumber2(g->lives, HUD_X, HUD_LINE_LIVES);
    DrawNumber2(g->enemies_killed, HUD_X, HUD_LINE_KILLS);
}

static void DrawMessage(const char *title, const char *hint)
{
    for (int y = MSG_BOX_Y1; y <= MSG_BOX_Y2; y++) {
        for (int x = MSG_BOX_X1; x <= MSG_BOX_X2; x++) {
            int border = (x == MSG_BOX_X1 || x == MSG_BOX_X2 || y == MSG_BOX_Y1 || y == MSG_BOX_Y2);
            oled_DrawPixel((uint8_t)x, (uint8_t)y, border ? White : Black);
        }
    }
    oled_SetCursor(MSG_BOX_X1 + 16, MSG_BOX_Y1 + 3);
    oled_WriteString((char *)title, Font_7x10, White);
    oled_SetCursor(MSG_BOX_X1 + 16, MSG_BOX_Y1 + 12);
    oled_WriteString((char *)hint, Font_7x10, White);
}

void Draw_Game(const Game *g)
{
    oled_Fill(Black);

    for (int ty = 0; ty < MAP_H; ty++) {
        for (int tx = 0; tx < MAP_W; tx++) {
            switch (g->map[ty][tx]) {
            case TILE_BRICK: DrawSprite(SPRITE_BRICK, tx * TILE_SIZE, ty * TILE_SIZE); break;
            case TILE_STEEL: DrawSprite(SPRITE_STEEL, tx * TILE_SIZE, ty * TILE_SIZE); break;
            case TILE_BASE:  DrawSprite(SPRITE_BASE,  tx * TILE_SIZE, ty * TILE_SIZE); break;
            default: break;
            }
        }
    }

    for (int i = 0; i < TANK_COUNT; i++) {
        if (g->tanks[i].alive) {
            DrawTank(&g->tanks[i], i != PLAYER_INDEX);
        }
        if (g->bullets[i].active) {
            for (int dy = 0; dy < BULLET_DRAW_SIZE; dy++) {
                for (int dx = 0; dx < BULLET_DRAW_SIZE; dx++) {
                    oled_DrawPixel((uint8_t)(g->bullets[i].x + dx), (uint8_t)(g->bullets[i].y + dy), White);
                }
            }
        }
    }

    DrawHud(g);

    if (g->phase == PHASE_WON) {
        DrawMessage("YOU WIN!", "5: again");
    } else if (g->phase == PHASE_LOST) {
        DrawMessage("GAME OVER", "5: again");
    }
}
