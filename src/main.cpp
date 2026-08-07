#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <avr/wdt.h>

#include <FastLED.h>
#include <U8g2lib.h>
#include <mcp_can.h>

#include "Alerts.h"
#include "PtCan.h"
#include "Screen.h"
#include "pins.h"

// Page-режим U8g2: буфер 128 байт вместо 1024. Полнокадровый на Nano
// вместе с остальным не помещается.
static U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

static MCP_CAN can(PIN_CAN_CS);
static CRGB leds[NUM_LEDS];

static PtCan pt;

static const uint16_t BLINK_MS = 400;

static bool can_ok = false;
static Alert cur_alert = Alert::None;

// Кадры, которые нам нужны. У MCP2515 шесть аппаратных фильтров; заняты
// три, остальные пока дублируют последний. Три свободных — это запас под
// то, что найдёт разведка (температура впуска, топливо).
static const uint32_t kWanted[] = {
    ptcan::ID_EGS_TORQUE,  // 0x0B5 температура масла АКПП
    ptcan::ID_ENGINE_DATA, // 0x1D0 ОЖ и масло двигателя
    ptcan::ID_BATTERY,     // 0x3B4 напряжение
};
static const uint8_t N_WANTED = sizeof(kWanted) / sizeof(kWanted[0]);

static void setup_can() {
  const uint8_t clk =
#if MCP2515_CRYSTAL_MHZ == 16
      MCP_16MHZ;
#else
      MCP_8MHZ;
#endif

  // PT-CAN — 500 кбит/с.
  can_ok = (can.begin(MCP_ANY, CAN_500KBPS, clk) == CAN_OK);
  if (!can_ok) return;

  // Без фильтров тут никак: PT-CAN даёт порядка полутора тысяч кадров в
  // секунду, у MCP2515 всего два приёмных буфера, и Nano разгребает их по
  // SPI. Переполнение было бы молчаливым — кадры просто терялись бы.
  // Маска 0x7FF означает точное совпадение всех 11 бит.
  can.init_Mask(0, 0, 0x07FF);
  can.init_Mask(1, 0, 0x07FF);

  // Буфер RXB0 обслуживают фильтры 0..1, RXB1 — фильтры 2..5.
  // Незанятый фильтр пропускал бы всё подряд, поэтому лишние дублируют
  // последний нужный ID.
  for (uint8_t i = 0; i < 6; ++i) {
    can.init_Filt(i, 0, kWanted[i < N_WANTED ? i : N_WANTED - 1]);
  }

  // Только слушаем. В моторную шину машины мы ничего не передаём.
  can.setMode(MCP_LISTENONLY);
  pinMode(PIN_CAN_INT, INPUT);
}

static void poll_rx() {
  const uint32_t now = millis();
  while (digitalRead(PIN_CAN_INT) == LOW) {
    // Тип диктует библиотека: INT32U — это unsigned long, а не uint32_t.
    // На AVR они совпадают, на 32-битных платформах нет.
    unsigned long id = 0;
    uint8_t len = 0, buf[8];
    if (can.readMsgBuf(&id, &len, buf) != CAN_OK) break;
    pt.apply(static_cast<uint32_t>(id) & 0x7FFul, buf, len, now);
  }
}

static void update_alerts() {
  const uint32_t now = millis();
  const PtCanData& d = pt.data();

  cur_alert = alerts::evaluate(d.coolant_c, PtCan::fresh(d.coolant_ms, now),
                               d.atf_c, PtCan::fresh(d.atf_ms, now));

  // Желаемое состояние светодиода: 0 погашен, 1 красный (двигатель),
  // 2 оранжевый (коробка). Медленное мигание — период BLINK_MS.
  uint8_t want = 0;
  if (cur_alert != Alert::None && (now / BLINK_MS) % 2)
    want = (cur_alert == Alert::CoolantHot) ? 1 : 2;

  // ВАЖНО: FastLED.show() на AVR запрещает прерывания на всё время
  // передачи. Вызов на каждом проходе loop() держал бы процессор с
  // выключенными прерываниями почти постоянно: начинает отставать millis(),
  // а на нём висит вся логика протухания данных, ломается tone() и
  // страдает приём по I2C. Поэтому дёргаем только при смене состояния —
  // это пара раз в секунду вместо тысяч.
  static uint8_t shown = 0xFF;
  if (want != shown) {
    shown = want;
    fill_solid(leds, NUM_LEDS,
               want == 0 ? CRGB::Black
                         : (want == 1 ? CRGB::Red : CRGB::OrangeRed));
    FastLED.show();
  }

  static Alert prev = Alert::None;
  if (cur_alert != Alert::None && prev != cur_alert) {
    tone(PIN_BUZZER, cur_alert == Alert::CoolantHot ? 2000 : 1500, 200);
  }
  prev = cur_alert;
}

// Вся вёрстка живёт в lib/Screen на C-API u8g2 — тот же код гоняет
// симулятор на хосте (см. sim/), поэтому картинка не расходится с реальной.
static void draw() {
  const uint32_t now = millis();
  const PtCanData& d = pt.data();

  ScreenData sd;
  sd.can_ok = can_ok;
  sd.alert = cur_alert;

  sd.coolant_c = d.coolant_c;
  sd.coolant_ok = PtCan::fresh(d.coolant_ms, now);
  sd.atf_c = d.atf_c;
  sd.atf_ok = PtCan::fresh(d.atf_ms, now);
  sd.oil_c = d.oil_c;
  sd.oil_ok = PtCan::fresh(d.oil_ms, now);
  sd.volts_mv = d.volts_mv;
  sd.volts_ok = PtCan::fresh(d.volts_ms, now);
  // Температура впуска и топливо на PT-CAN пока не найдены — см. README,
  // раздел про разведку. Места в вёрстке готовы.
  sd.iat_ok = false;
  sd.fuel_ok = false;
  sd.tanks_ok = false;

  screen_draw(oled.getU8g2(), sd);
}

void setup() {
  pinMode(PIN_BUZZER, OUTPUT);

  oled.begin();
  Wire.setClock(400000);

  FastLED.addLeds<WS2812B, PIN_LEDS, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(60);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  setup_can();

  // Сторожевой таймер включается последним: инициализация дисплея и
  // MCP2515 не должна его тревожить. Устройство живёт в машине без
  // присмотра, зависший блок там никто не перезагрузит.
  //
  // Безопасно именно потому, что плата на новом загрузчике (Optiboot,
  // board = nanoatmega328new): он сбрасывает флаг WDRF и снимает таймер
  // при старте. Старый загрузчик после срабатывания WDT уходил в
  // бесконечную перезагрузку.
  wdt_enable(WDTO_2S);
}

void loop() {
  wdt_reset();

  poll_rx();
  update_alerts();

  static uint32_t draw_ms = 0;
  const uint32_t now = millis();
  if (now - draw_ms >= 100) {
    draw_ms = now;
    draw();
  }
}
