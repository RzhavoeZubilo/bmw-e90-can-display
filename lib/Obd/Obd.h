#pragma once

#include <stdint.h>

// Клиент OBD-II (ISO 15765-4) поверх CAN. Чистая логика без Arduino —
// собирается и тестируется на хосте.
//
// Схема обмена: мы шлём запрос на функциональный адрес 0x7DF, отвечает
// один или несколько ЭБУ с адресов 0x7E8..0x7EF. Диагностическая шина
// молчит, пока её не спросили, — пассивное прослушивание тут бесполезно.

namespace obd {

static const uint32_t REQ_ID = 0x7DF;
static const uint32_t RESP_ID_FIRST = 0x7E8;
static const uint32_t RESP_ID_LAST = 0x7EF;

static const uint8_t MODE_CURRENT = 0x01;
static const uint8_t POSITIVE_RESP = 0x41;  // MODE_CURRENT + 0x40
static const uint8_t NEGATIVE_RESP = 0x7F;

// Карты поддерживаемых PID: ответ на них — 4 байта битовой маски.
static const uint8_t PID_SUP_01_20 = 0x00;
static const uint8_t PID_SUP_21_40 = 0x20;
static const uint8_t PID_SUP_41_60 = 0x40;

static const uint8_t PID_COOLANT = 0x05;   // A-40, °C
static const uint8_t PID_IAT = 0x0F;       // A-40, °C, температура впуска
static const uint8_t PID_FUEL_PCT = 0x2F;  // A*100/255, % бака
static const uint8_t PID_VOLTAGE = 0x42;   // (A*256+B), мВ
static const uint8_t PID_OIL_TEMP = 0x5C;  // A-40, °C, поддерживается не всеми

// Объём бака E90 седан. Стандартный OBD отдаёт только проценты,
// литры считаются отсюда — точность соответствующая.
static const uint8_t TANK_LITERS = 61;

}  // namespace obd

struct ObdData {
  int16_t coolant_c = 0;
  int16_t iat_c = 0;
  int16_t oil_c = 0;
  uint16_t volts_mv = 0;
  uint16_t fuel_pct_x10 = 0;

  // Ноль означает «ни разу не приходило».
  uint32_t coolant_ms = 0;
  uint32_t iat_ms = 0;
  uint32_t oil_ms = 0;
  uint32_t volts_ms = 0;
  uint32_t fuel_ms = 0;
};

class Obd {
 public:
  // Опрос идёт по кругу, поэтому окно протухания шире, чем у вещания.
  static const uint32_t STALE_MS = 3000;

  void reset();

  // Собирает однокадровый запрос Mode 01. out — ровно 8 байт.
  static void build_request(uint8_t pid, uint8_t* out);

  // Разбирает ответный кадр. true, если кадр наш и что-то обновилось.
  bool apply(uint32_t id, const uint8_t* d, uint8_t len, uint32_t now_ms);

  const ObdData& data() const { return d_; }

  // Пришла ли карта поддержки для блока 0..2 (0x00, 0x20, 0x40).
  bool have_map(uint8_t block) const;
  bool any_map() const;

  // Поддерживается ли PID. Пока ни одной карты не пришло, отвечаем true —
  // иначе на старте опрос вообще не начнётся.
  bool supported(uint8_t pid) const;

  static bool fresh(uint32_t stamp, uint32_t now_ms) {
    return stamp != 0 && (now_ms - stamp) < STALE_MS;
  }

  // Литры из процентов. Отдаём десятые доли, чтобы не тащить float.
  uint16_t fuel_liters_x10() const;

 private:
  ObdData d_;
  uint32_t sup_[3] = {0, 0, 0};
  uint8_t map_seen_ = 0;  // битовая маска полученных карт
};
