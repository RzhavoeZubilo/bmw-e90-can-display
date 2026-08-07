#pragma once

#include <stdint.h>

// Антидребезг + распознавание короткого/длинного нажатия.
// Чистая логика: на вход подаётся уже считанный уровень и время в мс.
//
// TTP-223 в режиме по умолчанию сам даёт довольно чистый фронт, но при
// длинных проводах DuPont наводки реальны, поэтому дебаунс оставлен.

enum class BtnEvent : uint8_t {
  None = 0,
  Click,      // отпущена раньше LONG_MS
  LongPress,  // удержана LONG_MS, выдаётся один раз, не дожидаясь отпускания
};

class Button {
 public:
  static const uint32_t DEBOUNCE_MS = 25;
  static const uint32_t LONG_MS = 600;

  // level — текущий логический уровень (у TTP-223 нажатие = true).
  BtnEvent update(bool level, uint32_t now_ms);

  bool pressed() const { return stable_; }

 private:
  bool stable_ = false;
  bool last_raw_ = false;
  uint32_t raw_change_ms_ = 0;
  uint32_t press_ms_ = 0;
  bool long_fired_ = false;
};
