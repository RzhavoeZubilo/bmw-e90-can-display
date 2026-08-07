#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <avr/wdt.h>
#include <EEPROM.h>

#include <FastLED.h>
#include <U8g2lib.h>
#include <mcp_can.h>

#include "Alerts.h"
#include "BtnLearn.h"
#include "Buttons.h"
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

// --- кнопка руля ---

// Идентификатор кадра кнопок заранее неизвестен: на PT-CAN он у нас не
// подтверждён. Устройство находит его само при первом включении и хранит
// привязку в EEPROM. Подробности алгоритма — в lib/BtnLearn.
static const uint16_t EE_ADDR = 0;
static const uint8_t EE_MAGIC = 0xB7;   // менять при смене формата записи

static const uint16_t LEARN_BASELINE_MS = 15000;
static const uint16_t LEARN_HOLD_MS = 30000;
static const uint16_t LEARN_SAVED_MS = 2000;

static BtnLearn learner;
static BtnBinding binding;
static Button wheel_btn;

static LearnPhase learn_phase = LearnPhase::None;
static uint32_t learn_t0 = 0;
static ScreenId screen = ScreenId::All;

static void ee_load() {
  uint8_t magic = EEPROM.read(EE_ADDR);
  if (magic != EE_MAGIC) return;
  uint8_t lo = EEPROM.read(EE_ADDR + 1);
  uint8_t hi = EEPROM.read(EE_ADDR + 2);
  BtnBinding b;
  b.id = static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
  b.byte_idx = EEPROM.read(EE_ADDR + 3);
  b.mask = EEPROM.read(EE_ADDR + 4);
  // Пустая маска смысла не имеет — считаем запись битой.
  if (b.mask == 0) return;
  b.valid = true;
  binding = b;
}

static void ee_save(const BtnBinding& b) {
  EEPROM.update(EE_ADDR + 1, static_cast<uint8_t>(b.id & 0xFF));
  EEPROM.update(EE_ADDR + 2, static_cast<uint8_t>(b.id >> 8));
  EEPROM.update(EE_ADDR + 3, b.byte_idx);
  EEPROM.update(EE_ADDR + 4, b.mask);
  EEPROM.update(EE_ADDR, EE_MAGIC);   // магия пишется последней
}

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

  // Во время обучения фильтры должны быть открыты: мы ещё не знаем, в каком
  // кадре живёт кнопка. Нулевая маска означает «пропускать всё».
  if (learn_phase != LearnPhase::None) {
    can.init_Mask(0, 0, 0x0000);
    can.init_Mask(1, 0, 0x0000);
  } else {
    // Буфер RXB0 обслуживают фильтры 0..1, RXB1 — фильтры 2..5.
    // Незанятый фильтр пропускал бы всё подряд, поэтому лишние дублируют
    // последний нужный ID. Найденный кадр кнопки занимает свободный слот.
    for (uint8_t i = 0; i < 6; ++i) {
      uint32_t id = kWanted[i < N_WANTED ? i : N_WANTED - 1];
      if (i == N_WANTED && binding.valid) id = binding.id;
      can.init_Filt(i, 0, id);
    }
  }

  // Только слушаем. В моторную шину машины мы ничего не передаём.
  can.setMode(MCP_LISTENONLY);
  pinMode(PIN_CAN_INT, INPUT);
}

// Уровень кнопки обновляется только приходом её кадра. Сбрасывать его на
// каждом проходе loop() нельзя: кадры идут периодически, между ними уровень
// не меняется, и обнуление давало бы ложные отпускания.
static bool btn_level = false;

static void poll_rx() {
  const uint32_t now = millis();
  while (digitalRead(PIN_CAN_INT) == LOW) {
    // Тип диктует библиотека: INT32U — это unsigned long, а не uint32_t.
    // На AVR они совпадают, на 32-битных платформах нет.
    unsigned long id = 0;
    uint8_t len = 0, buf[8];
    if (can.readMsgBuf(&id, &len, buf) != CAN_OK) break;
    const uint16_t cid = static_cast<uint16_t>(id & 0x7FFul);

    pt.apply(cid, buf, len, now);

    if (learn_phase == LearnPhase::Baseline) {
      learner.observe_baseline(cid, buf, len);
    } else if (learn_phase == LearnPhase::Hold) {
      if (learner.observe_hunt(cid, buf, len)) {
        binding = learner.result();
        ee_save(binding);
        learn_phase = LearnPhase::Saved;
        learn_t0 = now;
      }
    } else if (binding.valid && cid == binding.id) {
      btn_level = BtnLearn::pressed(binding, cid, buf, len);
    }
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

static void poll_learn() {
  if (learn_phase == LearnPhase::None) return;
  const uint32_t now = millis();

  switch (learn_phase) {
    case LearnPhase::Baseline:
      if (now - learn_t0 >= LEARN_BASELINE_MS) {
        learn_phase = LearnPhase::Hold;
        learn_t0 = now;
      }
      break;

    case LearnPhase::Hold:
      // Нажатие ловится в poll_rx; сюда попадаем только если не дождались.
      if (now - learn_t0 >= LEARN_HOLD_MS) {
        learn_phase = LearnPhase::Failed;
        learn_t0 = now;
      }
      break;

    case LearnPhase::Saved:
    case LearnPhase::Failed:
      if (now - learn_t0 >= LEARN_SAVED_MS) {
        learn_phase = LearnPhase::None;
        // Обучение шло с открытыми фильтрами; теперь можно сузить приём
        // до нужных кадров и добавить туда найденную кнопку.
        setup_can();
      }
      break;

    default:
      break;
  }
}

static void poll_button() {
  if (!binding.valid || learn_phase != LearnPhase::None) return;
  if (wheel_btn.update(btn_level, millis()) == BtnEvent::Click) {
    screen = static_cast<ScreenId>(
        (static_cast<uint8_t>(screen) + 1) %
        static_cast<uint8_t>(ScreenId::COUNT));
  }
}

// Вся вёрстка живёт в lib/Screen на C-API u8g2 — тот же код гоняет
// симулятор на хосте (см. sim/), поэтому картинка не расходится с реальной.
static void draw() {
  const uint32_t now = millis();
  const PtCanData& d = pt.data();

  ScreenData sd;
  sd.screen = screen;
  sd.learn = learn_phase;
  sd.learn_secs = (learn_phase == LearnPhase::Baseline)
                      ? static_cast<uint8_t>((LEARN_BASELINE_MS -
                                              (now - learn_t0)) / 1000)
                      : 0;
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

  ee_load();
  // Привязки нет — значит первое включение, уходим в обучение.
  if (!binding.valid) {
    learn_phase = LearnPhase::Baseline;
    learn_t0 = millis();
    learner.reset();
  }

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
  poll_learn();
  poll_button();
  update_alerts();

  static uint32_t draw_ms = 0;
  const uint32_t now = millis();
  if (now - draw_ms >= 100) {
    draw_ms = now;
    draw();
  }
}
