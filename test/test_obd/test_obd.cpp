#include <unity.h>

#include "Obd.h"

static Obd o;

void setUp() { o.reset(); }
void tearDown() {}

// --- запрос ---

static void test_request_frame_layout() {
  uint8_t b[8];
  Obd::build_request(obd::PID_COOLANT, b);
  TEST_ASSERT_EQUAL_UINT8(0x02, b[0]);
  TEST_ASSERT_EQUAL_UINT8(0x01, b[1]);
  TEST_ASSERT_EQUAL_UINT8(0x05, b[2]);
}

// --- разбор ответов ---

static void test_coolant() {
  // 0x03 байта, ответ 0x41, PID 0x05, A=130 -> 130-40 = 90 °C
  const uint8_t d[8] = {0x03, 0x41, 0x05, 130, 0, 0, 0, 0};
  TEST_ASSERT_TRUE(o.apply(0x7E8, d, 8, 1000));
  TEST_ASSERT_EQUAL_INT16(90, o.data().coolant_c);
}

static void test_coolant_below_zero() {
  const uint8_t d[8] = {0x03, 0x41, 0x05, 20, 0, 0, 0, 0};  // -20 °C
  o.apply(0x7E8, d, 8, 1000);
  TEST_ASSERT_EQUAL_INT16(-20, o.data().coolant_c);
}

static void test_voltage_is_millivolts() {
  // 0x42: (A*256+B) мВ. 0x37 0x70 = 14192 -> 14.192 В
  const uint8_t d[8] = {0x04, 0x41, 0x42, 0x37, 0x70, 0, 0, 0};
  TEST_ASSERT_TRUE(o.apply(0x7E8, d, 8, 1000));
  TEST_ASSERT_EQUAL_UINT16(14192, o.data().volts_mv);
}

static void test_fuel_percent_and_liters() {
  const uint8_t d[8] = {0x03, 0x41, 0x2F, 255, 0, 0, 0, 0};  // полный бак
  o.apply(0x7E8, d, 8, 1000);
  TEST_ASSERT_EQUAL_UINT16(1000, o.data().fuel_pct_x10);
  // 100 % от 61 л = 61.0 л
  TEST_ASSERT_EQUAL_UINT16(610, o.fuel_liters_x10());
}

static void test_fuel_half_tank() {
  const uint8_t d[8] = {0x03, 0x41, 0x2F, 128, 0, 0, 0, 0};
  o.apply(0x7E8, d, 8, 1000);
  TEST_ASSERT_UINT16_WITHIN(5, 502, o.data().fuel_pct_x10);
  TEST_ASSERT_UINT16_WITHIN(5, 306, o.fuel_liters_x10());
}

static void test_response_from_second_ecu_accepted() {
  const uint8_t d[8] = {0x03, 0x41, 0x0F, 74, 0, 0, 0, 0};
  TEST_ASSERT_TRUE(o.apply(0x7E9, d, 8, 1000));
  TEST_ASSERT_EQUAL_INT16(34, o.data().iat_c);
}

static void test_foreign_id_ignored() {
  const uint8_t d[8] = {0x03, 0x41, 0x05, 130, 0, 0, 0, 0};
  TEST_ASSERT_FALSE(o.apply(0x1B4, d, 8, 1000));
  TEST_ASSERT_EQUAL_INT16(0, o.data().coolant_c);
}

static void test_negative_response_ignored() {
  // 0x7F = «сервис не поддерживается», данными быть не должно.
  const uint8_t d[8] = {0x03, 0x7F, 0x01, 0x12, 0, 0, 0, 0};
  TEST_ASSERT_FALSE(o.apply(0x7E8, d, 8, 1000));
}

static void test_multiframe_ignored() {
  // Первый кадр многокадрового ответа: старший ниббл PCI = 1.
  const uint8_t d[8] = {0x10, 0x14, 0x41, 0x05, 130, 0, 0, 0};
  TEST_ASSERT_FALSE(o.apply(0x7E8, d, 8, 1000));
}

static void test_truncated_frame_rejected() {
  // PCI обещает 3 байта, а в кадре их меньше.
  const uint8_t d[3] = {0x03, 0x41, 0x05};
  TEST_ASSERT_FALSE(o.apply(0x7E8, d, 3, 1000));
}

// --- карта поддерживаемых PID ---

static void test_support_map_bit_order() {
  // Ответ на PID 0x00. Старший бит байта A соответствует PID 0x01.
  // 0x80 0x00 0x00 0x00 -> поддерживается только 0x01.
  const uint8_t d[8] = {0x06, 0x41, 0x00, 0x80, 0x00, 0x00, 0x00, 0};
  TEST_ASSERT_TRUE(o.apply(0x7E8, d, 8, 1000));
  TEST_ASSERT_TRUE(o.supported(0x01));
  TEST_ASSERT_FALSE(o.supported(0x02));
  TEST_ASSERT_FALSE(o.supported(obd::PID_COOLANT));
}

static void test_support_map_last_bit() {
  // Младший бит байта D — это PID 0x20.
  const uint8_t d[8] = {0x06, 0x41, 0x00, 0x00, 0x00, 0x00, 0x01, 0};
  o.apply(0x7E8, d, 8, 1000);
  TEST_ASSERT_TRUE(o.supported(0x20));
  TEST_ASSERT_FALSE(o.supported(0x1F));
}

static void test_everything_supported_before_first_map() {
  // Пока карта не пришла, опрос должен идти, иначе он не стартует вообще.
  TEST_ASSERT_TRUE(o.supported(obd::PID_COOLANT));
  TEST_ASSERT_FALSE(o.any_map());
}

static void test_unqueried_block_is_unsupported() {
  const uint8_t d[8] = {0x06, 0x41, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0};
  o.apply(0x7E8, d, 8, 1000);
  TEST_ASSERT_TRUE(o.supported(obd::PID_COOLANT));   // блок 0 пришёл
  TEST_ASSERT_FALSE(o.supported(obd::PID_OIL_TEMP));  // блок 2 — нет
  TEST_ASSERT_TRUE(o.have_map(0));
  TEST_ASSERT_FALSE(o.have_map(2));
}

static void test_oil_temp_in_third_block() {
  // 0x5C — 28-й PID блока 0x41..0x60, значит бит (32-28)=4.
  const uint8_t d[8] = {0x06, 0x41, 0x40, 0x00, 0x00, 0x00, 0x10, 0};
  o.apply(0x7E8, d, 8, 1000);
  TEST_ASSERT_TRUE(o.supported(obd::PID_OIL_TEMP));
}

// --- протухание ---

static void test_staleness() {
  const uint8_t d[8] = {0x03, 0x41, 0x05, 130, 0, 0, 0, 0};
  o.apply(0x7E8, d, 8, 1000);
  TEST_ASSERT_TRUE(Obd::fresh(o.data().coolant_ms, 3000));
  TEST_ASSERT_FALSE(Obd::fresh(o.data().coolant_ms, 5000));
  // Ни разу не приходило — свежим быть не может.
  TEST_ASSERT_FALSE(Obd::fresh(o.data().oil_ms, 1000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_request_frame_layout);
  RUN_TEST(test_coolant);
  RUN_TEST(test_coolant_below_zero);
  RUN_TEST(test_voltage_is_millivolts);
  RUN_TEST(test_fuel_percent_and_liters);
  RUN_TEST(test_fuel_half_tank);
  RUN_TEST(test_response_from_second_ecu_accepted);
  RUN_TEST(test_foreign_id_ignored);
  RUN_TEST(test_negative_response_ignored);
  RUN_TEST(test_multiframe_ignored);
  RUN_TEST(test_truncated_frame_rejected);
  RUN_TEST(test_support_map_bit_order);
  RUN_TEST(test_support_map_last_bit);
  RUN_TEST(test_everything_supported_before_first_map);
  RUN_TEST(test_unqueried_block_is_unsupported);
  RUN_TEST(test_oil_temp_in_third_block);
  RUN_TEST(test_staleness);
  return UNITY_END();
}
