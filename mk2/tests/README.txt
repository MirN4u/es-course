Запуск на ПК (нужен gcc, например MSYS2): gcc -I . -I ../Inc test_timer.c ../Src/timer_app.c -o test_timer.exe && test_timer.exe
Это тест логики таймера с «подставным» железом (клавиатура, часы, экран, зуммер). В прошивку не входит.
