#include "BtnLearn.h"

void BtnLearn::reset() {
  n_ = 0;
  hits_ = 0;
  cand_ = BtnBinding();
  found_ = BtnBinding();
}

int8_t BtnLearn::index_of(uint16_t id) const {
  for (uint8_t i = 0; i < n_; ++i)
    if (tbl_[i].id == id) return static_cast<int8_t>(i);
  return -1;
}

void BtnLearn::observe_baseline(uint16_t id, const uint8_t* d, uint8_t len) {
  if (d == nullptr) return;

  int8_t i = index_of(id);
  if (i < 0) {
    if (n_ >= MAX_IDS) return;  // таблица полна, новый ID игнорируем
    i = static_cast<int8_t>(n_);
    tbl_[n_].id = id;
    for (uint8_t k = 0; k < TRACK_BYTES; ++k) tbl_[n_].ever[k] = 0;
    ++n_;
  }

  const uint8_t n = (len < TRACK_BYTES) ? len : TRACK_BYTES;
  for (uint8_t k = 0; k < n; ++k) tbl_[i].ever[k] |= d[k];
}

bool BtnLearn::observe_hunt(uint16_t id, const uint8_t* d, uint8_t len) {
  if (found_.valid || d == nullptr) return found_.valid;

  const int8_t i = index_of(id);

  // Кадры, которых не было в фоне, игнорируем. Соблазнительно считать их
  // «появившимися вместе с нажатием», но это опасно: если таблица фона
  // переполнилась или кадр потерялся при приёме, у него окажется пустой
  // фон — и первый же установленный бит станет ложной привязкой. Лучше
  // не выучить кнопку, чем выучить мусор.
  if (i < 0) return false;

  const uint8_t n = (len < TRACK_BYTES) ? len : TRACK_BYTES;

  for (uint8_t k = 0; k < n; ++k) {
    // Биты, которые сейчас единица, а в фоне не были ей ни разу.
    const uint8_t base = tbl_[i].ever[k];
    const uint8_t fresh = d[k] & ~base;
    if (fresh == 0) continue;

    // Берём младший из новых битов — если их несколько, любой годится,
    // лишь бы дальше он подтверждался стабильно.
    uint8_t bit = fresh & (uint8_t)(-(int8_t)fresh);

    if (cand_.valid && cand_.id == id && cand_.byte_idx == k &&
        cand_.mask == bit) {
      if (++hits_ >= CONFIRM_HITS) {
        found_ = cand_;
        found_.valid = true;
        return true;
      }
    } else {
      cand_.id = id;
      cand_.byte_idx = k;
      cand_.mask = bit;
      cand_.valid = true;
      hits_ = 1;
    }
    return false;
  }
  return false;
}

bool BtnLearn::pressed(const BtnBinding& b, uint16_t id, const uint8_t* d,
                       uint8_t len) {
  if (!b.valid || d == nullptr) return false;
  if (id != b.id || b.byte_idx >= len) return false;
  return (d[b.byte_idx] & b.mask) != 0;
}
