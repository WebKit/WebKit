/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_common.h"

#include <limits>
#include <type_traits>
#include <vector>

#include "test/gtest.h"

namespace webrtc_event_logging {
namespace {

template <typename T>
class SignednessConversionTest : public testing::Test {
 public:
  static_assert(std::is_integral<T>::value, "");
  static_assert(std::is_signed<T>::value, "");
};

TYPED_TEST_CASE_P(SignednessConversionTest);

TYPED_TEST_P(SignednessConversionTest, CorrectlyConvertsLegalValues) {
  using T = TypeParam;
  std::vector<T> legal_values = {std::numeric_limits<T>::min(),
                                 std::numeric_limits<T>::min() + 1,
                                 -1,
                                 0,
                                 1,
                                 std::numeric_limits<T>::max() - 1,
                                 std::numeric_limits<T>::max()};
  for (T val : legal_values) {
    const auto unsigned_val = ToUnsigned(val);
    T signed_val;
    ASSERT_TRUE(ToSigned<T>(unsigned_val, &signed_val))
        << "Failed on " << static_cast<uint64_t>(unsigned_val) << ".";
    EXPECT_EQ(val, signed_val)
        << "Failed on " << static_cast<uint64_t>(unsigned_val) << ".";
  }
}

TYPED_TEST_P(SignednessConversionTest, FailsOnConvertingIllegalValues) {
  using T = TypeParam;

  // Note that a signed integer whose width is N bits, has N-1 digits.
  constexpr bool width_is_64 = std::numeric_limits<T>::digits == 63;

  if (width_is_64) {
    return;  // Test irrelevant; illegal values do not exist.
  }

  const uint64_t max_legal_value = ToUnsigned(static_cast<T>(-1));

  const std::vector<uint64_t> illegal_values = {
      max_legal_value + 1u, max_legal_value + 2u,
      std::numeric_limits<uint64_t>::max() - 1u,
      std::numeric_limits<uint64_t>::max()};

  for (uint64_t unsigned_val : illegal_values) {
    T signed_val;
    EXPECT_FALSE(ToSigned<T>(unsigned_val, &signed_val))
        << "Failed on " << static_cast<uint64_t>(unsigned_val) << ".";
  }
}

REGISTER_TYPED_TEST_CASE_P(SignednessConversionTest,
                           CorrectlyConvertsLegalValues,
                           FailsOnConvertingIllegalValues);

using Types = ::testing::Types<int8_t, int16_t, int32_t, int64_t>;

INSTANTIATE_TYPED_TEST_CASE_P(_, SignednessConversionTest, Types);

}  // namespace
}  // namespace webrtc_event_logging
