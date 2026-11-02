#ifndef TANKS_H_
#define TANKS_H_

#include <stdint.h>

/* 0 */
#define TILE_SIZE            8
#define MAP_W                14
#define MAP_H                8
#define FIELD_W_PX           (MAP_W * TILE_SIZE)
#define FIELD_H_PX           (MAP_H * TILE_SIZE)

/* 1 */
#define MAX_ENEMIES_ON_FIELD 3
#define TANK_COUNT           (1 + MAX_ENEMIES_ON_FIELD)   /* танк 0 - игрок */
#define PLAYER_INDEX         0
#define ENEMIES_TOTAL        8
#define PLAYER_LIVES         3
#define SCORE_PER_ENEMY      100

#define TANK_SPEED_PX        2
#define ENEMY_MOVE_PERIOD    2                      /* враг двигается раз в N тактов */
#define BULLET_SPEED_PX      4
#define PLAYER_COOLDOWN      6
#define ENEMY_COOLDOWN       20
#define ENEMY_FIRE_PERCENT   4
#define ENEMY_KEEP_DIR_PERCENT 70
#define SPAWN_INTERVAL       40
#define RESPAWN_DELAY        20

/* 2 */
#define EV_PLAYER_FIRE       0x01
#define EV_ENEMY_DESTROYED   0x02
#define EV_PLAYER_DESTROYED  0x04
#define EV_BASE_DESTROYED    0x08
#define EV_WIN               0x10
#define EV_BRICK_HIT         0x20

typedef enum { TILE_EMPTY = 0, TILE_BRICK, TILE_STEEL, TILE_BASE } Tile;
typedef enum { DIR_UP = 0, DIR_RIGHT, DIR_DOWN, DIR_LEFT, DIR_NONE } Dir;
typedef enum { PHASE_PLAYING = 0, PHASE_WON, PHASE_LOST } Phase;

typedef struct {
    int8_t  tx, ty;        /* клетка, в которой танк (или из которой выезжает) */
    Dir     dir;
    uint8_t alive;
    uint8_t moving;
    uint8_t progress;      /* сколько пикселей уже проехал (0..TILE_SIZE) */
    uint8_t cooldown;
} Tank;

typedef struct {
    int16_t x, y;
    Dir     dir;
    uint8_t active;
} Bullet;

typedef struct {
    Dir     dir;           /* какую кнопку направления держат (DIR_NONE - никакую) */
    uint8_t fire;
} Input;

typedef struct {
    Tile    map[MAP_H][MAP_W];
    Tank    tanks[TANK_COUNT];
    Bullet  bullets[TANK_COUNT];       /* пуля i принадлежит танку i */
    Input   input;
    Phase   phase;
    uint8_t lives;
    uint8_t enemies_to_spawn;
    uint8_t enemies_killed;
    uint16_t score;
    uint16_t spawn_timer;
    uint16_t respawn_timer;
    uint8_t next_spawn_point;
    uint32_t tick;
    uint32_t rng;
    uint8_t events;                    /* накопленные события, сбрасывает потребитель */
} Game;

void Game_Init(Game *g, uint32_t seed);
void Game_Tick(Game *g);               /* один такт игры (вызывать каждые 50 мс) */

/* пиксельные координаты танка (с учётом движения) */
int16_t Tank_PixelX(const Tank *t);
int16_t Tank_PixelY(const Tank *t);

#endif /* TANKS_H_ */
