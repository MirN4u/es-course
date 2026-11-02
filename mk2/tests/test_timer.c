#include <stdio.h>
#include <string.h>
#include "main.h"
#include "kb.h"
#include "oled.h"
#include "buzzer.h"
#include "timer_app.h"

FontDef Font_7x10, Font_11x18, Font_16x26;

static uint32_t now_ms = 0;
static int pressed_row = -1, pressed_col = -1;       /* which key is held */
static char last_title[32], last_time[32], last_hint[32];
static int field = 0;                                /* which oled_WriteString comes next */
static int buzzer_volume = 0, screen_updates = 0, errors = 0;

uint32_t HAL_GetTick(void) { return now_ms; }
void HAL_Delay(uint32_t ms) { now_ms += ms; }
uint8_t oled_Init(void) { return 1; }
void oled_Fill(OLED_COLOR c) { (void)c; field = 0; }
void oled_SetCursor(uint8_t x, uint8_t y) { (void)x; (void)y; }
void oled_UpdateScreen(void) { screen_updates++; }
char oled_WriteString(char *s, FontDef f, OLED_COLOR c) {
    (void)f; (void)c;
    if (field == 0) strcpy(last_title, s);
    else if (field == 1) strcpy(last_time, s);
    else strcpy(last_hint, s);
    field++;
    return 0;
}
void Buzzer_Set_Freq(uint16_t f) { (void)f; }
void Buzzer_Set_Volume(uint16_t v) { buzzer_volume = v; }

uint8_t Check_Row(uint8_t Nrow) {
    static const uint8_t rows[4] = { ROW1, ROW2, ROW3, ROW4 };
    static const uint8_t cols[3] = { 0x04, 0x02, 0x01 };
    if (pressed_row >= 0 && rows[pressed_row] == Nrow) return cols[pressed_col];
    return 0;
}

static void run_ms(uint32_t ms) { uint32_t start = now_ms; while (now_ms - start < ms) { now_ms += 5; TimerApp_Step(); } }

static void press(char key, uint32_t hold_ms) {
    static const char map[4][3] = { {'1','2','3'}, {'4','5','6'}, {'7','8','9'}, {'*','0','#'} };
    for (int r = 0; r < 4; r++) for (int c = 0; c < 3; c++) if (map[r][c] == key) { pressed_row = r; pressed_col = c; }
    run_ms(hold_ms);
    pressed_row = pressed_col = -1;
    run_ms(60);
}

static void check(const char *name, int ok) {
    printf("%-58s %s\n", name, ok ? "OK" : "FAIL");
    if (!ok) errors++;
}

int main(void) {
    TimerApp_Init();
    check("start: input screen 00:00", !strcmp(last_title, "SET TIMER") && !strcmp(last_time, "00:00"));

    press('1', 60); press('2', 60); press('3', 60); press('0', 60);
    check("digits 1,2,3,0 -> 12:30", !strcmp(last_time, "12:30"));
    press('5', 60);
    check("5th digit shifts: 2305 -> 23:05", !strcmp(last_time, "23:05"));
    press('*', 60);
    check("* erases last digit -> 00:23", !strcmp(last_time, "02:30"));
    press('*', 60); press('*', 60); press('*', 60); press('*', 60); press('*', 60);
    check("erase on empty stays 00:00", !strcmp(last_time, "00:00"));

    press('#', 60);
    check("# with 00:00 does not start", !strcmp(last_title, "SET TIMER"));

    pressed_row = 0; pressed_col = 0; run_ms(15); pressed_row = pressed_col = -1; run_ms(60);
    check("short glitch (<2 scans) is ignored", !strcmp(last_time, "00:00"));

    press('0', 60); press('0', 60); press('0', 60); press('3', 60);
    check("entered 00:03", !strcmp(last_time, "00:03"));
    uint32_t t0 = now_ms;
    press('#', 60);
    check("# starts: RUNNING", !strcmp(last_title, "RUNNING"));
    run_ms(900);
    check("after ~1 s shows 00:02 or 00:03", !strcmp(last_time, "00:02") || !strcmp(last_time, "00:03"));
    run_ms(1000);
    check("after ~2 s shows 00:01 or 00:02", !strcmp(last_time, "00:01") || !strcmp(last_time, "00:02"));
    while (strcmp(last_title, "TIME IS UP!") && now_ms - t0 < 6000) run_ms(5);
    check("at the end: TIME IS UP and buzzer on (first beep)", !strcmp(last_title, "TIME IS UP!") && buzzer_volume == BUZZER_VOLUME_MAX);
    check("alarm began about 3 s after start", now_ms - t0 >= 3000 && now_ms - t0 < 5000);
    int on = 0, off = 0;
    for (int i = 0; i < 80; i++) { run_ms(50); if (buzzer_volume) on++; else off++; }
    check("buzzer beeps: both on and off states seen", on > 0 && off > 0);
    press('7', 60);
    check("any key stops alarm -> input, buzzer off", !strcmp(last_title, "SET TIMER") && buzzer_volume == 0);

    press('*', 60); press('*', 60); press('*', 60); press('*', 60);
    press('0', 60); press('0', 60); press('0', 60); press('1', 60);
    press('#', 60);
    run_ms(11000);
    check("alarm times out after 10 s by itself", !strcmp(last_title, "SET TIMER") && buzzer_volume == 0);

    press('*', 60); press('*', 60); press('*', 60); press('*', 60);
    press('0', 60); press('0', 60); press('9', 60); press('9', 60);
    check("00:99 entered", !strcmp(last_time, "00:99"));
    press('#', 60);
    check("00:99 normalised to 99 s = 01:39", !strcmp(last_time, "01:39"));
    press('*', 60);
    check("* cancels running -> input", !strcmp(last_title, "SET TIMER") && buzzer_volume == 0);

    press('9', 60); press('9', 60); press('9', 60); press('9', 60);
    press('#', 60);
    check("99:99 is clamped to 99:59", !strcmp(last_time, "99:59"));

    now_ms = 0xFFFFFFFFu - 1500;
    press('*', 60); press('*', 60); press('*', 60); press('*', 60); press('*', 60);
    press('0', 60); press('0', 60); press('0', 60); press('5', 60);
    press('#', 60);
    run_ms(6000);
    check("tick counter wrap-around: alarm still fires", !strcmp(last_title, "TIME IS UP!") || buzzer_volume > 0);

    printf("\n%s (%d errors)\n", errors ? "FAILED" : "ALL PASSED", errors);
    return errors != 0;
}
