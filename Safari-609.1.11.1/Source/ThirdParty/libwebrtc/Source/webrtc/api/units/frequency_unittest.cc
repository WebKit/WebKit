/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/units/frequency.h"

#include <limits>

#include "test/gtest.h"

namespace webrtc {
namespace test {
TEST(FrequencyTest, ConstExpr) {
  constexpr Frequency kFrequencyZero = Frequency::Zero();
  constexpr Frequency kFrequencyPlusInf = Frequency::PlusInfinity();
  constexpr Frequency kFrequencyMinusInf = Frequency::MinusInfinity();
  static_assert(kFrequencyZero.IsZero(), "");
  static_assert(kFrequencyPlusInf.IsPlusInfinity(), "");
  static_assert(kFrequencyMinusInf.IsMinusInfinity(), "");

  static_assert(kFrequencyPlusInf > kFrequencyZero, "");
}

TEST(FrequencyTest, GetBackSameValues) {
  const int64_t kValue = 31;
  EXPECT_EQ(Frequency::hertz(kValue).hertz<int64_t>(), kValue);
  EXPECT_EQ(Frequency::Zero().hertz<int64_t>(), 0);
}

TEST(FrequencyTest, GetDifferentPrefix) {
  const int64_t kValue = 30000;
  EXPECT_EQ(Frequency::millihertz(kValue).hertz<int64_t>(), kValue / 1000);
  EXPECT_EQ(Frequency::hertz(kValue).millihertz(), kValue * 1000);
}

TEST(FrequencyTest, IdentityChecks) {
  const int64_t kValue = 31;
  EXPECT_TRUE(Frequency::Zero().IsZero());
  EXPECT_FALSE(Frequency::hertz(kValue).IsZero());

  EXPECT_TRUE(Frequency::PlusInfinity().IsInfinite());
  EXPECT_TRUE(Frequency::MinusInfinity().IsInfinite());
  EXPECT_FALSE(Frequency::Zero().IsInfinite());
  EXPECT_FALSE(Frequency::hertz(kValue).IsInfinite());

  EXPECT_FALSE(Frequency::PlusInfinity().IsFinite());
  EXPECT_FALSE(Frequency::MinusInfinity().IsFinite());
  EXPECT_TRUE(Frequency::hertz(kValue).IsFinite());
  EXPECT_TRUE(Frequency::Zero().IsFinite());

  EXPECT_TRUE(Frequency::PlusInfinity().IsPlusInfinity());
  EXPECT_FALSE(Frequency::MinusInfinity().IsPlusInfinity());

  EXPECT_TRUE(Frequency::MinusInfinity().IsMinusInfinity());
  EXPECT_FALSE(Frequency::PlusInfinity().IsMinusInfinity());
}

TEST(FrequencyTest, ComparisonOperators) {
  const int64_t kSmall = 42;
  const int64_t kLarge = 45;
  const Frequency small = Frequency::hertz(kSmall);
  const Frequency large = Frequency::hertz(kLarge);

  EXPECT_EQ(Frequency::Zero(), Frequency::hertz(0));
  EXPECT_EQ(Frequency::PlusInfinity(), Frequency::PlusInfinity());
  EXPECT_EQ(small, Frequency::hertz(kSmall));
  EXPECT_LE(small, Frequency::hertz(kSmall));
  EXPECT_GE(small, Frequency::hertz(kSmall));
  EXPECT_NE(small, Frequency::hertz(kLarge));
  EXPECT_LE(small, Frequency::hertz(kLarge));
  EXPECT_LT(small, Frequency::hertz(kLarge));
  EXPECT_GE(large, Frequency::hertz(kSmall));
  EXPECT_GT(large, Frequency::hertz(kSmall));
  EXPECT_LT(Frequency::Zero(), small);

  EXPECT_GT(Frequency::PlusInfinity(), large);
  EXPECT_LT(Frequency::MinusInfinity(), Frequency::Zero());
}

TEST(FrequencyTest, Clamping) {
  const Frequency upper = Frequency::hertz(800);
  const Frequency lower = Frequency::hertz(100);
  const Frequency under = Frequency::hertz(100);
  const Frequency inside = Frequency::hertz(500);
  const Frequency over = Frequency::hertz(1000);
  EXPECT_EQ(under.Clamped(lower, upper), lower);
  EXPECT_EQ(inside.Clamped(lower, upper), inside);
  EXPECT_EQ(over.Clamped(lower, upper), upper);

  Frequency mutable_frequency = lower;
  mutable_frequency.Clamp(lower, upper);
  EXPECT_EQ(mutable_frequency, lower);
  mutable_frequency = inside;
  mutable_frequency.Clamp(lower, upper);
  EXPECT_EQ(mutable_frequency, inside);
  mutable_frequency = over;
  mutable_frequency.Clamp(lower, upper);
  EXPECT_EQ(mutable_frequency, upper);
}

TEST(FrequencyTest, MathOperations) {
  const int64_t kValueA = 457;
  const int64_t kValueB = 260;
  const Frequency frequency_a = Frequency::hertz(kValueA);
  const Frequency frequency_b = Frequency::hertz(kValueB);
  EXPECT_EQ((frequency_a + frequency_b).hertz<int64_t>(), kValueA + kValueB);
  EXPECT_EQ((frequency_a - frequency_b).hertz<int64_t>(), kValueA - kValueB);

  EXPECT_EQ((Frequency::hertz(kValueA) * kValueB).hertz<int64_t>(),
            kValueA * kValueB);

  EXPECT_EQ((frequency_b / 10).hertz<int64_t>(), kValueB / 10);
  EXPECT_EQ(frequency_b / frequency_a, static_cast<double>(kValueB) / kValueA);

  Frequency mutable_frequency = Frequency::hertz(kValueA);
  mutable_frequency += Frequency::hertz(kValueB);
  EXPECT_EQ(mutable_frequency, Frequency::hertz(kValueA + kValueB));
  mutable_frequency -= Frequency::hertz(kValueB);
  EXPECT_EQ(mutable_frequency, Frequency::hertz(kValueA));
}
TEST(FrequencyTest, Rounding) {
  const Frequency freq_high = Frequency::hertz(23.976);
  EXPECT_EQ(freq_high.hertz(), 24);
  EXPECT_EQ(freq_high.RoundDownTo(Frequency::hertz(1)), Frequency::hertz(23));
  EXPECT_EQ(freq_high.RoundTo(Frequency::hertz(1)), Frequency::hertz(24));
  EXPECT_EQ(freq_high.RoundUpTo(Frequency::hertz(1)), Frequency::hertz(24));

  const Frequency freq_low = Frequency::hertz(23.4);
  EXPECT_EQ(freq_low.hertz(), 23);
  EXPECT_EQ(freq_low.RoundDownTo(Frequency::hertz(1)), Frequency::hertz(23));
  EXPECT_EQ(freq_low.RoundTo(Frequency::hertz(1)), Frequency::hertz(23));
  EXPECT_EQ(freq_low.RoundUpTo(Frequency::hertz(1)), Frequency::hertz(24));
}

TEST(FrequencyTest, InfinityOperations) {
  const double kValue = 267;
  const Frequency finite = Frequency::hertz(kValue);
  EXPECT_TRUE((Frequency::PlusInfinity() + finite).IsPlusInfinity());
  EXPECT_TRUE((Frequency::PlusInfinity() - finite).IsPlusInfinity());
  EXPECT_TRUE((finite + Frequency::PlusInfinity()).IsPlusInfinity());
  EXPECT_TRUE((finite - Frequency::MinusInfinity()).IsPlusInfinity());

  EXPECT_TRUE((Frequency::MinusInfinity() + finite).IsMinusInfinity());
  EXPECT_TRUE((Frequency::MinusInfinity() - finite).IsMinusInfinity());
  EXPECT_TRUE((finite + Frequency::MinusInfinity()).IsMinusInfinity());
  EXPECT_TRUE((finite - Frequency::PlusInfinity()).IsMinusInfinity());
}

TEST(UnitConversionTest, TimeDeltaAndFrequency) {
  EXPECT_EQ(1 / Frequency::hertz(50), TimeDelta::ms(20));
  EXPECT_EQ(1 / TimeDelta::ms(20), Frequency::hertz(50));
}
}  // namespace test
}  // namespace webrtc
