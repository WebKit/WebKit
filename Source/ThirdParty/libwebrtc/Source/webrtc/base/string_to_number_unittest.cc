/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/string_to_number.h"

#include <string>
#include <type_traits>
#include <limits>

#include "webrtc/base/gunit.h"

namespace rtc {

namespace {
// clang-format off
using IntegerTypes =
    ::testing::Types<char,
                     signed char, unsigned char,       // NOLINT(runtime/int)
                     short,       unsigned short,      // NOLINT(runtime/int)
                     int,         unsigned int,        // NOLINT(runtime/int)
                     long,        unsigned long,       // NOLINT(runtime/int)
                     long long,   unsigned long long,  // NOLINT(runtime/int)
                     int8_t,      uint8_t,
                     int16_t,     uint16_t,
                     int32_t,     uint32_t,
                     int64_t,     uint64_t>;
// clang-format on

template <typename T>
class BasicNumberTest : public ::testing::Test {};

TYPED_TEST_CASE_P(BasicNumberTest);

TYPED_TEST_P(BasicNumberTest, TestValidNumbers) {
  using T = TypeParam;
  constexpr T min_value = std::numeric_limits<T>::lowest();
  constexpr T max_value = std::numeric_limits<T>::max();
  const std::string min_string = std::to_string(min_value);
  const std::string max_string = std::to_string(max_value);
  EXPECT_EQ(min_value, StringToNumber<T>(min_string));
  EXPECT_EQ(min_value, StringToNumber<T>(min_string.c_str()));
  EXPECT_EQ(max_value, StringToNumber<T>(max_string));
  EXPECT_EQ(max_value, StringToNumber<T>(max_string.c_str()));
  EXPECT_EQ(0, StringToNumber<T>("0"));
  EXPECT_EQ(0, StringToNumber<T>("-0"));
  EXPECT_EQ(0, StringToNumber<T>(std::string("-0000000000000")));
}

TYPED_TEST_P(BasicNumberTest, TestInvalidNumbers) {
  using T = TypeParam;
  // Value ranges aren't strictly enforced in this test, since that would either
  // require doctoring specific strings for each data type, which is a hassle
  // across platforms, or to be able to do addition of values larger than the
  // largest type, which is another hassle.
  constexpr T min_value = std::numeric_limits<T>::lowest();
  constexpr T max_value = std::numeric_limits<T>::max();
  // If the type supports negative values, make the large negative value
  // approximately ten times larger. If the type is unsigned, just use -2.
  const std::string too_low_string =
      (min_value == 0) ? "-2" : (std::to_string(min_value) + "1");
  // Make the large value approximately ten times larger than the maximum.
  const std::string too_large_string = std::to_string(max_value) + "1";
  EXPECT_EQ(rtc::Optional<T>(), StringToNumber<T>(too_low_string));
  EXPECT_EQ(rtc::Optional<T>(), StringToNumber<T>(too_low_string.c_str()));
  EXPECT_EQ(rtc::Optional<T>(), StringToNumber<T>(too_large_string));
  EXPECT_EQ(rtc::Optional<T>(), StringToNumber<T>(too_large_string.c_str()));
}

TYPED_TEST_P(BasicNumberTest, TestInvalidInputs) {
  using T = TypeParam;
  const char kInvalidCharArray[] = "Invalid string containing 47";
  const char kPlusMinusCharArray[] = "+-100";
  const char kNumberFollowedByCruft[] = "640x480";
  EXPECT_EQ(rtc::Optional<T>(), StringToNumber<T>(kInvalidCharArray));
  EXPECT_EQ(rtc::Optional<T>(),
            StringToNumber<T>(std::string(kInvalidCharArray)));
  EXPECT_EQ(rtc::Optional<T>(), StringToNumber<T>(kPlusMinusCharArray));
  EXPECT_EQ(rtc::Optional<T>(),
            StringToNumber<T>(std::string(kPlusMinusCharArray)));
  EXPECT_EQ(rtc::Optional<T>(), StringToNumber<T>(kNumberFollowedByCruft));
  EXPECT_EQ(rtc::Optional<T>(),
            StringToNumber<T>(std::string(kNumberFollowedByCruft)));
  EXPECT_EQ(rtc::Optional<T>(), StringToNumber<T>(" 5"));
  EXPECT_EQ(rtc::Optional<T>(), StringToNumber<T>(" - 5"));
  EXPECT_EQ(rtc::Optional<T>(), StringToNumber<T>("- 5"));
  EXPECT_EQ(rtc::Optional<T>(), StringToNumber<T>(" -5"));
  EXPECT_EQ(rtc::Optional<T>(), StringToNumber<T>("5 "));
}

REGISTER_TYPED_TEST_CASE_P(BasicNumberTest,
                           TestValidNumbers,
                           TestInvalidNumbers,
                           TestInvalidInputs);

}  // namespace

INSTANTIATE_TYPED_TEST_CASE_P(StringToNumberTest_Integers,
                              BasicNumberTest,
                              IntegerTypes);

TEST(StringToNumberTest, TestSpecificValues) {
  EXPECT_EQ(rtc::Optional<uint8_t>(), StringToNumber<uint8_t>("256"));
  EXPECT_EQ(rtc::Optional<uint8_t>(), StringToNumber<uint8_t>("-256"));
  EXPECT_EQ(rtc::Optional<int8_t>(), StringToNumber<int8_t>("256"));
  EXPECT_EQ(rtc::Optional<int8_t>(), StringToNumber<int8_t>("-256"));
}

}  // namespace rtc
