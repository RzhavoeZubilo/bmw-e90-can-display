#include "Alerts.h"

namespace alerts {

Alert evaluate(int16_t coolant_c, bool coolant_valid, int16_t atf_c,
               bool atf_valid) {
  if (coolant_valid && coolant_c >= COOLANT_HOT_C) return Alert::CoolantHot;
  if (atf_valid && atf_c >= ATF_HOT_C) return Alert::AtfHot;
  return Alert::None;
}

}  // namespace alerts
