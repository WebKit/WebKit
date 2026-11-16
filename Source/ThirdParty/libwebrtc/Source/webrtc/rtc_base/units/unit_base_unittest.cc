/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/units/unit_base.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
class TestUnit final : public rtc_units_impl::RelativeUnit<TestUnit> {
 public:
  TestUnit() = delete;

  using UnitBase::FromValue;
  using UnitBase::ToValue;
  using UnitBase::ToValueOr;

  template <typename T>
  static constexpr TestUnit FromKilo(T kilo) {
    return FromFraction(1000, kilo);
  }
  template <typename T = int64_t>
  T ToKilo() const {
    return UnitBase::ToFraction<1000, T>();
  }
  constexpr int64_t ToKiloOr(int64_t fallback) const {
    return UnitBase::ToFractionOr<1000>(fallback);
  }
  template <typename T>
  constexpr T ToMilli() const {
    return UnitBase::ToMultiple<1000, T>();
  }

 private:
  friend class rtc_units_impl::UnitBase<TestUnit>;
  static constexpr bool one_sided = false;
  using RelativeUnit<TestUnit>::RelativeUnit;
};
constexpr TestUnit TestUnitAddKilo(TestUnit value, int add_kilo) {
  value += TestUnit::FromKilo(add_kilo);
  return value;
}
}  // namespace
namespace test {
TEST(UnitBaseTest, ConstExpr) {
  constexpr int64_t kValue = -12345;
  constexpr TestUnit kTestUnitZero = TestUnit::Zero();
  constexpr TestUnit kTestUnitPlusInf = TestUnit::PlusInfinity();
  constexpr TestUnit kTestUnitMinusInf = TestUnit::MinusInfinity();

  static_assert(kTestUnitZero.IsZero(), "");
  static_assert(kTestUnitPlusInf.IsPlusInfinity(), "");
  static_assert(kTestUnitMinusInf.IsMinusInfinity(), "");
  static_assert(kTestUnitPlusInf.ToKiloOr(-1) == -1, "");

  // Check FromValue is constexpr for floats.
  static_assert(TestUnit::FromValue(0.0).IsZero());
  static_assert(TestUnit::FromValue(INFINITY).IsPlusInfinity());
  static_assert(TestUnit::FromValue(-INFINITY).IsMinusInfinity());
  static_assert(TestUnit::FromValue(250.0) == TestUnit::FromValue(250));
  static_assert(TestUnit::FromValue(-250.0) == TestUnit::FromValue(-250));

  static_assert(kTestUnitPlusInf > kTestUnitZero, "");

  constexpr TestUnit kTestUnitKilo = TestUnit::FromKilo(kValue);
  constexpr TestUnit kTestUnitValue = TestUnit::FromValue(kValue);

  static_assert(kTestUnitKilo.ToKiloOr(0) == kValue, "");
  static_assert(kTestUnitValue.ToValueOr(0) == kValue, "");
  static_assert(TestUnitAddKilo(kTestUnitValue, 2).ToValue() == kValue + 2000,
                "");
  static_assert(TestUnit::FromValue(500) / 2 == TestUnit::FromValue(250));
  static_assert(TestUnit::FromValue(500.0) / 2 == TestUnit::FromValue(250.0));
}

TEST(UnitBaseTest, GetBackSameValues) {
  const int64_t kValue = 499;
  for (int sign = -1; sign <= 1; ++sign) {
    int64_t value = kValue * sign;
    EXPECT_EQ(TestUnit::FromKilo(value).ToKilo(), value);
    EXPECT_EQ(TestUnit::FromValue(value).ToValue<int64_t>(), value);
  }
  EXPECT_EQ(TestUnit::Zero().ToValue<int64_t>(), 0);
}

TEST(UnitBaseTest, GetDifferentPrefix) {
  const int64_t kValue = 3000000;
  EXPECT_EQ(TestUnit::FromValue(kValue).ToKilo(), kValue / 1000);
  EXPECT_EQ(TestUnit::FromKilo(kValue).ToValue<int64_t>(), kValue * 1000);
}

TEST(UnitBaseTest, IdentityChecks) {
  const int64_t kValue = 3000;
  EXPECT_TRUE(TestUnit::Zero().IsZero());
  EXPECT_FALSE(TestUnit::FromKilo(kValue).IsZero());

  EXPECT_TRUE(TestUnit::PlusInfinity().IsInfinite());
  EXPECT_TRUE(TestUnit::MinusInfinity().IsInfinite());
  EXPECT_FALSE(TestUnit::Zero().IsInfinite());
  EXPECT_FALSE(TestUnit::FromKilo(-kValue).IsInfinite());
  EXPECT_FALSE(TestUnit::FromKilo(kValue).IsInfinite());

  EXPECT_FALSE(TestUnit::PlusInfinity().IsFinite());
  EXPECT_FALSE(TestUnit::MinusInfinity().IsFinite());
  EXPECT_TRUE(TestUnit::FromKilo(-kValue).IsFinite());
  EXPECT_TRUE(TestUnit::FromKilo(kValue).IsFinite());
  EXPECT_TRUE(TestUnit::Zero().IsFinite());

  EXPECT_TRUE(TestUnit::PlusInfinity().IsPlusInfinity());
  EXPECT_FALSE(TestUnit::MinusInfinity().IsPlusInfinity());

  EXPECT_TRUE(TestUnit::MinusInfinity().IsMinusInfinity());
  EXPECT_FALSE(TestUnit::PlusInfinity().IsMinusInfinity());
}

TEST(UnitBaseTest, ComparisonOperators) {
  const int64_t kSmall = 450;
  const int64_t kLarge = 451;
  const TestUnit small = TestUnit::FromKilo(kSmall);
  const TestUnit large = TestUnit::FromKilo(kLarge);

  EXPECT_EQ(TestUnit::Zero(), TestUnit::FromKilo(0));
  EXPECT_EQ(TestUnit::PlusInfinity(), TestUnit::PlusInfinity());
  EXPECT_EQ(small, TestUnit::FromKilo(kSmall));
  EXPECT_LE(small, TestUnit::FromKilo(kSmall));
  EXPECT_GE(small, TestUnit::FromKilo(kSmall));
  EXPECT_NE(small, TestUnit::FromKilo(kLarge));
  EXPECT_LE(small, TestUnit::FromKilo(kLarge));
  EXPECT_LT(small, TestUnit::FromKilo(kLarge));
  EXPECT_GE(large, TestUnit::FromKilo(kSmall));
  EXPECT_GT(large, TestUnit::FromKilo(kSmall));
  EXPECT_LT(TestUnit::Zero(), small);
  EXPECT_GT(TestUnit::Zero(), TestUnit::FromKilo(-kSmall));
  EXPECT_GT(TestUnit::Zero(), TestUnit::FromKilo(-kSmall));

  EXPECT_GT(TestUnit::PlusInfinity(), large);
  EXPECT_LT(TestUnit::MinusInfinity(), TestUnit::Zero());
}

TEST(UnitBaseTest, CanBeInititializedFromLargeInt) {
  const int kMaxInt = std::numeric_limits<int>::max();
  EXPECT_EQ(TestUnit::FromKilo(kMaxInt).ToValue<int64_t>(),
            static_cast<int64_t>(kMaxInt) * 1000);
}

TEST(UnitBaseTest, ConvertsToAndFromDouble) {
  const int64_t kValue = 17017;
  const double kMilliDouble = kValue * 1e3;
  const double kValueDouble = kValue;
  const double kKiloDouble = kValue * 1e-3;

  EXPECT_EQ(TestUnit::FromValue(kValue).ToKilo<double>(), kKiloDouble);
  EXPECT_EQ(TestUnit::FromKilo(kKiloDouble).ToValue<int64_t>(), kValue);

  EXPECT_EQ(TestUnit::FromValue(kValue).ToValue<double>(), kValueDouble);
  EXPECT_EQ(TestUnit::FromValue(kValueDouble).ToValue<int64_t>(), kValue);

  EXPECT_NEAR(TestUnit::FromValue(kValue).ToMilli<double>(), kMilliDouble, 1);

  const double kPlusInfinity = std::numeric_limits<double>::infinity();
  const double kMinusInfinity = -kPlusInfinity;

  EXPECT_EQ(TestUnit::PlusInfinity().ToKilo<double>(), kPlusInfinity);
  EXPECT_EQ(TestUnit::MinusInfinity().ToKilo<double>(), kMinusInfinity);
  EXPECT_EQ(TestUnit::PlusInfinity().ToValue<double>(), kPlusInfinity);
  EXPECT_EQ(TestUnit::MinusInfinity().ToValue<double>(), kMinusInfinity);
  EXPECT_EQ(TestUnit::PlusInfinity().ToMilli<double>(), kPlusInfinity);
  EXPECT_EQ(TestUnit::MinusInfinity().ToMilli<double>(), kMinusInfinity);

  EXPECT_TRUE(TestUnit::FromKilo(kPlusInfinity).IsPlusInfinity());
  EXPECT_TRUE(TestUnit::FromKilo(kMinusInfinity).IsMinusInfinity());
  EXPECT_TRUE(TestUnit::FromValue(kPlusInfinity).IsPlusInfinity());
  EXPECT_TRUE(TestUnit::FromValue(kMinusInfinity).IsMinusInfinity());
}

TEST(UnitBaseTest, MathOperations) {
  const int64_t kValueA = 267;
  const int64_t kValueB = 450;
  const TestUnit delta_a = TestUnit::FromKilo(kValueA);
  const TestUnit delta_b = TestUnit::FromKilo(kValueB);
  EXPECT_EQ((delta_a + delta_b).ToKilo(), kValueA + kValueB);
  EXPECT_EQ((delta_a - delta_b).ToKilo(), kValueA - kValueB);

  const int32_t kInt32Value = 123;
  const double kFloatValue = 123.0;
  EXPECT_EQ((TestUnit::FromValue(kValueA) * kValueB).ToValue<int64_t>(),
            kValueA * kValueB);
  EXPECT_EQ((TestUnit::FromValue(kValueA) * kInt32Value).ToValue<int64_t>(),
            kValueA * kInt32Value);
  EXPECT_EQ((TestUnit::FromValue(kValueA) * kFloatValue).ToValue<int64_t>(),
            kValueA * kFloatValue);

  EXPECT_EQ((delta_b / 10).ToKilo(), kValueB / 10);
  EXPECT_EQ(delta_b / delta_a, static_cast<double>(kValueB) / kValueA);

  TestUnit mutable_delta = TestUnit::FromKilo(kValueA);
  mutable_delta += TestUnit::FromKilo(kValueB);
  EXPECT_EQ(mutable_delta, TestUnit::FromKilo(kValueA + kValueB));
  mutable_delta -= TestUnit::FromKilo(kValueB);
  EXPECT_EQ(mutable_delta, TestUnit::FromKilo(kValueA));

  // Division by an int rounds towards zero to follow regular int division.
  EXPECT_EQ(TestUnit::FromValue(789) / 10, TestUnit::FromValue(78));
  EXPECT_EQ(TestUnit::FromValue(-789) / 10, TestUnit::FromValue(-78));
}

#if GTEST_HAS_DEATH_TEST && RTC_DCHECK_IS_ON && !defined(WEBRTC_ANDROID)
TEST(UnitBaseTest, CrashesWhenCreatedFromNan) {
  EXPECT_DEATH(TestUnit::FromValue(NAN), "");
  EXPECT_DEATH(TestUnit::FromValue(0.0 / 0.0), "");
  EXPECT_DEATH(TestUnit::FromValue(INFINITY - INFINITY), "");
}
#endif

TEST(UnitBaseTest, InfinityOperations) {
  const int64_t kValue = 267;
  const TestUnit finite = TestUnit::FromKilo(kValue);
  EXPECT_TRUE((TestUnit::PlusInfinity() + finite).IsPlusInfinity());
  EXPECT_TRUE((TestUnit::PlusInfinity() - finite).IsPlusInfinity());
  EXPECT_TRUE((finite + TestUnit::PlusInfinity()).IsPlusInfinity());
  EXPECT_TRUE((finite - TestUnit::MinusInfinity()).IsPlusInfinity());

  EXPECT_TRUE((TestUnit::MinusInfinity() + finite).IsMinusInfinity());
  EXPECT_TRUE((TestUnit::MinusInfinity() - finite).IsMinusInfinity());
  EXPECT_TRUE((finite + TestUnit::MinusInfinity()).IsMinusInfinity());
  EXPECT_TRUE((finite - TestUnit::PlusInfinity()).IsMinusInfinity());
}

TEST(UnitBaseTest, UnaryMinus) {
  const int64_t kValue = 1337;
  const TestUnit unit = TestUnit::FromValue(kValue);
  EXPECT_EQ(-unit.ToValue(), -kValue);

  // Check infinity.
  EXPECT_EQ(-TestUnit::PlusInfinity(), TestUnit::MinusInfinity());
  EXPECT_EQ(-TestUnit::MinusInfinity(), TestUnit::PlusInfinity());
}

}  // namespace test
}  // namespace webrtc
