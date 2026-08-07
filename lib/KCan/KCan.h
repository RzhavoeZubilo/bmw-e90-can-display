#pragma once

#include <stdint.h>

// Декодер широковещательных кадров K-CAN (100 кбит/с) для E90.
//
// В отличие от диагностической шины, здесь ничего запрашивать не нужно:
// блоки сами постоянно вещают. Чистая логика без Arduino — тестируется
// на хосте реальными кадрами из захватов.
//
// ИСТОЧНИК ОПРЕДЕЛЕНИЙ: bmw_e9x_e8x1_merged.dbc из репозитория
// DBC-mega-merge. Все сигналы ниже подтверждены захватами с живой машины
// и HIL-стенда, а не выведены из общих соображений.

namespace kcan {

static const uint32_t ID_ENGINE_DATA = 0x1D0;  // DME: температуры ОЖ и масла
static const uint32_t ID_FUEL_LEVEL = 0x349;   // Gate1: датчики баков, литры
static const uint32_t ID_BATTERY = 0x3B4;      // DME: напряжение бортсети
static const uint32_t ID_OUTSIDE_TEMP = 0x2CA; // Kombi: забортная температура

// Масло двигателя: на дизелях N47 байт всегда 0xFF — сигнал не выведен
// на K-CAN. Это не ошибка чтения, а отсутствие данных.
static const uint8_t OIL_TEMP_ABSENT = 0xFF;

}  // namespace kcan

struct KCanData {
  int16_t coolant_c = 0;
  int16_t oil_c = 0;
  int16_t outside_c_x10 = 0;   // десятые доли °C
  uint16_t volts_mv = 0;
  uint16_t fuel_left_l_x10 = 0;   // десятые доли литра
  uint16_t fuel_right_l_x10 = 0;

  uint32_t coolant_ms = 0;
  uint32_t oil_ms = 0;
  uint32_t outside_ms = 0;
  uint32_t volts_ms = 0;
  uint32_t fuel_ms = 0;
};

class KCan {
 public:
  // Все перечисленные кадры вещаются чаще, чем раз в секунду.
  static const uint32_t STALE_MS = 2000;

  void reset();

  // true, если кадр распознан и что-то обновилось.
  bool apply(uint32_t id, const uint8_t* d, uint8_t len, uint32_t now_ms);

  const KCanData& data() const { return d_; }

  static bool fresh(uint32_t stamp, uint32_t now_ms) {
    return stamp != 0 && (now_ms - stamp) < STALE_MS;
  }

  uint16_t fuel_total_l_x10() const {
    return d_.fuel_left_l_x10 + d_.fuel_right_l_x10;
  }

 private:
  KCanData d_;
};
