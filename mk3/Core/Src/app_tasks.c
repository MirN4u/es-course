#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "oled.h"
#include "buzzer.h"
#include "keypad.h"
#include "input_decode.h"
#include "tanks.h"
#include "game_draw.h"
#include "app_tasks.h"

/* 0 */
#define INPUT_PERIOD_MS      30
#define GAME_PERIOD_MS       50
#define PRIORITY_INPUT       3      /* кнопки не должны «залипать» */
#define PRIORITY_GAME        2
#define PRIORITY_SOUND       2
#define PRIORITY_RENDER      1      /* отрисовка самая медленная и самая нужная в последнюю очередь */
#define STACK_INPUT          384
#define STACK_GAME           256
#define STACK_RENDER         384
#define STACK_SOUND          192
#define SOUND_QUEUE_LENGTH   8

/* 1 */
#define TONE_FIRE_HZ        1500
#define TONE_FIRE_MS        25
#define TONE_BRICK_HZ       400
#define TONE_BRICK_MS       20
#define TONE_ENEMY_HZ       800
#define TONE_ENEMY_MS       80
#define TONE_PLAYER_HZ      200
#define TONE_PLAYER_MS      250
#define TONE_WIN_HZ         1200
#define TONE_LOSE_HZ        150
#define TONE_END_MS         400

/* 2 */
static Game game;                          /* состояние игры, защищено gameMutex */
static SemaphoreHandle_t gameMutex;
static SemaphoreHandle_t i2cMutex;         /* шина I2C одна на клавиатуру и дисплей */
static SemaphoreHandle_t frameSem;         /* игра -> отрисовка: «есть новый кадр» */
static QueueHandle_t inputQueue;           /* ввод -> игра: последнее состояние кнопок (длина 1) */
static QueueHandle_t soundQueue;           /* игра -> звук: события (маска EV_*) */

/* 3 */

static void InputTask(void *argument)
{
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();
    uint16_t previous = 0;
    Dir held = DIR_NONE;

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(INPUT_PERIOD_MS));

        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        uint16_t mask = Keypad_Read();
        xSemaphoreGive(i2cMutex);

        Input in = Input_Decode(mask, previous, &held);
        previous = mask;
        xQueueOverwrite(inputQueue, &in);     /* в очереди всегда только свежее состояние */
    }
}

/* 4 */

static void GameTask(void *argument)
{
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();
    uint8_t restart_armed = 0;      /* перезапуск только после отпускания «огня» */

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(GAME_PERIOD_MS));

        Input in = { DIR_NONE, 0 };
        xQueuePeek(inputQueue, &in, 0);

        xSemaphoreTake(gameMutex, portMAX_DELAY);
        game.input = in;
        if (game.phase == PHASE_PLAYING) {
            Game_Tick(&game);
            restart_armed = 0;
        } else {
            if (!in.fire) {
                restart_armed = 1;
            } else if (restart_armed) {
                Game_Init(&game, xTaskGetTickCount() | 1u);
                restart_armed = 0;
            }
        }
        uint8_t events = game.events;
        game.events = 0;
        xSemaphoreGive(gameMutex);

        if (events) {
            xQueueSend(soundQueue, &events, 0);      /* не ждём: звук не должен тормозить игру */
        }
        xSemaphoreGive(frameSem);
    }
}

/* 5 */

static void RenderTask(void *argument)
{
    (void)argument;

    for (;;) {
        xSemaphoreTake(frameSem, portMAX_DELAY);

        xSemaphoreTake(gameMutex, portMAX_DELAY);
        Draw_Game(&game);                            /* быстро: только запись в буфер в памяти */
        xSemaphoreGive(gameMutex);

        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        oled_UpdateScreen();                         /* медленно: передача буфера по I2C */
        xSemaphoreGive(i2cMutex);
    }
}

/* 6 */

static void Tone(uint16_t freq_hz, uint16_t duration_ms)
{
    Buzzer_Set_Freq(freq_hz);
    Buzzer_Set_Volume(BUZZER_VOLUME_MAX);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));          /* задержка отдаёт процессор другим задачам */
    Buzzer_Set_Volume(BUZZER_VOLUME_MUTE);
}

static void SoundTask(void *argument)
{
    (void)argument;
    uint8_t events;

    for (;;) {
        xQueueReceive(soundQueue, &events, portMAX_DELAY);

        if (events & EV_WIN) {
            Tone(TONE_WIN_HZ, TONE_END_MS);
        } else if (events & (EV_BASE_DESTROYED)) {
            Tone(TONE_LOSE_HZ, TONE_END_MS);
        } else if (events & EV_PLAYER_DESTROYED) {
            Tone(TONE_PLAYER_HZ, TONE_PLAYER_MS);
        } else if (events & EV_ENEMY_DESTROYED) {
            Tone(TONE_ENEMY_HZ, TONE_ENEMY_MS);
        } else if (events & EV_PLAYER_FIRE) {
            Tone(TONE_FIRE_HZ, TONE_FIRE_MS);
        } else if (events & EV_BRICK_HIT) {
            Tone(TONE_BRICK_HZ, TONE_BRICK_MS);
        }
    }
}

/* 7 */

void App_Init(void)
{
    MX_I2C1_Init();
    MX_TIM2_Init();
    oled_Init();
    Buzzer_Init();
    Buzzer_Set_Volume(BUZZER_VOLUME_MUTE);

    Game_Init(&game, HAL_GetTick() | 1u);

    gameMutex = xSemaphoreCreateMutex();
    i2cMutex = xSemaphoreCreateMutex();
    frameSem = xSemaphoreCreateBinary();
    inputQueue = xQueueCreate(1, sizeof(Input));
    soundQueue = xQueueCreate(SOUND_QUEUE_LENGTH, sizeof(uint8_t));

    xTaskCreate(InputTask,  "input",  STACK_INPUT,  NULL, PRIORITY_INPUT,  NULL);
    xTaskCreate(GameTask,   "game",   STACK_GAME,   NULL, PRIORITY_GAME,   NULL);
    xTaskCreate(RenderTask, "render", STACK_RENDER, NULL, PRIORITY_RENDER, NULL);
    xTaskCreate(SoundTask,  "sound",  STACK_SOUND,  NULL, PRIORITY_SOUND,  NULL);
}
