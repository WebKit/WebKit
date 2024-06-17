/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"

#include "av1/common/enums.h"

#include "test/acm_random.h"
#include "test/function_equivalence_test.h"
#include "test/register_state_check.h"

#define WEDGE_WEIGHT_BITS 6
#define MAX_MASK_VALUE (1 << (WEDGE_WEIGHT_BITS))

using libaom_test::ACMRandom;
using libaom_test::FunctionEquivalenceTest;

namespace {

static const int16_t kInt13Max = (1 << 12) - 1;

//////////////////////////////////////////////////////////////////////////////
// av1_wedge_sse_from_residuals - functionality
//////////////////////////////////////////////////////////////////////////////

class WedgeUtilsSSEFuncTest : public testing::Test {
 protected:
  WedgeUtilsSSEFuncTest() : rng_(ACMRandom::DeterministicSeed()) {}

  static const int kIterations = 1000;

  ACMRandom rng_;
};

static void equiv_blend_residuals(int16_t *r, const int16_t *r0,
                                  const int16_t *r1, const uint8_t *m, int N) {
  for (int i = 0; i < N; i++) {
    const int32_t m0 = m[i];
    const int32_t m1 = MAX_MASK_VALUE - m0;
    const int16_t R = m0 * r0[i] + m1 * r1[i];
    // Note that this rounding is designed to match the result
    // you would get when actually blending the 2 predictors and computing
    // the residuals.
    r[i] = ROUND_POWER_OF_TWO(R - 1, WEDGE_WEIGHT_BITS);
  }
}

static uint64_t equiv_sse_from_residuals(const int16_t *r0, const int16_t *r1,
                                         const uint8_t *m, int N) {
  uint64_t acc = 0;
  for (int i = 0; i < N; i++) {
    const int32_t m0 = m[i];
    const int32_t m1 = MAX_MASK_VALUE - m0;
    const int16_t R = m0 * r0[i] + m1 * r1[i];
    const int32_t r = ROUND_POWER_OF_TWO(R - 1, WEDGE_WEIGHT_BITS);
    acc += r * r;
  }
  return acc;
}

TEST_F(WedgeUtilsSSEFuncTest, ResidualBlendingEquiv) {
  DECLARE_ALIGNED(32, uint8_t, s[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, uint8_t, p0[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, uint8_t, p1[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, uint8_t, p[MAX_SB_SQUARE]);

  DECLARE_ALIGNED(32, int16_t, r0[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, r1[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, r_ref[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, r_tst[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, uint8_t, m[MAX_SB_SQUARE]);

  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_SB_SQUARE; ++i) {
      s[i] = rng_.Rand8();
      m[i] = rng_(MAX_MASK_VALUE + 1);
    }

    const int w = 1 << (rng_(MAX_SB_SIZE_LOG2 + 1 - 3) + 3);
    const int h = 1 << (rng_(MAX_SB_SIZE_LOG2 + 1 - 3) + 3);
    const int N = w * h;

    for (int j = 0; j < N; j++) {
      p0[j] = clamp(s[j] + rng_(33) - 16, 0, UINT8_MAX);
      p1[j] = clamp(s[j] + rng_(33) - 16, 0, UINT8_MAX);
    }

    aom_blend_a64_mask(p, w, p0, w, p1, w, m, w, w, h, 0, 0);

    aom_subtract_block(h, w, r0, w, s, w, p0, w);
    aom_subtract_block(h, w, r1, w, s, w, p1, w);

    aom_subtract_block(h, w, r_ref, w, s, w, p, w);
    equiv_blend_residuals(r_tst, r0, r1, m, N);

    for (int i = 0; i < N; ++i) ASSERT_EQ(r_ref[i], r_tst[i]);

    uint64_t ref_sse = aom_sum_squares_i16(r_ref, N);
    uint64_t tst_sse = equiv_sse_from_residuals(r0, r1, m, N);

    ASSERT_EQ(ref_sse, tst_sse);
  }
}

static uint64_t sse_from_residuals(const int16_t *r0, const int16_t *r1,
                                   const uint8_t *m, int N) {
  uint64_t acc = 0;
  for (int i = 0; i < N; i++) {
    const int32_t m0 = m[i];
    const int32_t m1 = MAX_MASK_VALUE - m0;
    const int32_t r = m0 * r0[i] + m1 * r1[i];
    acc += r * r;
  }
  return ROUND_POWER_OF_TWO(acc, 2 * WEDGE_WEIGHT_BITS);
}

TEST_F(WedgeUtilsSSEFuncTest, ResidualBlendingMethod) {
  DECLARE_ALIGNED(32, int16_t, r0[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, r1[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, d[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, uint8_t, m[MAX_SB_SQUARE]);

  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_SB_SQUARE; ++i) {
      r1[i] = rng_(2 * INT8_MAX - 2 * INT8_MIN + 1) + 2 * INT8_MIN;
      d[i] = rng_(2 * INT8_MAX - 2 * INT8_MIN + 1) + 2 * INT8_MIN;
      m[i] = rng_(MAX_MASK_VALUE + 1);
    }

    const int N = 64 * (rng_(MAX_SB_SQUARE / 64) + 1);

    for (int i = 0; i < N; i++) r0[i] = r1[i] + d[i];

    const uint64_t ref_res = sse_from_residuals(r0, r1, m, N);
    const uint64_t tst_res = av1_wedge_sse_from_residuals(r1, d, m, N);

    ASSERT_EQ(ref_res, tst_res);
  }
}

//////////////////////////////////////////////////////////////////////////////
// av1_wedge_sse_from_residuals - optimizations
//////////////////////////////////////////////////////////////////////////////

typedef uint64_t (*FSSE)(const int16_t *r1, const int16_t *d, const uint8_t *m,
                         int N);
typedef libaom_test::FuncParam<FSSE> TestFuncsFSSE;

class WedgeUtilsSSEOptTest : public FunctionEquivalenceTest<FSSE> {
 protected:
  static const int kIterations = 10000;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WedgeUtilsSSEOptTest);

TEST_P(WedgeUtilsSSEOptTest, RandomValues) {
  DECLARE_ALIGNED(32, int16_t, r1[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, d[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, uint8_t, m[MAX_SB_SQUARE]);

  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_SB_SQUARE; ++i) {
      r1[i] = rng_(2 * kInt13Max + 1) - kInt13Max;
      d[i] = rng_(2 * kInt13Max + 1) - kInt13Max;
      m[i] = rng_(MAX_MASK_VALUE + 1);
    }

    const int N = 64 * (rng_(MAX_SB_SQUARE / 64) + 1);

    const uint64_t ref_res = params_.ref_func(r1, d, m, N);
    uint64_t tst_res;
    API_REGISTER_STATE_CHECK(tst_res = params_.tst_func(r1, d, m, N));

    ASSERT_EQ(ref_res, tst_res);
  }
}

TEST_P(WedgeUtilsSSEOptTest, ExtremeValues) {
  DECLARE_ALIGNED(32, int16_t, r1[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, d[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, uint8_t, m[MAX_SB_SQUARE]);

  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    if (rng_(2)) {
      for (int i = 0; i < MAX_SB_SQUARE; ++i) r1[i] = kInt13Max;
    } else {
      for (int i = 0; i < MAX_SB_SQUARE; ++i) r1[i] = -kInt13Max;
    }

    if (rng_(2)) {
      for (int i = 0; i < MAX_SB_SQUARE; ++i) d[i] = kInt13Max;
    } else {
      for (int i = 0; i < MAX_SB_SQUARE; ++i) d[i] = -kInt13Max;
    }

    for (int i = 0; i < MAX_SB_SQUARE; ++i) m[i] = MAX_MASK_VALUE;

    const int N = 64 * (rng_(MAX_SB_SQUARE / 64) + 1);

    const uint64_t ref_res = params_.ref_func(r1, d, m, N);
    uint64_t tst_res;
    API_REGISTER_STATE_CHECK(tst_res = params_.tst_func(r1, d, m, N));

    ASSERT_EQ(ref_res, tst_res);
  }
}

//////////////////////////////////////////////////////////////////////////////
// av1_wedge_sign_from_residuals
//////////////////////////////////////////////////////////////////////////////

typedef int8_t (*FSign)(const int16_t *ds, const uint8_t *m, int N,
                        int64_t limit);
typedef libaom_test::FuncParam<FSign> TestFuncsFSign;

class WedgeUtilsSignOptTest : public FunctionEquivalenceTest<FSign> {
 protected:
  static const int kIterations = 10000;
  static const int kMaxSize = 8196;  // Size limited by SIMD implementation.
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WedgeUtilsSignOptTest);

TEST_P(WedgeUtilsSignOptTest, RandomValues) {
  DECLARE_ALIGNED(32, int16_t, r0[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, r1[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, ds[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, uint8_t, m[MAX_SB_SQUARE]);

  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_SB_SQUARE; ++i) {
      r0[i] = rng_(2 * kInt13Max + 1) - kInt13Max;
      r1[i] = rng_(2 * kInt13Max + 1) - kInt13Max;
      m[i] = rng_(MAX_MASK_VALUE + 1);
    }

    const int maxN = AOMMIN(kMaxSize, MAX_SB_SQUARE);
    const int N = 64 * (rng_(maxN / 64 - 1) + 1);

    int64_t limit;
    limit = (int64_t)aom_sum_squares_i16(r0, N);
    limit -= (int64_t)aom_sum_squares_i16(r1, N);
    limit *= (1 << WEDGE_WEIGHT_BITS) / 2;

    for (int i = 0; i < N; i++)
      ds[i] = clamp(r0[i] * r0[i] - r1[i] * r1[i], INT16_MIN, INT16_MAX);

    const int ref_res = params_.ref_func(ds, m, N, limit);
    int tst_res;
    API_REGISTER_STATE_CHECK(tst_res = params_.tst_func(ds, m, N, limit));

    ASSERT_EQ(ref_res, tst_res);
  }
}

TEST_P(WedgeUtilsSignOptTest, ExtremeValues) {
  DECLARE_ALIGNED(32, int16_t, r0[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, r1[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, ds[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, uint8_t, m[MAX_SB_SQUARE]);

  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    switch (rng_(4)) {
      case 0:
        for (int i = 0; i < MAX_SB_SQUARE; ++i) {
          r0[i] = 0;
          r1[i] = kInt13Max;
        }
        break;
      case 1:
        for (int i = 0; i < MAX_SB_SQUARE; ++i) {
          r0[i] = kInt13Max;
          r1[i] = 0;
        }
        break;
      case 2:
        for (int i = 0; i < MAX_SB_SQUARE; ++i) {
          r0[i] = 0;
          r1[i] = -kInt13Max;
        }
        break;
      default:
        for (int i = 0; i < MAX_SB_SQUARE; ++i) {
          r0[i] = -kInt13Max;
          r1[i] = 0;
        }
        break;
    }

    for (int i = 0; i < MAX_SB_SQUARE; ++i) m[i] = MAX_MASK_VALUE;

    const int maxN = AOMMIN(kMaxSize, MAX_SB_SQUARE);
    const int N = 64 * (rng_(maxN / 64 - 1) + 1);

    int64_t limit;
    limit = (int64_t)aom_sum_squares_i16(r0, N);
    limit -= (int64_t)aom_sum_squares_i16(r1, N);
    limit *= (1 << WEDGE_WEIGHT_BITS) / 2;

    for (int i = 0; i < N; i++)
      ds[i] = clamp(r0[i] * r0[i] - r1[i] * r1[i], INT16_MIN, INT16_MAX);

    const int ref_res = params_.ref_func(ds, m, N, limit);
    int tst_res;
    API_REGISTER_STATE_CHECK(tst_res = params_.tst_func(ds, m, N, limit));

    ASSERT_EQ(ref_res, tst_res);
  }
}

//////////////////////////////////////////////////////////////////////////////
// av1_wedge_compute_delta_squares
//////////////////////////////////////////////////////////////////////////////

typedef void (*FDS)(int16_t *d, const int16_t *a, const int16_t *b, int N);
typedef libaom_test::FuncParam<FDS> TestFuncsFDS;

class WedgeUtilsDeltaSquaresOptTest : public FunctionEquivalenceTest<FDS> {
 protected:
  static const int kIterations = 10000;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WedgeUtilsDeltaSquaresOptTest);

TEST_P(WedgeUtilsDeltaSquaresOptTest, RandomValues) {
  DECLARE_ALIGNED(32, int16_t, a[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, b[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, d_ref[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, d_tst[MAX_SB_SQUARE]);

  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_SB_SQUARE; ++i) {
      a[i] = rng_.Rand16Signed();
      b[i] = rng_(2 * INT16_MAX + 1) - INT16_MAX;
    }

    const int N = 64 * (rng_(MAX_SB_SQUARE / 64) + 1);

    memset(&d_ref, INT16_MAX, sizeof(d_ref));
    memset(&d_tst, INT16_MAX, sizeof(d_tst));

    params_.ref_func(d_ref, a, b, N);
    API_REGISTER_STATE_CHECK(params_.tst_func(d_tst, a, b, N));

    for (int i = 0; i < MAX_SB_SQUARE; ++i) ASSERT_EQ(d_ref[i], d_tst[i]);
  }
}

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, WedgeUtilsSSEOptTest,
    ::testing::Values(TestFuncsFSSE(av1_wedge_sse_from_residuals_c,
                                    av1_wedge_sse_from_residuals_sse2)));

INSTANTIATE_TEST_SUITE_P(
    SSE2, WedgeUtilsSignOptTest,
    ::testing::Values(TestFuncsFSign(av1_wedge_sign_from_residuals_c,
                                     av1_wedge_sign_from_residuals_sse2)));

INSTANTIATE_TEST_SUITE_P(
    SSE2, WedgeUtilsDeltaSquaresOptTest,
    ::testing::Values(TestFuncsFDS(av1_wedge_compute_delta_squares_c,
                                   av1_wedge_compute_delta_squares_sse2)));
#endif  // HAVE_SSE2

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, WedgeUtilsSSEOptTest,
    ::testing::Values(TestFuncsFSSE(av1_wedge_sse_from_residuals_c,
                                    av1_wedge_sse_from_residuals_neon)));

INSTANTIATE_TEST_SUITE_P(
    NEON, WedgeUtilsSignOptTest,
    ::testing::Values(TestFuncsFSign(av1_wedge_sign_from_residuals_c,
                                     av1_wedge_sign_from_residuals_neon)));

INSTANTIATE_TEST_SUITE_P(
    NEON, WedgeUtilsDeltaSquaresOptTest,
    ::testing::Values(TestFuncsFDS(av1_wedge_compute_delta_squares_c,
                                   av1_wedge_compute_delta_squares_neon)));
#endif  // HAVE_NEON

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, WedgeUtilsSSEOptTest,
    ::testing::Values(TestFuncsFSSE(av1_wedge_sse_from_residuals_sse2,
                                    av1_wedge_sse_from_residuals_avx2)));

INSTANTIATE_TEST_SUITE_P(
    AVX2, WedgeUtilsSignOptTest,
    ::testing::Values(TestFuncsFSign(av1_wedge_sign_from_residuals_sse2,
                                     av1_wedge_sign_from_residuals_avx2)));

INSTANTIATE_TEST_SUITE_P(
    AVX2, WedgeUtilsDeltaSquaresOptTest,
    ::testing::Values(TestFuncsFDS(av1_wedge_compute_delta_squares_sse2,
                                   av1_wedge_compute_delta_squares_avx2)));
#endif  // HAVE_AVX2

#if HAVE_SVE
INSTANTIATE_TEST_SUITE_P(
    SVE, WedgeUtilsSSEOptTest,
    ::testing::Values(TestFuncsFSSE(av1_wedge_sse_from_residuals_c,
                                    av1_wedge_sse_from_residuals_sve)));

INSTANTIATE_TEST_SUITE_P(
    SVE, WedgeUtilsSignOptTest,
    ::testing::Values(TestFuncsFSign(av1_wedge_sign_from_residuals_c,
                                     av1_wedge_sign_from_residuals_sve)));
#endif  // HAVE_SVE

}  // namespace
