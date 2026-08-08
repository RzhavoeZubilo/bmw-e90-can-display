// Диагностический инструмент. Собрать и залить:
//   pio run -e sniffer -t upload && pio device monitor
//
// ГЛАВНЫЙ ВОПРОС, НА КОТОРЫЙ ОН ОТВЕЧАЕТ: к какой шине вы подключились.
// У E90 три кандидата, и на глаз они не различаются:
//   K-CAN   100 кбит/с — кузов, приборка, баки, кнопки руля
//   PT-CAN  500 кбит/с — мотор, EGS, температура масла АКПП
//   D-CAN   500 кбит/с — диагностика, молчит пока не спросят
//
// Фаза 1 — ПЕРЕПИСЬ, дважды по 10 секунд: на 100 и на 500 кбит/с.
//   Слушает молча (MCP_LISTENONLY), в шину ничего не уходит. Считает кадры,
//   собирает словарь уникальных ID и сверяет его с маркерами, у которых
//   известна принадлежность к шине. Печатается только итоговая таблица:
//   сырой дамп на живой PT-CAN — это полторы-две тысячи кадров в секунду,
//   Serial на 115200 таким потоком захлебнётся.
//
//   Тишина на обеих скоростях -> D-CAN, работает только опрос запросами.
//   Трафик -> смотрите вердикт по маркерам внизу таблицы.
//
// Фаза 2 — СВИП PID на 500 кбит/с. Перебирает стандартные PID режима 01
//   (0x00..0xA0) и печатает ответы. Диапазон расширен до 0xA0: DBC-mega-merge
//   упоминает PID 0x6C как температуру масла АКПП (не подтверждено).
//
// Для BMW-специфичных данных на PT-CAN есть адресный UDS-канал:
//   DME  запрос 0x600 -> ответ 0x608
//   EGS  запрос 0x604 -> ответ 0x60C   (адаптации и калибровки коробки)
//   DSC  запрос 0x612 -> ответ 0x61A
// Сервис 0x22 с нужным идентификатором данных шлётся туда же.

#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>

#include "pins.h"

static MCP_CAN can(PIN_CAN_CS);

static const uint32_t REQ_ID = 0x7DF;
static const uint32_t CENSUS_MS = 10000;
static const uint16_t STEP_MS = 150;

// Словарь ID. 40 записей с запасом: на PT-CAN у E90 обычно
// несколько десятков уникальных идентификаторов.
static const uint8_t MAX_IDS = 40;

struct Seen {
  uint16_t id;
  uint16_t count;
  uint8_t len;
  uint8_t data[8];
};

static Seen tbl[MAX_IDS];
static uint8_t n_ids = 0;
static uint32_t total_frames = 0;
static uint16_t overflow_ids = 0;  // сколько ID не влезло в таблицу

// Маркеры шин. 'P' встречается только на PT-CAN, 'K' — только на K-CAN,
// 'B' есть на обеих (DME вещает, шлюз ретранслирует). По набору сразу
// понятно, куда вы подключились. Определения из DBC-mega-merge.
struct Known {
  uint16_t id;
  char bus;
  const char* name;
};

static const Known kKnown[] = {
    {0x0B5, 'P', "EGS torque + ATF temp"},
    {0x0BA, 'P', "TransmissionData gear"},
    {0x198, 'P', "GearSelectorSwitch"},
    {0x349, 'K', "FuelLevel left/right"},
    {0x2CA, 'K', "OutsideTemp"},
    {0x1D6, 'K', "SteeringButtons"},
    {0x0AA, 'B', "AccPedal/RPM"},
    {0x1D0, 'B', "EngineData coolant/oil"},
    {0x3B4, 'B', "BatteryVoltage"},
};
static const uint8_t N_KNOWN = sizeof(kKnown) / sizeof(kKnown[0]);

static const Known* known_entry(uint16_t id) {
  for (uint8_t i = 0; i < N_KNOWN; ++i) {
    if (kKnown[i].id == id) return &kKnown[i];
  }
  return nullptr;
}

static void record(uint16_t id, uint8_t len, const uint8_t* d) {
  ++total_frames;
  for (uint8_t i = 0; i < n_ids; ++i) {
    if (tbl[i].id == id) {
      ++tbl[i].count;
      tbl[i].len = len;
      for (uint8_t k = 0; k < len && k < 8; ++k) tbl[i].data[k] = d[k];
      return;
    }
  }
  if (n_ids >= MAX_IDS) {
    ++overflow_ids;
    return;
  }
  tbl[n_ids].id = id;
  tbl[n_ids].count = 1;
  tbl[n_ids].len = len;
  for (uint8_t k = 0; k < len && k < 8; ++k) tbl[n_ids].data[k] = d[k];
  ++n_ids;
}

static void print_hex(uint8_t v) {
  if (v < 0x10) Serial.print('0');
  Serial.print(v, HEX);
}

static void reset_table() {
  n_ids = 0;
  total_frames = 0;
  overflow_ids = 0;
}

static void census(const __FlashStringHelper* label) {
  Serial.print(F("== phase 1: silent census @ "));
  Serial.print(label);
  Serial.println(F(", 10 s =="));
  const uint32_t t0 = millis();
  while (millis() - t0 < CENSUS_MS) {
    if (digitalRead(PIN_CAN_INT) != LOW) continue;
    // INT32U из библиотеки — это unsigned long, а не uint32_t.
    unsigned long id = 0;
    uint8_t len = 0, buf[8];
    if (can.readMsgBuf(&id, &len, buf) != CAN_OK) continue;
    record(static_cast<uint16_t>(id & 0x7FFul), len, buf);
  }

  Serial.print(F("frames: "));
  Serial.print(total_frames);
  Serial.print(F("   ~"));
  Serial.print(total_frames / (CENSUS_MS / 1000));
  Serial.print(F("/s   unique ids: "));
  Serial.println(n_ids);

  if (n_ids == 0) {
    Serial.println(F("SILENT at this bitrate."));
    return;
  }

  // Сортируем по убыванию частоты: периодические кадры всплывают наверх,
  // а редкие события оседают вниз. Выбором, n не больше 40.
  for (uint8_t i = 0; i + 1 < n_ids; ++i) {
    uint8_t mx = i;
    for (uint8_t j = i + 1; j < n_ids; ++j)
      if (tbl[j].count > tbl[mx].count) mx = j;
    if (mx != i) {
      Seen t = tbl[i];
      tbl[i] = tbl[mx];
      tbl[mx] = t;
    }
  }

  Serial.println(F("TRAFFIC -> broadcast bus. Hardware filters required."));
  Serial.println(F("id\tcount\tdata\t\tsignal"));

  uint8_t hits_pt = 0, hits_k = 0;
  for (uint8_t i = 0; i < n_ids; ++i) {
    Serial.print(F("0x"));
    Serial.print(tbl[i].id, HEX);
    Serial.print(F("\t"));
    Serial.print(tbl[i].count);
    Serial.print(F("\t"));
    for (uint8_t k = 0; k < tbl[i].len; ++k) {
      print_hex(tbl[i].data[k]);
      Serial.print(' ');
    }
    const Known* k = known_entry(tbl[i].id);
    if (k) {
      if (k->bus == 'P') ++hits_pt;
      if (k->bus == 'K') ++hits_k;
      Serial.print(F("\t<- ["));
      Serial.print(k->bus);
      Serial.print(F("] "));
      Serial.print(k->name);
    }
    Serial.println();
  }

  Serial.print(F("markers: PT-CAN-only "));
  Serial.print(hits_pt);
  Serial.print(F(", K-CAN-only "));
  Serial.println(hits_k);
  if (hits_pt > hits_k) {
    Serial.println(F("=> PT-CAN. lib/PtCan decoders apply. ATF temp available."));
  } else if (hits_k > hits_pt) {
    Serial.println(F("=> K-CAN. lib/KCan applies. No ATF temp on this bus."));
  } else {
    Serial.println(F("=> inconclusive. Compare ids against the DBC by hand."));
  }
  if (overflow_ids) {
    Serial.print(F("WARNING: table full, ids dropped: "));
    Serial.println(overflow_ids);
  }
}

void setup() {
  Serial.begin(115200);
  // Ожидания готовности порта здесь нет намеренно: у ATmega328P
  // operator bool() класса HardwareSerial всегда возвращает true, так что
  // строка вида while (!Serial) была бы пустой формальностью. Она нужна
  // только платам с нативным USB (Leonardo, Micro), где действительно
  // подвесила бы устройство при автономном запуске без компьютера.

  const uint8_t clk =
#if MCP2515_CRYSTAL_MHZ == 16
      MCP_16MHZ;
#else
      MCP_8MHZ;
#endif

  // Перепись идёт на обеих скоростях: K-CAN работает на 100 кбит/с,
  // PT-CAN и D-CAN — на 500. На неверной скорости шина выглядит немой,
  // поэтому одной попытки мало.
  if (can.begin(MCP_ANY, CAN_100KBPS, clk) != CAN_OK) {
    Serial.println(F("MCP2515 init FAILED: check CS wiring and crystal"));
    while (true) {}
  }
  pinMode(PIN_CAN_INT, INPUT);
  can.setMode(MCP_LISTENONLY);
  census(F("100 kbit/s (K-CAN)"));

  reset_table();
  if (can.begin(MCP_ANY, CAN_500KBPS, clk) != CAN_OK) {
    Serial.println(F("MCP2515 re-init at 500k FAILED"));
    while (true) {}
  }
  can.setMode(MCP_LISTENONLY);
  census(F("500 kbit/s (PT-CAN / D-CAN)"));

  // Свип идёт на 500: диагностика живёт только там.
  if (can.begin(MCP_ANY, CAN_500KBPS, clk) != CAN_OK) {
    Serial.println(F("MCP2515 re-init for sweep FAILED"));
    while (true) {}
  }
  can.setMode(MCP_NORMAL);  // для передачи нужен нормальный режим
  Serial.println(F("== phase 2: PID sweep 0x00..0xA0 @ 500 kbit/s =="));
}

void loop() {
  static uint8_t pid = 0x00;
  static uint32_t last = 0;

  while (digitalRead(PIN_CAN_INT) == LOW) {
    unsigned long id = 0;
    uint8_t len = 0, buf[8];
    if (can.readMsgBuf(&id, &len, buf) != CAN_OK) break;
    // В фазе 2 интересны только ответы диагностики, остальное молча
    // пропускаем, чтобы широковещание PT-CAN не забило вывод.
    const uint32_t clean = static_cast<uint32_t>(id) & 0x1FFFFFFFul;
    if (clean < 0x7E8 || clean > 0x7EF) continue;
    Serial.print(F("   <- 0x"));
    Serial.print(clean, HEX);
    Serial.print(F("  "));
    for (uint8_t i = 0; i < len; ++i) {
      print_hex(buf[i]);
      Serial.print(' ');
    }
    Serial.println();
  }

  if (millis() - last < STEP_MS) return;
  last = millis();

  uint8_t req[8] = {0x02, 0x01, pid, 0x55, 0x55, 0x55, 0x55, 0x55};
  Serial.print(F("-> PID 0x"));
  print_hex(pid);
  Serial.println();
  can.sendMsgBuf(REQ_ID, 0, 8, req);

  if (pid == 0xA0) {
    // Фаза 3: адресный запрос к DME на PT-CAN. Печатается сырой ответ —
    // по нему проверяется раскладка, которую lib/Diag пока лишь выводит
    // из порядка результатов в SGBD, а не знает наверняка.
    Serial.println(F("== phase 3: DME intake temp, raw response =="));
    uint8_t r[8] = {0x03, 0x30, 0x0A, 0x01, 0x55, 0x55, 0x55, 0x55};
    Serial.println(F("-> 0x600  03 30 0A 01"));
    can.sendMsgBuf(0x600, 0, 8, r);
    delay(200);
    while (digitalRead(PIN_CAN_INT) == LOW) {
      unsigned long id = 0;
      uint8_t len = 0, buf[8];
      if (can.readMsgBuf(&id, &len, buf) != CAN_OK) break;
      if ((id & 0x7FFul) != 0x608) continue;
      Serial.print(F("   <- 0x608  "));
      for (uint8_t i = 0; i < len; ++i) { print_hex(buf[i]); Serial.print(' '); }
      Serial.println();
      Serial.println(F("   байты 6-7 при делении на 16 должны дать разумную"));
      Serial.println(F("   температуру впуска; если нет — раскладка иная"));
    }
    Serial.println(F("== done, restarting =="));
    pid = 0x00;
  } else {
    ++pid;
  }
}
