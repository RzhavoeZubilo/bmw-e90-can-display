#include <unity.h>

#include "Alerts.h"
#include "Buttons.h"

void setUp() {}
void tearDown() {}

// --- Alerts ---

static void test_coolant_threshold() {
  TEST_ASSERT_EQUAL(Alert::None, alerts::evaluate(109, true, 0, false));
  TEST_ASSERT_EQUAL(Alert::CoolantHot, alerts::evaluate(110, true, 0, false));
}

static void test_atf_threshold() {
  // ZF 6HP19: рабочий диапазон 60..90, тревога выше 130.
  TEST_ASSERT_EQUAL(Alert::None, alerts::evaluate(0, false, 129, true));
  TEST_ASSERT_EQUAL(Alert::AtfHot, alerts::evaluate(0, false, 130, true));
}

static void test_coolant_beats_atf() {
  // Перегрев двигателя важнее перегрева коробки.
  TEST_ASSERT_EQUAL(Alert::CoolantHot, alerts::evaluate(120, true, 150, true));
}

static void test_no_alert_when_data_invalid() {
  // Датчик молчит — нельзя пугать водителя мусором.
  TEST_ASSERT_EQUAL(Alert::None, alerts::evaluate(150, false, 200, false));
}

// --- Button (пока не используется в сборке, оставлено под расширение) ---

static void test_click_requires_debounce() {
  Button b;
  TEST_ASSERT_EQUAL(BtnEvent::None, b.update(true, 0));
  TEST_ASSERT_EQUAL(BtnEvent::None, b.update(false, 10));
  TEST_ASSERT_FALSE(b.pressed());
}

static void test_short_press_gives_click() {
  Button b;
  b.update(true, 0);
  b.update(true, 30);
  TEST_ASSERT_TRUE(b.pressed());
  b.update(false, 200);
  TEST_ASSERT_EQUAL(BtnEvent::Click, b.update(false, 230));
}

static void test_long_press_fires_once_and_suppresses_click() {
  Button b;
  b.update(true, 0);
  b.update(true, 30);
  TEST_ASSERT_EQUAL(BtnEvent::LongPress, b.update(true, 700));
  TEST_ASSERT_EQUAL(BtnEvent::None, b.update(true, 900));
  b.update(false, 1000);
  TEST_ASSERT_EQUAL(BtnEvent::None, b.update(false, 1030));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_coolant_threshold);
  RUN_TEST(test_atf_threshold);
  RUN_TEST(test_coolant_beats_atf);
  RUN_TEST(test_no_alert_when_data_invalid);
  RUN_TEST(test_click_requires_debounce);
  RUN_TEST(test_short_press_gives_click);
  RUN_TEST(test_long_press_fires_once_and_suppresses_click);
  return UNITY_END();
}
