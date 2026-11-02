Запуск на ПК (нужен gcc, например MSYS2), из этой папки:
  gcc -I ../Core/Inc test_tanks.c -o test_tanks.exe && test_tanks.exe
  gcc -I ../Core/Inc test_input.c -o test_input.exe && test_input.exe
Проверяют игровую логику (Core/Src/tanks.c) и разбор клавиш (Core/Src/input_decode.c) без платы. В прошивку не входят.
