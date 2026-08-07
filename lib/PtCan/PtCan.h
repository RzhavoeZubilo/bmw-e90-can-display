#pragma once

#include <stdint.h>

// Декодер широковещательных кадров PT-CAN (500 кбит/с) для E90.
//
// PT-CAN — моторная шина: DME, EGS, DSC. Здесь, в отличие от диагностической
// шины, блоки вещают сами, запрашивать ничего не нужно. Главное её
// преимущество для нас: только тут есть температура масла АКПП.
//
// ИСТОЧНИК ОПРЕДЕЛЕНИЙ: bmw_e9x_e8x1_merged.dbc (DBC-mega-merge).
// Кадры DME (0x1D0, 0x3B4) подтверждены реальными захватами — они же
// ретранслируются шлюзом на K-CAN, откуда сняты логи. Кадр EGS (0x0B5)
// на K-CAN не выходит, поэтому проверен только по DBC: разрядность и
// формулы взяты оттуда, но байтов с живой машины у нас нет.

namespace ptcan {

static const uint32_t ID_EGS_TORQUE = 0x0B5;   // EGS: температура масла АКПП
static const uint32_t ID_ENGINE_DATA = 0x1D0;  // DME: температуры ОЖ и масла
static const uint32_t ID_BATTERY = 0x3B4;      // DME: напряжение бортсети

// Масло двигателя: на дизелях N47 байт всегда 0xFF. На бензиновых
// (включая N46) приходит настоящее значение.
static const uint8_t OIL_TEMP_ABSENT = 0xFF;

// ST_OTMP_GRB — статус перегрева коробки, который EGS считает сам.
static const uint8_t GEARBOX_TEMP_OK = 0;

}  // namespace ptcan

struct PtCanData {
  int16_t coolant_c = 0;
  int16_t oil_c = 0;
  int16_t atf_c = 0;
  uint16_t volts_mv = 0;
  uint8_t gearbox_temp_st = 0;  // ST_OTMP_GRB, 0 = норма

  uint32_t coolant_ms = 0;
  uint32_t oil_ms = 0;
  uint32_t atf_ms = 0;
  uint32_t volts_ms = 0;
};

class PtCan {
 public:
  // Перечисленные кадры вещаются чаще, чем раз в 100 мс.
  static const uint32_t STALE_MS = 1000;

  void reset();

  // true, если кадр распознан и что-то обновилось.
  bool apply(uint32_t id, const uint8_t* d, uint8_t len, uint32_t now_ms);

  const PtCanData& data() const { return d_; }

  static bool fresh(uint32_t stamp, uint32_t now_ms) {
    return stamp != 0 && (now_ms - stamp) < STALE_MS;
  }

 private:
  PtCanData d_;
};
