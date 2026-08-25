// Copyright 2026 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Include guard (still compiled once per target)
#if defined(HIGHWAY_HWY_CONTRIB_MATH_FAST_MATH_INL_H_) == \
    defined(HWY_TARGET_TOGGLE)  // NOLINT
#ifdef HIGHWAY_HWY_CONTRIB_MATH_FAST_MATH_INL_H_
#undef HIGHWAY_HWY_CONTRIB_MATH_FAST_MATH_INL_H_
#else
#define HIGHWAY_HWY_CONTRIB_MATH_FAST_MATH_INL_H_
#endif

#include <stddef.h>
#include <stdint.h>

#include "third_party/highway/hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

namespace impl {

// Port of reduce_angle_tan_SIMD
template <class D, class V = VFromD<D>>
HWY_INLINE void ReduceAngleTan(D d, V ang, V& x_red, V& sign) {
  using T = TFromD<D>;
  const auto pi = Set(d, static_cast<T>(3.14159265358979323846));
  const auto zero = Set(d, static_cast<T>(0.0));
  const auto one = Set(d, static_cast<T>(1.0));
  const auto minus_one = Set(d, static_cast<T>(-1.0));

  const auto inv_pi = Set(d, static_cast<T>(0.31830988618379067153777));

  // Modulo pi
  auto quotient = Mul(ang, inv_pi);
  quotient = Round(quotient);
  auto ang_mod = NegMulAdd(quotient, pi, ang);

  // Determine sign
  auto mask_neg = Lt(ang_mod, zero);
  sign = IfThenElse(mask_neg, minus_one, one);

  // Absolute value
  x_red = Abs(ang_mod);
}

// Range reduction and exponent extraction for logarithm functions.
// Normalizes x to y in [0.707, 1.414] and extracts the exponent as a float in
// 'exp'. If kHandleSubnormals is true, scales subnormal inputs to prevent
// underflow.
template <bool kHandleSubnormals = true, class D, class V>
HWY_INLINE void FastLogRangeReduction(D d, V x, V& y, V& exp) {
  using T = TFromD<D>;
  const RebindToSigned<D> di;
  const RebindToUnsigned<D> du;
  using TI = TFromD<decltype(di)>;
  using VI = decltype(Zero(di));

  constexpr bool kIsF32 = (sizeof(T) == 4);

  const VI kExpMagicDiff = Set(
      di, kIsF32
              ? static_cast<TI>(0x3F800000L - 0x3F3504F3L)
              : static_cast<TI>(0x3FF0000000000000LL - 0x3FE6A09E00000000LL));

  MFromD<D> is_denormal;
  if constexpr (kHandleSubnormals) {
    const V kMinNormal =
        Set(d, kIsF32 ? static_cast<T>(1.175494351e-38f)
                      : static_cast<T>(2.2250738585072014e-308));
    const V kScale = Set(d, kIsF32 ? static_cast<T>(3.355443200e+7f)
                                   : static_cast<T>(1.8014398509481984e+16));
    is_denormal = Lt(x, kMinNormal);
    x = MaskedMulOr(x, is_denormal, x, kScale);
  } else {
    (void)is_denormal;
  }

  auto exp_bits = Add(BitCast(di, x), kExpMagicDiff);

  constexpr int kMantissaShift = kIsF32 ? 23 : 52;
  const auto kBias = Set(di, kIsF32 ? 0x7F : 0x3FF);
  const auto exp_int = Sub(
      BitCast(di, ShiftRight<kMantissaShift>(BitCast(du, exp_bits))), kBias);
  exp = ConvertTo(d, exp_int);

  if constexpr (kHandleSubnormals) {
    const V kExpScaleFloat =
        Set(d, kIsF32 ? static_cast<T>(-25.0) : static_cast<T>(-54.0));
    exp = MaskedAddOr(exp, is_denormal, exp, kExpScaleFloat);
  }

  const VI exp_int_shifted = ShiftLeft<kMantissaShift>(exp_int);
  const VI y_bits = Sub(BitCast(di, x), exp_int_shifted);
  y = BitCast(d, y_bits);
}

}  // namespace impl

namespace impl {

template <class T>
struct FastExpImpl {};

template <>
struct FastExpImpl<float> {
  // Rounds float toward zero and returns as int32_t.
  template <class D, class V = VFromD<D>, HWY_IF_F32_D(D)>
  HWY_INLINE Vec<Rebind<int32_t, D>> ToInt32(D /*unused*/, V x) {
    return ConvertInRangeTo(Rebind<int32_t, D>(), x);
  }

  // Computes 2^x, where x is an integer.
  template <class D, class VI32 = Vec<Rebind<int32_t, D>>, HWY_IF_F32_D(D)>
  HWY_INLINE Vec<D> Pow2I(D d, VI32 x) {
    const Rebind<int32_t, D> di32;
    const VI32 kOffset = Set(di32, 0x7F);
    return BitCast(d, ShiftLeft<23>(Add(x, kOffset)));
  }

  // Sets the exponent of 'x' to 2^e.
  template <class D, class V = VFromD<D>, class VI32 = Vec<Rebind<int32_t, D>>,
            HWY_IF_F32_D(D)>
  HWY_INLINE V LoadExpShortRange(D d, V x, VI32 e) {
    const VI32 y = ShiftRight<1>(e);
    return Mul(Mul(x, Pow2I(d, y)), Pow2I(d, Sub(e, y)));
  }

  template <class D, class V = VFromD<D>, class VI32 = Vec<Rebind<int32_t, D>>,
            HWY_IF_F32_D(D)>
  HWY_INLINE V ExpReduce(D d, V x, VI32 q) {
    // kMinusLn2 ~= -ln(2)
    const V kMinusLn2 = Set(d, -0.69314718056f);

    // Extended precision modular arithmetic.
    const V qf = ConvertTo(d, q);
    return MulAdd(qf, kMinusLn2, x);
  }

  template <class D, class V = VFromD<D>, class VI32 = Vec<Rebind<int32_t, D>>,
            HWY_IF_F32_D(D)>
  HWY_INLINE V Exp2Reduce(D d, V x, VI32 q) {
    const V qf = ConvertTo(d, q);
    return Sub(x, qf);
  }
};

#if HWY_HAVE_FLOAT64 && HWY_HAVE_INTEGER64
template <>
struct FastExpImpl<double> {
  // Rounds double toward zero and returns as int32_t.
  template <class D, class V = VFromD<D>, HWY_IF_F64_D(D)>
  HWY_INLINE Vec<Rebind<int32_t, D>> ToInt32(D /*unused*/, V x) {
    return DemoteInRangeTo(Rebind<int32_t, D>(), x);
  }

  // Computes 2^x, where x is an integer.
  template <class D, class VI32 = Vec<Rebind<int32_t, D>>, HWY_IF_F64_D(D)>
  HWY_INLINE Vec<D> Pow2I(D d, VI32 x) {
    const Rebind<int32_t, D> di32;
    const Rebind<int64_t, D> di64;
    const VI32 kOffset = Set(di32, 0x3FF);
    return BitCast(d, ShiftLeft<52>(PromoteTo(di64, Add(x, kOffset))));
  }

  // Sets the exponent of 'x' to 2^e.
  template <class D, class V = VFromD<D>, class VI32 = Vec<Rebind<int32_t, D>>,
            HWY_IF_F64_D(D)>
  HWY_INLINE V LoadExpShortRange(D d, V x, VI32 e) {
    const VI32 y = ShiftRight<1>(e);
    return Mul(Mul(x, Pow2I(d, y)), Pow2I(d, Sub(e, y)));
  }

  template <class D, class V = VFromD<D>, class VI32 = Vec<Rebind<int32_t, D>>,
            HWY_IF_F64_D(D)>
  HWY_INLINE V ExpReduce(D d, V x, VI32 q) {
    // kMinusLn2 ~= -ln(2)
    const V kMinusLn2 = Set(d, -0.6931471805599453);

    // Extended precision modular arithmetic.
    const V qf = PromoteTo(d, q);
    return MulAdd(qf, kMinusLn2, x);
  }

  template <class D, class V = VFromD<D>, class VI32 = Vec<Rebind<int32_t, D>>,
            HWY_IF_F64_D(D)>
  HWY_INLINE V Exp2Reduce(D d, V x, VI32 q) {
    const V qf = PromoteTo(d, q);
    return Sub(x, qf);
  }
};
#endif

}  // namespace impl

/**
 * Fast approximation of tan(x).
 *
 * Valid Lane Types: float32, float64
 * Max Relative Error: < 0.35% for angles equivalent to falling between [-89.99,
 * +89.99] degrees (float32) and
 *                     [-89.9999999, +89.9999999] degrees (float64).
 * Valid Range: float32 : [-20, +20]rads
 *              float64 : [-39000, +39000]rads
 *
 * Note: Inputs extremely close to asymptotes may result in
 * a sign flip due to precision limits.
 *
 * @return tangent of 'x'
 */
template <class D, class V>
HWY_INLINE V FastTan(D d, V x) {
  using T = TFromD<D>;

  // Reduction
  V x_red, sign;
  impl::ReduceAngleTan(d, x, x_red, sign);

  V b, c, d_val;

  if constexpr (CanLookup8(d)) {
    // --- Table Lookup ---
    const auto scale = Set(d, static_cast<T>(3.8197186342));
    auto idx_float = Floor(Mul(x_red, scale));

    // Convert to Integer Vector (Signed)
    auto idx_int = ConvertTo(RebindToSigned<D>(), idx_float);

    HWY_ALIGN static constexpr T arr_b[8] = {
        static_cast<T>(0),
        static_cast<T>(0.0174532925199432955),
        static_cast<T>(0.133808575986231942),
        static_cast<T>(0.378736447682769484),
        static_cast<T>(1.29590696960578966),
        static_cast<T>(9.45968454580926554),
        static_cast<T>(9.45968454580926554),
        static_cast<T>(9.45968454580926554)};

    HWY_ALIGN static constexpr T arr_c[8] = {
        static_cast<T>(-0.0909090909092633431),
        static_cast<T>(-0.400000000000000022),
        static_cast<T>(-0.83333333333333337),
        static_cast<T>(-1.29999999999999982),
        static_cast<T>(-2.5),
        static_cast<T>(-10.9999999999791349),
        static_cast<T>(-10.9999999999791349),
        static_cast<T>(-10.9999999999791349)};

    HWY_ALIGN static constexpr T arr_d[8] = {
        static_cast<T>(1.00277098842046231),
        static_cast<T>(1.14668131856027444),
        static_cast<T>(1.57370520888155374),
        static_cast<T>(2.18515222349690053),
        static_cast<T>(3.97062404828709958),
        static_cast<T>(17.2787595947438639),
        static_cast<T>(17.2787595947438639),
        static_cast<T>(17.2787595947438639)};

    // Since Lookup8 is available for HWY_MIN_BYTES / sizeof(T) >= 4, this
    // condition covers all cases we encounter inside the top level if block
    // inside FastTan
    b = Lookup8(d, arr_b, idx_int);
    c = Lookup8(d, arr_c, idx_int);
    d_val = Lookup8(d, arr_d, idx_int);
  } else {
    // --- FALLBACK PATH: Blend Chain ---
    if constexpr (HWY_REGISTERS >= 32) {
      // Split into two parallel chains to reduce dependency latency.
      const auto t0 = Set(d, static_cast<T>(0.2617993877995256));
      const auto t1 = Set(d, static_cast<T>(0.5235987755990512));
      const auto t2 = Set(d, static_cast<T>(0.7853981633985767));
      const auto t3 = Set(d, static_cast<T>(1.0471975511981024));
      const auto t4 = Set(d, static_cast<T>(1.3089969389976279));

      // -- Chain 1: Indices 0 to 2 (Evaluated starting from t1 down to t0)
      auto b_low = Set(d, static_cast<T>(0.133808575986231942));  // idx 2
      auto c_low = Set(d, static_cast<T>(-0.83333333333333337));
      auto d_low = Set(d, static_cast<T>(1.57370520888155374));

      auto mask = Lt(x_red, t1);
      b_low = IfThenElse(mask, Set(d, static_cast<T>(0.0174532925199432955)),
                         b_low);
      c_low = IfThenElse(mask, Set(d, static_cast<T>(-0.400000000000000022)),
                         c_low);
      d_low =
          IfThenElse(mask, Set(d, static_cast<T>(1.14668131856027444)), d_low);

      mask = Lt(x_red, t0);
      b_low = IfThenZeroElse(mask, b_low);
      c_low = IfThenElse(mask, Set(d, static_cast<T>(-0.0909090909092633431)),
                         c_low);
      d_low =
          IfThenElse(mask, Set(d, static_cast<T>(1.00277098842046231)), d_low);

      // -- Chain 2: Indices 3 to 5 (Evaluated starting from t4 down to t3)
      auto b_high = Set(d, static_cast<T>(9.45968454580926554));  // idx 5
      auto c_high = Set(d, static_cast<T>(-10.9999999999791349));
      auto d_high = Set(d, static_cast<T>(17.2787595947438639));

      mask = Lt(x_red, t4);
      b_high =
          IfThenElse(mask, Set(d, static_cast<T>(1.29590696960578966)), b_high);
      c_high = IfThenElse(mask, Set(d, static_cast<T>(-2.5)), c_high);
      d_high =
          IfThenElse(mask, Set(d, static_cast<T>(3.97062404828709958)), d_high);

      mask = Lt(x_red, t3);
      b_high = IfThenElse(mask, Set(d, static_cast<T>(0.378736447682769484)),
                          b_high);
      c_high = IfThenElse(mask, Set(d, static_cast<T>(-1.29999999999999982)),
                          c_high);
      d_high =
          IfThenElse(mask, Set(d, static_cast<T>(2.18515222349690053)), d_high);

      // -- Merge the two chains
      auto merge_mask = Lt(x_red, t2);
      b = IfThenElse(merge_mask, b_low, b_high);
      c = IfThenElse(merge_mask, c_low, c_high);
      d_val = IfThenElse(merge_mask, d_low, d_high);
    } else {
      b = Set(d, static_cast<T>(9.45968454580926554));
      c = Set(d, static_cast<T>(-10.9999999999791349));
      d_val = Set(d, static_cast<T>(17.2787595947438639));

      auto mask = Lt(x_red, Set(d, static_cast<T>(1.3089969389976279)));
      b = IfThenElse(mask, Set(d, static_cast<T>(1.29590696960578966)), b);
      c = IfThenElse(mask, Set(d, static_cast<T>(-2.5)), c);
      d_val =
          IfThenElse(mask, Set(d, static_cast<T>(3.97062404828709958)), d_val);

      mask = Lt(x_red, Set(d, static_cast<T>(1.0471975511981024)));
      b = IfThenElse(mask, Set(d, static_cast<T>(0.378736447682769484)), b);
      c = IfThenElse(mask, Set(d, static_cast<T>(-1.29999999999999982)), c);
      d_val =
          IfThenElse(mask, Set(d, static_cast<T>(2.18515222349690053)), d_val);

      mask = Lt(x_red, Set(d, static_cast<T>(0.7853981633985767)));
      b = IfThenElse(mask, Set(d, static_cast<T>(0.133808575986231942)), b);
      c = IfThenElse(mask, Set(d, static_cast<T>(-0.83333333333333337)), c);
      d_val =
          IfThenElse(mask, Set(d, static_cast<T>(1.57370520888155374)), d_val);

      mask = Lt(x_red, Set(d, static_cast<T>(0.5235987755990512)));
      b = IfThenElse(mask, Set(d, static_cast<T>(0.0174532925199432955)), b);
      c = IfThenElse(mask, Set(d, static_cast<T>(-0.400000000000000022)), c);
      d_val =
          IfThenElse(mask, Set(d, static_cast<T>(1.14668131856027444)), d_val);

      mask = Lt(x_red, Set(d, static_cast<T>(0.2617993877995256)));
      b = IfThenZeroElse(mask, b);
      c = IfThenElse(mask, Set(d, static_cast<T>(-0.0909090909092633431)), c);
      d_val =
          IfThenElse(mask, Set(d, static_cast<T>(1.00277098842046231)), d_val);
    }
  }

  // Math: y=(x + b)/(cx + d)
  auto num = Add(x_red, b);
  auto den = MulAdd(c, x_red, d_val);

  // Guard against denominator underflow/sign-flip near singularities
  T epsilon_val;
  if constexpr (sizeof(T) == 8) {
    epsilon_val = static_cast<T>(1e-15);
  } else {
    epsilon_val = static_cast<T>(1e-6);
  }
  const auto kMinDenom = Set(d, epsilon_val);
  // We use Abs() because on the reduced interval [0, pi/2], the tangent
  // magnitude must be positive. If the polynomial approximation calculates a
  // negative denominator (overshoot), it is an error, and we force it to be
  // positive.
  den = Max(Abs(den), kMinDenom);

  auto result = Div(num, den);

  // Apply Sign
  return CopySign(result, sign);
}

/**
 * Fast approximation of atan(x).
 *
 * Valid Lane Types: float32, float64
 * Max Relative Error: 0.0034%
 * Average Relative Error : 0.0002% for float32
 *                          0.0002% for float64
 * Valid Range: float32: [-1e35, +1e35]
 *              float64: [-1e305, +1e305]
 *
 * @return arctangent of 'x'
 */
// if kAssumePositive is true, we assume inputs are non-negative.
template <bool kAssumePositive = false, class D, class V>
HWY_INLINE V FastAtan(D d, V val) {
  using T = TFromD<D>;

  // Abs(val) and preserve sign for later (if needed)
  V y;
  if constexpr (kAssumePositive) {
    y = val;
  } else {
    y = Abs(val);
  }

  const V kOne = Set(d, static_cast<T>(1.0));
  const auto gt1_mask = Gt(y, kOne);
  // Domain reduction: map [1, inf) to [0, 1]
  const V mapped_y = MaskedDivOr(y, gt1_mask, kOne, y);

  // Degree 4 polynomial for atan(x) / x over [0, 1]
  const V c0 = Set(d, static_cast<T>(0.9999653683169244));
  const V c1 = Set(d, static_cast<T>(-0.3315525587266785));
  const V c2 = Set(d, static_cast<T>(0.1844770291758270));
  const V c3 = Set(d, static_cast<T>(-0.0907475543745560));
  const V c4 = Set(d, static_cast<T>(0.0232748721030191));

  const V z = Mul(mapped_y, mapped_y);
  const V z2 = Mul(z, z);

  const V p01 = MulAdd(c1, z, c0);
  const V p23 = MulAdd(c3, z, c2);
  const V p234 = MulAdd(z2, c4, p23);
  const V p = MulAdd(z2, p234, p01);

  const V poly = Mul(mapped_y, p);

  const V kPiOverTwo = Set(d, static_cast<T>(1.57079632679489661923));
  auto result = MaskedSubOr(poly, gt1_mask, kPiOverTwo, poly);

  if constexpr (kAssumePositive) {
    return result;
  } else {
    return CopySign(result, val);
  }
}

/**
 * Fast approximation of atan2(y, x).
 *
 * Valid Lane Types: float32, float64
 * Valid Range: As long as y/x is in Valid Range for FastAtan()
 * Correctly handles negative zero, infinities, and NaN.
 * @return atan2 of 'y', 'x'
 */
template <class D, class V>
HWY_INLINE V FastAtan2(const D d, V y, V x) {
  using T = TFromD<D>;
  using M = MFromD<D>;

  const V kPi = Set(d, static_cast<T>(3.14159265358979323846264));
  const V kPiOverTwo = Set(d, static_cast<T>(1.57079632679489661923));
  const V kOne = Set(d, static_cast<T>(1.0));
  const V k0 = Zero(d);

  const V ax = Abs(x);
  const V ay = Abs(y);

  const V num = Min(ax, ay);
  const V den = Max(ax, ay);

  const M is_inf = IsInf(num);
  V mapped_y = MaskedDivOr(k0, Ne(den, k0), num, den);
  mapped_y = IfThenElse(is_inf, kOne, mapped_y);

  // Degree 4 polynomial for atan(x) / x over [0, 1]
  const V c0 = Set(d, static_cast<T>(0.9999653683169244));
  const V c1 = Set(d, static_cast<T>(-0.3315525587266785));
  const V c2 = Set(d, static_cast<T>(0.1844770291758270));
  const V c3 = Set(d, static_cast<T>(-0.0907475543745560));
  const V c4 = Set(d, static_cast<T>(0.0232748721030191));

  const V z = Mul(mapped_y, mapped_y);
  const V z2 = Mul(z, z);

  const V p01 = MulAdd(c1, z, c0);
  const V p23 = MulAdd(c3, z, c2);
  const V p234 = MulAdd(z2, c4, p23);
  const V p = MulAdd(z2, p234, p01);

  const V poly = Mul(mapped_y, p);

  const M ay_gt_ax = Gt(ay, ax);
  V angle = MaskedSubOr(poly, ay_gt_ax, kPiOverTwo, poly);

  const M x_neg = Lt(x, k0);
  angle = MaskedSubOr(angle, x_neg, kPi, angle);

  const M is_nan = IsEitherNaN(y, x);
  return IfThenElse(is_nan, NaN(d), CopySign(angle, y));
}

namespace impl {

// Computes the index vector required for Lookup8 when the
// intervals are uneven. Runs either an adder tree or a sequential add chain
// depending on the number of registers.
template <class D, class V>
HWY_INLINE Vec<RebindToSigned<D>> ComputeIndices8Intervals(
    D d, V y, const TFromD<D>* HWY_RESTRICT thresholds) {
  using DI = RebindToSigned<D>;
  auto idx_i = Zero(DI());
  const auto one_i = Set(DI(), 1);

  const auto t0 = Set(d, thresholds[0]);
  const auto t1 = Set(d, thresholds[1]);
  const auto t2 = Set(d, thresholds[2]);
  const auto t3 = Set(d, thresholds[3]);
  const auto t4 = Set(d, thresholds[4]);
  const auto t5 = Set(d, thresholds[5]);
  const auto t6 = Set(d, thresholds[6]);

  const auto mask0 = RebindMask(DI(), Ge(y, t0));
  const auto mask1 = RebindMask(DI(), Ge(y, t1));
  const auto mask2 = RebindMask(DI(), Ge(y, t2));
  const auto mask3 = RebindMask(DI(), Ge(y, t3));
  const auto mask4 = RebindMask(DI(), Ge(y, t4));
  const auto mask5 = RebindMask(DI(), Ge(y, t5));
  const auto mask6 = RebindMask(DI(), Ge(y, t6));

#ifdef HWY_NATIVE_MASK
  if constexpr (HWY_REGISTERS >= 32) {
    // Adder tree for native masks.
    const auto sum0 = IfThenElseZero(mask0, one_i);
    const auto sum01 = MaskedAddOr(sum0, mask1, sum0, one_i);

    const auto sum2 = IfThenElseZero(mask2, one_i);
    const auto sum23 = MaskedAddOr(sum2, mask3, sum2, one_i);

    const auto sum4 = IfThenElseZero(mask4, one_i);
    const auto sum45 = MaskedAddOr(sum4, mask5, sum4, one_i);

    const auto sum6 = IfThenElseZero(mask6, one_i);

    const auto sum03 = Add(sum01, sum23);
    const auto sum46 = Add(sum45, sum6);

    idx_i = Add(sum03, sum46);
  } else {
    // 2x unrolled sequential chain.
    const auto sum0 = IfThenElseZero(mask0, one_i);
    const auto sum02 = MaskedAddOr(sum0, mask2, sum0, one_i);
    const auto sum024 = MaskedAddOr(sum02, mask4, sum02, one_i);
    const auto sum0246 = MaskedAddOr(sum024, mask6, sum024, one_i);

    const auto sum1 = IfThenElseZero(mask1, one_i);
    const auto sum13 = MaskedAddOr(sum1, mask3, sum1, one_i);
    const auto sum135 = MaskedAddOr(sum13, mask5, sum13, one_i);

    idx_i = Add(sum0246, sum135);
  }
#else
  (void)one_i;
  if constexpr (HWY_REGISTERS >= 32) {
    // Accummulate -1s in a tree to reduce latency
    const auto m0 = VecFromMask(DI(), mask0);
    const auto m1 = VecFromMask(DI(), mask1);
    const auto m2 = VecFromMask(DI(), mask2);
    const auto m3 = VecFromMask(DI(), mask3);
    const auto m4 = VecFromMask(DI(), mask4);
    const auto m5 = VecFromMask(DI(), mask5);
    const auto m6 = VecFromMask(DI(), mask6);

    const auto sum01 = Add(m0, m1);
    const auto sum23 = Add(m2, m3);
    const auto sum45 = Add(m4, m5);

    const auto sum03 = Add(sum01, sum23);
    const auto sum46 = Add(sum45, m6);

    idx_i = Neg(Add(sum03, sum46));
  } else {
    // Subtract in a 2x unrolled chain
    auto sum0246 = Sub(idx_i, VecFromMask(DI(), mask0));
    sum0246 = Sub(sum0246, VecFromMask(DI(), mask2));
    sum0246 = Sub(sum0246, VecFromMask(DI(), mask4));
    sum0246 = Sub(sum0246, VecFromMask(DI(), mask6));

    auto sum135 = Zero(DI());
    sum135 = Sub(sum135, VecFromMask(DI(), mask1));
    sum135 = Sub(sum135, VecFromMask(DI(), mask3));
    sum135 = Sub(sum135, VecFromMask(DI(), mask5));

    idx_i = Add(sum0246, sum135);
  }
#endif
  return idx_i;
}

}  // namespace impl

/**
 * Fast approximation of tanh(x).
 *
 * Valid Lane Types: float32, float64
 * Max Relative Error : 0.0006% for float32, 0.0006% for float64
 * Average Relative Error : 0.00002% for float32, 3e-6% for float64
 * Max Relative Error for [-0.01, 0.01] : 0.00003%
 * Average Relative Error for [-0.01, 0.01] : 3e-7%
 * Valid Range: float32: [-1e35, +1e35]
 *              float64: [-1e305, +1e305]
 *
 * @return hyperbolic tangent of 'x'
 */
template <class D, class V>
HWY_INLINE V FastTanh(D d, V val) {
  using T = TFromD<D>;

  // Abs(val) and preserve sign for later
  auto y = Abs(val);

  V a, b, c, d_val, e, f;

  HWY_ALIGN static constexpr T thresholds[7] = {
      static_cast<T>(0.168236118310606), static_cast<T>(0.365443754271396),
      static_cast<T>(0.549306144334055), static_cast<T>(0.804718956217050),
      static_cast<T>(1.203972804325936), static_cast<T>(2.969315202883957),
      static_cast<T>(4.734657601441978)};

  if constexpr (CanLookup8(d)) {
    auto idx_i = impl::ComputeIndices8Intervals(d, y, thresholds);

    HWY_ALIGN static constexpr T arr_a[8] = {
        static_cast<T>(0.124683326807972),
        static_cast<T>(0.0650303189120701),
        static_cast<T>(-0.012865365312548),
        static_cast<T>(-0.0600814996891072),
        static_cast<T>(-0.0456234607880718),
        static_cast<T>(0.00382424142943801),
        static_cast<T>(0.000272471748022028),
        static_cast<T>(8.15222218981581e-06)};

    HWY_ALIGN static constexpr T arr_b[8] = {
        static_cast<T>(0.00220499585798237),
        static_cast<T>(0.0576751179885766),
        static_cast<T>(0.197711481341899),
        static_cast<T>(0.325772792901464),
        static_cast<T>(0.256935827482807),
        static_cast<T>(-0.0559644608292387),
        static_cast<T>(-0.00594131995144914),
        static_cast<T>(-0.000249501181979395)};

    HWY_ALIGN static constexpr T arr_c[8] = {
        static_cast<T>(-0.333553679129082), static_cast<T>(-0.355207242694923),
        static_cast<T>(-0.457494048166093), static_cast<T>(-0.597530579050227),
        static_cast<T>(-0.467928171646331), static_cast<T>(0.337223118234543),
        static_cast<T>(0.0522965013918652), static_cast<T>(0.0030684314549725)};

    HWY_ALIGN static constexpr T arr_d[8] = {
        static_cast<T>(9.56515391952438e-06),
        static_cast<T>(0.00439112349520911),
        static_cast<T>(0.042321724042285),
        static_cast<T>(0.119506151013929),
        static_cast<T>(-0.00146222560953702),
        static_cast<T>(-1.05430328521368),
        static_cast<T>(-0.232902952602555),
        static_cast<T>(-0.0189739119945283)};

    HWY_ALIGN static constexpr T arr_e[8] = {
        static_cast<T>(0.999999846647538), static_cast<T>(0.999545718850479),
        static_cast<T>(0.992411248107215), static_cast<T>(0.970968585888465),
        static_cast<T>(1.02705747414947),  static_cast<T>(1.7260732085471),
        static_cast<T>(0.526554911169526), static_cast<T>(0.0590636129554906)};

    HWY_ALIGN static constexpr T arr_f[8] = {
        static_cast<T>(4.72832130652986e-10),
        static_cast<T>(1.91000262234958e-05),
        static_cast<T>(0.000562934372196535),
        static_cast<T>(0.00296459124064406),
        static_cast<T>(-0.0073852515760983),
        static_cast<T>(-0.195632701517054),
        static_cast<T>(0.51454722991951),
        static_cast<T>(0.92584756176511)};

    a = Lookup8(d, arr_a, idx_i);
    b = Lookup8(d, arr_b, idx_i);
    c = Lookup8(d, arr_c, idx_i);
    d_val = Lookup8(d, arr_d, idx_i);
    e = Lookup8(d, arr_e, idx_i);
    f = Lookup8(d, arr_f, idx_i);
  } else {
    const auto t0 = Set(d, thresholds[0]);
    const auto t1 = Set(d, thresholds[1]);
    const auto t2 = Set(d, thresholds[2]);
    const auto t3 = Set(d, thresholds[3]);
    const auto t4 = Set(d, thresholds[4]);
    const auto t5 = Set(d, thresholds[5]);
    const auto t6 = Set(d, thresholds[6]);
    // --- FALLBACK PATH: Blend Chain ---
    if constexpr (HWY_REGISTERS >= 32) {
      // Split into two parallel chains to reduce dependency latency.

      // -- Chain 1: Indices 0 to 3
      auto a_low = Set(d, static_cast<T>(-0.0600814996891072));  // idx 3
      auto b_low = Set(d, static_cast<T>(0.325772792901464));
      auto c_low = Set(d, static_cast<T>(-0.597530579050227));
      auto d_low = Set(d, static_cast<T>(0.119506151013929));
      auto e_low = Set(d, static_cast<T>(0.970968585888465));
      auto f_low = Set(d, static_cast<T>(0.00296459124064406));

      auto mask = Lt(y, t2);
      a_low =
          IfThenElse(mask, Set(d, static_cast<T>(-0.012865365312548)), a_low);
      b_low =
          IfThenElse(mask, Set(d, static_cast<T>(0.197711481341899)), b_low);
      c_low =
          IfThenElse(mask, Set(d, static_cast<T>(-0.457494048166093)), c_low);
      d_low =
          IfThenElse(mask, Set(d, static_cast<T>(0.042321724042285)), d_low);
      e_low =
          IfThenElse(mask, Set(d, static_cast<T>(0.992411248107215)), e_low);
      f_low =
          IfThenElse(mask, Set(d, static_cast<T>(0.000562934372196535)), f_low);

      mask = Lt(y, t1);
      a_low =
          IfThenElse(mask, Set(d, static_cast<T>(0.0650303189120701)), a_low);
      b_low =
          IfThenElse(mask, Set(d, static_cast<T>(0.0576751179885766)), b_low);
      c_low =
          IfThenElse(mask, Set(d, static_cast<T>(-0.355207242694923)), c_low);
      d_low =
          IfThenElse(mask, Set(d, static_cast<T>(0.00439112349520911)), d_low);
      e_low =
          IfThenElse(mask, Set(d, static_cast<T>(0.999545718850479)), e_low);
      f_low =
          IfThenElse(mask, Set(d, static_cast<T>(1.91000262234958e-05)), f_low);

      mask = Lt(y, t0);
      a_low =
          IfThenElse(mask, Set(d, static_cast<T>(0.124683326807972)), a_low);
      b_low =
          IfThenElse(mask, Set(d, static_cast<T>(0.00220499585798237)), b_low);
      c_low =
          IfThenElse(mask, Set(d, static_cast<T>(-0.333553679129082)), c_low);
      d_low =
          IfThenElse(mask, Set(d, static_cast<T>(9.56515391952438e-06)), d_low);
      e_low =
          IfThenElse(mask, Set(d, static_cast<T>(0.999999846647538)), e_low);
      f_low =
          IfThenElse(mask, Set(d, static_cast<T>(4.72832130652986e-10)), f_low);

      // -- Chain 2: Indices 4 to 7
      auto a_high = Set(d, static_cast<T>(8.15222218981581e-06));  // idx 7
      auto b_high = Set(d, static_cast<T>(-0.000249501181979395));
      auto c_high = Set(d, static_cast<T>(0.0030684314549725));
      auto d_high = Set(d, static_cast<T>(-0.0189739119945283));
      auto e_high = Set(d, static_cast<T>(0.0590636129554906));
      auto f_high = Set(d, static_cast<T>(0.92584756176511));

      mask = Lt(y, t6);
      a_high = IfThenElse(mask, Set(d, static_cast<T>(0.000272471748022028)),
                          a_high);
      b_high = IfThenElse(mask, Set(d, static_cast<T>(-0.00594131995144914)),
                          b_high);
      c_high =
          IfThenElse(mask, Set(d, static_cast<T>(0.0522965013918652)), c_high);
      d_high =
          IfThenElse(mask, Set(d, static_cast<T>(-0.232902952602555)), d_high);
      e_high =
          IfThenElse(mask, Set(d, static_cast<T>(0.526554911169526)), e_high);
      f_high =
          IfThenElse(mask, Set(d, static_cast<T>(0.51454722991951)), f_high);

      mask = Lt(y, t5);
      a_high =
          IfThenElse(mask, Set(d, static_cast<T>(0.00382424142943801)), a_high);
      b_high =
          IfThenElse(mask, Set(d, static_cast<T>(-0.0559644608292387)), b_high);
      c_high =
          IfThenElse(mask, Set(d, static_cast<T>(0.337223118234543)), c_high);
      d_high =
          IfThenElse(mask, Set(d, static_cast<T>(-1.05430328521368)), d_high);
      e_high =
          IfThenElse(mask, Set(d, static_cast<T>(1.7260732085471)), e_high);
      f_high =
          IfThenElse(mask, Set(d, static_cast<T>(-0.195632701517054)), f_high);

      mask = Lt(y, t4);
      a_high =
          IfThenElse(mask, Set(d, static_cast<T>(-0.0456234607880718)), a_high);
      b_high =
          IfThenElse(mask, Set(d, static_cast<T>(0.256935827482807)), b_high);
      c_high =
          IfThenElse(mask, Set(d, static_cast<T>(-0.467928171646331)), c_high);
      d_high = IfThenElse(mask, Set(d, static_cast<T>(-0.00146222560953702)),
                          d_high);
      e_high =
          IfThenElse(mask, Set(d, static_cast<T>(1.02705747414947)), e_high);
      f_high =
          IfThenElse(mask, Set(d, static_cast<T>(-0.0073852515760983)), f_high);

      // Combine chains
      mask = Lt(y, t3);
      a = IfThenElse(mask, a_low, a_high);
      b = IfThenElse(mask, b_low, b_high);
      c = IfThenElse(mask, c_low, c_high);
      d_val = IfThenElse(mask, d_low, d_high);
      e = IfThenElse(mask, e_low, e_high);
      f = IfThenElse(mask, f_low, f_high);
    } else {
      // Serial chain for lower register count
      // Start with highest index (7)
      a = Set(d, static_cast<T>(8.15222218981581e-06));
      b = Set(d, static_cast<T>(-0.000249501181979395));
      c = Set(d, static_cast<T>(0.0030684314549725));
      d_val = Set(d, static_cast<T>(-0.0189739119945283));
      e = Set(d, static_cast<T>(0.0590636129554906));
      f = Set(d, static_cast<T>(0.92584756176511));

      // If y < t6 (idx 6)
      auto mask = Lt(y, t6);
      a = IfThenElse(mask, Set(d, static_cast<T>(0.000272471748022028)), a);
      b = IfThenElse(mask, Set(d, static_cast<T>(-0.00594131995144914)), b);
      c = IfThenElse(mask, Set(d, static_cast<T>(0.0522965013918652)), c);
      d_val =
          IfThenElse(mask, Set(d, static_cast<T>(-0.232902952602555)), d_val);
      e = IfThenElse(mask, Set(d, static_cast<T>(0.526554911169526)), e);
      f = IfThenElse(mask, Set(d, static_cast<T>(0.51454722991951)), f);

      // If y < t5 (idx 5)
      mask = Lt(y, t5);
      a = IfThenElse(mask, Set(d, static_cast<T>(0.00382424142943801)), a);
      b = IfThenElse(mask, Set(d, static_cast<T>(-0.0559644608292387)), b);
      c = IfThenElse(mask, Set(d, static_cast<T>(0.337223118234543)), c);
      d_val =
          IfThenElse(mask, Set(d, static_cast<T>(-1.05430328521368)), d_val);
      e = IfThenElse(mask, Set(d, static_cast<T>(1.7260732085471)), e);
      f = IfThenElse(mask, Set(d, static_cast<T>(-0.195632701517054)), f);

      // If y < t4 (idx 4)
      mask = Lt(y, t4);
      a = IfThenElse(mask, Set(d, static_cast<T>(-0.0456234607880718)), a);
      b = IfThenElse(mask, Set(d, static_cast<T>(0.256935827482807)), b);
      c = IfThenElse(mask, Set(d, static_cast<T>(-0.467928171646331)), c);
      d_val =
          IfThenElse(mask, Set(d, static_cast<T>(-0.00146222560953702)), d_val);
      e = IfThenElse(mask, Set(d, static_cast<T>(1.02705747414947)), e);
      f = IfThenElse(mask, Set(d, static_cast<T>(-0.0073852515760983)), f);

      // If y < t3 (idx 3)
      mask = Lt(y, t3);
      a = IfThenElse(mask, Set(d, static_cast<T>(-0.0600814996891072)), a);
      b = IfThenElse(mask, Set(d, static_cast<T>(0.325772792901464)), b);
      c = IfThenElse(mask, Set(d, static_cast<T>(-0.597530579050227)), c);
      d_val =
          IfThenElse(mask, Set(d, static_cast<T>(0.119506151013929)), d_val);
      e = IfThenElse(mask, Set(d, static_cast<T>(0.970968585888465)), e);
      f = IfThenElse(mask, Set(d, static_cast<T>(0.00296459124064406)), f);

      // If y < t2 (idx 2)
      mask = Lt(y, t2);
      a = IfThenElse(mask, Set(d, static_cast<T>(-0.012865365312548)), a);
      b = IfThenElse(mask, Set(d, static_cast<T>(0.197711481341899)), b);
      c = IfThenElse(mask, Set(d, static_cast<T>(-0.457494048166093)), c);
      d_val =
          IfThenElse(mask, Set(d, static_cast<T>(0.042321724042285)), d_val);
      e = IfThenElse(mask, Set(d, static_cast<T>(0.992411248107215)), e);
      f = IfThenElse(mask, Set(d, static_cast<T>(0.000562934372196535)), f);

      // If y < t1 (idx 1)
      mask = Lt(y, t1);
      a = IfThenElse(mask, Set(d, static_cast<T>(0.0650303189120701)), a);
      b = IfThenElse(mask, Set(d, static_cast<T>(0.0576751179885766)), b);
      c = IfThenElse(mask, Set(d, static_cast<T>(-0.355207242694923)), c);
      d_val =
          IfThenElse(mask, Set(d, static_cast<T>(0.00439112349520911)), d_val);
      e = IfThenElse(mask, Set(d, static_cast<T>(0.999545718850479)), e);
      f = IfThenElse(mask, Set(d, static_cast<T>(1.91000262234958e-05)), f);

      // If y < t0 (idx 0)
      mask = Lt(y, t0);
      a = IfThenElse(mask, Set(d, static_cast<T>(0.124683326807972)), a);
      b = IfThenElse(mask, Set(d, static_cast<T>(0.00220499585798237)), b);
      c = IfThenElse(mask, Set(d, static_cast<T>(-0.333553679129082)), c);
      d_val =
          IfThenElse(mask, Set(d, static_cast<T>(9.56515391952438e-06)), d_val);
      e = IfThenElse(mask, Set(d, static_cast<T>(0.999999846647538)), e);
      f = IfThenElse(mask, Set(d, static_cast<T>(4.72832130652986e-10)), f);
    }
  }

  // Math: f(y) = ay^5 + by^4 + cy^3 + dy^2 + ey + f
  // Using Estrin's scheme
  const auto y2 = Mul(y, y);
  // term0 = e*y + f
  const auto term0 = MulAdd(e, y, f);
  // term1 = c*y + d
  const auto term1 = MulAdd(c, y, d_val);
  // term2 = a*y + b
  const auto term2 = MulAdd(a, y, b);
  // term3 = term2 * y2 + term1
  const auto term3 = MulAdd(term2, y2, term1);
  // result = term3 * y2 + term0
  auto result = MulAdd(term3, y2, term0);

  const auto kSmall = Set(d, static_cast<T>(0.001));
  result = IfThenElse(Lt(y, kSmall), y, result);

  const auto k1 = Set(d, static_cast<T>(1.0));
  // We can take Min since the 5 degree polynomial  approximation for index 7 is
  // monotonically increasing, so for inputs >6.5 the polynomial approximation
  // will output >1.0 allowing us to use Min() directly instead of IfThenElse()
  result = Min(result, k1);

  return CopySign(result, val);  // Restore sign
}

namespace impl {

// Fallback path used when Lookup8 cannot be used. Computes 4 final coefficient
// vectors by running a blend chain (either serially or in parallel depending
// on the number of registers)
template <class D, class V>
HWY_INLINE void FallbackBlendChain4Coeff(
    D d, V y, const TFromD<D>* HWY_RESTRICT thresholds,
    const TFromD<D>* HWY_RESTRICT arr_a, const TFromD<D>* HWY_RESTRICT arr_b,
    const TFromD<D>* HWY_RESTRICT arr_c, const TFromD<D>* HWY_RESTRICT arr_d,
    V& a, V& b, V& c, V& d_val) {
  const auto t0 = Set(d, thresholds[0]);
  const auto t1 = Set(d, thresholds[1]);
  const auto t2 = Set(d, thresholds[2]);
  const auto t3 = Set(d, thresholds[3]);
  const auto t4 = Set(d, thresholds[4]);
  const auto t5 = Set(d, thresholds[5]);
  const auto t6 = Set(d, thresholds[6]);

  if constexpr (HWY_REGISTERS >= 32) {
    // Split into two parallel chains to reduce dependency latency.
    // -- Chain 1: Indices 0 to 3 (Evaluated starting from t3 down to t0)
    auto a_low = Set(d, arr_a[3]);
    auto b_low = Set(d, arr_b[3]);
    auto c_low = Set(d, arr_c[3]);
    auto d_low = Set(d, arr_d[3]);

    auto mask = Lt(y, t2);
    a_low = IfThenElse(mask, Set(d, arr_a[2]), a_low);
    b_low = IfThenElse(mask, Set(d, arr_b[2]), b_low);
    c_low = IfThenElse(mask, Set(d, arr_c[2]), c_low);
    d_low = IfThenElse(mask, Set(d, arr_d[2]), d_low);

    mask = Lt(y, t1);
    a_low = IfThenElse(mask, Set(d, arr_a[1]), a_low);
    b_low = IfThenElse(mask, Set(d, arr_b[1]), b_low);
    c_low = IfThenElse(mask, Set(d, arr_c[1]), c_low);
    d_low = IfThenElse(mask, Set(d, arr_d[1]), d_low);

    mask = Lt(y, t0);
    a_low = IfThenElse(mask, Set(d, arr_a[0]), a_low);
    b_low = IfThenElse(mask, Set(d, arr_b[0]), b_low);
    c_low = IfThenElse(mask, Set(d, arr_c[0]), c_low);
    d_low = IfThenElse(mask, Set(d, arr_d[0]), d_low);

    // -- Chain 2: Indices 4 to 7 (Evaluated starting from t6 down to t4)
    auto a_high = Set(d, arr_a[7]);
    auto b_high = Set(d, arr_b[7]);
    auto c_high = Set(d, arr_c[7]);
    auto d_high = Set(d, arr_d[7]);

    mask = Lt(y, t6);
    a_high = IfThenElse(mask, Set(d, arr_a[6]), a_high);
    b_high = IfThenElse(mask, Set(d, arr_b[6]), b_high);
    c_high = IfThenElse(mask, Set(d, arr_c[6]), c_high);
    d_high = IfThenElse(mask, Set(d, arr_d[6]), d_high);

    mask = Lt(y, t5);
    a_high = IfThenElse(mask, Set(d, arr_a[5]), a_high);
    b_high = IfThenElse(mask, Set(d, arr_b[5]), b_high);
    c_high = IfThenElse(mask, Set(d, arr_c[5]), c_high);
    d_high = IfThenElse(mask, Set(d, arr_d[5]), d_high);

    mask = Lt(y, t4);
    a_high = IfThenElse(mask, Set(d, arr_a[4]), a_high);
    b_high = IfThenElse(mask, Set(d, arr_b[4]), b_high);
    c_high = IfThenElse(mask, Set(d, arr_c[4]), c_high);
    d_high = IfThenElse(mask, Set(d, arr_d[4]), d_high);

    // -- Merge the two chains
    auto merge_mask = Lt(y, t3);
    a = IfThenElse(merge_mask, a_low, a_high);
    b = IfThenElse(merge_mask, b_low, b_high);
    c = IfThenElse(merge_mask, c_low, c_high);
    d_val = IfThenElse(merge_mask, d_low, d_high);
  } else {
    // Start with highest index (7)
    a = Set(d, arr_a[7]);
    b = Set(d, arr_b[7]);
    c = Set(d, arr_c[7]);
    d_val = Set(d, arr_d[7]);

    // If y < t6 (idx 6)
    auto mask = Lt(y, t6);
    a = IfThenElse(mask, Set(d, arr_a[6]), a);
    b = IfThenElse(mask, Set(d, arr_b[6]), b);
    c = IfThenElse(mask, Set(d, arr_c[6]), c);
    d_val = IfThenElse(mask, Set(d, arr_d[6]), d_val);

    // If y < t5 (idx 5)
    mask = Lt(y, t5);
    a = IfThenElse(mask, Set(d, arr_a[5]), a);
    b = IfThenElse(mask, Set(d, arr_b[5]), b);
    c = IfThenElse(mask, Set(d, arr_c[5]), c);
    d_val = IfThenElse(mask, Set(d, arr_d[5]), d_val);

    // If y < t4 (idx 4)
    mask = Lt(y, t4);
    a = IfThenElse(mask, Set(d, arr_a[4]), a);
    b = IfThenElse(mask, Set(d, arr_b[4]), b);
    c = IfThenElse(mask, Set(d, arr_c[4]), c);
    d_val = IfThenElse(mask, Set(d, arr_d[4]), d_val);

    // If y < t3 (idx 3)
    mask = Lt(y, t3);
    a = IfThenElse(mask, Set(d, arr_a[3]), a);
    b = IfThenElse(mask, Set(d, arr_b[3]), b);
    c = IfThenElse(mask, Set(d, arr_c[3]), c);
    d_val = IfThenElse(mask, Set(d, arr_d[3]), d_val);

    // If y < t2 (idx 2)
    mask = Lt(y, t2);
    a = IfThenElse(mask, Set(d, arr_a[2]), a);
    b = IfThenElse(mask, Set(d, arr_b[2]), b);
    c = IfThenElse(mask, Set(d, arr_c[2]), c);
    d_val = IfThenElse(mask, Set(d, arr_d[2]), d_val);

    // If y < t1 (idx 1)
    mask = Lt(y, t1);
    a = IfThenElse(mask, Set(d, arr_a[1]), a);
    b = IfThenElse(mask, Set(d, arr_b[1]), b);
    c = IfThenElse(mask, Set(d, arr_c[1]), c);
    d_val = IfThenElse(mask, Set(d, arr_d[1]), d_val);

    // If y < t0 (idx 0)
    mask = Lt(y, t0);
    a = IfThenElse(mask, Set(d, arr_a[0]), a);
    b = IfThenElse(mask, Set(d, arr_b[0]), b);
    c = IfThenElse(mask, Set(d, arr_c[0]), c);
    d_val = IfThenElse(mask, Set(d, arr_d[0]), d_val);
  }
}

}  // namespace impl

/**
 * Fast approximation of log(x).
 *
 * Valid Lane Types: float32, float64
 * Max Relative Error: 0.0012%
 * Average Relative Error: 3e-6% for float32, 1.8e-7% for float64
 * Valid Range: float32: (0, +FLT_MAX]
 *              float64: (0, +DBL_MAX]
 *
 * @return natural logarithm of 'x'
 */
// If false, subnormals are treated as zero.
template <bool kHandleSubnormals = true, class D, class V>
HWY_INLINE V FastLog(D d, V x) {
  using T = TFromD<D>;
  const V kLn2 = Set(d, static_cast<T>(0.6931471805599453));
  V y, exp;
  impl::FastLogRangeReduction<kHandleSubnormals>(d, x, y, exp);

  V approx;

  V a, b, c, d_val;
  // Centering the approximation around y=1.0 by using z = y - 1.0 significantly
  // improves accuracy for low-degree polynomials compared to approximating
  // log(y) directly.
  const V z = Sub(y, Set(d, static_cast<T>(1.0)));

  HWY_ALIGN static constexpr T arr_a[8] = {
      static_cast<T>(0.78766119873962426), static_cast<T>(0.56395605885234767),
      static_cast<T>(0.41755823888409732), static_cast<T>(0.31775546220809975),
      static_cast<T>(0.24738922014476947), static_cast<T>(0.19635862241628779),
      static_cast<T>(0.15845269802741027), static_cast<T>(0.12974944454997622)};

  HWY_ALIGN static constexpr T arr_b[8] = {
      static_cast<T>(-0.29967724727628686),
      static_cast<T>(-0.43890059639104201),
      static_cast<T>(-0.49106265092580692),
      static_cast<T>(-0.50008637171949),
      static_cast<T>(-0.48774751153444412),
      static_cast<T>(-0.46524164863536055),
      static_cast<T>(-0.43845622820596808),
      static_cast<T>(-0.41055729878598496)};

  HWY_ALIGN static constexpr T arr_c[8] = {
      static_cast<T>(1.0358118335702087),  static_cast<T>(1.0067153345685411),
      static_cast<T>(1.000379283174812),   static_cast<T>(1.0000110351938951),
      static_cast<T>(0.99922180103707492), static_cast<T>(0.99586383428901692),
      static_cast<T>(0.98951797571207256), static_cast<T>(0.98045123777070986)};

  HWY_ALIGN static constexpr T arr_d[8] = {
      static_cast<T>(0.0023082932745966296),
      static_cast<T>(0.00026712584767189665),
      static_cast<T>(5.4447452042709148e-06),
      static_cast<T>(0),
      static_cast<T>(1.8158320065986679e-05),
      static_cast<T>(0.00018763480353754217),
      static_cast<T>(0.0006917031865196592),
      static_cast<T>(0.0016769113228540019)};

  if constexpr (CanLookup8(d)) {
    // --- Table Lookup ---
    const auto scale = Set(d, static_cast<T>(11.3137085));
    // Input is always non-negative, so Floor() + ConvertTo()
    // can be replaced by direct ConvertTo() (truncation), which is faster.
    // We use MulAdd(y, scale, -8.0) instead of Mul(Sub(y, lower_bound), scale)
    // to save instructions. 0.70710678 * 11.3137085 ~= 8.0.
    auto idx_i = ConvertInRangeTo(
        RebindToSigned<D>(), MulAdd(y, scale, Set(d, static_cast<T>(-8.0))));

    // Clamp index to 7 to handle overshoots
    idx_i = Min(idx_i, Set(RebindToSigned<D>(), 7));

    a = Lookup8(d, arr_a, idx_i);
    b = Lookup8(d, arr_b, idx_i);
    c = Lookup8(d, arr_c, idx_i);
    d_val = Lookup8(d, arr_d, idx_i);
  } else {
    HWY_ALIGN static constexpr T thresholds[7] = {
        static_cast<T>(0.7954951287634819), static_cast<T>(0.8838834764038688),
        static_cast<T>(0.9722718240442556), static_cast<T>(1.0606601716846424),
        static_cast<T>(1.1490485193250295), static_cast<T>(1.2374368669654163),
        static_cast<T>(1.3258252146058032)};
    impl::FallbackBlendChain4Coeff(d, y, thresholds, arr_a, arr_b, arr_c, arr_d,
                                   a, b, c, d_val);
  }
  // Math: approx = (a*z + b)*z^2 + (c*z + d_val)
  const auto z2 = Mul(z, z);
  const auto pab = MulAdd(a, z, b);
  const auto pcd = MulAdd(c, z, d_val);
  approx = MulAdd(pab, z2, pcd);

  return MulAdd(exp, kLn2, approx);
}

/**
 * Fast approximation of exp(x).
 *
 * Valid Lane Types: float32, float64
 * Max Relative Error: 0.0007% for float32 [-87, 88]
 * Max Relative Error: 0.0007% for float64 [-708, 706]
 * Average Relative Error: 0.00002% for float32 [-87, 88]
 * Average Relative Error: 0.00001% for float64 [-708, 706]
 * Max Relative Error for Subnormals: 2.4% for float32 [-FLT_MAX, -87]
 * Max Relative Error for Subnormals: 0.006% for float64 [-DBL_MAX, -708]
 * Valid Range: float32[-FLT_MAX, +88], float64[-DBL_MAX, +706]
 *
 * @return e^x
 */
template <bool kHandleSubnormals = true, class D, class V>
HWY_INLINE V FastExp(D d, V x) {
  using T = TFromD<D>;
  impl::FastExpImpl<T> impl;

  T lower_bound_val;
  if constexpr (kHandleSubnormals) {
    lower_bound_val = sizeof(T) == 4 ? -104.0 : -1000.0;
  } else {
    lower_bound_val = sizeof(T) == 4 ? -88.0 : -709.0;
  }
  const V kLowerBound = Set(d, static_cast<T>(lower_bound_val));

  const V kHalf = Set(d, static_cast<T>(+0.5));
  const V kNegZero = Set(d, static_cast<T>(-0.0));

  const V kOneOverLog2 = Set(d, static_cast<T>(+1.442695040888963407359924681));

  using TI = MakeSigned<T>;
  const Rebind<TI, D> di;

  V x_clamped = x;
  if constexpr (!kHandleSubnormals) {
    x_clamped = Max(x, kLowerBound);
  }

  const auto rounded_offs = BitCast(
      d,
      OrAnd(BitCast(di, kHalf), BitCast(di, x_clamped), BitCast(di, kNegZero)));

  const auto q = impl.ToInt32(d, MulAdd(x_clamped, kOneOverLog2, rounded_offs));

  const auto x_red = impl.ExpReduce(d, x_clamped, q);

  // Degree 4 polynomial approximation of e^x on [-ln2/2, ln2/2]
  // Generated via Caratheodory-Fejer approximation.
  const auto c0 = Set(d, static_cast<T>(1.0000001510806224569));
  const auto c1 = Set(d, static_cast<T>(0.99996228117046825901));
  const auto c2 = Set(d, static_cast<T>(0.49998365704575670199));
  const auto c3 = Set(d, static_cast<T>(0.16792157982876812494));
  const auto c4 = Set(d, static_cast<T>(0.041959439862987071845));

  // Estrin's scheme
  const auto x2 = Mul(x_red, x_red);
  // term0 = c1*x + c0
  const auto term0 = MulAdd(c1, x_red, c0);
  // term1 = c3*x + c2
  const auto term1 = MulAdd(c3, x_red, c2);
  // term2 = c4*x^2 + term1
  const auto term2 = MulAdd(c4, x2, term1);
  // approx = term2 * x^2 + term0
  const auto approx = MulAdd(term2, x2, term0);

  if constexpr (kHandleSubnormals) {
    const V res = impl.LoadExpShortRange(d, approx, q);
    // Handle underflow
    return IfThenElseZero(Ge(x, kLowerBound), res);
  } else {
    // Optimization: avoid splitting the exponent since 'q' is guaranteed
    // to fall within the normal floating-point ranges.
    return Mul(approx, impl.Pow2I(d, q));
  }
}

/**
 * Fast approximation of exp2(x).
 *
 * Valid Lane Types: float32, float64
 * Max Relative Error: 0.0007% for float32 [-150, 128]
 * Max Relative Error: 0.0007% for float64 [-1075, 1024]
 * Average Relative Error: 0.00002% for float32 [-150, 128]
 * Average Relative Error: 0.00001% for float64 [-1075, 1024]
 * Max Relative Error for Subnormals: 0.08% for float32 [-FLT_MAX, -150]
 * Max Relative Error for Subnormals: 0.03% for float64 [-DBL_MAX, -1075]
 * Valid Range: float32[-FLT_MAX, +128], float64[-DBL_MAX, +1024]
 *
 * @return 2^x
 */
template <bool kHandleSubnormals = true, class D, class V>
HWY_INLINE V FastExp2(D d, V x) {
  using T = TFromD<D>;
  impl::FastExpImpl<T> impl;

  T lower_bound_val;
  if constexpr (kHandleSubnormals) {
    // FastExp uses kLowerBound = -104.0 / -1000.0 since it operates on e^x. For
    // FastExp2, we use lower limits correspondingly to -150.0 and -1075.0.
    lower_bound_val = sizeof(T) == 4 ? -150.0 : -1075.0;
  } else {
    lower_bound_val = sizeof(T) == 4 ? -127.0 : -1023.0;
  }
  const V kLowerBound = Set(d, static_cast<T>(lower_bound_val));

  const V kHalf = Set(d, static_cast<T>(+0.5));
  const V kNegZero = Set(d, static_cast<T>(-0.0));

  using TI = MakeSigned<T>;
  const Rebind<TI, D> di;

  V x_clamped = x;
  if constexpr (!kHandleSubnormals) {
    x_clamped = Max(x, kLowerBound);
  }

  const auto rounded_offs = BitCast(
      d,
      OrAnd(BitCast(di, kHalf), BitCast(di, x_clamped), BitCast(di, kNegZero)));

  // FastExp calculates q = ToInt32(x * (1/ln(2)) + rounded_offs)
  // FastExp2 does not need the (1/ln(2)) scaling factor since the input is
  // already in base 2.
  const auto q = impl.ToInt32(d, Add(x_clamped, rounded_offs));

  const auto x_red = impl.Exp2Reduce(d, x_clamped, q);

  // Degree 4 polynomial approximation of 2^x on [-1/2, 1/2]
  // Derived from FastExp coefficients by pre-absorbing ln2:
  // c_fast_exp2[i] = c_fast_exp[i] * (ln2)^i.
  const auto c0 = Set(d, static_cast<T>(1.0000001510806224569));
  const auto c1 = Set(d, static_cast<T>(0.69312104523363065471));
  const auto c2 = Set(d, static_cast<T>(0.24021865239713606622));
  const auto c3 = Set(d, static_cast<T>(0.05592203117565365516));
  const auto c4 = Set(d, static_cast<T>(0.00968574163456345638));

  // Estrin's scheme
  const auto x2 = Mul(x_red, x_red);
  // term0 = c1*x + c0
  const auto term0 = MulAdd(c1, x_red, c0);
  // term1 = c3*x + c2
  const auto term1 = MulAdd(c3, x_red, c2);
  // term2 = c4*x^2 + term1
  const auto term2 = MulAdd(c4, x2, term1);
  // approx = term2 * x^2 + term0
  const auto approx = MulAdd(term2, x2, term0);

  if constexpr (kHandleSubnormals) {
    const V res = impl.LoadExpShortRange(d, approx, q);
    // Handle underflow
    return IfThenElseZero(Ge(x, kLowerBound), res);
  } else {
    // Optimization: avoid splitting the exponent since 'q' is guaranteed
    // to fall within the normal floating-point ranges.
    return Mul(approx, impl.Pow2I(d, q));
  }
}

/**
 * Fast approximation of exp(x) for x <= 0. Subnormals are flushed to zero.
 *
 * Valid Lane Types: float32, float64
 * Max Relative Error: 0.0007% for float32 [-87, 0]
 * Max Relative Error: 0.0007% for float64 [-708, 0]
 * Average Relative Error: 0.00002% for float32 [-87, 0]
 * Average Relative Error: 0.00001% for float64 [-708, 0]
 * Valid Range: float32[-FLT_MAX, +0.0], float64[-DBL_MAX, +0.0]
 *
 * @return e^x
 */
template <class D, class V>
HWY_INLINE V FastExpMinusOrZero(D d, V x) {
  using T = TFromD<D>;
  impl::FastExpImpl<T> impl;

  const V kHalfMinus = Set(d, static_cast<T>(-0.5));
  const V kLowerBound =
      Set(d, static_cast<T>((sizeof(T) == 4 ? -88.0 : -709.0)));

  const V kOneOverLog2 = Set(d, static_cast<T>(+1.442695040888963407359924681));

  // Optimization for x <= 0:
  // FastExp computes `rounded_offs = sign(x) ? -0.5 : 0.5` to round the
  // multiplied argument towards zero. Since x <= 0, we avoid the dynamic
  // calculation and simply use a constant -0.5 (kHalfMinus).
  //
  // We clamp x to be >= kLowerBound. For x < kLowerBound, the remapped
  // exponent q becomes -127 (f32) or -1023 (f64), which Pow2I converts to
  // exactly 0.0. This avoids subnormals and the need for a final mask.
  const auto x_clamped = Max(x, kLowerBound);
  const auto q = impl.ToInt32(d, MulAdd(x_clamped, kOneOverLog2, kHalfMinus));

  const auto x_red = impl.ExpReduce(d, x_clamped, q);

  // Degree 4 polynomial approximation of e^x on [-ln2/2, ln2/2]
  // Generated via Caratheodory-Fejer approximation.
  const auto c0 = Set(d, static_cast<T>(1.0000001510806224569));
  const auto c1 = Set(d, static_cast<T>(0.99996228117046825901));
  const auto c2 = Set(d, static_cast<T>(0.49998365704575670199));
  const auto c3 = Set(d, static_cast<T>(0.16792157982876812494));
  const auto c4 = Set(d, static_cast<T>(0.041959439862987071845));

  // Estrin's scheme
  const auto x2 = Mul(x_red, x_red);
  // term0 = c1*x + c0
  const auto term0 = MulAdd(c1, x_red, c0);
  // term1 = c3*x + c2
  const auto term1 = MulAdd(c3, x_red, c2);
  // term2 = c4*x^2 + term1
  const auto term2 = MulAdd(c4, x2, term1);
  // approx = term2 * x^2 + term0
  const auto approx = MulAdd(term2, x2, term0);

  // Since inputs < -88.0 (f32) and < -709.0 (f64) are flushed to zero,
  // we do not generate subnormals. Therefore, q is guaranteed to be >= -127
  // and we can use Pow2I directly without splitting the exponent computation.
  return Mul(approx, impl.Pow2I(d, q));
}

/**
 * Fast approximation of log2(x).
 *
 * Valid Lane Types: float32, float64
 * Max Relative Error: 0.0012%
 * Average Relative Error: 1.2e-6% for float32, 1.8e-7% for float64
 * Valid Range: float32: (0, +FLT_MAX]
 *              float64: (0, +DBL_MAX]
 *
 * @return base 2 logarithm of 'x'
 */
// If false, subnormals are treated as zero.
template <bool kHandleSubnormals = true, class D, class V>
HWY_INLINE V FastLog2(D d, V x) {
  using T = TFromD<D>;
  V y, exp;
  impl::FastLogRangeReduction<kHandleSubnormals>(d, x, y, exp);

  V approx;

  V a, b, c, d_val;
  // Centering the approximation around y=1.0 by using z = y - 1.0 significantly
  // improves accuracy for low-degree polynomials compared to approximating
  // log(y) directly.
  const V z = Sub(y, Set(d, static_cast<T>(1.0)));

  HWY_ALIGN static constexpr T arr_a[8] = {
      static_cast<T>(1.136354905322312),   static_cast<T>(0.8136166093855663),
      static_cast<T>(0.6024092005204164),  static_cast<T>(0.45842422954300593),
      static_cast<T>(0.35690720107224694), static_cast<T>(0.28328561079576686),
      static_cast<T>(0.22859892165962123), static_cast<T>(0.18718888021034824)};

  HWY_ALIGN static constexpr T arr_b[8] = {static_cast<T>(-0.43234287851275466),
                                           static_cast<T>(-0.6331997138565648),
                                           static_cast<T>(-0.7084536512564498),
                                           static_cast<T>(-0.721472128495863),
                                           static_cast<T>(-0.703670916096675),
                                           static_cast<T>(-0.6712018193012402),
                                           static_cast<T>(-0.6325586260796298),
                                           static_cast<T>(-0.5923089789593089)};

  HWY_ALIGN static constexpr T arr_c[8] = {
      static_cast<T>(1.4943605955858443), static_cast<T>(1.4523832207689078),
      static_cast<T>(1.4432422308443573), static_cast<T>(1.4427109613084712),
      static_cast<T>(1.4415723371043265), static_cast<T>(1.4367278151294332),
      static_cast<T>(1.4275726764302927), static_cast<T>(1.4144921385652491)};

  HWY_ALIGN static constexpr T arr_d[8] = {
      static_cast<T>(0.0033301632601779037),
      static_cast<T>(0.00038538113572950594),
      static_cast<T>(7.855106905105614e-06),
      static_cast<T>(0.0),
      static_cast<T>(2.6196918310073536e-05),
      static_cast<T>(0.000270699800561787),
      static_cast<T>(0.000997916756959006),
      static_cast<T>(0.00241927164949202)};

  if constexpr (CanLookup8(d)) {
    // --- Table Lookup ---
    const auto scale = Set(d, static_cast<T>(11.3137085));
    auto idx_i = ConvertInRangeTo(
        RebindToSigned<D>(), MulAdd(y, scale, Set(d, static_cast<T>(-8.0))));

    idx_i = Min(idx_i, Set(RebindToSigned<D>(), 7));

    a = Lookup8(d, arr_a, idx_i);
    b = Lookup8(d, arr_b, idx_i);
    c = Lookup8(d, arr_c, idx_i);
    d_val = Lookup8(d, arr_d, idx_i);
  } else {
    HWY_ALIGN static constexpr T thresholds[7] = {
        static_cast<T>(0.7954951287634819), static_cast<T>(0.8838834764038688),
        static_cast<T>(0.9722718240442556), static_cast<T>(1.0606601716846424),
        static_cast<T>(1.1490485193250295), static_cast<T>(1.2374368669654163),
        static_cast<T>(1.3258252146058032)};
    impl::FallbackBlendChain4Coeff(d, y, thresholds, arr_a, arr_b, arr_c, arr_d,
                                   a, b, c, d_val);
  }
  // Math: approx = (a*z + b)*z^2 + (c*z + d_val)
  const auto z2 = Mul(z, z);
  const auto pab = MulAdd(a, z, b);
  const auto pcd = MulAdd(c, z, d_val);
  approx = MulAdd(pab, z2, pcd);

  return Add(exp, approx);
}

/**
 * Fast approximation of log10(x).
 *
 * Valid Lane Types: float32, float64
 * Max Relative Error: 0.0012%
 * Average Relative Error: 5.4e-6% for float32, 1.8e-7% for float64
 * Valid Range: float32: (0, +FLT_MAX]
 *              float64: (0, +DBL_MAX]
 *
 * @return base 10 logarithm of 'x'
 */
// If false, subnormals are treated as zero.
template <bool kHandleSubnormals = true, class D, class V>
HWY_INLINE V FastLog10(D d, V x) {
  using T = TFromD<D>;
  V y, exp;
  impl::FastLogRangeReduction<kHandleSubnormals>(d, x, y, exp);

  V approx;

  V a, b, c, d_val;
  // Centering the approximation around y=1.0 by using z = y - 1.0 significantly
  // improves accuracy for low-degree polynomials compared to approximating
  // log(y) directly.
  const V z = Sub(y, Set(d, static_cast<T>(1.0)));

  HWY_ALIGN static constexpr T arr_a[8] = {
      static_cast<T>(0.3420769122219194),  static_cast<T>(0.24492300439548012),
      static_cast<T>(0.18134323902060331), static_cast<T>(0.137999443831595),
      static_cast<T>(0.10743977319122217), static_cast<T>(0.08527746618951795),
      static_cast<T>(0.06881513239598655), static_cast<T>(0.05634946779806663)};

  HWY_ALIGN static constexpr T arr_b[8] = {
      static_cast<T>(-0.1301481748440477),
      static_cast<T>(-0.1906121071166758),
      static_cast<T>(-0.21326579956586073),
      static_cast<T>(-0.21718475171279292),
      static_cast<T>(-0.21182605282145175),
      static_cast<T>(-0.20205188075390862),
      static_cast<T>(-0.19041912046596485),
      static_cast<T>(-0.17830276936785788)};

  HWY_ALIGN static constexpr T arr_c[8] = {
      static_cast<T>(0.44984736360963107), static_cast<T>(0.4372109146505034),
      static_cast<T>(0.4344592024931514),  static_cast<T>(0.4342992744270672),
      static_cast<T>(0.4339565143878306),  static_cast<T>(0.4324981679587344),
      static_cast<T>(0.4297421965958291),  static_cast<T>(0.4258045623390324)};

  HWY_ALIGN static constexpr T arr_d[8] = {
      static_cast<T>(0.0010024790317717039),
      static_cast<T>(0.00011601128161763332),
      static_cast<T>(2.364622797584052e-06),
      static_cast<T>(0.0),
      static_cast<T>(7.886058205291105e-06),
      static_cast<T>(8.148875978935532e-05),
      static_cast<T>(0.00030040287702038374),
      static_cast<T>(0.0007282733341565754)};

  if constexpr (CanLookup8(d)) {
    // --- Table Lookup ---
    const auto scale = Set(d, static_cast<T>(11.3137085));
    auto idx_i = ConvertInRangeTo(
        RebindToSigned<D>(), MulAdd(y, scale, Set(d, static_cast<T>(-8.0))));

    idx_i = Min(idx_i, Set(RebindToSigned<D>(), 7));

    a = Lookup8(d, arr_a, idx_i);
    b = Lookup8(d, arr_b, idx_i);
    c = Lookup8(d, arr_c, idx_i);
    d_val = Lookup8(d, arr_d, idx_i);
  } else {
    HWY_ALIGN static constexpr T thresholds[7] = {
        static_cast<T>(0.7954951287634819), static_cast<T>(0.8838834764038688),
        static_cast<T>(0.9722718240442556), static_cast<T>(1.0606601716846424),
        static_cast<T>(1.1490485193250295), static_cast<T>(1.2374368669654163),
        static_cast<T>(1.3258252146058032)};
    impl::FallbackBlendChain4Coeff(d, y, thresholds, arr_a, arr_b, arr_c, arr_d,
                                   a, b, c, d_val);
  }
  // Math: approx = (a*z + b)*z^2 + (c*z + d_val)
  const auto z2 = Mul(z, z);
  const auto pab = MulAdd(a, z, b);
  const auto pcd = MulAdd(c, z, d_val);
  approx = MulAdd(pab, z2, pcd);

  const auto kLog10_2 = Set(d, static_cast<T>(0.3010299956639812));  // log10(2)
  // Computes exp * log10(2) + approx. Since approx was scaled by 1/Ln(10)
  // via the pre-scaled coefficients, this yields the correct log10 result
  // using a single MulAdd instruction.
  return MulAdd(exp, kLog10_2, approx);
}

/**
 * Fast approximation of log(1 + x).
 *
 * Valid Lane Types: float32, float64
 * Max Relative Error: 0.0012%
 * Average Relative Error: 0.00013% for float32, 0.000039% for float64
 * Valid Range: float32: [-1 + epsilon, +FLT_MAX]
 *              float64: [-1 + epsilon, +DBL_MAX]
 *
 * @return natural logarithm of '1 + x'
 */
// If false, subnormals are treated as zero.
template <bool kHandleSubnormals = true, class D, class V>
HWY_INLINE V FastLog1p(const D d, V x) {
  using T = TFromD<D>;
  const V kOne = Set(d, static_cast<T>(+1.0));

  const V y = Add(x, kOne);
  const Mask<D> not_pole = Ne(y, kOne);
  // If y == 1, divisor becomes 1 (dummy), avoiding division by zero.
  const V divisor = MaskedSubOr(y, not_pole, y, kOne);
  // Ensure exactly 1.0 when x == divisor. This is necessary because some
  // platforms (like Armv7) use Newton-Raphson for division, which can return
  // 0.0, instead of 1.0 when the reciprocal calculation underflows
  // for very large x.
  const V div_res = MaskedDivOr(kOne, Ne(x, divisor), x, divisor);
  const V non_pole = Mul(FastLog<kHandleSubnormals>(d, y), div_res);
  return IfThenElse(not_pole, non_pole, x);
}

/**
 * Fast approximation of base^exp.
 *
 * Valid Lane Types: float32, float64
 * Valid Range: float32: base in (0, +FLT_MAX], exp * log(base) in [-25.0,
 * +25.0] float64: base in (0, +DBL_MAX], exp * log(base) in [-25.0, +25.0] Max
 * Relative Error for Valid Range: float32 : 0.03%, float64 : 0.03%
 * @return base^exp
 */
// If false, subnormals are treated as zero.
template <bool kHandleSubnormals = true, class D, class V>
HWY_INLINE V FastPow(D d, V base, V exp) {
  return FastExp<kHandleSubnormals>(
      d, Mul(exp, FastLog<kHandleSubnormals>(d, base)));
}

template <class D, class V>
HWY_NOINLINE V CallFastAtan(const D d, VecArg<V> x) {
  return FastAtan(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastTan(const D d, VecArg<V> x) {
  return FastTan(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastAtan2(const D d, VecArg<V> y, VecArg<V> x) {
  return FastAtan2(d, y, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastTanh(const D d, VecArg<V> x) {
  return FastTanh(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastLog(const D d, VecArg<V> x) {
  return FastLog<>(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastExp(const D d, VecArg<V> x) {
  return FastExp(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastExp2(const D d, VecArg<V> x) {
  return FastExp2(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastExpMinusOrZero(const D d, VecArg<V> x) {
  return FastExpMinusOrZero(d, x);
}
template <class D, class V>
HWY_NOINLINE V CallFastLog2(const D d, VecArg<V> x) {
  return FastLog2<>(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastLog10(const D d, VecArg<V> x) {
  return FastLog10<>(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastLog1p(const D d, VecArg<V> x) {
  return FastLog1p<>(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastPow(const D d, VecArg<V> base, VecArg<V> exp) {
  return FastPow<>(d, base, exp);
}

template <class D, class V>
HWY_NOINLINE V CallFastExpNormal(const D d, VecArg<V> x) {
  return FastExp</*kHandleSubnormals=*/false>(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastExp2Normal(const D d, VecArg<V> x) {
  return FastExp2</*kHandleSubnormals=*/false>(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastLogPositiveNormal(const D d, VecArg<V> x) {
  return FastLog</*kHandleSubnormals=*/false>(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastLog2PositiveNormal(const D d, VecArg<V> x) {
  return FastLog2</*kHandleSubnormals=*/false>(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastLog10PositiveNormal(const D d, VecArg<V> x) {
  return FastLog10</*kHandleSubnormals=*/false>(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastLog1pPositiveNormal(const D d, VecArg<V> x) {
  return FastLog1p</*kHandleSubnormals=*/false>(d, x);
}

template <class D, class V>
HWY_NOINLINE V CallFastAtanPositive(const D d, VecArg<V> x) {
  return FastAtan</*kAssumePositive=*/true>(d, x);
}
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // HIGHWAY_HWY_CONTRIB_MATH_FAST_MATH_INL_H_
