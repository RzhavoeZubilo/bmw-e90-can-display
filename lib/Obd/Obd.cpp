#include "Obd.h"

void Obd::reset() {
  d_ = ObdData();
  sup_[0] = sup_[1] = sup_[2] = 0;
  map_seen_ = 0;
}

void Obd::build_request(uint8_t pid, uint8_t* out) {
  if (out == nullptr) return;
  out[0] = 0x02;  // длина полезной части: режим + PID
  out[1] = obd::MODE_CURRENT;
  out[2] = pid;
  // Хвост принято забивать 0x55 либо нулями; на приём это не влияет.
  for (uint8_t i = 3; i < 8; ++i) out[i] = 0x55;
}

bool Obd::have_map(uint8_t block) const {
  return block < 3 && (map_seen_ & (1u << block)) != 0;
}

bool Obd::any_map() const { return map_seen_ != 0; }

bool Obd::supported(uint8_t pid) const {
  if (!any_map()) return true;  // ещё не спросили — считаем, что есть
  if (pid == 0 || pid > 0x60) return false;

  uint8_t block;
  uint8_t base;
  if (pid <= 0x20) {
    block = 0;
    base = 0x00;
  } else if (pid <= 0x40) {
    block = 1;
    base = 0x20;
  } else {
    block = 2;
    base = 0x40;
  }
  if (!have_map(block)) return false;

  // В маске старший бит байта A соответствует первому PID блока.
  const uint8_t idx = pid - base;  // 1..32
  return (sup_[block] >> (32 - idx)) & 1u;
}

uint16_t Obd::fuel_liters_x10() const {
  // pct_x10 (0..1000) -> десятые доли литра.
  return static_cast<uint16_t>(
      (static_cast<uint32_t>(d_.fuel_pct_x10) * obd::TANK_LITERS) / 100u);
}

bool Obd::apply(uint32_t id, const uint8_t* d, uint8_t len, uint32_t now_ms) {
  if (d == nullptr) return false;
  if (id < obd::RESP_ID_FIRST || id > obd::RESP_ID_LAST) return false;
  if (len < 3) return false;

  const uint8_t pci = d[0];
  // Старший ниббл PCI: 0 — однокадровое сообщение. Многокадровые (0x1x)
  // нам сейчас не нужны, стандартные PID в один кадр укладываются.
  if ((pci & 0xF0) != 0x00) return false;

  const uint8_t nbytes = pci & 0x0F;
  if (nbytes < 3 || nbytes > 7) return false;
  if (static_cast<uint16_t>(nbytes) + 1 > len) return false;
  if (d[1] != obd::POSITIVE_RESP) return false;

  const uint8_t pid = d[2];
  const uint8_t plen = nbytes - 2;  // байт полезных данных
  const uint8_t* p = d + 3;

  switch (pid) {
    case obd::PID_SUP_01_20:
    case obd::PID_SUP_21_40:
    case obd::PID_SUP_41_60: {
      if (plen < 4) return false;
      const uint8_t block = pid / 0x20;  // 0x00->0, 0x20->1, 0x40->2
      sup_[block] = (static_cast<uint32_t>(p[0]) << 24) |
                    (static_cast<uint32_t>(p[1]) << 16) |
                    (static_cast<uint32_t>(p[2]) << 8) |
                    static_cast<uint32_t>(p[3]);
      map_seen_ |= (1u << block);
      return true;
    }

    case obd::PID_COOLANT:
      if (plen < 1) return false;
      d_.coolant_c = static_cast<int16_t>(p[0]) - 40;
      d_.coolant_ms = now_ms;
      return true;

    case obd::PID_IAT:
      if (plen < 1) return false;
      d_.iat_c = static_cast<int16_t>(p[0]) - 40;
      d_.iat_ms = now_ms;
      return true;

    case obd::PID_OIL_TEMP:
      if (plen < 1) return false;
      d_.oil_c = static_cast<int16_t>(p[0]) - 40;
      d_.oil_ms = now_ms;
      return true;

    case obd::PID_VOLTAGE:
      if (plen < 2) return false;
      d_.volts_mv = (static_cast<uint16_t>(p[0]) << 8) | p[1];
      d_.volts_ms = now_ms;
      return true;

    case obd::PID_FUEL_PCT:
      if (plen < 1) return false;
      // A*100/255 процентов; храним десятые доли, отсюда 1000.
      d_.fuel_pct_x10 =
          static_cast<uint16_t>((static_cast<uint32_t>(p[0]) * 1000u) / 255u);
      d_.fuel_ms = now_ms;
      return true;

    default:
      return false;
  }
}
