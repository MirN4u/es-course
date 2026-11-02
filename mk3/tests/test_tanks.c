#include <stdio.h>
#include <string.h>
#include "../Core/Src/tanks.c"

static int errors = 0;
static int checks = 0;

#define CHECK(cond, name) do { checks++; if (!(cond)) { errors++; printf("FAIL: %s (line %d)\n", name, __LINE__); } else { printf("ok   %s\n", name); } } while (0)

static void Run(Game *g, int ticks) { for (int i = 0; i < ticks; i++) Game_Tick(g); }

static void ClearMap(Game *g) { memset(g->map, 0, sizeof(g->map)); }

/* врагов нет и появляться не будут, но победа не засчитывается */
static void NoEnemies(Game *g) { g->enemies_to_spawn = 1; g->spawn_timer = 60000; }

static void PlaceEnemy(Game *g, int idx, int tx, int ty, Dir dir)
{
    Tank *t = &g->tanks[idx];
    memset(t, 0, sizeof(*t));
    t->tx = (int8_t)tx; t->ty = (int8_t)ty; t->dir = dir; t->alive = 1;
    t->cooldown = 100;               /* чтобы не стрелял в тесте */
}

static void PlacePlayer(Game *g, int tx, int ty, Dir dir)
{
    Tank *t = &g->tanks[PLAYER_INDEX];
    memset(t, 0, sizeof(*t));
    t->tx = (int8_t)tx; t->ty = (int8_t)ty; t->dir = dir; t->alive = 1;
}

/* ---- проверки уровня ---- */
static void TestLevel(void)
{
    int ok_len = 1;
    for (int y = 0; y < MAP_H; y++) {
        if ((int)strlen(LEVEL[y]) != MAP_W) ok_len = 0;
    }
    CHECK(ok_len, "every level row has exactly MAP_W chars");

    Game g;
    Game_Init(&g, 1);
    int bases = 0;
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) if (g.map[y][x] == TILE_BASE) bases++;
    CHECK(bases == 1, "exactly one base on the map");
    CHECK(g.map[PLAYER_SPAWN_TY][PLAYER_SPAWN_TX] == TILE_EMPTY, "player spawn tile is empty");
    int spawns_free = 1;
    for (int i = 0; i < SPAWN_POINTS; i++) if (g.map[0][SPAWN_TX[i]] != TILE_EMPTY) spawns_free = 0;
    CHECK(spawns_free, "enemy spawn tiles are empty");

    /* поиск в ширину от игрока: кирпич проходим (его можно расстрелять), броня и база нет */
    int seen[MAP_H][MAP_W];
    memset(seen, 0, sizeof(seen));
    int qx[MAP_W * MAP_H], qy[MAP_W * MAP_H], head = 0, tail = 0;
    qx[tail] = PLAYER_SPAWN_TX; qy[tail] = PLAYER_SPAWN_TY; tail++;
    seen[PLAYER_SPAWN_TY][PLAYER_SPAWN_TX] = 1;
    while (head < tail) {
        int x = qx[head], y = qy[head]; head++;
        for (int d = 0; d < DIR_COUNT; d++) {
            int nx = x + DIR_DX[d], ny = y + DIR_DY[d];
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H || seen[ny][nx]) continue;
            if (g.map[ny][nx] == TILE_STEEL || g.map[ny][nx] == TILE_BASE) continue;
            seen[ny][nx] = 1; qx[tail] = nx; qy[tail] = ny; tail++;
        }
    }
    int reach = 1;
    for (int i = 0; i < SPAWN_POINTS; i++) if (!seen[0][SPAWN_TX[i]]) reach = 0;
    CHECK(reach, "all enemy spawns reachable from player (bricks passable)");
}

/* ---- движение ---- */
static void TestMovement(void)
{
    Game g;
    Game_Init(&g, 1);
    NoEnemies(&g);
    g.input.dir = DIR_UP;
    Run(&g, 4);
    CHECK(g.tanks[0].ty == PLAYER_SPAWN_TY - 1 && !g.tanks[0].moving, "one tile takes 4 ticks (2 px per tick)");
    Run(&g, 40);
    CHECK(g.tanks[0].ty == 6, "steel tile stops the tank");

    Game_Init(&g, 1);
    NoEnemies(&g);
    g.input.dir = DIR_LEFT;
    Run(&g, 60);
    CHECK(g.tanks[0].tx == 0, "tank stops at left border");
    g.input.dir = DIR_DOWN;
    Run(&g, 60);
    CHECK(g.tanks[0].ty == MAP_H - 1, "tank stops at bottom border");
    g.input.dir = DIR_RIGHT;
    Run(&g, 100);
    CHECK(g.tanks[0].tx == 4, "brick wall in front of base stops the tank (tile 5,7 is brick)");

    Game_Init(&g, 1);
    NoEnemies(&g);
    PlaceEnemy(&g, 1, 3, 6, DIR_DOWN);
    g.input.dir = DIR_UP;
    Run(&g, 10);
    CHECK(g.tanks[0].ty == 7 && !g.tanks[0].moving, "tank cannot drive into another tank");

    Game_Init(&g, 1);
    NoEnemies(&g);
    g.input.dir = DIR_UP; Run(&g, 2);
    CHECK(g.tanks[0].moving && Tank_PixelY(&g.tanks[0]) == PLAYER_SPAWN_TY * TILE_SIZE - 2 * TANK_SPEED_PX, "pixel position while moving");
}

/* ---- пули ---- */
static void TestBullets(void)
{
    Game g;
    Game_Init(&g, 1);
    NoEnemies(&g);
    PlacePlayer(&g, 4, 6, DIR_RIGHT);
    g.input.dir = DIR_NONE; g.input.fire = 1;
    uint8_t seen_events = 0;
    for (int i = 0; i < 10; i++) { Game_Tick(&g); seen_events |= g.events; g.events = 0; }
    CHECK(g.map[6][5] == TILE_EMPTY, "bullet destroys a brick");
    CHECK(!g.bullets[0].active || g.bullets[0].x > 5 * TILE_SIZE, "bullet disappears after hitting brick");
    CHECK((seen_events & EV_PLAYER_FIRE) && (seen_events & EV_BRICK_HIT), "fire and brick-hit events raised");

    Game_Init(&g, 1);
    NoEnemies(&g);
    PlacePlayer(&g, 1, 5, DIR_RIGHT);
    g.input.fire = 1;
    Run(&g, 15);
    CHECK(g.map[5][2] == TILE_STEEL, "bullet does not destroy steel");

    Game_Init(&g, 1);
    ClearMap(&g);
    g.enemies_to_spawn = 0;
    PlacePlayer(&g, 2, 2, DIR_RIGHT);
    PlaceEnemy(&g, 1, 8, 2, DIR_LEFT);
    g.input.fire = 1;
    for (int i = 0; i < 20; i++) Game_Tick(&g);
    CHECK(!g.tanks[1].alive && g.enemies_killed == 1 && g.score == SCORE_PER_ENEMY, "bullet kills enemy, score +100");
    CHECK(g.phase == PHASE_WON, "last enemy killed -> WON");
    CHECK(g.events & EV_WIN, "win event raised");

    Game_Init(&g, 1);
    ClearMap(&g);
    g.enemies_to_spawn = 0;
    PlacePlayer(&g, 2, 2, DIR_RIGHT);
    PlaceEnemy(&g, 1, 8, 2, DIR_LEFT);
    g.bullets[1].active = 1; g.bullets[1].dir = DIR_LEFT; g.bullets[1].x = 8 * TILE_SIZE; g.bullets[1].y = 2 * TILE_SIZE + 4;
    Run(&g, 20);
    CHECK(!g.tanks[0].alive && g.lives == PLAYER_LIVES - 1, "enemy bullet kills player, one life lost");
    CHECK(g.events & EV_PLAYER_DESTROYED, "player destroyed event raised");
    g.enemies_to_spawn = 1;
    Run(&g, RESPAWN_DELAY + 3);
    CHECK(g.tanks[0].alive && g.tanks[0].tx == PLAYER_SPAWN_TX && g.tanks[0].ty == PLAYER_SPAWN_TY, "player respawns at spawn point");

    Game_Init(&g, 1);
    g.enemies_to_spawn = 0;
    g.map[6][6] = TILE_EMPTY;
    g.bullets[1].active = 1; g.bullets[1].dir = DIR_DOWN; g.bullets[1].x = 6 * TILE_SIZE + 4; g.bullets[1].y = 6 * TILE_SIZE;
    PlaceEnemy(&g, 1, 0, 0, DIR_DOWN);
    Run(&g, 6);
    CHECK(g.phase == PHASE_LOST && (g.events & EV_BASE_DESTROYED), "bullet hits base -> LOST");

    Game_Init(&g, 1);
    g.enemies_to_spawn = 0;
    g.lives = 1;
    PlacePlayer(&g, 2, 2, DIR_RIGHT);
    ClearMap(&g);
    PlaceEnemy(&g, 1, 8, 2, DIR_LEFT);
    g.bullets[1].active = 1; g.bullets[1].dir = DIR_LEFT; g.bullets[1].x = 8 * TILE_SIZE; g.bullets[1].y = 2 * TILE_SIZE + 4;
    Run(&g, 20);
    CHECK(g.lives == 0 && g.phase == PHASE_LOST, "no lives left -> LOST");
    int tick_before = (int)g.tick;
    Run(&g, 10);
    CHECK((int)g.tick == tick_before, "game stops after the end");
}

static void TestFireRate(void)
{
    Game g;
    Game_Init(&g, 1);
    NoEnemies(&g);
    ClearMap(&g);
    g.input.fire = 1;
    PlacePlayer(&g, 0, 0, DIR_RIGHT);
    int shots = 0, max_in_flight = 0, last_shot = -1000, spacing_ok = 1;
    for (int i = 0; i < 100; i++) {
        Game_Tick(&g);
        if (g.events & EV_PLAYER_FIRE) { shots++; if (i - last_shot < PLAYER_COOLDOWN) spacing_ok = 0; last_shot = i; }
        g.events = 0;
        if (g.bullets[0].active) max_in_flight = 1;
    }
    CHECK(shots >= 3, "holding fire gives repeated shots");
    CHECK(max_in_flight == 1 && spacing_ok, "one bullet at a time and cooldown respected");
}

static void TestRespawnBlocked(void)
{
    Game g;
    Game_Init(&g, 1);
    ClearMap(&g);
    g.enemies_to_spawn = 0;
    PlacePlayer(&g, 2, 2, DIR_RIGHT);
    PlaceEnemy(&g, 1, 8, 2, DIR_LEFT);
    g.bullets[1].active = 1; g.bullets[1].dir = DIR_LEFT; g.bullets[1].x = 8 * TILE_SIZE; g.bullets[1].y = 2 * TILE_SIZE + 4;
    Run(&g, 20);
    g.map[PLAYER_SPAWN_TY][PLAYER_SPAWN_TX - 1] = TILE_STEEL;
    g.map[PLAYER_SPAWN_TY][PLAYER_SPAWN_TX + 1] = TILE_STEEL;
    g.map[PLAYER_SPAWN_TY - 1][PLAYER_SPAWN_TX] = TILE_STEEL;
    PlaceEnemy(&g, 2, PLAYER_SPAWN_TX, PLAYER_SPAWN_TY, DIR_UP);
    g.tanks[2].cooldown = 100;
    g.tanks[1].alive = 0;
    g.enemies_to_spawn = 5;
    g.spawn_timer = 1000;
    Run(&g, RESPAWN_DELAY + 20);
    CHECK(!g.tanks[0].alive, "player does not respawn into an occupied tile");
}

/* ---- случайные прогоны с проверкой инвариантов ---- */
static int Invariants(const Game *g)
{
    int alive_enemies = 0;
    for (int i = 0; i < TANK_COUNT; i++) {
        const Tank *t = &g->tanks[i];
        if (!t->alive) continue;
        if (i > 0) alive_enemies++;
        if (t->tx < 0 || t->tx >= MAP_W || t->ty < 0 || t->ty >= MAP_H) return 1;
        if (g->map[t->ty][t->tx] != TILE_EMPTY) return 2;
        int dx = t->tx, dy = t->ty;
        if (t->moving) {
            dx += DIR_DX[t->dir]; dy += DIR_DY[t->dir];
            if (dx < 0 || dx >= MAP_W || dy < 0 || dy >= MAP_H) return 3;
            if (g->map[dy][dx] != TILE_EMPTY) return 4;
        }
        for (int j = i + 1; j < TANK_COUNT; j++) {
            const Tank *u = &g->tanks[j];
            if (!u->alive) continue;
            int ex = u->tx, ey = u->ty;
            if (u->moving) { ex += DIR_DX[u->dir]; ey += DIR_DY[u->dir]; }
            if (t->tx == u->tx && t->ty == u->ty) return 5;
            if (dx == ex && dy == ey) return 6;
        }
        int px = Tank_PixelX(t), py = Tank_PixelY(t);
        if (px < 0 || px + TILE_SIZE > FIELD_W_PX || py < 0 || py + TILE_SIZE > FIELD_H_PX) return 7;
    }
    if (alive_enemies > MAX_ENEMIES_ON_FIELD) return 8;
    if (g->enemies_killed + alive_enemies + g->enemies_to_spawn != ENEMIES_TOTAL) return 9;
    if (g->lives > PLAYER_LIVES) return 10;
    for (int i = 0; i < TANK_COUNT; i++) {
        if (g->bullets[i].active && (g->bullets[i].x < 0 || g->bullets[i].x >= FIELD_W_PX ||
                                     g->bullets[i].y < 0 || g->bullets[i].y >= FIELD_H_PX)) return 11;
    }
    return 0;
}

static void TestFuzz(void)
{
    int bad_seed = 0, bad_code = 0, won = 0, lost = 0, playing = 0;
    for (int seed = 1; seed <= 300; seed++) {
        Game g;
        Game_Init(&g, (uint32_t)seed * 7919u);
        uint32_t rnd = (uint32_t)seed;
        for (int t = 0; t < 4000 && g.phase == PHASE_PLAYING; t++) {
            if (t % 8 == 0) {
                rnd = rnd * 1103515245u + 12345u;
                g.input.dir = (Dir)((rnd >> 16) % 5);
                g.input.fire = (rnd >> 20) & 1;
            }
            Game_Tick(&g);
            g.events = 0;
            int code = Invariants(&g);
            if (code && !bad_seed) { bad_seed = seed; bad_code = code; }
        }
        if (g.phase == PHASE_WON) won++; else if (g.phase == PHASE_LOST) lost++; else playing++;
    }
    CHECK(bad_seed == 0, "invariants hold in 300 random games x 4000 ticks");
    if (bad_seed) printf("   first violation: seed %d, code %d\n", bad_seed, bad_code);
    printf("   outcomes: won %d, lost %d, still playing %d\n", won, lost, playing);
    CHECK(won + lost > 0, "random games reach an end sometimes");
}

/* информационно: сколько живёт база, если игрок ничего не делает */
static void IdleStats(void)
{
    long total = 0; int min_ticks = 100000;
    for (int seed = 1; seed <= 300; seed++) {
        Game g;
        Game_Init(&g, (uint32_t)seed * 7919u);
        int t = 0;
        while (g.phase == PHASE_PLAYING && t < 20000) { Game_Tick(&g); g.events = 0; t++; }
        total += t; if (t < min_ticks) min_ticks = t;
    }
    printf("   idle player: average game length %ld ticks (%.1f s at 50 ms), shortest %d ticks\n", total / 300, total / 300 * 0.05, min_ticks);
}

int main(void)
{
    TestLevel();
    TestMovement();
    TestBullets();
    TestFireRate();
    TestRespawnBlocked();
    TestFuzz();
    IdleStats();
    printf("\n%s: %d checks, %d errors\n", errors ? "FAILED" : "ALL PASSED", checks, errors);
    return errors != 0;
}
