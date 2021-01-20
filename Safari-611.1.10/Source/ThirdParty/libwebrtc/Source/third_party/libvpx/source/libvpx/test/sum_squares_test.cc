/*
 *  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>
#include <cstdlib>
#include <string>
#include <tuple>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "vpx_ports/mem.h"

using libvpx_test::ACMRandom;

namespace {
const int kNumIterations = 10000;

typedef uint64_t (*SSI16Func)(const int16_t *src, int stride, int size);
typedef std::tuple<SSI16Func, SSI16Func> SumSquaresParam;

class SumSquaresTest : public ::testing::TestWithParam<SumSquaresParam> {
 public:
  virtual ~SumSquaresTest() {}
  virtual void SetUp() {
    ref_func_ = GET_PARAM(0);
    tst_func_ = GET_PARAM(1);
  }

  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  SSI16Func ref_func_;
  SSI16Func tst_func_;
};

TEST_P(SumSquaresTest, OperationCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, int16_t, src[256 * 256]);
  const int msb = 11;  // Up to 12 bit input
  const int limit = 1 << (msb + 1);

  for (int k = 0; k < kNumIterations; k++) {
    const int size = 4 << rnd(6);  // Up to 128x128
    int stride = 4 << rnd(7);      // Up to 256 stride
    while (stride < size) {        // Make sure it's valid
      stride = 4 << rnd(7);
    }

    for (int i = 0; i < size; ++i) {
      for (int j = 0; j < size; ++j) {
        src[i * stride + j] = rnd(2) ? rnd(limit) : -rnd(limit);
      }
    }

    const uint64_t res_ref = ref_func_(src, stride, size);
    uint64_t res_tst;
    ASM_REGISTER_STATE_CHECK(res_tst = tst_func_(src, stride, size));

    ASSERT_EQ(res_ref, res_tst) << "Error: Sum Squares Test"
                                << " C output does not match optimized output.";
  }
}

TEST_P(SumSquaresTest, ExtremeValues) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, int16_t, src[256 * 256]);
  const int msb = 11;  // Up to 12 bit input
  const int limit = 1 << (msb + 1);

  for (int k = 0; k < kNumIterations; k++) {
    const int size = 4 << rnd(6);  // Up to 128x128
    int stride = 4 << rnd(7);      // Up to 256 stride
    while (stride < size) {        // Make sure it's valid
      stride = 4 << rnd(7);
    }

    const int val = rnd(2) ? limit - 1 : -(limit - 1);
    for (int i = 0; i < size; ++i) {
      for (int j = 0; j < size; ++j) {
        src[i * stride + j] = val;
      }
    }

    const uint64_t res_ref = ref_func_(src, stride, size);
    uint64_t res_tst;
    ASM_REGISTER_STATE_CHECK(res_tst = tst_func_(src, stride, size));

    ASSERT_EQ(res_ref, res_tst) << "Error: Sum Squares Test"
                                << " C output does not match optimized output.";
  }
}

using std::make_tuple;

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, SumSquaresTest,
    ::testing::Values(make_tuple(&vpx_sum_squares_2d_i16_c,
                                 &vpx_sum_squares_2d_i16_neon)));
#endif  // HAVE_NEON

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, SumSquaresTest,
    ::testing::Values(make_tuple(&vpx_sum_squares_2d_i16_c,
                                 &vpx_sum_squares_2d_i16_sse2)));
#endif  // HAVE_SSE2

#if HAVE_MSA
INSTANTIATE_TEST_CASE_P(
    MSA, SumSquaresTest,
    ::testing::Values(make_tuple(&vpx_sum_squares_2d_i16_c,
                                 &vpx_sum_squares_2d_i16_msa)));
#endif  // HAVE_MSA
}  // namespace
