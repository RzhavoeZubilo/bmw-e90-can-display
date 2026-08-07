#!/bin/bash
# Собирает и запускает симулятор дисплея на хосте.
#
# Гоняет тот же lib/Screen, что и прошивка, поверх C-ядра u8g2, и пишет
# картинку в sim/out/screens.png. Железо не нужно.
set -euo pipefail

cd "$(dirname "$0")/.."

U8G2_SRC=".pio/libdeps/nano/U8g2/src"
OUT="sim/out"
OBJ="$OUT/u8g2_objs"

if [ ! -d "$U8G2_SRC" ]; then
  echo "U8g2 не найдена. Сначала соберите прошивку: pio run" >&2
  exit 1
fi

mkdir -p "$OBJ"

# C-ядро u8g2 компилируется один раз и кэшируется: файлов там больше сотни,
# и таблицы шрифтов собираются заметно дольше всего остального.
if [ ! -f "$OBJ/.stamp" ]; then
  echo "compiling u8g2 core (once)..."
  for f in "$U8G2_SRC"/clib/*.c; do
    cc -O1 -w -c "$f" -I "$U8G2_SRC/clib" -o "$OBJ/$(basename "$f" .c).o"
  done
  touch "$OBJ/.stamp"
fi

echo "compiling simulator..."
c++ -std=c++17 -O1 -Wall \
  -I lib/Screen -I lib/Alerts -I "$U8G2_SRC" \
  sim/sim_main.cpp lib/Screen/Screen.cpp lib/Alerts/Alerts.cpp "$OBJ"/*.o \
  -o "$OUT/sim"

"$OUT/sim" "$OUT/screens.bmp"

# BMP пишется вручную, чтобы не тащить зависимости; PNG удобнее смотреть.
if command -v sips >/dev/null 2>&1; then
  sips -s format png "$OUT/screens.bmp" --out "$OUT/screens.png" >/dev/null
  rm -f "$OUT/screens.bmp"
  echo "-> $OUT/screens.png"
fi
