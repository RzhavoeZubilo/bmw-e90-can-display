#include <unity.h>

#include "BtnLearn.h"

static BtnLearn L;

void setUp() { L.reset(); }
void tearDown() {}

// Кадр кнопок MFL по DBC: 0x1D6, два байта. Telephone — бит 0 байта 0.
static const uint16_t MFL = 0x1D6;

static void feed_baseline(uint8_t times) {
  const uint8_t idle[2] = {0x00, 0xFC};
  for (uint8_t i = 0; i < times; ++i) L.observe_baseline(MFL, idle, 2);
}

static void test_finds_new_bit() {
  feed_baseline(20);
  const uint8_t held[2] = {0x01, 0xFC};  // нажат Telephone
  bool ok = false;
  for (uint8_t i = 0; i < BtnLearn::CONFIRM_HITS; ++i)
    ok = L.observe_hunt(MFL, held, 2);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_UINT16(MFL, L.result().id);
  TEST_ASSERT_EQUAL_UINT8(0, L.result().byte_idx);
  TEST_ASSERT_EQUAL_UINT8(0x01, L.result().mask);
}

static void test_needs_confirmation() {
  feed_baseline(20);
  const uint8_t held[2] = {0x01, 0xFC};
  // Одного кадра мало: случайный бит не должен становиться привязкой.
  TEST_ASSERT_FALSE(L.observe_hunt(MFL, held, 2));
  TEST_ASSERT_FALSE(L.result().valid);
}

static void test_background_bits_ignored() {
  // Байт 1 в фоне уже был 0xFC — эти биты кандидатами быть не должны.
  feed_baseline(20);
  const uint8_t same[2] = {0x00, 0xFC};
  for (uint8_t i = 0; i < 20; ++i) L.observe_hunt(MFL, same, 2);
  TEST_ASSERT_FALSE(L.result().valid);
}

static void test_noisy_background_does_not_confuse() {
  // Кадр, у которого биты гуляют сами: счётчик в младшем нибблике.
  const uint16_t noisy = 0x0AA;
  for (uint8_t v = 0; v < 16; ++v) {
    const uint8_t d[4] = {v, 0x00, 0x00, 0x00};
    L.observe_baseline(noisy, d, 4);
  }
  feed_baseline(20);

  // Шумный кадр продолжает гулять — он весь в фоне и не должен ловиться.
  for (uint8_t v = 0; v < 16; ++v) {
    const uint8_t d[4] = {v, 0x00, 0x00, 0x00};
    L.observe_hunt(noisy, d, 4);
  }
  TEST_ASSERT_FALSE(L.result().valid);

  // А настоящее нажатие ловится и на фоне шума.
  const uint8_t held[2] = {0x01, 0xFC};
  for (uint8_t i = 0; i < BtnLearn::CONFIRM_HITS; ++i)
    L.observe_hunt(MFL, held, 2);
  TEST_ASSERT_TRUE(L.result().valid);
  TEST_ASSERT_EQUAL_UINT16(MFL, L.result().id);
}

static void test_frame_absent_from_baseline_is_ignored() {
  // Кадр, которого не было в фоне, привязкой стать не должен: у него пустой
  // фон, и любой бит выглядел бы новым. Так ловится мусор при переполнении
  // таблицы или потере кадров на приёме.
  feed_baseline(20);
  const uint8_t d[2] = {0x08, 0x00};
  for (uint8_t i = 0; i < 20; ++i) L.observe_hunt(0x123, d, 2);
  TEST_ASSERT_FALSE(L.result().valid);
}

static void test_overflow_does_not_produce_false_binding() {
  // Забиваем таблицу фона под завязку, кадр кнопки в неё не попадает.
  for (uint16_t id = 0x400; id < 0x400 + BtnLearn::MAX_IDS; ++id) {
    const uint8_t d[4] = {0, 0, 0, 0};
    L.observe_baseline(id, d, 4);
  }
  const uint8_t idle[2] = {0x00, 0xFC};
  L.observe_baseline(MFL, idle, 2);   // не влезет
  TEST_ASSERT_EQUAL_UINT8(BtnLearn::MAX_IDS, L.seen_ids());

  const uint8_t held[2] = {0x01, 0xFC};
  for (uint8_t i = 0; i < 20; ++i) L.observe_hunt(MFL, held, 2);
  TEST_ASSERT_FALSE(L.result().valid);
}

static void test_flapping_candidate_resets_counter() {
  feed_baseline(20);
  const uint8_t a[2] = {0x01, 0xFC};
  const uint8_t b[2] = {0x02, 0xFC};
  // Кандидат всё время меняется — подтверждения не набирается.
  for (uint8_t i = 0; i < 20; ++i) {
    L.observe_hunt(MFL, (i % 2) ? a : b, 2);
  }
  TEST_ASSERT_FALSE(L.result().valid);
}

static void test_table_overflow_is_safe() {
  // Больше идентификаторов, чем помещается: не должно ломаться.
  for (uint16_t id = 0; id < BtnLearn::MAX_IDS + 10; ++id) {
    const uint8_t d[4] = {0x00, 0x00, 0x00, 0x00};
    L.observe_baseline(id, d, 4);
  }
  TEST_ASSERT_EQUAL_UINT8(BtnLearn::MAX_IDS, L.seen_ids());
}

// --- применение найденной привязки ---

static void test_pressed_check() {
  BtnBinding b;
  b.id = MFL;
  b.byte_idx = 0;
  b.mask = 0x01;
  b.valid = true;

  const uint8_t up[2] = {0x00, 0xFC};
  const uint8_t down[2] = {0x01, 0xFC};
  TEST_ASSERT_FALSE(BtnLearn::pressed(b, MFL, up, 2));
  TEST_ASSERT_TRUE(BtnLearn::pressed(b, MFL, down, 2));

  // Чужой кадр не считается нажатием, даже если бит совпал.
  TEST_ASSERT_FALSE(BtnLearn::pressed(b, 0x1D0, down, 2));
}

static void test_pressed_rejects_short_frame() {
  BtnBinding b;
  b.id = MFL;
  b.byte_idx = 3;
  b.mask = 0x01;
  b.valid = true;
  const uint8_t d[2] = {0xFF, 0xFF};
  TEST_ASSERT_FALSE(BtnLearn::pressed(b, MFL, d, 2));
}

static void test_pressed_on_invalid_binding() {
  BtnBinding b;  // valid = false
  const uint8_t d[2] = {0xFF, 0xFF};
  TEST_ASSERT_FALSE(BtnLearn::pressed(b, MFL, d, 2));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_finds_new_bit);
  RUN_TEST(test_needs_confirmation);
  RUN_TEST(test_background_bits_ignored);
  RUN_TEST(test_noisy_background_does_not_confuse);
  RUN_TEST(test_frame_absent_from_baseline_is_ignored);
  RUN_TEST(test_overflow_does_not_produce_false_binding);
  RUN_TEST(test_flapping_candidate_resets_counter);
  RUN_TEST(test_table_overflow_is_safe);
  RUN_TEST(test_pressed_check);
  RUN_TEST(test_pressed_rejects_short_frame);
  RUN_TEST(test_pressed_on_invalid_binding);
  return UNITY_END();
}
