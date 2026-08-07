#include "PtCan.h"

void PtCan::reset() { d_ = PtCanData(); }

bool PtCan::apply(uint32_t id, const uint8_t* d, uint8_t len, uint32_t now_ms) {
  if (d == nullptr) return false;

  switch (id) {
    case ptcan::ID_ENGINE_DATA: {
      if (len < 2) return false;
      // TEMP_ENG и TEMP_EOI: обе со смещением -48 °C.
      d_.coolant_c = static_cast<int16_t>(d[0]) - 48;
      d_.coolant_ms = now_ms;
      if (d[1] != ptcan::OIL_TEMP_ABSENT) {
        d_.oil_c = static_cast<int16_t>(d[1]) - 48;
        d_.oil_ms = now_ms;
      }
      return true;
    }

    case ptcan::ID_EGS_TORQUE: {
      if (len < 8) return false;
      // TEMP_GRB: стартовый бит 56, то есть байт 7. Смещение -40 °C.
      d_.atf_c = static_cast<int16_t>(d[7]) - 40;
      d_.atf_ms = now_ms;
      // ST_OTMP_GRB: стартовый бит 38 — байт 4, биты 6..7.
      d_.gearbox_temp_st = (d[4] >> 6) & 0x03;
      return true;
    }

    case ptcan::ID_BATTERY: {
      if (len < 2) return false;
      // 12 бит little-endian, 0.015 В на единицу -> милливольты = raw * 15.
      const uint16_t raw = static_cast<uint16_t>(d[0]) |
                           (static_cast<uint16_t>(d[1] & 0x0F) << 8);
      d_.volts_mv = raw * 15u;
      d_.volts_ms = now_ms;
      return true;
    }

    default:
      return false;
  }
}
