#include "tanks.h"

#define SPAWN_POINTS         3
#define PLAYER_SPAWN_TX      3
#define PLAYER_SPAWN_TY      7
#define DIR_COUNT            4
#define PERCENT              100
#define BULLET_START_OFFSET  (TILE_SIZE / 2)   /* пуля вылетает из центра танка */

/* Карта: . пусто, B кирпич, S броня, E база (орёл). Строки сверху вниз. */
static const char LEVEL[MAP_H][MAP_W + 1] = {
    "..............",
    ".B..B......B.B",
    ".B.SS.BB.SS.B.",
    "..BB........BB",
    ".B..B..BB..B.B",
    "..SS..BB..SS..",
    ".....BBB......",
    ".....BEB......"
};

static const int8_t SPAWN_TX[SPAWN_POINTS] = { 0, 6, MAP_W - 1 };
static const int8_t DIR_DX[DIR_COUNT] = { 0, 1, 0, -1 };
static const int8_t DIR_DY[DIR_COUNT] = { -1, 0, 1, 0 };

/* 0 */

static uint32_t NextRandom(Game *g)
{
    g->rng = g->rng * 1664525u + 1013904223u;
    return g->rng >> 16;
}

int16_t Tank_PixelX(const Tank *t)
{
    int16_t x = (int16_t)(t->tx * TILE_SIZE);
    if (t->moving) {
        x = (int16_t)(x + DIR_DX[t->dir] * t->progress);
    }
    return x;
}

int16_t Tank_PixelY(const Tank *t)
{
    int16_t y = (int16_t)(t->ty * TILE_SIZE);
    if (t->moving) {
        y = (int16_t)(y + DIR_DY[t->dir] * t->progress);
    }
    return y;
}

static uint8_t TileFree(const Game *g, int tx, int ty)
{
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) {
        return 0;
    }
    return g->map[ty][tx] == TILE_EMPTY;
}

/* Занята ли клетка каким-нибудь танком (кроме танка skip), учитывая клетку назначения. */
static uint8_t TileHasTank(const Game *g, int tx, int ty, int skip)
{
    for (int i = 0; i < TANK_COUNT; i++) {
        const Tank *t = &g->tanks[i];
        if (i == skip || !t->alive) {
            continue;
        }
        if (t->tx == tx && t->ty == ty) {
            return 1;
        }
        if (t->moving && t->tx + DIR_DX[t->dir] == tx && t->ty + DIR_DY[t->dir] == ty) {
            return 1;
        }
    }
    return 0;
}

/* 1 */

/* wanted - куда хочет ехать танк; speed - пикселей за такт */
static void TankStep(Game *g, int i, Dir wanted, int speed)
{
    Tank *t = &g->tanks[i];

    if (!t->moving && wanted != DIR_NONE) {
        t->dir = wanted;
        int nx = t->tx + DIR_DX[wanted];
        int ny = t->ty + DIR_DY[wanted];
        if (TileFree(g, nx, ny) && !TileHasTank(g, nx, ny, i)) {
            t->moving = 1;
            t->progress = 0;
        }
    }
    if (t->moving) {
        t->progress = (uint8_t)(t->progress + speed);
        if (t->progress >= TILE_SIZE) {
            t->tx = (int8_t)(t->tx + DIR_DX[t->dir]);
            t->ty = (int8_t)(t->ty + DIR_DY[t->dir]);
            t->moving = 0;
            t->progress = 0;
        }
    }
}

/* 2 */

/* Возвращает 1, если выстрел состоялся (одна пуля на танк, плюс перезарядка). */
static uint8_t Fire(Game *g, int i, uint8_t cooldown)
{
    Tank *t = &g->tanks[i];
    Bullet *b = &g->bullets[i];

    if (b->active || t->cooldown > 0) {
        return 0;
    }
    b->active = 1;
    b->dir = t->dir;
    b->x = (int16_t)(Tank_PixelX(t) + BULLET_START_OFFSET);
    b->y = (int16_t)(Tank_PixelY(t) + BULLET_START_OFFSET);
    t->cooldown = cooldown;
    return 1;
}

static uint8_t PointInTank(const Tank *t, int x, int y)
{
    int tx = Tank_PixelX(t);
    int ty = Tank_PixelY(t);
    return x >= tx && x < tx + TILE_SIZE && y >= ty && y < ty + TILE_SIZE;
}

static void KillEnemy(Game *g, int k)
{
    g->tanks[k].alive = 0;
    g->enemies_killed++;
    g->score = (uint16_t)(g->score + SCORE_PER_ENEMY);
    g->events |= EV_ENEMY_DESTROYED;
}

static void KillPlayer(Game *g)
{
    Tank *p = &g->tanks[PLAYER_INDEX];
    p->alive = 0;
    p->moving = 0;
    g->events |= EV_PLAYER_DESTROYED;
    if (g->lives > 0) {
        g->lives--;
    }
    if (g->lives == 0) {
        g->phase = PHASE_LOST;
    } else {
        g->respawn_timer = RESPAWN_DELAY;
    }
}

/* Один шаг пули на 1 пиксель; возвращает 1, если пуля исчезла. */
static uint8_t BulletPixelStep(Game *g, int i)
{
    Bullet *b = &g->bullets[i];
    b->x = (int16_t)(b->x + DIR_DX[b->dir]);
    b->y = (int16_t)(b->y + DIR_DY[b->dir]);

    if (b->x < 0 || b->x >= FIELD_W_PX || b->y < 0 || b->y >= FIELD_H_PX) {
        return 1;
    }

    int tx = b->x / TILE_SIZE;
    int ty = b->y / TILE_SIZE;
    Tile tile = g->map[ty][tx];
    if (tile == TILE_BRICK) {
        g->map[ty][tx] = TILE_EMPTY;
        g->events |= EV_BRICK_HIT;
        return 1;
    }
    if (tile == TILE_STEEL) {
        return 1;
    }
    if (tile == TILE_BASE) {
        g->map[ty][tx] = TILE_EMPTY;
        g->events |= EV_BASE_DESTROYED;
        g->phase = PHASE_LOST;
        return 1;
    }

    if (i == PLAYER_INDEX) {
        for (int k = 1; k < TANK_COUNT; k++) {
            if (g->tanks[k].alive && PointInTank(&g->tanks[k], b->x, b->y)) {
                KillEnemy(g, k);
                return 1;
            }
        }
    } else {
        if (g->tanks[PLAYER_INDEX].alive && PointInTank(&g->tanks[PLAYER_INDEX], b->x, b->y)) {
            KillPlayer(g);
            return 1;
        }
    }
    return 0;
}

static void BulletsStep(Game *g)
{
    for (int i = 0; i < TANK_COUNT; i++) {
        if (!g->bullets[i].active) {
            continue;
        }
        for (int s = 0; s < BULLET_SPEED_PX; s++) {
            if (BulletPixelStep(g, i)) {
                g->bullets[i].active = 0;
                break;
            }
            if (g->phase != PHASE_PLAYING) {
                break;
            }
        }
    }
}

/* 3 */

static Dir RandomDir(Game *g)
{
    uint32_t r = NextRandom(g) % 8;
    if (r < 3) return DIR_DOWN;
    if (r < 5) return DIR_LEFT;
    if (r < 7) return DIR_RIGHT;
    return DIR_UP;
}

static void EnemyStep(Game *g, int i)
{
    Tank *t = &g->tanks[i];

    if (g->tick % ENEMY_MOVE_PERIOD == 0) {
        Dir wanted = t->dir;
        if (!t->moving) {
            /* в клетке решаем: ехать дальше или выбрать новое направление */
            int nx = t->tx + DIR_DX[t->dir];
            int ny = t->ty + DIR_DY[t->dir];
            uint8_t can_go = TileFree(g, nx, ny) && !TileHasTank(g, nx, ny, i);
            if (!can_go || (NextRandom(g) % PERCENT) >= ENEMY_KEEP_DIR_PERCENT) {
                wanted = RandomDir(g);
            }
        }
        TankStep(g, i, wanted, TANK_SPEED_PX);
    }

    if ((NextRandom(g) % PERCENT) < ENEMY_FIRE_PERCENT) {
        Fire(g, i, ENEMY_COOLDOWN);
    }
}

static void SpawnEnemies(Game *g)
{
    if (g->enemies_to_spawn == 0) {
        return;
    }
    if (g->spawn_timer > 0) {
        g->spawn_timer--;
        return;
    }
    for (int i = 1; i < TANK_COUNT; i++) {
        if (g->tanks[i].alive) {
            continue;
        }
        int tx = SPAWN_TX[g->next_spawn_point];
        if (TileHasTank(g, tx, 0, -1)) {
            return;   /* точка занята - попробуем в следующем такте */
        }
        Tank *t = &g->tanks[i];
        t->tx = (int8_t)tx;
        t->ty = 0;
        t->dir = DIR_DOWN;
        t->alive = 1;
        t->moving = 0;
        t->progress = 0;
        t->cooldown = ENEMY_COOLDOWN;
        g->bullets[i].active = 0;
        g->next_spawn_point = (uint8_t)((g->next_spawn_point + 1) % SPAWN_POINTS);
        g->enemies_to_spawn--;
        g->spawn_timer = SPAWN_INTERVAL;
        return;
    }
}

/* 4 */

static void PlayerSpawn(Game *g)
{
    Tank *p = &g->tanks[PLAYER_INDEX];
    if (TileHasTank(g, PLAYER_SPAWN_TX, PLAYER_SPAWN_TY, PLAYER_INDEX)) {
        return;   /* место занято врагом - ждём */
    }
    p->tx = PLAYER_SPAWN_TX;
    p->ty = PLAYER_SPAWN_TY;
    p->dir = DIR_UP;
    p->alive = 1;
    p->moving = 0;
    p->progress = 0;
    p->cooldown = 0;
    g->bullets[PLAYER_INDEX].active = 0;
    g->respawn_timer = 0;
}

static void PlayerStep(Game *g)
{
    Tank *p = &g->tanks[PLAYER_INDEX];

    if (!p->alive) {
        if (g->respawn_timer > 0) {
            g->respawn_timer--;
        } else if (g->phase == PHASE_PLAYING) {
            PlayerSpawn(g);
        }
        return;
    }
    TankStep(g, PLAYER_INDEX, g->input.dir, TANK_SPEED_PX);
    if (g->input.fire && Fire(g, PLAYER_INDEX, PLAYER_COOLDOWN)) {
        g->events |= EV_PLAYER_FIRE;
    }
}

/* 5 */

void Game_Init(Game *g, uint32_t seed)
{
    uint8_t *raw = (uint8_t *)g;
    for (unsigned i = 0; i < sizeof(Game); i++) {
        raw[i] = 0;
    }
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            char c = LEVEL[y][x];
            g->map[y][x] = (c == 'B') ? TILE_BRICK : (c == 'S') ? TILE_STEEL : (c == 'E') ? TILE_BASE : TILE_EMPTY;
        }
    }
    g->rng = seed;
    g->phase = PHASE_PLAYING;
    g->lives = PLAYER_LIVES;
    g->enemies_to_spawn = ENEMIES_TOTAL;
    g->input.dir = DIR_NONE;
    PlayerSpawn(g);
}

void Game_Tick(Game *g)
{
    if (g->phase != PHASE_PLAYING) {
        return;
    }
    g->tick++;

    PlayerStep(g);
    for (int i = 1; i < TANK_COUNT; i++) {
        if (g->tanks[i].alive) {
            EnemyStep(g, i);
        }
    }
    BulletsStep(g);
    SpawnEnemies(g);

    for (int i = 0; i < TANK_COUNT; i++) {
        if (g->tanks[i].cooldown > 0) {
            g->tanks[i].cooldown--;
        }
    }

    if (g->phase == PHASE_PLAYING && g->enemies_to_spawn == 0) {
        uint8_t any_alive = 0;
        for (int i = 1; i < TANK_COUNT; i++) {
            any_alive |= g->tanks[i].alive;
        }
        if (!any_alive) {
            g->phase = PHASE_WON;
            g->events |= EV_WIN;
        }
    }
}
