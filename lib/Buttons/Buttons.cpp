#include "Buttons.h"

BtnEvent Button::update(bool level, uint32_t now_ms) {
  if (level != last_raw_) {
    last_raw_ = level;
    raw_change_ms_ = now_ms;
  }

  BtnEvent ev = BtnEvent::None;

  if (level != stable_ && (now_ms - raw_change_ms_) >= DEBOUNCE_MS) {
    stable_ = level;
    if (stable_) {
      press_ms_ = now_ms;
      long_fired_ = false;
    } else if (!long_fired_) {
      // Отпустили до порога длинного нажатия — это клик.
      ev = BtnEvent::Click;
    }
  }

  if (stable_ && !long_fired_ && (now_ms - press_ms_) >= LONG_MS) {
    long_fired_ = true;
    ev = BtnEvent::LongPress;
  }

  return ev;
}
