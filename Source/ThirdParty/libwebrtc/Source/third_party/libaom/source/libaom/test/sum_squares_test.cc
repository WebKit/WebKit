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
#include "config/aom_dsp_rtcd.h"

#include "aom_ports/mem.h"
#include "av1/common/common_data.h"
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

static const int16_t kInt13Max = (1 << 12) - 1;

typedef uint64_t (*SSI16Func)(const int16_t *src, int stride, int width,
                              int height);
typedef libaom_test::FuncParam<SSI16Func> TestFuncs;

class SumSquaresTest : public ::testing::TestWithParam<TestFuncs> {
 public:
  ~SumSquaresTest() override = default;
  void SetUp() override {
    params_ = this->GetParam();
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src_ = reinterpret_cast<int16_t *>(aom_memalign(16, 256 * 256 * 2));
    ASSERT_NE(src_, nullptr);
  }

  void TearDown() override { aom_free(src_); }
  void RunTest(bool is_random);
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
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SumSquaresTest);

void SumSquaresTest::RunTest(bool is_random) {
  int failed = 0;
  for (int k = 0; k < kNumIterations; k++) {
    const int width = 4 * (rnd_(31) + 1);   // Up to 128x128
    const int height = 4 * (rnd_(31) + 1);  // Up to 128x128
    int stride = 4 << rnd_(7);              // Up to 256 stride
    while (stride < width) {                // Make sure it's valid
      stride = 4 << rnd_(7);
    }
    if (is_random) {
      GenRandomData(width, height, stride);
    } else {
      GenExtremeData(width, height, stride);
    }
    const uint64_t res_ref = params_.ref_func(src_, stride, width, height);
    uint64_t res_tst;
    API_REGISTER_STATE_CHECK(res_tst =
                                 params_.tst_func(src_, stride, width, height));

    if (!failed) {
      failed = res_ref != res_tst;
      EXPECT_EQ(res_ref, res_tst)
          << "Error: Sum Squares Test [" << width << "x" << height
          << "] C output does not match optimized output.";
    }
  }
}

void SumSquaresTest::RunSpeedTest() {
  for (int block = BLOCK_4X4; block < BLOCK_SIZES_ALL; block++) {
    const int width = block_size_wide[block];   // Up to 128x128
    const int height = block_size_high[block];  // Up to 128x128
    int stride = 4 << rnd_(7);                  // Up to 256 stride
    while (stride < width) {                    // Make sure it's valid
      stride = 4 << rnd_(7);
    }
    GenExtremeData(width, height, stride);
    const int num_loops = 1000000000 / (width + height);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);

    for (int i = 0; i < num_loops; ++i)
      params_.ref_func(src_, stride, width, height);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("SumSquaresTest C %3dx%-3d: %7.2f ns\n", width, height,
           1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);
    for (int i = 0; i < num_loops; ++i)
      params_.tst_func(src_, stride, width, height);
    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("SumSquaresTest Test %3dx%-3d: %7.2f ns\n", width, height,
           1000.0 * elapsed_time1 / num_loops);
  }
}

TEST_P(SumSquaresTest, OperationCheck) {
  RunTest(true);  // GenRandomData
}

TEST_P(SumSquaresTest, ExtremeValues) {
  RunTest(false);  // GenExtremeData
}

TEST_P(SumSquaresTest, DISABLED_Speed) { RunSpeedTest(); }

#if HAVE_SSE2

INSTANTIATE_TEST_SUITE_P(
    SSE2, SumSquaresTest,
    ::testing::Values(TestFuncs(&aom_sum_squares_2d_i16_c,
                                &aom_sum_squares_2d_i16_sse2)));

#endif  // HAVE_SSE2

#if HAVE_NEON

INSTANTIATE_TEST_SUITE_P(
    NEON, SumSquaresTest,
    ::testing::Values(TestFuncs(&aom_sum_squares_2d_i16_c,
                                &aom_sum_squares_2d_i16_neon)));

#endif  // HAVE_NEON

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, SumSquaresTest,
    ::testing::Values(TestFuncs(&aom_sum_squares_2d_i16_c,
                                &aom_sum_squares_2d_i16_avx2)));
#endif  // HAVE_AVX2

//////////////////////////////////////////////////////////////////////////////
// 1D version
//////////////////////////////////////////////////////////////////////////////

typedef uint64_t (*F1D)(const int16_t *src, uint32_t n);
typedef libaom_test::FuncParam<F1D> TestFuncs1D;

class SumSquares1DTest : public FunctionEquivalenceTest<F1D> {
 protected:
  static const int kIterations = 1000;
  static const int kMaxSize = 256;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SumSquares1DTest);

TEST_P(SumSquares1DTest, RandomValues) {
  DECLARE_ALIGNED(16, int16_t, src[kMaxSize * kMaxSize]);

  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < kMaxSize * kMaxSize; ++i)
      src[i] = rng_(kInt13Max * 2 + 1) - kInt13Max;

    const int n = rng_(2) ? rng_(kMaxSize * kMaxSize + 1 - kMaxSize) + kMaxSize
                          : rng_(kMaxSize) + 1;

    const uint64_t ref_res = params_.ref_func(src, n);
    uint64_t tst_res;
    API_REGISTER_STATE_CHECK(tst_res = params_.tst_func(src, n));

    ASSERT_EQ(ref_res, tst_res);
  }
}

TEST_P(SumSquares1DTest, ExtremeValues) {
  DECLARE_ALIGNED(16, int16_t, src[kMaxSize * kMaxSize]);

  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    if (rng_(2)) {
      for (int i = 0; i < kMaxSize * kMaxSize; ++i) src[i] = kInt13Max;
    } else {
      for (int i = 0; i < kMaxSize * kMaxSize; ++i) src[i] = -kInt13Max;
    }

    const int n = rng_(2) ? rng_(kMaxSize * kMaxSize + 1 - kMaxSize) + kMaxSize
                          : rng_(kMaxSize) + 1;

    const uint64_t ref_res = params_.ref_func(src, n);
    uint64_t tst_res;
    API_REGISTER_STATE_CHECK(tst_res = params_.tst_func(src, n));

    ASSERT_EQ(ref_res, tst_res);
  }
}

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, SumSquares1DTest,
                         ::testing::Values(TestFuncs1D(
                             aom_sum_squares_i16_c, aom_sum_squares_i16_sse2)));

#endif  // HAVE_SSE2

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, SumSquares1DTest,
                         ::testing::Values(TestFuncs1D(
                             aom_sum_squares_i16_c, aom_sum_squares_i16_neon)));

#endif  // HAVE_NEON

typedef int64_t (*SSEFunc)(const uint8_t *a, int a_stride, const uint8_t *b,
                           int b_stride, int width, int height);
typedef libaom_test::FuncParam<SSEFunc> TestSSEFuncs;

typedef std::tuple<TestSSEFuncs, int> SSETestParam;

class SSETest : public ::testing::TestWithParam<SSETestParam> {
 public:
  ~SSETest() override = default;
  void SetUp() override {
    params_ = GET_PARAM(0);
    width_ = GET_PARAM(1);
    is_hbd_ =
#if CONFIG_AV1_HIGHBITDEPTH
        params_.ref_func == aom_highbd_sse_c;
#else
        false;
#endif
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src_ = reinterpret_cast<uint8_t *>(aom_memalign(32, 256 * 256 * 2));
    ref_ = reinterpret_cast<uint8_t *>(aom_memalign(32, 256 * 256 * 2));
    ASSERT_NE(src_, nullptr);
    ASSERT_NE(ref_, nullptr);
  }

  void TearDown() override {
    aom_free(src_);
    aom_free(ref_);
  }
  void RunTest(bool is_random, int width, int height, int run_times);

  void GenRandomData(int width, int height, int stride) {
    uint16_t *src16 = reinterpret_cast<uint16_t *>(src_);
    uint16_t *ref16 = reinterpret_cast<uint16_t *>(ref_);
    const int msb = 11;  // Up to 12 bit input
    const int limit = 1 << (msb + 1);
    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        if (!is_hbd_) {
          src_[ii * stride + jj] = rnd_.Rand8();
          ref_[ii * stride + jj] = rnd_.Rand8();
        } else {
          src16[ii * stride + jj] = rnd_(limit);
          ref16[ii * stride + jj] = rnd_(limit);
        }
      }
    }
  }

  void GenExtremeData(int width, int height, int stride, uint8_t *data,
                      int16_t val) {
    uint16_t *data16 = reinterpret_cast<uint16_t *>(data);
    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        if (!is_hbd_) {
          data[ii * stride + jj] = static_cast<uint8_t>(val);
        } else {
          data16[ii * stride + jj] = val;
        }
      }
    }
  }

 protected:
  bool is_hbd_;
  int width_;
  TestSSEFuncs params_;
  uint8_t *src_;
  uint8_t *ref_;
  ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SSETest);

void SSETest::RunTest(bool is_random, int width, int height, int run_times) {
  int failed = 0;
  aom_usec_timer ref_timer, test_timer;
  for (int k = 0; k < 3; k++) {
    int stride = 4 << rnd_(7);  // Up to 256 stride
    while (stride < width) {    // Make sure it's valid
      stride = 4 << rnd_(7);
    }
    if (is_random) {
      GenRandomData(width, height, stride);
    } else {
      const int msb = is_hbd_ ? 12 : 8;  // Up to 12 bit input
      const int limit = (1 << msb) - 1;
      if (k == 0) {
        GenExtremeData(width, height, stride, src_, 0);
        GenExtremeData(width, height, stride, ref_, limit);
      } else {
        GenExtremeData(width, height, stride, src_, limit);
        GenExtremeData(width, height, stride, ref_, 0);
      }
    }
    int64_t res_ref, res_tst;
    uint8_t *src = src_;
    uint8_t *ref = ref_;
    if (is_hbd_) {
      src = CONVERT_TO_BYTEPTR(src_);
      ref = CONVERT_TO_BYTEPTR(ref_);
    }
    res_ref = params_.ref_func(src, stride, ref, stride, width, height);
    res_tst = params_.tst_func(src, stride, ref, stride, width, height);
    if (run_times > 1) {
      aom_usec_timer_start(&ref_timer);
      for (int j = 0; j < run_times; j++) {
        params_.ref_func(src, stride, ref, stride, width, height);
      }
      aom_usec_timer_mark(&ref_timer);
      const int elapsed_time_c =
          static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

      aom_usec_timer_start(&test_timer);
      for (int j = 0; j < run_times; j++) {
        params_.tst_func(src, stride, ref, stride, width, height);
      }
      aom_usec_timer_mark(&test_timer);
      const int elapsed_time_simd =
          static_cast<int>(aom_usec_timer_elapsed(&test_timer));

      printf(
          "c_time=%d \t simd_time=%d \t "
          "gain=%d\n",
          elapsed_time_c, elapsed_time_simd,
          (elapsed_time_c / elapsed_time_simd));
    } else {
      if (!failed) {
        failed = res_ref != res_tst;
        EXPECT_EQ(res_ref, res_tst)
            << "Error:" << (is_hbd_ ? "hbd " : " ") << k << " SSE Test ["
            << width << "x" << height
            << "] C output does not match optimized output.";
      }
    }
  }
}

TEST_P(SSETest, OperationCheck) {
  for (int height = 4; height <= 128; height += 4) {
    RunTest(true, width_, height, 1);  // GenRandomData
  }
}

TEST_P(SSETest, ExtremeValues) {
  for (int height = 4; height <= 128; height += 4) {
    RunTest(false, width_, height, 1);
  }
}

TEST_P(SSETest, DISABLED_Speed) {
  for (int height = 4; height <= 128; height += 4) {
    RunTest(true, width_, height, 100);
  }
}

#if HAVE_NEON
TestSSEFuncs sse_neon[] = {
  TestSSEFuncs(&aom_sse_c, &aom_sse_neon),
#if CONFIG_AV1_HIGHBITDEPTH
  TestSSEFuncs(&aom_highbd_sse_c, &aom_highbd_sse_neon)
#endif
};
INSTANTIATE_TEST_SUITE_P(NEON, SSETest,
                         Combine(ValuesIn(sse_neon), Range(4, 129, 4)));
#endif  // HAVE_NEON

#if HAVE_NEON_DOTPROD
TestSSEFuncs sse_neon_dotprod[] = {
  TestSSEFuncs(&aom_sse_c, &aom_sse_neon_dotprod),
};
INSTANTIATE_TEST_SUITE_P(NEON_DOTPROD, SSETest,
                         Combine(ValuesIn(sse_neon_dotprod), Range(4, 129, 4)));
#endif  // HAVE_NEON_DOTPROD

#if HAVE_SSE4_1
TestSSEFuncs sse_sse4[] = {
  TestSSEFuncs(&aom_sse_c, &aom_sse_sse4_1),
#if CONFIG_AV1_HIGHBITDEPTH
  TestSSEFuncs(&aom_highbd_sse_c, &aom_highbd_sse_sse4_1)
#endif
};
INSTANTIATE_TEST_SUITE_P(SSE4_1, SSETest,
                         Combine(ValuesIn(sse_sse4), Range(4, 129, 4)));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2

TestSSEFuncs sse_avx2[] = {
  TestSSEFuncs(&aom_sse_c, &aom_sse_avx2),
#if CONFIG_AV1_HIGHBITDEPTH
  TestSSEFuncs(&aom_highbd_sse_c, &aom_highbd_sse_avx2)
#endif
};
INSTANTIATE_TEST_SUITE_P(AVX2, SSETest,
                         Combine(ValuesIn(sse_avx2), Range(4, 129, 4)));
#endif  // HAVE_AVX2

//////////////////////////////////////////////////////////////////////////////
// get_blk sum squares test functions
//////////////////////////////////////////////////////////////////////////////

typedef void (*sse_sum_func)(const int16_t *data, int stride, int bw, int bh,
                             int *x_sum, int64_t *x2_sum);
typedef libaom_test::FuncParam<sse_sum_func> TestSSE_SumFuncs;

typedef std::tuple<TestSSE_SumFuncs, TX_SIZE> SSE_SumTestParam;

class SSE_Sum_Test : public ::testing::TestWithParam<SSE_SumTestParam> {
 public:
  ~SSE_Sum_Test() override = default;
  void SetUp() override {
    params_ = GET_PARAM(0);
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src_ = reinterpret_cast<int16_t *>(aom_memalign(32, 256 * 256 * 2));
    ASSERT_NE(src_, nullptr);
  }

  void TearDown() override { aom_free(src_); }
  void RunTest(bool is_random, int tx_size, int run_times);

  void GenRandomData(int width, int height, int stride) {
    const int msb = 11;  // Up to 12 bit input
    const int limit = 1 << (msb + 1);
    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        src_[ii * stride + jj] = rnd_(limit);
      }
    }
  }

  void GenExtremeData(int width, int height, int stride, int16_t *data,
                      int16_t val) {
    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        data[ii * stride + jj] = val;
      }
    }
  }

 protected:
  TestSSE_SumFuncs params_;
  int16_t *src_;
  ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SSE_Sum_Test);

void SSE_Sum_Test::RunTest(bool is_random, int tx_size, int run_times) {
  aom_usec_timer ref_timer, test_timer;
  int width = tx_size_wide[tx_size];
  int height = tx_size_high[tx_size];
  for (int k = 0; k < 3; k++) {
    int stride = 4 << rnd_(7);  // Up to 256 stride
    while (stride < width) {    // Make sure it's valid
      stride = 4 << rnd_(7);
    }
    if (is_random) {
      GenRandomData(width, height, stride);
    } else {
      const int msb = 12;  // Up to 12 bit input
      const int limit = (1 << msb) - 1;
      if (k == 0) {
        GenExtremeData(width, height, stride, src_, limit);
      } else {
        GenExtremeData(width, height, stride, src_, -limit);
      }
    }
    int sum_c = 0;
    int64_t sse_intr = 0;
    int sum_intr = 0;
    int64_t sse_c = 0;

    params_.ref_func(src_, stride, width, height, &sum_c, &sse_c);
    params_.tst_func(src_, stride, width, height, &sum_intr, &sse_intr);

    if (run_times > 1) {
      aom_usec_timer_start(&ref_timer);
      for (int j = 0; j < run_times; j++) {
        params_.ref_func(src_, stride, width, height, &sum_c, &sse_c);
      }
      aom_usec_timer_mark(&ref_timer);
      const int elapsed_time_c =
          static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

      aom_usec_timer_start(&test_timer);
      for (int j = 0; j < run_times; j++) {
        params_.tst_func(src_, stride, width, height, &sum_intr, &sse_intr);
      }
      aom_usec_timer_mark(&test_timer);
      const int elapsed_time_simd =
          static_cast<int>(aom_usec_timer_elapsed(&test_timer));

      printf(
          "c_time=%d \t simd_time=%d \t "
          "gain=%f\t width=%d\t height=%d \n",
          elapsed_time_c, elapsed_time_simd,
          (float)((float)elapsed_time_c / (float)elapsed_time_simd), width,
          height);

    } else {
      EXPECT_EQ(sum_c, sum_intr)
          << "Error:" << k << " SSE Sum Test [" << width << "x" << height
          << "] C output does not match optimized output.";
      EXPECT_EQ(sse_c, sse_intr)
          << "Error:" << k << " SSE Sum Test [" << width << "x" << height
          << "] C output does not match optimized output.";
    }
  }
}

TEST_P(SSE_Sum_Test, OperationCheck) {
  RunTest(true, GET_PARAM(1), 1);  // GenRandomData
}

TEST_P(SSE_Sum_Test, ExtremeValues) { RunTest(false, GET_PARAM(1), 1); }

TEST_P(SSE_Sum_Test, DISABLED_Speed) { RunTest(true, GET_PARAM(1), 10000); }

#if HAVE_SSE2 || HAVE_AVX2 || HAVE_NEON
const TX_SIZE kValidBlockSize[] = { TX_4X4,   TX_8X8,   TX_16X16, TX_32X32,
                                    TX_64X64, TX_4X8,   TX_8X4,   TX_8X16,
                                    TX_16X8,  TX_16X32, TX_32X16, TX_64X32,
                                    TX_32X64, TX_4X16,  TX_16X4,  TX_8X32,
                                    TX_32X8,  TX_16X64, TX_64X16 };
#endif

#if HAVE_SSE2
TestSSE_SumFuncs sse_sum_sse2[] = { TestSSE_SumFuncs(
    &aom_get_blk_sse_sum_c, &aom_get_blk_sse_sum_sse2) };
INSTANTIATE_TEST_SUITE_P(SSE2, SSE_Sum_Test,
                         Combine(ValuesIn(sse_sum_sse2),
                                 ValuesIn(kValidBlockSize)));
#endif  // HAVE_SSE2

#if HAVE_AVX2
TestSSE_SumFuncs sse_sum_avx2[] = { TestSSE_SumFuncs(
    &aom_get_blk_sse_sum_c, &aom_get_blk_sse_sum_avx2) };
INSTANTIATE_TEST_SUITE_P(AVX2, SSE_Sum_Test,
                         Combine(ValuesIn(sse_sum_avx2),
                                 ValuesIn(kValidBlockSize)));
#endif  // HAVE_AVX2

#if HAVE_NEON
TestSSE_SumFuncs sse_sum_neon[] = { TestSSE_SumFuncs(
    &aom_get_blk_sse_sum_c, &aom_get_blk_sse_sum_neon) };
INSTANTIATE_TEST_SUITE_P(NEON, SSE_Sum_Test,
                         Combine(ValuesIn(sse_sum_neon),
                                 ValuesIn(kValidBlockSize)));
#endif  // HAVE_NEON

//////////////////////////////////////////////////////////////////////////////
// 2D Variance test functions
//////////////////////////////////////////////////////////////////////////////

typedef uint64_t (*Var2DFunc)(uint8_t *src, int stride, int width, int height);
typedef libaom_test::FuncParam<Var2DFunc> TestFuncVar2D;

const uint16_t test_block_size[2] = { 128, 256 };

class Lowbd2dVarTest : public ::testing::TestWithParam<TestFuncVar2D> {
 public:
  ~Lowbd2dVarTest() override = default;
  void SetUp() override {
    params_ = this->GetParam();
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src_ = reinterpret_cast<uint8_t *>(
        aom_memalign(16, 512 * 512 * sizeof(uint8_t)));
    ASSERT_NE(src_, nullptr);
  }

  void TearDown() override { aom_free(src_); }
  void RunTest(bool is_random);
  void RunSpeedTest();

  void GenRandomData(int width, int height, int stride) {
    const int msb = 7;  // Up to 8 bit input
    const int limit = 1 << (msb + 1);
    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        src_[ii * stride + jj] = rnd_(limit);
      }
    }
  }

  void GenExtremeData(int width, int height, int stride) {
    const int msb = 7;  // Up to 8 bit input
    const int limit = 1 << (msb + 1);
    const int val = rnd_(2) ? limit - 1 : 0;
    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        src_[ii * stride + jj] = val;
      }
    }
  }

 protected:
  TestFuncVar2D params_;
  uint8_t *src_;
  ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Lowbd2dVarTest);

void Lowbd2dVarTest::RunTest(bool is_random) {
  int failed = 0;
  for (int k = 0; k < kNumIterations; k++) {
    const int width = 4 * (rnd_(63) + 1);   // Up to 256x256
    const int height = 4 * (rnd_(63) + 1);  // Up to 256x256
    int stride = 4 << rnd_(8);              // Up to 512 stride
    while (stride < width) {                // Make sure it's valid
      stride = 4 << rnd_(8);
    }
    if (is_random) {
      GenRandomData(width, height, stride);
    } else {
      GenExtremeData(width, height, stride);
    }

    const uint64_t res_ref = params_.ref_func(src_, stride, width, height);
    uint64_t res_tst;
    API_REGISTER_STATE_CHECK(res_tst =
                                 params_.tst_func(src_, stride, width, height));

    if (!failed) {
      failed = res_ref != res_tst;
      EXPECT_EQ(res_ref, res_tst)
          << "Error: Sum Squares Test [" << width << "x" << height
          << "] C output does not match optimized output.";
    }
  }
}

void Lowbd2dVarTest::RunSpeedTest() {
  for (int block = 0; block < 2; block++) {
    const int width = test_block_size[block];
    const int height = test_block_size[block];
    int stride = 4 << rnd_(8);  // Up to 512 stride
    while (stride < width) {    // Make sure it's valid
      stride = 4 << rnd_(8);
    }
    GenExtremeData(width, height, stride);
    const int num_loops = 1000000000 / (width + height);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);

    for (int i = 0; i < num_loops; ++i)
      params_.ref_func(src_, stride, width, height);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);
    for (int i = 0; i < num_loops; ++i)
      params_.tst_func(src_, stride, width, height);
    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("%3dx%-3d: Scaling = %.2f\n", width, height,
           (double)elapsed_time / elapsed_time1);
  }
}

TEST_P(Lowbd2dVarTest, OperationCheck) {
  RunTest(true);  // GenRandomData
}

TEST_P(Lowbd2dVarTest, ExtremeValues) {
  RunTest(false);  // GenExtremeData
}

TEST_P(Lowbd2dVarTest, DISABLED_Speed) { RunSpeedTest(); }

#if HAVE_SSE2

INSTANTIATE_TEST_SUITE_P(SSE2, Lowbd2dVarTest,
                         ::testing::Values(TestFuncVar2D(&aom_var_2d_u8_c,
                                                         &aom_var_2d_u8_sse2)));

#endif  // HAVE_SSE2

#if HAVE_AVX2

INSTANTIATE_TEST_SUITE_P(AVX2, Lowbd2dVarTest,
                         ::testing::Values(TestFuncVar2D(&aom_var_2d_u8_c,
                                                         &aom_var_2d_u8_avx2)));

#endif  // HAVE_SSE2

#if HAVE_NEON

INSTANTIATE_TEST_SUITE_P(NEON, Lowbd2dVarTest,
                         ::testing::Values(TestFuncVar2D(&aom_var_2d_u8_c,
                                                         &aom_var_2d_u8_neon)));

#endif  // HAVE_NEON

#if HAVE_NEON_DOTPROD

INSTANTIATE_TEST_SUITE_P(NEON_DOTPROD, Lowbd2dVarTest,
                         ::testing::Values(TestFuncVar2D(
                             &aom_var_2d_u8_c, &aom_var_2d_u8_neon_dotprod)));

#endif  // HAVE_NEON_DOTPROD

class Highbd2dVarTest : public ::testing::TestWithParam<TestFuncVar2D> {
 public:
  ~Highbd2dVarTest() override = default;
  void SetUp() override {
    params_ = this->GetParam();
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, 512 * 512 * sizeof(uint16_t)));
    ASSERT_NE(src_, nullptr);
  }

  void TearDown() override { aom_free(src_); }
  void RunTest(bool is_random);
  void RunSpeedTest();

  void GenRandomData(int width, int height, int stride) {
    const int msb = 11;  // Up to 12 bit input
    const int limit = 1 << (msb + 1);
    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        src_[ii * stride + jj] = rnd_(limit);
      }
    }
  }

  void GenExtremeData(int width, int height, int stride) {
    const int msb = 11;  // Up to 12 bit input
    const int limit = 1 << (msb + 1);
    const int val = rnd_(2) ? limit - 1 : 0;
    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        src_[ii * stride + jj] = val;
      }
    }
  }

 protected:
  TestFuncVar2D params_;
  uint16_t *src_;
  ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(Highbd2dVarTest);

void Highbd2dVarTest::RunTest(bool is_random) {
  int failed = 0;
  for (int k = 0; k < kNumIterations; k++) {
    const int width = 4 * (rnd_(63) + 1);   // Up to 256x256
    const int height = 4 * (rnd_(63) + 1);  // Up to 256x256
    int stride = 4 << rnd_(8);              // Up to 512 stride
    while (stride < width) {                // Make sure it's valid
      stride = 4 << rnd_(8);
    }
    if (is_random) {
      GenRandomData(width, height, stride);
    } else {
      GenExtremeData(width, height, stride);
    }

    const uint64_t res_ref =
        params_.ref_func(CONVERT_TO_BYTEPTR(src_), stride, width, height);
    uint64_t res_tst;
    API_REGISTER_STATE_CHECK(
        res_tst =
            params_.tst_func(CONVERT_TO_BYTEPTR(src_), stride, width, height));

    if (!failed) {
      failed = res_ref != res_tst;
      EXPECT_EQ(res_ref, res_tst)
          << "Error: Sum Squares Test [" << width << "x" << height
          << "] C output does not match optimized output.";
    }
  }
}

void Highbd2dVarTest::RunSpeedTest() {
  for (int block = 0; block < 2; block++) {
    const int width = test_block_size[block];
    const int height = test_block_size[block];
    int stride = 4 << rnd_(8);  // Up to 512 stride
    while (stride < width) {    // Make sure it's valid
      stride = 4 << rnd_(8);
    }
    GenExtremeData(width, height, stride);
    const int num_loops = 1000000000 / (width + height);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);

    for (int i = 0; i < num_loops; ++i)
      params_.ref_func(CONVERT_TO_BYTEPTR(src_), stride, width, height);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);
    for (int i = 0; i < num_loops; ++i)
      params_.tst_func(CONVERT_TO_BYTEPTR(src_), stride, width, height);
    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("%3dx%-3d: Scaling = %.2f\n", width, height,
           (double)elapsed_time / elapsed_time1);
  }
}

TEST_P(Highbd2dVarTest, OperationCheck) {
  RunTest(true);  // GenRandomData
}

TEST_P(Highbd2dVarTest, ExtremeValues) {
  RunTest(false);  // GenExtremeData
}

TEST_P(Highbd2dVarTest, DISABLED_Speed) { RunSpeedTest(); }

#if HAVE_SSE2

INSTANTIATE_TEST_SUITE_P(
    SSE2, Highbd2dVarTest,
    ::testing::Values(TestFuncVar2D(&aom_var_2d_u16_c, &aom_var_2d_u16_sse2)));

#endif  // HAVE_SSE2

#if HAVE_AVX2

INSTANTIATE_TEST_SUITE_P(
    AVX2, Highbd2dVarTest,
    ::testing::Values(TestFuncVar2D(&aom_var_2d_u16_c, &aom_var_2d_u16_avx2)));

#endif  // HAVE_SSE2

#if HAVE_NEON

INSTANTIATE_TEST_SUITE_P(
    NEON, Highbd2dVarTest,
    ::testing::Values(TestFuncVar2D(&aom_var_2d_u16_c, &aom_var_2d_u16_neon)));

#endif  // HAVE_NEON
}  // namespace
