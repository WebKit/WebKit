/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/denormal_disabler.h"

#include <cmath>
#include <limits>
#include <vector>

#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr float kSmallest = std::numeric_limits<float>::min();

// Float values such that, if used as divisors of `kSmallest`, the division
// produces a denormal or zero depending on whether denormals are enabled.
constexpr float kDenormalDivisors[] = {123.125f, 97.0f, 32.0f, 5.0f, 1.5f};

// Returns true if the result of `dividend` / `divisor` is a denormal.
// `dividend` and `divisor` must not be denormals.
bool DivisionIsDenormal(float dividend, float divisor) {
  RTC_DCHECK_GE(std::fabsf(dividend), kSmallest);
  RTC_DCHECK_GE(std::fabsf(divisor), kSmallest);
  volatile float division = dividend / divisor;
  return division != 0.0f && std::fabsf(division) < kSmallest;
}

}  // namespace

class DenormalDisablerParametrization : public ::testing::TestWithParam<bool> {
};

// Checks that +inf and -inf are not zeroed regardless of whether
// architecture and compiler are supported.
TEST_P(DenormalDisablerParametrization, InfNotZeroedExplicitlySetEnabled) {
  DenormalDisabler denormal_disabler(/*enabled=*/GetParam());
  constexpr float kMax = std::numeric_limits<float>::max();
  for (float x : {-2.0f, 2.0f}) {
    SCOPED_TRACE(x);
    volatile float multiplication = kMax * x;
    EXPECT_TRUE(std::isinf(multiplication));
  }
}

// Checks that a NaN is not zeroed regardless of whether architecture and
// compiler are supported.
TEST_P(DenormalDisablerParametrization, NanNotZeroedExplicitlySetEnabled) {
  DenormalDisabler denormal_disabler(/*enabled=*/GetParam());
  volatile float kNan = std::sqrt(-1.0f);
  EXPECT_TRUE(std::isnan(kNan));
}

INSTANTIATE_TEST_SUITE_P(DenormalDisabler,
                         DenormalDisablerParametrization,
                         ::testing::Values(false, true),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "enabled" : "disabled";
                         });

// Checks that +inf and -inf are not zeroed regardless of whether
// architecture and compiler are supported.
TEST(DenormalDisabler, InfNotZeroed) {
  DenormalDisabler denormal_disabler;
  constexpr float kMax = std::numeric_limits<float>::max();
  for (float x : {-2.0f, 2.0f}) {
    SCOPED_TRACE(x);
    volatile float multiplication = kMax * x;
    EXPECT_TRUE(std::isinf(multiplication));
  }
}

// Checks that a NaN is not zeroed regardless of whether architecture and
// compiler are supported.
TEST(DenormalDisabler, NanNotZeroed) {
  DenormalDisabler denormal_disabler;
  volatile float kNan = std::sqrt(-1.0f);
  EXPECT_TRUE(std::isnan(kNan));
}

// Checks that denormals are not zeroed if `DenormalDisabler` is disabled and
// architecture and compiler are supported.
TEST(DenormalDisabler, DoNotZeroDenormalsIfDisabled) {
  if (!DenormalDisabler::IsSupported()) {
    GTEST_SKIP() << "Unsupported platform.";
  }
  ASSERT_TRUE(DivisionIsDenormal(kSmallest, kDenormalDivisors[0]))
      << "Precondition not met: denormals must be enabled.";
  DenormalDisabler denormal_disabler(/*enabled=*/false);
  for (float x : kDenormalDivisors) {
    SCOPED_TRACE(x);
    EXPECT_TRUE(DivisionIsDenormal(-kSmallest, x));
    EXPECT_TRUE(DivisionIsDenormal(kSmallest, x));
  }
}

// Checks that denormals are zeroed if `DenormalDisabler` is enabled if
// architecture and compiler are supported.
TEST(DenormalDisabler, ZeroDenormals) {
  if (!DenormalDisabler::IsSupported()) {
    GTEST_SKIP() << "Unsupported platform.";
  }
  DenormalDisabler denormal_disabler;
  for (float x : kDenormalDivisors) {
    SCOPED_TRACE(x);
    EXPECT_FALSE(DivisionIsDenormal(-kSmallest, x));
    EXPECT_FALSE(DivisionIsDenormal(kSmallest, x));
  }
}

// Checks that denormals are zeroed if `DenormalDisabler` is enabled if
// architecture and compiler are supported.
TEST(DenormalDisabler, ZeroDenormalsExplicitlyEnabled) {
  if (!DenormalDisabler::IsSupported()) {
    GTEST_SKIP() << "Unsupported platform.";
  }
  DenormalDisabler denormal_disabler(/*enabled=*/true);
  for (float x : kDenormalDivisors) {
    SCOPED_TRACE(x);
    EXPECT_FALSE(DivisionIsDenormal(-kSmallest, x));
    EXPECT_FALSE(DivisionIsDenormal(kSmallest, x));
  }
}

// Checks that the `DenormalDisabler` dtor re-enables denormals if previously
// enabled and architecture and compiler are supported.
TEST(DenormalDisabler, RestoreDenormalsEnabled) {
  if (!DenormalDisabler::IsSupported()) {
    GTEST_SKIP() << "Unsupported platform.";
  }
  ASSERT_TRUE(DivisionIsDenormal(kSmallest, kDenormalDivisors[0]))
      << "Precondition not met: denormals must be enabled.";
  {
    DenormalDisabler denormal_disabler;
    ASSERT_FALSE(DivisionIsDenormal(kSmallest, kDenormalDivisors[0]));
  }
  EXPECT_TRUE(DivisionIsDenormal(kSmallest, kDenormalDivisors[0]));
}

// Checks that the `DenormalDisabler` dtor re-enables denormals if previously
// enabled and architecture and compiler are supported.
TEST(DenormalDisabler, RestoreDenormalsEnabledExplicitlyEnabled) {
  if (!DenormalDisabler::IsSupported()) {
    GTEST_SKIP() << "Unsupported platform.";
  }
  ASSERT_TRUE(DivisionIsDenormal(kSmallest, kDenormalDivisors[0]))
      << "Precondition not met: denormals must be enabled.";
  {
    DenormalDisabler denormal_disabler(/*enabled=*/true);
    ASSERT_FALSE(DivisionIsDenormal(kSmallest, kDenormalDivisors[0]));
  }
  EXPECT_TRUE(DivisionIsDenormal(kSmallest, kDenormalDivisors[0]));
}

// Checks that the `DenormalDisabler` dtor keeps denormals disabled if
// architecture and compiler are supported and if previously disabled - i.e.,
// nested usage is supported.
TEST(DenormalDisabler, ZeroDenormalsNested) {
  if (!DenormalDisabler::IsSupported()) {
    GTEST_SKIP() << "Unsupported platform.";
  }
  DenormalDisabler d1;
  ASSERT_FALSE(DivisionIsDenormal(kSmallest, kDenormalDivisors[0]));
  {
    DenormalDisabler d2;
    ASSERT_FALSE(DivisionIsDenormal(kSmallest, kDenormalDivisors[0]));
  }
  EXPECT_FALSE(DivisionIsDenormal(kSmallest, kDenormalDivisors[0]));
}

// Checks that the `DenormalDisabler` dtor keeps denormals disabled if
// architecture and compiler are supported and if previously disabled - i.e.,
// nested usage is supported.
TEST(DenormalDisabler, ZeroDenormalsNestedExplicitlyEnabled) {
  if (!DenormalDisabler::IsSupported()) {
    GTEST_SKIP() << "Unsupported platform.";
  }
  DenormalDisabler d1(/*enabled=*/true);
  ASSERT_FALSE(DivisionIsDenormal(kSmallest, kDenormalDivisors[0]));
  {
    DenormalDisabler d2(/*enabled=*/true);
    ASSERT_FALSE(DivisionIsDenormal(kSmallest, kDenormalDivisors[0]));
  }
  EXPECT_FALSE(DivisionIsDenormal(kSmallest, kDenormalDivisors[0]));
}

// Checks that `DenormalDisabler` does not zero denormals if architecture and
// compiler are not supported.
TEST(DenormalDisabler, DoNotZeroDenormalsIfUnsupported) {
  if (DenormalDisabler::IsSupported()) {
    GTEST_SKIP() << "This test should only run on platforms without support "
                    "for DenormalDisabler.";
  }
  DenormalDisabler denormal_disabler;
  for (float x : kDenormalDivisors) {
    SCOPED_TRACE(x);
    EXPECT_TRUE(DivisionIsDenormal(-kSmallest, x));
    EXPECT_TRUE(DivisionIsDenormal(kSmallest, x));
  }
}

// Checks that `DenormalDisabler` does not zero denormals if architecture and
// compiler are not supported.
TEST(DenormalDisabler, DoNotZeroDenormalsIfUnsupportedExplicitlyEnabled) {
  if (DenormalDisabler::IsSupported()) {
    GTEST_SKIP() << "This test should only run on platforms without support "
                    "for DenormalDisabler.";
  }
  DenormalDisabler denormal_disabler(/*enabled=*/true);
  for (float x : kDenormalDivisors) {
    SCOPED_TRACE(x);
    EXPECT_TRUE(DivisionIsDenormal(-kSmallest, x));
    EXPECT_TRUE(DivisionIsDenormal(kSmallest, x));
  }
}

}  // namespace webrtc
