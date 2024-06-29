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

#include <cmath>
#include <cstdlib>
#include <string>
#include <tuple>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/entropy.h"
#include "aom/aom_codec.h"
#include "aom/aom_integer.h"

using libaom_test::ACMRandom;

namespace {
const int kNumIterations = 1000;

using ErrorBlockFunc = int64_t (*)(const tran_low_t *coeff,
                                   const tran_low_t *dqcoeff,
                                   intptr_t block_size, int64_t *ssz, int bps);

using ErrorBlockFunc8Bits = int64_t (*)(const tran_low_t *coeff,
                                        const tran_low_t *dqcoeff,
                                        intptr_t block_size, int64_t *ssz);

using ErrorBlockLpFunc = int64_t (*)(const int16_t *coeff,
                                     const int16_t *dqcoeff,
                                     intptr_t block_size);

using ErrorBlockParam =
    std::tuple<ErrorBlockFunc, ErrorBlockFunc, aom_bit_depth_t>;

template <ErrorBlockFunc8Bits fn>
int64_t BlockError8BitWrapper(const tran_low_t *coeff,
                              const tran_low_t *dqcoeff, intptr_t block_size,
                              int64_t *ssz, int bps) {
  EXPECT_EQ(bps, 8);
  return fn(coeff, dqcoeff, block_size, ssz);
}

template <ErrorBlockLpFunc fn>
int64_t BlockErrorLpWrapper(const tran_low_t *coeff, const tran_low_t *dqcoeff,
                            intptr_t block_size, int64_t *ssz, int bps) {
  EXPECT_EQ(bps, 8);
  *ssz = -1;
  return fn(reinterpret_cast<const int16_t *>(coeff),
            reinterpret_cast<const int16_t *>(dqcoeff), block_size);
}

class ErrorBlockTest : public ::testing::TestWithParam<ErrorBlockParam> {
 public:
  ~ErrorBlockTest() override = default;
  void SetUp() override {
    error_block_op_ = GET_PARAM(0);
    ref_error_block_op_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
  }

 protected:
  aom_bit_depth_t bit_depth_;
  ErrorBlockFunc error_block_op_;
  ErrorBlockFunc ref_error_block_op_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ErrorBlockTest);

TEST_P(ErrorBlockTest, OperationCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, tran_low_t, coeff[4096]);
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff[4096]);
  int err_count_total = 0;
  int first_failure = -1;
  intptr_t block_size;
  int64_t ssz;
  int64_t ret;
  int64_t ref_ssz;
  int64_t ref_ret;
  const int msb = bit_depth_ + 8 - 1;
  for (int i = 0; i < kNumIterations; ++i) {
    int err_count = 0;
    block_size = 16 << (i % 9);  // All block sizes from 4x4, 8x4 ..64x64
    for (int j = 0; j < block_size; j++) {
      // coeff and dqcoeff will always have at least the same sign, and this
      // can be used for optimization, so generate test input precisely.
      if (rnd(2)) {
        // Positive number
        coeff[j] = rnd(1 << msb);
        dqcoeff[j] = rnd(1 << msb);
      } else {
        // Negative number
        coeff[j] = -rnd(1 << msb);
        dqcoeff[j] = -rnd(1 << msb);
      }
    }
    ref_ret =
        ref_error_block_op_(coeff, dqcoeff, block_size, &ref_ssz, bit_depth_);
    API_REGISTER_STATE_CHECK(
        ret = error_block_op_(coeff, dqcoeff, block_size, &ssz, bit_depth_));
    err_count += (ref_ret != ret) | (ref_ssz != ssz);
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Error Block Test, C output doesn't match optimized output. "
      << "First failed at test case " << first_failure;
}

TEST_P(ErrorBlockTest, ExtremeValues) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, tran_low_t, coeff[4096]);
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff[4096]);
  int err_count_total = 0;
  int first_failure = -1;
  intptr_t block_size;
  int64_t ssz;
  int64_t ret;
  int64_t ref_ssz;
  int64_t ref_ret;
  const int msb = bit_depth_ + 8 - 1;
  int max_val = ((1 << msb) - 1);
  for (int i = 0; i < kNumIterations; ++i) {
    int err_count = 0;
    int k = (i / 9) % 9;

    // Change the maximum coeff value, to test different bit boundaries
    if (k == 8 && (i % 9) == 0) {
      max_val >>= 1;
    }
    block_size = 16 << (i % 9);  // All block sizes from 4x4, 8x4 ..64x64
    for (int j = 0; j < block_size; j++) {
      if (k < 4) {
        // Test at positive maximum values
        coeff[j] = k % 2 ? max_val : 0;
        dqcoeff[j] = (k >> 1) % 2 ? max_val : 0;
      } else if (k < 8) {
        // Test at negative maximum values
        coeff[j] = k % 2 ? -max_val : 0;
        dqcoeff[j] = (k >> 1) % 2 ? -max_val : 0;
      } else {
        if (rnd(2)) {
          // Positive number
          coeff[j] = rnd(1 << 14);
          dqcoeff[j] = rnd(1 << 14);
        } else {
          // Negative number
          coeff[j] = -rnd(1 << 14);
          dqcoeff[j] = -rnd(1 << 14);
        }
      }
    }
    ref_ret =
        ref_error_block_op_(coeff, dqcoeff, block_size, &ref_ssz, bit_depth_);
    API_REGISTER_STATE_CHECK(
        ret = error_block_op_(coeff, dqcoeff, block_size, &ssz, bit_depth_));
    err_count += (ref_ret != ret) | (ref_ssz != ssz);
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Error Block Test, C output doesn't match optimized output. "
      << "First failed at test case " << first_failure;
}

TEST_P(ErrorBlockTest, DISABLED_Speed) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, tran_low_t, coeff[4096]);
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff[4096]);
  intptr_t block_size;
  int64_t ssz;
  int num_iters = 100000;
  int64_t ref_ssz;
  const int msb = bit_depth_ + 8 - 1;
  for (int i = 0; i < 9; ++i) {
    block_size = 16 << (i % 9);  // All block sizes from 4x4, 8x4 ..64x64
    for (int k = 0; k < 9; k++) {
      for (int j = 0; j < block_size; j++) {
        if (k < 5) {
          if (rnd(2)) {
            // Positive number
            coeff[j] = rnd(1 << msb);
            dqcoeff[j] = rnd(1 << msb);
          } else {
            // Negative number
            coeff[j] = -rnd(1 << msb);
            dqcoeff[j] = -rnd(1 << msb);
          }
        } else {
          if (rnd(2)) {
            // Positive number
            coeff[j] = rnd(1 << 14);
            dqcoeff[j] = rnd(1 << 14);
          } else {
            // Negative number
            coeff[j] = -rnd(1 << 14);
            dqcoeff[j] = -rnd(1 << 14);
          }
        }
      }
      aom_usec_timer ref_timer, test_timer;

      aom_usec_timer_start(&ref_timer);
      for (int iter = 0; iter < num_iters; ++iter) {
        ref_error_block_op_(coeff, dqcoeff, block_size, &ref_ssz, bit_depth_);
      }
      aom_usec_timer_mark(&ref_timer);
      const int elapsed_time_c =
          static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

      aom_usec_timer_start(&test_timer);
      for (int iter = 0; iter < num_iters; ++iter) {
        error_block_op_(coeff, dqcoeff, block_size, &ssz, bit_depth_);
      }
      aom_usec_timer_mark(&test_timer);

      const int elapsed_time_simd =
          static_cast<int>(aom_usec_timer_elapsed(&test_timer));

      printf(
          " c_time=%d \t simd_time=%d \t "
          "gain=%d \n",
          elapsed_time_c, elapsed_time_simd,
          (elapsed_time_c / elapsed_time_simd));
    }
  }
}

using std::make_tuple;

#if HAVE_SSE2
const ErrorBlockParam kErrorBlockTestParamsSse2[] = {
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(&av1_highbd_block_error_sse2, &av1_highbd_block_error_c,
             AOM_BITS_10),
  make_tuple(&av1_highbd_block_error_sse2, &av1_highbd_block_error_c,
             AOM_BITS_12),
  make_tuple(&av1_highbd_block_error_sse2, &av1_highbd_block_error_c,
             AOM_BITS_8),
#endif
  make_tuple(&BlockError8BitWrapper<av1_block_error_sse2>,
             &BlockError8BitWrapper<av1_block_error_c>, AOM_BITS_8),
  make_tuple(&BlockErrorLpWrapper<av1_block_error_lp_sse2>,
             &BlockErrorLpWrapper<av1_block_error_lp_c>, AOM_BITS_8)
};

INSTANTIATE_TEST_SUITE_P(SSE2, ErrorBlockTest,
                         ::testing::ValuesIn(kErrorBlockTestParamsSse2));
#endif  // HAVE_SSE2

#if HAVE_AVX2
const ErrorBlockParam kErrorBlockTestParamsAvx2[] = {
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(&av1_highbd_block_error_avx2, &av1_highbd_block_error_c,
             AOM_BITS_10),
  make_tuple(&av1_highbd_block_error_avx2, &av1_highbd_block_error_c,
             AOM_BITS_12),
  make_tuple(&av1_highbd_block_error_avx2, &av1_highbd_block_error_c,
             AOM_BITS_8),
#endif
  make_tuple(&BlockError8BitWrapper<av1_block_error_avx2>,
             &BlockError8BitWrapper<av1_block_error_c>, AOM_BITS_8),
  make_tuple(&BlockErrorLpWrapper<av1_block_error_lp_avx2>,
             &BlockErrorLpWrapper<av1_block_error_lp_c>, AOM_BITS_8)
};

INSTANTIATE_TEST_SUITE_P(AVX2, ErrorBlockTest,
                         ::testing::ValuesIn(kErrorBlockTestParamsAvx2));
#endif  // HAVE_AVX2

#if HAVE_NEON
const ErrorBlockParam kErrorBlockTestParamsNeon[] = {
#if CONFIG_AV1_HIGHBITDEPTH
  make_tuple(&av1_highbd_block_error_neon, &av1_highbd_block_error_c,
             AOM_BITS_10),
  make_tuple(&av1_highbd_block_error_neon, &av1_highbd_block_error_c,
             AOM_BITS_12),
  make_tuple(&av1_highbd_block_error_neon, &av1_highbd_block_error_c,
             AOM_BITS_8),
#endif
  make_tuple(&BlockError8BitWrapper<av1_block_error_neon>,
             &BlockError8BitWrapper<av1_block_error_c>, AOM_BITS_8),
  make_tuple(&BlockErrorLpWrapper<av1_block_error_lp_neon>,
             &BlockErrorLpWrapper<av1_block_error_lp_c>, AOM_BITS_8)
};

INSTANTIATE_TEST_SUITE_P(NEON, ErrorBlockTest,
                         ::testing::ValuesIn(kErrorBlockTestParamsNeon));
#endif  // HAVE_NEON

#if HAVE_SVE
const ErrorBlockParam kErrorBlockTestParamsSVE[] = {
  make_tuple(&BlockError8BitWrapper<av1_block_error_sve>,
             &BlockError8BitWrapper<av1_block_error_c>, AOM_BITS_8),
  make_tuple(&BlockErrorLpWrapper<av1_block_error_lp_sve>,
             &BlockErrorLpWrapper<av1_block_error_lp_c>, AOM_BITS_8)
};

INSTANTIATE_TEST_SUITE_P(SVE, ErrorBlockTest,
                         ::testing::ValuesIn(kErrorBlockTestParamsSVE));
#endif  // HAVE_SVE
}  // namespace
