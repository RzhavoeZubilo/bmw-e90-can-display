#include "Screen.h"

// Раскладка под жёлто-синюю матрицу SSD1306. Деление цветов физическое:
// строки 0..15 всегда жёлтые, 16..63 синие, границу не подвинуть.
// Жёлтая полоса — статус, все данные целиком ниже неё.
static const uint8_t Y_BAR = 11;
static const uint8_t SCREEN_W = 128;
static const uint8_t CHAR_W = 6;  // шрифт 6x12 моноширинный
static const uint8_t COL_L = 0, COL_R = 66;
static const uint8_t VAL_DX = 30;
static const uint8_t R1 = 27, R2 = 38, R3 = 49, R4 = 60;

// Отступ от правого края: без него статус упирается в самую кромку
// матрицы и читается как обрезанный.
static const uint8_t RIGHT_PAD = 2;

static const char* const DASHES = "---";

// Форматирование без sprintf: на AVR он тянет около полутора килобайт.

static uint8_t put_uint(char* out, uint16_t v) {
  char tmp[6];
  uint8_t n = 0;
  do {
    tmp[n++] = static_cast<char>('0' + (v % 10));
    v /= 10;
  } while (v);
  for (uint8_t i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
  return n;
}

static void fmt_int(char* out, int16_t v, char suffix) {
  uint8_t n = 0;
  if (v < 0) {
    out[n++] = '-';
    v = static_cast<int16_t>(-v);
  }
  n += put_uint(out + n, static_cast<uint16_t>(v));
  if (suffix) out[n++] = suffix;
  out[n] = '\0';
}

// Десятые доли: 145 -> "14.5". Так обходимся без плавающей точки.
static void fmt_dec1(char* out, uint16_t x10, char suffix) {
  uint8_t n = put_uint(out, static_cast<uint16_t>(x10 / 10));
  out[n++] = '.';
  out[n++] = static_cast<char>('0' + (x10 % 10));
  if (suffix) out[n++] = suffix;
  out[n] = '\0';
}

static void draw_right(u8g2_t* u, const char* s, uint8_t y) {
  uint8_t len = 0;
  while (s[len]) ++len;
  u8g2_DrawStr(u, SCREEN_W - RIGHT_PAD - CHAR_W * len, y, s);
}

static void draw_field(u8g2_t* u, uint8_t x, uint8_t y, const char* label,
                       const char* value) {
  u8g2_DrawStr(u, x, y, label);
  u8g2_DrawStr(u, x + VAL_DX, y, value);
}

static void draw_temp(u8g2_t* u, uint8_t x, uint8_t y, const char* label,
                      int16_t v, bool ok) {
  char buf[8];
  if (ok) {
    fmt_int(buf, v, 'C');
    draw_field(u, x, y, label, buf);
  } else {
    draw_field(u, x, y, label, DASHES);
  }
}

static void draw_status_bar(u8g2_t* u, const ScreenData& d) {
  u8g2_DrawStr(u, 0, Y_BAR, "E90");

  // Правый край жёлтой полосы — самое заметное место, отдано тревогам.
  if (d.alert == Alert::CoolantHot) {
    draw_right(u, "HOT!", Y_BAR);
  } else if (d.alert == Alert::AtfHot) {
    draw_right(u, "ATF HOT", Y_BAR);
  } else if (!d.can_ok) {
    draw_right(u, "NO CAN", Y_BAR);
  }
}

// Раздельные баки: у E90 седловидный бак с двумя датчиками. На PT-CAN их
// пока не нашли, кадр 0x349 живёт на K-CAN.
static void draw_tanks(u8g2_t* u, const ScreenData& d) {
  char buf[24];
  uint8_t n = 0;
  const char* pre = "TANK L ";
  while (*pre) buf[n++] = *pre++;

  if (d.tanks_ok) {
    char v[8];
    fmt_dec1(v, d.fuel_left_l_x10, 0);
    for (uint8_t i = 0; v[i]; ++i) buf[n++] = v[i];
    buf[n++] = ' ';
    buf[n++] = 'R';
    buf[n++] = ' ';
    fmt_dec1(v, d.fuel_right_l_x10, 0);
    for (uint8_t i = 0; v[i]; ++i) buf[n++] = v[i];
  } else {
    const char* rest = "---  R ---";
    while (*rest) buf[n++] = *rest++;
  }
  buf[n] = '\0';
  u8g2_DrawStr(u, COL_L, R4, buf);
}

void screen_draw(u8g2_t* u, const ScreenData& d) {
  char buf[8];

  u8g2_FirstPage(u);
  do {
    u8g2_SetFont(u, u8g2_font_6x12_tr);

    draw_status_bar(u, d);

    draw_temp(u, COL_L, R1, "COOL", d.coolant_c, d.coolant_ok);
    draw_temp(u, COL_R, R1, "ATF", d.atf_c, d.atf_ok);

    draw_temp(u, COL_L, R2, "OIL", d.oil_c, d.oil_ok);
    if (d.volts_ok) {
      fmt_dec1(buf, d.volts_mv / 100, 'V');
      draw_field(u, COL_R, R2, "BATT", buf);
    } else {
      draw_field(u, COL_R, R2, "BATT", DASHES);
    }

    draw_temp(u, COL_L, R3, "IAT", d.iat_c, d.iat_ok);
    if (d.fuel_ok) {
      fmt_dec1(buf, d.fuel_total_l_x10, 'L');
      draw_field(u, COL_R, R3, "FUEL", buf);
    } else {
      draw_field(u, COL_R, R3, "FUEL", DASHES);
    }

    draw_tanks(u, d);
  } while (u8g2_NextPage(u));
}
