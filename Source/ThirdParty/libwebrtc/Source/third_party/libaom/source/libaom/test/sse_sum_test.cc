/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
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
#include "config/aom_dsp_rtcd.h"

#include "aom_ports/mem.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "test/function_equivalence_test.h"

using libaom_test::ACMRandom;
using libaom_test::FunctionEquivalenceTest;
using ::testing::Combine;
using ::testing::Range;
using ::testing::Values;
using ::testing::ValuesIn;

namespace {
const int kNumIterations = 10000;

typedef uint64_t (*SSI16Func)(const int16_t *src, int src_stride, int width,
                              int height, int *sum);
typedef libaom_test::FuncParam<SSI16Func> TestFuncs;

class SumSSETest : public ::testing::TestWithParam<TestFuncs> {
 public:
  virtual ~SumSSETest() {}
  virtual void SetUp() {
    params_ = this->GetParam();
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src_ = reinterpret_cast<int16_t *>(aom_memalign(16, 256 * 256 * 2));
    ASSERT_NE(src_, nullptr);
  }

  virtual void TearDown() { aom_free(src_); }
  void RunTest(int isRandom);
  void RunSpeedTest();

  void GenRandomData(int width, int height, int stride) {
    const int msb = 11;  // Up to 12 bit input
    const int limit = 1 << (msb + 1);
    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        src_[ii * stride + jj] = rnd_(2) ? rnd_(limit) : -rnd_(limit);
      }
    }
  }

  void GenExtremeData(int width, int height, int stride) {
    const int msb = 11;  // Up to 12 bit input
    const int limit = 1 << (msb + 1);
    const int val = rnd_(2) ? limit - 1 : -(limit - 1);
    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        src_[ii * stride + jj] = val;
      }
    }
  }

 protected:
  TestFuncs params_;
  int16_t *src_;
  ACMRandom rnd_;
};

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SumSSETest);

void SumSSETest::RunTest(int isRandom) {
  for (int k = 0; k < kNumIterations; k++) {
    const int width = 4 * (rnd_(31) + 1);   // Up to 128x128
    const int height = 4 * (rnd_(31) + 1);  // Up to 128x128
    int stride = 4 << rnd_(7);              // Up to 256 stride
    while (stride < width) {                // Make sure it's valid
      stride = 4 << rnd_(7);
    }
    if (isRandom) {
      GenRandomData(width, height, stride);
    } else {
      GenExtremeData(width, height, stride);
    }
    int sum_ref = 0, sum_tst = 0;
    const uint64_t sse_ref =
        params_.ref_func(src_, stride, width, height, &sum_ref);
    const uint64_t sse_tst =
        params_.tst_func(src_, stride, width, height, &sum_tst);

    EXPECT_EQ(sse_ref, sse_tst)
        << "Error: SumSSETest [" << width << "x" << height
        << "] C SSE does not match optimized output.";
    EXPECT_EQ(sum_ref, sum_tst)
        << "Error: SumSSETest [" << width << "x" << height
        << "] C Sum does not match optimized output.";
  }
}

void SumSSETest::RunSpeedTest() {
  for (int block = BLOCK_4X4; block < BLOCK_SIZES_ALL; block++) {
    const int width = block_size_wide[block];   // Up to 128x128
    const int height = block_size_high[block];  // Up to 128x128
    int stride = 4 << rnd_(7);                  // Up to 256 stride
    while (stride < width) {                    // Make sure it's valid
      stride = 4 << rnd_(7);
    }
    GenExtremeData(width, height, stride);
    const int num_loops = 1000000000 / (width + height);
    int sum_ref = 0, sum_tst = 0;

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);

    for (int i = 0; i < num_loops; ++i)
      params_.ref_func(src_, stride, width, height, &sum_ref);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("SumSquaresTest C %3dx%-3d: %7.2f ns\n", width, height,
           1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);
    for (int i = 0; i < num_loops; ++i)
      params_.tst_func(src_, stride, width, height, &sum_tst);
    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("SumSquaresTest Test %3dx%-3d: %7.2f ns\n", width, height,
           1000.0 * elapsed_time1 / num_loops);
  }
}

TEST_P(SumSSETest, OperationCheck) {
  RunTest(1);  // GenRandomData
}

TEST_P(SumSSETest, ExtremeValues) {
  RunTest(0);  // GenExtremeData
}

TEST_P(SumSSETest, DISABLED_Speed) { RunSpeedTest(); }

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, SumSSETest,
                         ::testing::Values(TestFuncs(
                             &aom_sum_sse_2d_i16_c, &aom_sum_sse_2d_i16_sse2)));

#endif  // HAVE_SSE2

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, SumSSETest,
                         ::testing::Values(TestFuncs(
                             &aom_sum_sse_2d_i16_c, &aom_sum_sse_2d_i16_neon)));
#endif  // HAVE_NEON

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, SumSSETest,
                         ::testing::Values(TestFuncs(
                             &aom_sum_sse_2d_i16_c, &aom_sum_sse_2d_i16_avx2)));
#endif  // HAVE_AVX2

}  // namespace
