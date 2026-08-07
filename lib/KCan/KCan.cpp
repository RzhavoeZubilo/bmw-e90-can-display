#include "KCan.h"

// Датчик бака: 0.006249 литра на единицу (DBC, сигналы FuelSensorLeft /
// FuelSensorRight). Считаем в десятых долях литра целочисленно:
//   литры_x10 = raw * 0.06249 = raw * 6249 / 100000
// Максимум raw ~10000, произведение ~6.2e7 — в uint32 помещается.
static uint16_t fuel_raw_to_l_x10(uint16_t raw) {
  return static_cast<uint16_t>((static_cast<uint32_t>(raw) * 6249u) / 100000u);
}

void KCan::reset() { d_ = KCanData(); }

bool KCan::apply(uint32_t id, const uint8_t* d, uint8_t len, uint32_t now_ms) {
  if (d == nullptr) return false;

  switch (id) {
    case kcan::ID_ENGINE_DATA: {
      if (len < 2) return false;
      // TEMP_ENG и TEMP_EOI: обе температуры со смещением -48 °C.
      d_.coolant_c = static_cast<int16_t>(d[0]) - 48;
      d_.coolant_ms = now_ms;

      if (d[1] != kcan::OIL_TEMP_ABSENT) {
        d_.oil_c = static_cast<int16_t>(d[1]) - 48;
        d_.oil_ms = now_ms;
      }
      return true;
    }

    case kcan::ID_FUEL_LEVEL: {
      if (len < 4) return false;
      const uint16_t l = static_cast<uint16_t>(d[0]) |
                         (static_cast<uint16_t>(d[1]) << 8);
      const uint16_t r = static_cast<uint16_t>(d[2]) |
                         (static_cast<uint16_t>(d[3]) << 8);
      d_.fuel_left_l_x10 = fuel_raw_to_l_x10(l);
      d_.fuel_right_l_x10 = fuel_raw_to_l_x10(r);
      d_.fuel_ms = now_ms;
      return true;
    }

    case kcan::ID_BATTERY: {
      if (len < 2) return false;
      // 12 бит little-endian, 0.015 В на единицу -> милливольты = raw * 15.
      const uint16_t raw = static_cast<uint16_t>(d[0]) |
                           (static_cast<uint16_t>(d[1] & 0x0F) << 8);
      d_.volts_mv = raw * 15u;
      d_.volts_ms = now_ms;
      return true;
    }

    case kcan::ID_OUTSIDE_TEMP: {
      if (len < 1) return false;
      // 0.5 °C на единицу, смещение -40: десятые доли = raw*5 - 400.
      d_.outside_c_x10 = static_cast<int16_t>(d[0]) * 5 - 400;
      d_.outside_ms = now_ms;
      return true;
    }

    default:
      return false;
  }
}
