/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "gtest/gtest.h"

#include "test/function_equivalence_test.h"
#include "test/register_state_check.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"

#define MAX_SB_SQUARE (MAX_SB_SIZE * MAX_SB_SIZE)

using libaom_test::FunctionEquivalenceTest;

namespace {

static const int kIterations = 1000;
static const int kMaskMax = 64;

typedef unsigned int (*ObmcSadF)(const uint8_t *pre, int pre_stride,
                                 const int32_t *wsrc, const int32_t *mask);
typedef libaom_test::FuncParam<ObmcSadF> TestFuncs;

////////////////////////////////////////////////////////////////////////////////
// 8 bit
////////////////////////////////////////////////////////////////////////////////

class ObmcSadTest : public FunctionEquivalenceTest<ObmcSadF> {};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ObmcSadTest);

TEST_P(ObmcSadTest, RandomValues) {
  DECLARE_ALIGNED(32, uint8_t, pre[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int32_t, wsrc[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int32_t, mask[MAX_SB_SQUARE]);

  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    const int pre_stride = rng_(MAX_SB_SIZE + 1);

    for (int i = 0; i < MAX_SB_SQUARE; ++i) {
      pre[i] = rng_.Rand8();
      wsrc[i] = rng_.Rand8() * rng_(kMaskMax * kMaskMax + 1);
      mask[i] = rng_(kMaskMax * kMaskMax + 1);
    }

    const unsigned int ref_res = params_.ref_func(pre, pre_stride, wsrc, mask);
    unsigned int tst_res;
    API_REGISTER_STATE_CHECK(tst_res =
                                 params_.tst_func(pre, pre_stride, wsrc, mask));

    ASSERT_EQ(ref_res, tst_res);
  }
}

TEST_P(ObmcSadTest, ExtremeValues) {
  DECLARE_ALIGNED(32, uint8_t, pre[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int32_t, wsrc[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int32_t, mask[MAX_SB_SQUARE]);

  for (int iter = 0; iter < MAX_SB_SIZE && !HasFatalFailure(); ++iter) {
    const int pre_stride = iter;

    for (int i = 0; i < MAX_SB_SQUARE; ++i) {
      pre[i] = UINT8_MAX;
      wsrc[i] = UINT8_MAX * kMaskMax * kMaskMax;
      mask[i] = kMaskMax * kMaskMax;
    }

    const unsigned int ref_res = params_.ref_func(pre, pre_stride, wsrc, mask);
    unsigned int tst_res;
    API_REGISTER_STATE_CHECK(tst_res =
                                 params_.tst_func(pre, pre_stride, wsrc, mask));

    ASSERT_EQ(ref_res, tst_res);
  }
}

#if HAVE_SSE4_1
const ObmcSadTest::ParamType sse4_functions[] = {
  TestFuncs(aom_obmc_sad128x128_c, aom_obmc_sad128x128_sse4_1),
  TestFuncs(aom_obmc_sad128x64_c, aom_obmc_sad128x64_sse4_1),
  TestFuncs(aom_obmc_sad64x128_c, aom_obmc_sad64x128_sse4_1),
  TestFuncs(aom_obmc_sad64x64_c, aom_obmc_sad64x64_sse4_1),
  TestFuncs(aom_obmc_sad64x32_c, aom_obmc_sad64x32_sse4_1),
  TestFuncs(aom_obmc_sad32x64_c, aom_obmc_sad32x64_sse4_1),
  TestFuncs(aom_obmc_sad32x32_c, aom_obmc_sad32x32_sse4_1),
  TestFuncs(aom_obmc_sad32x16_c, aom_obmc_sad32x16_sse4_1),
  TestFuncs(aom_obmc_sad16x32_c, aom_obmc_sad16x32_sse4_1),
  TestFuncs(aom_obmc_sad16x16_c, aom_obmc_sad16x16_sse4_1),
  TestFuncs(aom_obmc_sad16x8_c, aom_obmc_sad16x8_sse4_1),
  TestFuncs(aom_obmc_sad8x16_c, aom_obmc_sad8x16_sse4_1),
  TestFuncs(aom_obmc_sad8x8_c, aom_obmc_sad8x8_sse4_1),
  TestFuncs(aom_obmc_sad8x4_c, aom_obmc_sad8x4_sse4_1),
  TestFuncs(aom_obmc_sad4x8_c, aom_obmc_sad4x8_sse4_1),
  TestFuncs(aom_obmc_sad4x4_c, aom_obmc_sad4x4_sse4_1),

  TestFuncs(aom_obmc_sad64x16_c, aom_obmc_sad64x16_sse4_1),
  TestFuncs(aom_obmc_sad16x64_c, aom_obmc_sad16x64_sse4_1),
  TestFuncs(aom_obmc_sad32x8_c, aom_obmc_sad32x8_sse4_1),
  TestFuncs(aom_obmc_sad8x32_c, aom_obmc_sad8x32_sse4_1),
  TestFuncs(aom_obmc_sad16x4_c, aom_obmc_sad16x4_sse4_1),
  TestFuncs(aom_obmc_sad4x16_c, aom_obmc_sad4x16_sse4_1),
};

INSTANTIATE_TEST_SUITE_P(SSE4_1, ObmcSadTest,
                         ::testing::ValuesIn(sse4_functions));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
const ObmcSadTest::ParamType avx2_functions[] = {
  TestFuncs(aom_obmc_sad128x128_c, aom_obmc_sad128x128_avx2),
  TestFuncs(aom_obmc_sad128x64_c, aom_obmc_sad128x64_avx2),
  TestFuncs(aom_obmc_sad64x128_c, aom_obmc_sad64x128_avx2),
  TestFuncs(aom_obmc_sad64x64_c, aom_obmc_sad64x64_avx2),
  TestFuncs(aom_obmc_sad64x32_c, aom_obmc_sad64x32_avx2),
  TestFuncs(aom_obmc_sad32x64_c, aom_obmc_sad32x64_avx2),
  TestFuncs(aom_obmc_sad32x32_c, aom_obmc_sad32x32_avx2),
  TestFuncs(aom_obmc_sad32x16_c, aom_obmc_sad32x16_avx2),
  TestFuncs(aom_obmc_sad16x32_c, aom_obmc_sad16x32_avx2),
  TestFuncs(aom_obmc_sad16x16_c, aom_obmc_sad16x16_avx2),
  TestFuncs(aom_obmc_sad16x8_c, aom_obmc_sad16x8_avx2),
  TestFuncs(aom_obmc_sad8x16_c, aom_obmc_sad8x16_avx2),
  TestFuncs(aom_obmc_sad8x8_c, aom_obmc_sad8x8_avx2),
  TestFuncs(aom_obmc_sad8x4_c, aom_obmc_sad8x4_avx2),
  TestFuncs(aom_obmc_sad4x8_c, aom_obmc_sad4x8_avx2),
  TestFuncs(aom_obmc_sad4x4_c, aom_obmc_sad4x4_avx2),

  TestFuncs(aom_obmc_sad64x16_c, aom_obmc_sad64x16_avx2),
  TestFuncs(aom_obmc_sad16x64_c, aom_obmc_sad16x64_avx2),
  TestFuncs(aom_obmc_sad32x8_c, aom_obmc_sad32x8_avx2),
  TestFuncs(aom_obmc_sad8x32_c, aom_obmc_sad8x32_avx2),
  TestFuncs(aom_obmc_sad16x4_c, aom_obmc_sad16x4_avx2),
  TestFuncs(aom_obmc_sad4x16_c, aom_obmc_sad4x16_avx2),
};

INSTANTIATE_TEST_SUITE_P(AVX2, ObmcSadTest,
                         ::testing::ValuesIn(avx2_functions));
#endif  // HAVE_AVX2

#if HAVE_NEON
const ObmcSadTest::ParamType neon_functions[] = {
  TestFuncs(aom_obmc_sad128x128_c, aom_obmc_sad128x128_neon),
  TestFuncs(aom_obmc_sad128x64_c, aom_obmc_sad128x64_neon),
  TestFuncs(aom_obmc_sad64x128_c, aom_obmc_sad64x128_neon),
  TestFuncs(aom_obmc_sad64x64_c, aom_obmc_sad64x64_neon),
  TestFuncs(aom_obmc_sad64x32_c, aom_obmc_sad64x32_neon),
  TestFuncs(aom_obmc_sad32x64_c, aom_obmc_sad32x64_neon),
  TestFuncs(aom_obmc_sad32x32_c, aom_obmc_sad32x32_neon),
  TestFuncs(aom_obmc_sad32x16_c, aom_obmc_sad32x16_neon),
  TestFuncs(aom_obmc_sad16x32_c, aom_obmc_sad16x32_neon),
  TestFuncs(aom_obmc_sad16x16_c, aom_obmc_sad16x16_neon),
  TestFuncs(aom_obmc_sad16x8_c, aom_obmc_sad16x8_neon),
  TestFuncs(aom_obmc_sad8x16_c, aom_obmc_sad8x16_neon),
  TestFuncs(aom_obmc_sad8x8_c, aom_obmc_sad8x8_neon),
  TestFuncs(aom_obmc_sad8x4_c, aom_obmc_sad8x4_neon),
  TestFuncs(aom_obmc_sad4x8_c, aom_obmc_sad4x8_neon),
  TestFuncs(aom_obmc_sad4x4_c, aom_obmc_sad4x4_neon),

  TestFuncs(aom_obmc_sad64x16_c, aom_obmc_sad64x16_neon),
  TestFuncs(aom_obmc_sad16x64_c, aom_obmc_sad16x64_neon),
  TestFuncs(aom_obmc_sad32x8_c, aom_obmc_sad32x8_neon),
  TestFuncs(aom_obmc_sad8x32_c, aom_obmc_sad8x32_neon),
  TestFuncs(aom_obmc_sad16x4_c, aom_obmc_sad16x4_neon),
  TestFuncs(aom_obmc_sad4x16_c, aom_obmc_sad4x16_neon),
};

INSTANTIATE_TEST_SUITE_P(NEON, ObmcSadTest,
                         ::testing::ValuesIn(neon_functions));
#endif  // HAVE_NEON

#if CONFIG_AV1_HIGHBITDEPTH
////////////////////////////////////////////////////////////////////////////////
// High bit-depth
////////////////////////////////////////////////////////////////////////////////

class ObmcSadHBDTest : public FunctionEquivalenceTest<ObmcSadF> {};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ObmcSadHBDTest);

TEST_P(ObmcSadHBDTest, RandomValues) {
  DECLARE_ALIGNED(32, uint16_t, pre[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int32_t, wsrc[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int32_t, mask[MAX_SB_SQUARE]);

  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    const int pre_stride = rng_(MAX_SB_SIZE + 1);

    for (int i = 0; i < MAX_SB_SQUARE; ++i) {
      pre[i] = rng_(1 << 12);
      wsrc[i] = rng_(1 << 12) * rng_(kMaskMax * kMaskMax + 1);
      mask[i] = rng_(kMaskMax * kMaskMax + 1);
    }

    const unsigned int ref_res =
        params_.ref_func(CONVERT_TO_BYTEPTR(pre), pre_stride, wsrc, mask);
    unsigned int tst_res;
    API_REGISTER_STATE_CHECK(
        tst_res =
            params_.tst_func(CONVERT_TO_BYTEPTR(pre), pre_stride, wsrc, mask));

    ASSERT_EQ(ref_res, tst_res);
  }
}

TEST_P(ObmcSadHBDTest, ExtremeValues) {
  DECLARE_ALIGNED(32, uint16_t, pre[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int32_t, wsrc[MAX_SB_SQUARE]);
  DECLARE_ALIGNED(32, int32_t, mask[MAX_SB_SQUARE]);

  for (int iter = 0; iter < MAX_SB_SIZE && !HasFatalFailure(); ++iter) {
    const int pre_stride = iter;

    for (int i = 0; i < MAX_SB_SQUARE; ++i) {
      pre[i] = (1 << 12) - 1;
      wsrc[i] = ((1 << 12) - 1) * kMaskMax * kMaskMax;
      mask[i] = kMaskMax * kMaskMax;
    }

    const unsigned int ref_res =
        params_.ref_func(CONVERT_TO_BYTEPTR(pre), pre_stride, wsrc, mask);
    unsigned int tst_res;
    API_REGISTER_STATE_CHECK(
        tst_res =
            params_.tst_func(CONVERT_TO_BYTEPTR(pre), pre_stride, wsrc, mask));

    ASSERT_EQ(ref_res, tst_res);
  }
}

#if HAVE_NEON
ObmcSadHBDTest::ParamType neon_functions_hbd[] = {
  TestFuncs(aom_highbd_obmc_sad128x128_c, aom_highbd_obmc_sad128x128_neon),
  TestFuncs(aom_highbd_obmc_sad128x64_c, aom_highbd_obmc_sad128x64_neon),
  TestFuncs(aom_highbd_obmc_sad64x128_c, aom_highbd_obmc_sad64x128_neon),
  TestFuncs(aom_highbd_obmc_sad64x64_c, aom_highbd_obmc_sad64x64_neon),
  TestFuncs(aom_highbd_obmc_sad64x32_c, aom_highbd_obmc_sad64x32_neon),
  TestFuncs(aom_highbd_obmc_sad32x64_c, aom_highbd_obmc_sad32x64_neon),
  TestFuncs(aom_highbd_obmc_sad32x32_c, aom_highbd_obmc_sad32x32_neon),
  TestFuncs(aom_highbd_obmc_sad32x16_c, aom_highbd_obmc_sad32x16_neon),
  TestFuncs(aom_highbd_obmc_sad16x32_c, aom_highbd_obmc_sad16x32_neon),
  TestFuncs(aom_highbd_obmc_sad16x16_c, aom_highbd_obmc_sad16x16_neon),
  TestFuncs(aom_highbd_obmc_sad16x8_c, aom_highbd_obmc_sad16x8_neon),
  TestFuncs(aom_highbd_obmc_sad8x16_c, aom_highbd_obmc_sad8x16_neon),
  TestFuncs(aom_highbd_obmc_sad8x8_c, aom_highbd_obmc_sad8x8_neon),
  TestFuncs(aom_highbd_obmc_sad8x4_c, aom_highbd_obmc_sad8x4_neon),
  TestFuncs(aom_highbd_obmc_sad4x8_c, aom_highbd_obmc_sad4x8_neon),
  TestFuncs(aom_highbd_obmc_sad4x4_c, aom_highbd_obmc_sad4x4_neon),
#if !CONFIG_REALTIME_ONLY
  TestFuncs(aom_highbd_obmc_sad64x16_c, aom_highbd_obmc_sad64x16_neon),
  TestFuncs(aom_highbd_obmc_sad16x64_c, aom_highbd_obmc_sad16x64_neon),
  TestFuncs(aom_highbd_obmc_sad32x8_c, aom_highbd_obmc_sad32x8_neon),
  TestFuncs(aom_highbd_obmc_sad8x32_c, aom_highbd_obmc_sad8x32_neon),
  TestFuncs(aom_highbd_obmc_sad16x4_c, aom_highbd_obmc_sad16x4_neon),
  TestFuncs(aom_highbd_obmc_sad4x16_c, aom_highbd_obmc_sad4x16_neon),
#endif  // !CONFIG_REALTIME_ONLY
};

INSTANTIATE_TEST_SUITE_P(NEON, ObmcSadHBDTest,
                         ::testing::ValuesIn(neon_functions_hbd));
#endif  // HAVE_NEON

#if HAVE_SSE4_1
ObmcSadHBDTest::ParamType sse4_functions_hbd[] = {
  TestFuncs(aom_highbd_obmc_sad128x128_c, aom_highbd_obmc_sad128x128_sse4_1),
  TestFuncs(aom_highbd_obmc_sad128x64_c, aom_highbd_obmc_sad128x64_sse4_1),
  TestFuncs(aom_highbd_obmc_sad64x128_c, aom_highbd_obmc_sad64x128_sse4_1),
  TestFuncs(aom_highbd_obmc_sad64x64_c, aom_highbd_obmc_sad64x64_sse4_1),
  TestFuncs(aom_highbd_obmc_sad64x32_c, aom_highbd_obmc_sad64x32_sse4_1),
  TestFuncs(aom_highbd_obmc_sad32x64_c, aom_highbd_obmc_sad32x64_sse4_1),
  TestFuncs(aom_highbd_obmc_sad32x32_c, aom_highbd_obmc_sad32x32_sse4_1),
  TestFuncs(aom_highbd_obmc_sad32x16_c, aom_highbd_obmc_sad32x16_sse4_1),
  TestFuncs(aom_highbd_obmc_sad16x32_c, aom_highbd_obmc_sad16x32_sse4_1),
  TestFuncs(aom_highbd_obmc_sad16x16_c, aom_highbd_obmc_sad16x16_sse4_1),
  TestFuncs(aom_highbd_obmc_sad16x8_c, aom_highbd_obmc_sad16x8_sse4_1),
  TestFuncs(aom_highbd_obmc_sad8x16_c, aom_highbd_obmc_sad8x16_sse4_1),
  TestFuncs(aom_highbd_obmc_sad8x8_c, aom_highbd_obmc_sad8x8_sse4_1),
  TestFuncs(aom_highbd_obmc_sad8x4_c, aom_highbd_obmc_sad8x4_sse4_1),
  TestFuncs(aom_highbd_obmc_sad4x8_c, aom_highbd_obmc_sad4x8_sse4_1),
  TestFuncs(aom_highbd_obmc_sad4x4_c, aom_highbd_obmc_sad4x4_sse4_1),

  TestFuncs(aom_highbd_obmc_sad64x16_c, aom_highbd_obmc_sad64x16_sse4_1),
  TestFuncs(aom_highbd_obmc_sad16x64_c, aom_highbd_obmc_sad16x64_sse4_1),
  TestFuncs(aom_highbd_obmc_sad32x8_c, aom_highbd_obmc_sad32x8_sse4_1),
  TestFuncs(aom_highbd_obmc_sad8x32_c, aom_highbd_obmc_sad8x32_sse4_1),
  TestFuncs(aom_highbd_obmc_sad16x4_c, aom_highbd_obmc_sad16x4_sse4_1),
  TestFuncs(aom_highbd_obmc_sad4x16_c, aom_highbd_obmc_sad4x16_sse4_1),
};

INSTANTIATE_TEST_SUITE_P(SSE4_1, ObmcSadHBDTest,
                         ::testing::ValuesIn(sse4_functions_hbd));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
ObmcSadHBDTest::ParamType avx2_functions_hbd[] = {
  TestFuncs(aom_highbd_obmc_sad128x128_c, aom_highbd_obmc_sad128x128_avx2),
  TestFuncs(aom_highbd_obmc_sad128x64_c, aom_highbd_obmc_sad128x64_avx2),
  TestFuncs(aom_highbd_obmc_sad64x128_c, aom_highbd_obmc_sad64x128_avx2),
  TestFuncs(aom_highbd_obmc_sad64x64_c, aom_highbd_obmc_sad64x64_avx2),
  TestFuncs(aom_highbd_obmc_sad64x32_c, aom_highbd_obmc_sad64x32_avx2),
  TestFuncs(aom_highbd_obmc_sad32x64_c, aom_highbd_obmc_sad32x64_avx2),
  TestFuncs(aom_highbd_obmc_sad32x32_c, aom_highbd_obmc_sad32x32_avx2),
  TestFuncs(aom_highbd_obmc_sad32x16_c, aom_highbd_obmc_sad32x16_avx2),
  TestFuncs(aom_highbd_obmc_sad16x32_c, aom_highbd_obmc_sad16x32_avx2),
  TestFuncs(aom_highbd_obmc_sad16x16_c, aom_highbd_obmc_sad16x16_avx2),
  TestFuncs(aom_highbd_obmc_sad16x8_c, aom_highbd_obmc_sad16x8_avx2),
  TestFuncs(aom_highbd_obmc_sad8x16_c, aom_highbd_obmc_sad8x16_avx2),
  TestFuncs(aom_highbd_obmc_sad8x8_c, aom_highbd_obmc_sad8x8_avx2),
  TestFuncs(aom_highbd_obmc_sad8x4_c, aom_highbd_obmc_sad8x4_avx2),
  TestFuncs(aom_highbd_obmc_sad4x8_c, aom_highbd_obmc_sad4x8_avx2),
  TestFuncs(aom_highbd_obmc_sad4x4_c, aom_highbd_obmc_sad4x4_avx2),

  TestFuncs(aom_highbd_obmc_sad64x16_c, aom_highbd_obmc_sad64x16_avx2),
  TestFuncs(aom_highbd_obmc_sad16x64_c, aom_highbd_obmc_sad16x64_avx2),
  TestFuncs(aom_highbd_obmc_sad32x8_c, aom_highbd_obmc_sad32x8_avx2),
  TestFuncs(aom_highbd_obmc_sad8x32_c, aom_highbd_obmc_sad8x32_avx2),
  TestFuncs(aom_highbd_obmc_sad16x4_c, aom_highbd_obmc_sad16x4_avx2),
  TestFuncs(aom_highbd_obmc_sad4x16_c, aom_highbd_obmc_sad4x16_avx2),
};

INSTANTIATE_TEST_SUITE_P(AVX2, ObmcSadHBDTest,
                         ::testing::ValuesIn(avx2_functions_hbd));
#endif  // HAVE_AVX2
#endif  // CONFIG_AV1_HIGHBITDEPTH
}  // namespace
