#include "main.h"
#include "kb.h"
#include "oled.h"
#include "fonts.h"
#include "buzzer.h"
#include "timer_app.h"

/* 0 */
#define KEYBOARD_TEST_MODE   0      /* 1 - только показывать нажатую клавишу и её строку/столбец */

#define KEY_ROWS             4
#define KEY_COLS             3
#define NO_KEY               0

#define KEY_POLL_MS          20
#define KEY_STABLE_SCANS     2      /* сколько опросов подряд клавиша должна держаться (дребезг) */

#define COL_CODE_LEFT        0x04
#define COL_CODE_CENTER      0x02
#define COL_CODE_RIGHT       0x01

#define DIGITS_MAX           10000
#define SECONDS_IN_MINUTE    60
#define MS_IN_SECOND         1000
#define MAX_MINUTES          99
#define MAX_TOTAL_SECONDS    (MAX_MINUTES * SECONDS_IN_MINUTE + 59)

#define ALARM_FREQ           N_A5
#define ALARM_BEEP_MS        300
#define ALARM_DURATION_MS    10000

#define KEY_START            '#'
#define KEY_CANCEL           '*'    /* в вводе - стереть цифру, при отсчёте - отмена */

/* 1 */
typedef enum {
    STATE_INPUT,
    STATE_RUNNING,
    STATE_ALARM
} AppState;

/* Раскладка клавиш: строки сверху вниз (ROW1..ROW4), столбцы слева направо.
   Если на стенде не так - проверить режимом KEYBOARD_TEST_MODE и поправить таблицу. */
static const uint8_t ROW_CODES[KEY_ROWS] = { ROW1, ROW2, ROW3, ROW4 };
static const char KEY_MAP[KEY_ROWS][KEY_COLS] = {
    { '1', '2', '3' },
    { '4', '5', '6' },
    { '7', '8', '9' },
    { '*', '0', '#' }
};

static AppState state = STATE_INPUT;
static uint32_t input_digits = 0;
static uint32_t end_tick = 0;
static uint32_t alarm_start_tick = 0;
static uint32_t last_scan_tick = 0;
static char last_raw_key = NO_KEY;
static uint8_t stable_count = 0;
static uint8_t key_reported = 0;
static int32_t shown_seconds = -1;
static uint8_t alarm_sound_on = 0;

/* 2 */

static char ScanKey(uint8_t *row, uint8_t *col)
{
    for (uint8_t r = 0; r < KEY_ROWS; r++) {
        uint8_t code = Check_Row(ROW_CODES[r]);
        uint8_t c = KEY_COLS;
        if (code == COL_CODE_LEFT)        c = 0;
        else if (code == COL_CODE_CENTER) c = 1;
        else if (code == COL_CODE_RIGHT)  c = 2;
        if (c < KEY_COLS) {
            *row = r;
            *col = c;
            return KEY_MAP[r][c];
        }
    }
    return NO_KEY;
}

/* Возвращает клавишу один раз за нажатие (после отсева дребезга), иначе NO_KEY. */
static char GetKeyEvent(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t row, col;
    char key;

    if (now - last_scan_tick < KEY_POLL_MS) {
        return NO_KEY;
    }
    last_scan_tick = now;

    key = ScanKey(&row, &col);
    if (key == NO_KEY) {
        last_raw_key = NO_KEY;
        stable_count = 0;
        key_reported = 0;
        return NO_KEY;
    }
    if (key == last_raw_key) {
        if (stable_count < KEY_STABLE_SCANS) {
            stable_count++;
        }
    } else {
        last_raw_key = key;
        stable_count = 1;
        key_reported = 0;
    }
    if (stable_count >= KEY_STABLE_SCANS && !key_reported) {
        key_reported = 1;
        return key;
    }
    return NO_KEY;
}

/* 3 */

static void FormatTime(char *buf, uint32_t minutes, uint32_t seconds)
{
    buf[0] = '0' + (minutes / 10) % 10;
    buf[1] = '0' + minutes % 10;
    buf[2] = ':';
    buf[3] = '0' + (seconds / 10) % 10;
    buf[4] = '0' + seconds % 10;
    buf[5] = '\0';
}

static void DrawScreen(const char *title, const char *time_text, const char *hint)
{
    oled_Fill(Black);
    oled_SetCursor(0, 0);
    oled_WriteString((char *)title, Font_11x18, White);
    oled_SetCursor(24, 22);
    oled_WriteString((char *)time_text, Font_16x26, White);
    oled_SetCursor(0, 54);
    oled_WriteString((char *)hint, Font_7x10, White);
    oled_UpdateScreen();
}

static void DrawInput(void)
{
    char text[6];
    FormatTime(text, input_digits / 100, input_digits % 100);
    DrawScreen("SET TIMER", text, "#:start  *:erase");
}

static void DrawRunning(uint32_t seconds_left)
{
    char text[6];
    FormatTime(text, seconds_left / SECONDS_IN_MINUTE, seconds_left % SECONDS_IN_MINUTE);
    DrawScreen("RUNNING", text, "*:cancel");
}

static void DrawAlarm(void)
{
    DrawScreen("TIME IS UP!", "00:00", "press any key");
}

/* 4 */

static void AlarmSoundOn(void)
{
    Buzzer_Set_Freq(ALARM_FREQ);
    Buzzer_Set_Volume(BUZZER_VOLUME_MAX);
    alarm_sound_on = 1;
}

static void AlarmSoundOff(void)
{
    Buzzer_Set_Volume(BUZZER_VOLUME_MUTE);
    alarm_sound_on = 0;
}

/* 5 */

static void GoInput(void)
{
    AlarmSoundOff();
    state = STATE_INPUT;
    DrawInput();
}

static void GoRunning(uint32_t total_seconds)
{
    end_tick = HAL_GetTick() + total_seconds * MS_IN_SECOND;
    shown_seconds = -1;
    state = STATE_RUNNING;
}

static void GoAlarm(void)
{
    state = STATE_ALARM;
    alarm_start_tick = HAL_GetTick();
    AlarmSoundOn();
    DrawAlarm();
}

/* 6 */

static void StepInput(char key)
{
    if (key >= '0' && key <= '9') {
        input_digits = (input_digits * 10 + (uint32_t)(key - '0')) % DIGITS_MAX;
        DrawInput();
    } else if (key == KEY_CANCEL) {
        input_digits /= 10;
        DrawInput();
    } else if (key == KEY_START) {
        uint32_t total = (input_digits / 100) * SECONDS_IN_MINUTE + input_digits % 100;
        if (total > MAX_TOTAL_SECONDS) {
            total = MAX_TOTAL_SECONDS;   /* больше 99:59 на экран не влезет */
        }
        if (total > 0) {
            GoRunning(total);
        }
    }
}

static void StepRunning(char key)
{
    uint32_t now = HAL_GetTick();
    int32_t left_ms = (int32_t)(end_tick - now);

    if (key == KEY_CANCEL) {
        GoInput();
        return;
    }
    if (left_ms <= 0) {
        GoAlarm();
        return;
    }
    /* секунды округляем вверх, чтобы 00:00 показывался только по окончании */
    int32_t seconds_left = (left_ms + MS_IN_SECOND - 1) / MS_IN_SECOND;
    if (seconds_left != shown_seconds) {
        shown_seconds = seconds_left;
        DrawRunning((uint32_t)seconds_left);
    }
}

static void StepAlarm(char key)
{
    uint32_t elapsed = HAL_GetTick() - alarm_start_tick;

    if (key != NO_KEY || elapsed >= ALARM_DURATION_MS) {
        GoInput();
        return;
    }
    /* пик-пауза-пик: звук включён в чётных интервалах */
    uint8_t should_sound = ((elapsed / ALARM_BEEP_MS) % 2 == 0);
    if (should_sound && !alarm_sound_on) {
        AlarmSoundOn();
    } else if (!should_sound && alarm_sound_on) {
        AlarmSoundOff();
    }
}

/* 7 */

#if KEYBOARD_TEST_MODE
static void KeyboardTestLoop(void)
{
    char text[6] = "     ";
    char info[16];
    uint8_t row = 0, col = 0;

    while (1) {
        char key = ScanKey(&row, &col);
        if (key != NO_KEY) {
            text[0] = key;
            text[1] = '\0';
            info[0] = 'r'; info[1] = '0' + row; info[2] = ' ';
            info[3] = 'c'; info[4] = '0' + col; info[5] = '\0';
            DrawScreen("KEY TEST", text, info);
        }
        HAL_Delay(KEY_POLL_MS);
    }
}
#endif

/* 8 */

void TimerApp_Init(void)
{
    oled_Init();
    AlarmSoundOff();
#if KEYBOARD_TEST_MODE
    KeyboardTestLoop();
#endif
    GoInput();
}

void TimerApp_Step(void)
{
    char key = GetKeyEvent();

    switch (state) {
    case STATE_INPUT:
        StepInput(key);
        break;
    case STATE_RUNNING:
        StepRunning(key);
        break;
    case STATE_ALARM:
        StepAlarm(key);
        break;
    }
}
