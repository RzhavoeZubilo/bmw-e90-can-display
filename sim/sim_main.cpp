// Симулятор дисплея. Гоняет НАСТОЯЩИЙ код отрисовки из lib/Screen на
// хосте и складывает результат в BMP, который можно посмотреть глазами.
//
// Работает это так: на AVR u8g2 рисует по страницам в буфер на 128 байт,
// а здесь — в полный буфер 128x64, куда мы просто заглядываем напрямую.
// Никакого дисплея и SPI не нужно, колбэки-заглушки идут в никуда.
//
// Сборка и запуск:  sim/build.sh

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Screen.h"

static const int W = 128, H = 64;

// Физическое деление жёлто-синей матрицы: строки 0..15 жёлтые, ниже синие.
static const int YELLOW_ROWS = 16;

static const int SCALE = 4;
static const int MARGIN = 10;
static const int COL_GAP = 26;   // чтобы соседние панели не сливались
static const int ROW_GAP = 18;
static const int CAPTION_H = 13;
static const int GAP = 7;

struct RGB {
  uint8_t r, g, b;
};

static const RGB BG = {26, 26, 30};
static const RGB PIXEL_OFF = {10, 10, 12};
static const RGB PIXEL_YELLOW = {255, 199, 44};
static const RGB PIXEL_BLUE = {96, 205, 255};
static const RGB CAPTION = {150, 150, 158};

// --- холст ---

struct Canvas {
  int w, h;
  std::vector<RGB> px;

  Canvas(int w_, int h_) : w(w_), h(h_), px(w_ * h_, BG) {}

  void put(int x, int y, RGB c) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    px[y * w + x] = c;
  }

  void block(int x, int y, int size, RGB c) {
    for (int dy = 0; dy < size; ++dy)
      for (int dx = 0; dx < size; ++dx) put(x + dx, y + dy, c);
  }
};

// 24-битный BMP: формат без сжатия, строки снизу вверх, паддинг до 4 байт.
static bool write_bmp(const char* path, const Canvas& c) {
  FILE* f = fopen(path, "wb");
  if (!f) return false;

  const int row_bytes = c.w * 3;
  const int pad = (4 - (row_bytes % 4)) % 4;
  const uint32_t data_size = (row_bytes + pad) * c.h;
  const uint32_t file_size = 14 + 40 + data_size;

  uint8_t hdr[54] = {0};
  hdr[0] = 'B';
  hdr[1] = 'M';
  memcpy(hdr + 2, &file_size, 4);
  const uint32_t offset = 54;
  memcpy(hdr + 10, &offset, 4);
  const uint32_t dib = 40;
  memcpy(hdr + 14, &dib, 4);
  memcpy(hdr + 18, &c.w, 4);
  memcpy(hdr + 22, &c.h, 4);
  const uint16_t planes = 1, bpp = 24;
  memcpy(hdr + 26, &planes, 2);
  memcpy(hdr + 28, &bpp, 2);
  memcpy(hdr + 34, &data_size, 4);
  fwrite(hdr, 1, 54, f);

  std::vector<uint8_t> row(row_bytes + pad, 0);
  for (int y = c.h - 1; y >= 0; --y) {
    for (int x = 0; x < c.w; ++x) {
      const RGB& p = c.px[y * c.w + x];
      row[x * 3 + 0] = p.b;
      row[x * 3 + 1] = p.g;
      row[x * 3 + 2] = p.r;
    }
    fwrite(row.data(), 1, row.size(), f);
  }
  fclose(f);
  return true;
}

// --- u8g2 на хосте ---

static u8g2_t u8g2;

static void u8g2_host_init() {
  // Полнобуферный (_f) вариант того же контроллера, что стоит в машине.
  // Колбэки-заглушки: ничего никуда не передаётся, буфер нужен только нам.
  u8g2_Setup_ssd1306_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_empty,
                                     u8x8_dummy_cb);
  u8g2_InitDisplay(&u8g2);
  u8g2_SetPowerSave(&u8g2, 0);
}

// Пиксель из буфера u8g2: страницы по 8 строк, бит внутри байта — строка.
static bool pixel_at(int x, int y) {
  const uint8_t* buf = u8g2_GetBufferPtr(&u8g2);
  return (buf[(y / 8) * W + x] >> (y % 8)) & 1;
}

static void blit_panel(Canvas& c, int ox, int oy) {
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      RGB col = PIXEL_OFF;
      if (pixel_at(x, y)) col = (y < YELLOW_ROWS) ? PIXEL_YELLOW : PIXEL_BLUE;
      c.block(ox + x * SCALE, oy + y * SCALE, SCALE, col);
    }
  }
}

static void blit_caption(Canvas& c, int ox, int oy, const char* text) {
  u8g2_ClearBuffer(&u8g2);
  u8g2_SetFont(&u8g2, u8g2_font_6x12_tr);
  u8g2_DrawStr(&u8g2, 0, 10, text);
  for (int y = 0; y < CAPTION_H; ++y)
    for (int x = 0; x < W; ++x)
      if (pixel_at(x, y)) c.block(ox + x * SCALE, oy + y * SCALE, SCALE, CAPTION);
}

// --- сценарии ---

struct Scenario {
  const char* caption;
  ScreenData data;
};

static std::vector<Scenario> build_scenarios() {
  std::vector<Scenario> out;

  {
    // Прогретая машина в движении. Значения правдоподобные для N46 + 6HP19.
    ScreenData d;
    d.coolant_c = 91;   d.coolant_ok = true;
    d.atf_c = 78;       d.atf_ok = true;
    d.oil_c = 105;      d.oil_ok = true;
    d.volts_mv = 14192; d.volts_ok = true;
    out.push_back({"1. PT-CAN, cruising", d});
  }
  {
    // Перегрев коробки: порог 130 по ZF 6HP19.
    ScreenData d;
    d.coolant_c = 98;   d.coolant_ok = true;
    d.atf_c = 134;      d.atf_ok = true;
    d.oil_c = 112;      d.oil_ok = true;
    d.volts_mv = 14020; d.volts_ok = true;
    d.alert = Alert::AtfHot;
    out.push_back({"2. Gearbox overheat", d});
  }
  {
    // Перегрев двигателя перебивает перегрев коробки.
    ScreenData d;
    d.coolant_c = 117;  d.coolant_ok = true;
    d.atf_c = 141;      d.atf_ok = true;
    d.oil_c = 128;      d.oil_ok = true;
    d.volts_mv = 13980; d.volts_ok = true;
    d.alert = Alert::CoolantHot;
    out.push_back({"3. Coolant overheat", d});
  }
  {
    // Холодный пуск зимой, стоим на месте.
    ScreenData d;
    d.coolant_c = 14;   d.coolant_ok = true;
    d.atf_c = -8;       d.atf_ok = true;
    d.oil_c = 12;       d.oil_ok = true;
    d.volts_mv = 14610; d.volts_ok = true;
    out.push_back({"4. Cold start", d});
  }
  {
    // MCP2515 не поднялся: кварц, обрыв CS или терминатор 120 Ом.
    ScreenData d;
    d.can_ok = false;
    out.push_back({"5. CAN init failed", d});
  }
  {
    // Зажигание выключено: шина замолчала, данные протухли.
    ScreenData d;
    out.push_back({"6. Bus silent", d});
  }

  return out;
}

int main(int argc, char** argv) {
  const char* out_path = (argc > 1) ? argv[1] : "sim/out/screens.bmp";

  u8g2_host_init();
  const std::vector<Scenario> scenarios = build_scenarios();

  const int cols = 2;
  const int rows = (static_cast<int>(scenarios.size()) + cols - 1) / cols;
  const int cell_w = W * SCALE + COL_GAP;
  const int cell_h = (CAPTION_H + GAP + H) * SCALE + ROW_GAP;

  Canvas canvas(cols * cell_w - COL_GAP + 2 * MARGIN,
                rows * cell_h - ROW_GAP + 2 * MARGIN);

  for (size_t i = 0; i < scenarios.size(); ++i) {
    const int cx = MARGIN + static_cast<int>(i % cols) * cell_w;
    const int cy = MARGIN + static_cast<int>(i / cols) * cell_h;

    blit_caption(canvas, cx, cy, scenarios[i].caption);

    u8g2_ClearBuffer(&u8g2);
    screen_draw(&u8g2, scenarios[i].data);
    blit_panel(canvas, cx, cy + (CAPTION_H + GAP) * SCALE);
  }

  if (!write_bmp(out_path, canvas)) {
    fprintf(stderr, "cannot write %s\n", out_path);
    return 1;
  }
  printf("wrote %s  (%dx%d, %zu scenarios)\n", out_path, canvas.w, canvas.h,
         scenarios.size());
  return 0;
}
