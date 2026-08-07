#pragma once

#include <stdint.h>

// Поиск кнопки руля в потоке кадров, без знания её идентификатора заранее.
//
// Зачем так. Кадр кнопок MFL у нас подтверждён только для K-CAN (0x1D6 в
// захватах DBC-mega-merge). Есть ли он на PT-CAN и в том же ли виде — мы не
// знаем: PT-CAN-логов нет вообще. Вместо того чтобы гадать, устройство
// выясняет это само на конкретной машине.
//
// Алгоритм в два прохода:
//
//   1. БАЗА. Водитель ничего не трогает. Для каждого ID копим маску битов,
//      которые хоть раз были единицей. Это «фон» — всё, что шевелится само.
//
//   2. ПОИСК. Водитель удерживает нужную кнопку. Ищем бит, который сейчас
//      единица, а в фоне единицей не был ни разу. Чтобы не поймать случайный
//      кадр, требуем повторения CONFIRM_HITS раз.
//
// Кадры, не попавшие в фон, на втором проходе игнорируются — см. комментарий
// в observe_hunt(). Это защита от ложной привязки при переполнении таблицы.
//
// Чистая логика без Arduino — проверяется тестами на хосте.

struct BtnBinding {
  uint16_t id = 0;
  uint8_t byte_idx = 0;
  uint8_t mask = 0;
  bool valid = false;
};

class BtnLearn {
 public:
  // 28 идентификаторов хватает: на PT-CAN у E90 их несколько десятков, а
  // кнопки почти наверняка среди часто повторяющихся.
  static const uint8_t MAX_IDS = 28;

  // Следим только за первыми четырьмя байтами: кадр кнопок короткий
  // (у 0x1D6 всего два байта), а память на Nano дорогая.
  static const uint8_t TRACK_BYTES = 4;

  // Сколько раз подряд нужно увидеть кандидата, чтобы ему поверить.
  static const uint8_t CONFIRM_HITS = 5;

  void reset();

  // Фаза 1: копим фон.
  void observe_baseline(uint16_t id, const uint8_t* d, uint8_t len);

  // Сколько ID успели увидеть — для диагностики и для проверки, что шина
  // вообще говорит.
  uint8_t seen_ids() const { return n_; }

  // Фаза 2: ищем новый бит. true — привязка найдена и лежит в result().
  bool observe_hunt(uint16_t id, const uint8_t* d, uint8_t len);

  const BtnBinding& result() const { return found_; }

  // Нажата ли кнопка в этом кадре по уже найденной привязке.
  static bool pressed(const BtnBinding& b, uint16_t id, const uint8_t* d,
                      uint8_t len);

 private:
  struct Entry {
    uint16_t id;
    uint8_t ever[TRACK_BYTES];
  };

  int8_t index_of(uint16_t id) const;

  Entry tbl_[MAX_IDS];
  uint8_t n_ = 0;

  BtnBinding cand_;
  uint8_t hits_ = 0;
  BtnBinding found_;
};
