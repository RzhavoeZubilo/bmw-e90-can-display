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
  // Слева — что за экран. На обзорном писать нечего, там и так всё подписано.
  switch (d.screen) {
    case ScreenId::Coolant: u8g2_DrawStr(u, 0, Y_BAR, "COOLANT"); break;
    case ScreenId::Voltage: u8g2_DrawStr(u, 0, Y_BAR, "BATTERY"); break;
    default:                u8g2_DrawStr(u, 0, Y_BAR, "E90");     break;
  }

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

// Иконка термометра рисуется примитивами, а не шрифтом: отдельный
// графический шрифт стоил бы сотни байт флеша ради одного символа.
static void draw_thermo(u8g2_t* u, uint8_t x, uint8_t y) {
  u8g2_DrawDisc(u, x + 6, y + 26, 6, U8G2_DRAW_ALL);   // колба
  u8g2_DrawBox(u, x + 4, y + 4, 5, 20);                // столбик
  u8g2_DrawDisc(u, x + 6, y + 4, 2, U8G2_DRAW_ALL);    // скруглённый верх
  for (uint8_t i = 0; i < 3; ++i)                      // насечки шкалы
    u8g2_DrawHLine(u, x + 10, y + 8 + i * 5, 3);
}

// Крупное значение: целая часть большим шрифтом, дробная и единица —
// мелким. Так обходим подвох наборов logisoso*_tn: там только цифры,
// и точки с минусом может не оказаться.
static void draw_big_value(u8g2_t* u, uint8_t x, int16_t whole,
                           const char* tail, bool ok) {
  char buf[8];
  if (!ok) {
    u8g2_SetFont(u, u8g2_font_6x12_tr);
    u8g2_DrawStr(u, x, 48, "---");
    return;
  }

  uint8_t n = 0;
  if (whole < 0) {
    // Минус большим шрифтом не нарисовать, рисуем чертой вручную.
    u8g2_DrawBox(u, x, 36, 10, 3);
    x += 14;
    whole = static_cast<int16_t>(-whole);
  }
  n = put_uint(buf, static_cast<uint16_t>(whole));
  buf[n] = 0;

  u8g2_SetFont(u, u8g2_font_logisoso28_tn);
  u8g2_DrawStr(u, x, 56, buf);
  // Ширину берём у самого шрифта: прикидка «столько-то пикселей на цифру»
  // врёт, потому что у единицы и восьмёрки разный вынос.
  const uint8_t w = u8g2_GetStrWidth(u, buf);

  u8g2_SetFont(u, u8g2_font_6x12_tr);
  u8g2_DrawStr(u, x + w + 2, 56, tail);
}

static void draw_learn(u8g2_t* u, const ScreenData& d) {
  char buf[6];
  u8g2_SetFont(u, u8g2_font_6x12_tr);

  switch (d.learn) {
    case LearnPhase::Baseline:
      u8g2_DrawStr(u, 0, Y_BAR, "LEARN");
      u8g2_DrawStr(u, 0, 28, "Do not touch");
      u8g2_DrawStr(u, 0, 40, "the buttons");
      {
        const uint8_t n = put_uint(buf, d.learn_secs);
        buf[n] = 0;
        u8g2_SetFont(u, u8g2_font_logisoso20_tn);
        // Прижимаем к правому краю по фактической ширине.
        const uint8_t w = u8g2_GetStrWidth(u, buf);
        u8g2_DrawStr(u, SCREEN_W - RIGHT_PAD - w, 62, buf);
      }
      break;

    case LearnPhase::Hold:
      u8g2_DrawStr(u, 0, Y_BAR, "LEARN");
      u8g2_DrawStr(u, 0, 32, "HOLD the button");
      u8g2_DrawStr(u, 0, 46, "you want to use");
      u8g2_DrawStr(u, 0, 60, "for switching");
      break;

    case LearnPhase::Saved:
      u8g2_DrawStr(u, 0, Y_BAR, "LEARN");
      u8g2_DrawStr(u, 0, 40, "Button saved");
      break;

    default:
      u8g2_DrawStr(u, 0, Y_BAR, "LEARN");
      u8g2_DrawStr(u, 0, 34, "Button not found");
      u8g2_DrawStr(u, 0, 50, "running without it");
      break;
  }
}

static void draw_all(u8g2_t* u, const ScreenData& d) {
  char buf[8];
  {
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
  }
}

static void draw_coolant(u8g2_t* u, const ScreenData& d) {
  draw_thermo(u, 6, 24);
  draw_big_value(u, 42, d.coolant_c, "C", d.coolant_ok);
}

static void draw_voltage(u8g2_t* u, const ScreenData& d) {
  // Целые вольты крупно, десятые доли и буква V — мелким.
  char tail[6];
  if (d.volts_ok) {
    const uint16_t x10 = d.volts_mv / 100;
    tail[0] = '.';
    tail[1] = static_cast<char>('0' + (x10 % 10));
    tail[2] = 'V';
    tail[3] = 0;
    draw_big_value(u, 20, static_cast<int16_t>(x10 / 10), tail, true);
  } else {
    draw_big_value(u, 20, 0, "", false);
  }
}

void screen_draw(u8g2_t* u, const ScreenData& d) {
  u8g2_FirstPage(u);
  do {
    u8g2_SetFont(u, u8g2_font_6x12_tr);

    if (d.learn != LearnPhase::None) {
      draw_learn(u, d);
      continue;
    }

    draw_status_bar(u, d);
    switch (d.screen) {
      case ScreenId::Coolant: draw_coolant(u, d); break;
      case ScreenId::Voltage: draw_voltage(u, d); break;
      default:                draw_all(u, d);     break;
    }
  } while (u8g2_NextPage(u));
}
