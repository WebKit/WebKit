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
  virtual ~SumSquaresTest() {}
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
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SumSquaresTest);

void SumSquaresTest::RunTest(int isRandom) {
  int failed = 0;
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
  RunTest(1);  // GenRandomData
}

TEST_P(SumSquaresTest, ExtremeValues) {
  RunTest(0);  // GenExtremeData
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

typedef uint64_t (*F1D)(const int16_t *src, uint32_t N);
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

    const int N = rng_(2) ? rng_(kMaxSize * kMaxSize + 1 - kMaxSize) + kMaxSize
                          : rng_(kMaxSize) + 1;

    const uint64_t ref_res = params_.ref_func(src, N);
    uint64_t tst_res;
    API_REGISTER_STATE_CHECK(tst_res = params_.tst_func(src, N));

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

    const int N = rng_(2) ? rng_(kMaxSize * kMaxSize + 1 - kMaxSize) + kMaxSize
                          : rng_(kMaxSize) + 1;

    const uint64_t ref_res = params_.ref_func(src, N);
    uint64_t tst_res;
    API_REGISTER_STATE_CHECK(tst_res = params_.tst_func(src, N));

    ASSERT_EQ(ref_res, tst_res);
  }
}

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, SumSquares1DTest,
                         ::testing::Values(TestFuncs1D(
                             aom_sum_squares_i16_c, aom_sum_squares_i16_sse2)));

#endif  // HAVE_SSE2

typedef int64_t (*sse_func)(const uint8_t *a, int a_stride, const uint8_t *b,
                            int b_stride, int width, int height);
typedef libaom_test::FuncParam<sse_func> TestSSEFuncs;

typedef std::tuple<TestSSEFuncs, int> SSETestParam;

class SSETest : public ::testing::TestWithParam<SSETestParam> {
 public:
  virtual ~SSETest() {}
  virtual void SetUp() {
    params_ = GET_PARAM(0);
    width_ = GET_PARAM(1);
    isHbd_ =
#if CONFIG_AV1_HIGHBITDEPTH
        params_.ref_func == aom_highbd_sse_c;
#else
        0;
#endif
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src_ = reinterpret_cast<uint8_t *>(aom_memalign(32, 256 * 256 * 2));
    ref_ = reinterpret_cast<uint8_t *>(aom_memalign(32, 256 * 256 * 2));
    ASSERT_NE(src_, nullptr);
    ASSERT_NE(ref_, nullptr);
  }

  virtual void TearDown() {
    aom_free(src_);
    aom_free(ref_);
  }
  void RunTest(int isRandom, int width, int height, int run_times);

  void GenRandomData(int width, int height, int stride) {
    uint16_t *pSrc = (uint16_t *)src_;
    uint16_t *pRef = (uint16_t *)ref_;
    const int msb = 11;  // Up to 12 bit input
    const int limit = 1 << (msb + 1);
    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        if (!isHbd_) {
          src_[ii * stride + jj] = rnd_.Rand8();
          ref_[ii * stride + jj] = rnd_.Rand8();
        } else {
          pSrc[ii * stride + jj] = rnd_(limit);
          pRef[ii * stride + jj] = rnd_(limit);
        }
      }
    }
  }

  void GenExtremeData(int width, int height, int stride, uint8_t *data,
                      int16_t val) {
    uint16_t *pData = (uint16_t *)data;
    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        if (!isHbd_) {
          data[ii * stride + jj] = (uint8_t)val;
        } else {
          pData[ii * stride + jj] = val;
        }
      }
    }
  }

 protected:
  int isHbd_;
  int width_;
  TestSSEFuncs params_;
  uint8_t *src_;
  uint8_t *ref_;
  ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SSETest);

void SSETest::RunTest(int isRandom, int width, int height, int run_times) {
  int failed = 0;
  aom_usec_timer ref_timer, test_timer;
  for (int k = 0; k < 3; k++) {
    int stride = 4 << rnd_(7);  // Up to 256 stride
    while (stride < width) {    // Make sure it's valid
      stride = 4 << rnd_(7);
    }
    if (isRandom) {
      GenRandomData(width, height, stride);
    } else {
      const int msb = isHbd_ ? 12 : 8;  // Up to 12 bit input
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
    uint8_t *pSrc = src_;
    uint8_t *pRef = ref_;
    if (isHbd_) {
      pSrc = CONVERT_TO_BYTEPTR(src_);
      pRef = CONVERT_TO_BYTEPTR(ref_);
    }
    res_ref = params_.ref_func(pSrc, stride, pRef, stride, width, height);
    res_tst = params_.tst_func(pSrc, stride, pRef, stride, width, height);
    if (run_times > 1) {
      aom_usec_timer_start(&ref_timer);
      for (int j = 0; j < run_times; j++) {
        params_.ref_func(pSrc, stride, pRef, stride, width, height);
      }
      aom_usec_timer_mark(&ref_timer);
      const int elapsed_time_c =
          static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

      aom_usec_timer_start(&test_timer);
      for (int j = 0; j < run_times; j++) {
        params_.tst_func(pSrc, stride, pRef, stride, width, height);
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
            << "Error:" << (isHbd_ ? "hbd " : " ") << k << " SSE Test ["
            << width << "x" << height
            << "] C output does not match optimized output.";
      }
    }
  }
}

TEST_P(SSETest, OperationCheck) {
  for (int height = 4; height <= 128; height += 4) {
    RunTest(1, width_, height, 1);  // GenRandomData
  }
}

TEST_P(SSETest, ExtremeValues) {
  for (int height = 4; height <= 128; height += 4) {
    RunTest(0, width_, height, 1);
  }
}

TEST_P(SSETest, DISABLED_Speed) {
  for (int height = 4; height <= 128; height += 4) {
    RunTest(1, width_, height, 100);
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

typedef std::tuple<TestSSE_SumFuncs, int> SSE_SumTestParam;

class SSE_Sum_Test : public ::testing::TestWithParam<SSE_SumTestParam> {
 public:
  virtual ~SSE_Sum_Test() {}
  virtual void SetUp() {
    params_ = GET_PARAM(0);
    width_ = GET_PARAM(1);
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src_ = reinterpret_cast<int16_t *>(aom_memalign(32, 256 * 256 * 2));
    ASSERT_NE(src_, nullptr);
  }

  virtual void TearDown() { aom_free(src_); }
  void RunTest(int isRandom, int width, int height, int run_times);

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
  int width_;
  TestSSE_SumFuncs params_;
  int16_t *src_;
  ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SSE_Sum_Test);

void SSE_Sum_Test::RunTest(int isRandom, int width, int height, int run_times) {
  aom_usec_timer ref_timer, test_timer;
  for (int k = 0; k < 3; k++) {
    int stride = 4 << rnd_(7);  // Up to 256 stride
    while (stride < width) {    // Make sure it's valid
      stride = 4 << rnd_(7);
    }
    if (isRandom) {
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
  for (int height = 4; height <= 64; height = height * 2) {
    RunTest(1, width_, height, 1);  // GenRandomData
  }
}

TEST_P(SSE_Sum_Test, ExtremeValues) {
  for (int height = 4; height <= 64; height = height * 2) {
    RunTest(0, width_, height, 1);
  }
}

TEST_P(SSE_Sum_Test, DISABLED_Speed) {
  for (int height = 4; height <= 64; height = height * 2) {
    RunTest(1, width_, height, 10000);
  }
}

#if HAVE_SSE2
TestSSE_SumFuncs sse_sum_sse2[] = { TestSSE_SumFuncs(
    &aom_get_blk_sse_sum_c, &aom_get_blk_sse_sum_sse2) };
INSTANTIATE_TEST_SUITE_P(SSE2, SSE_Sum_Test,
                         Combine(ValuesIn(sse_sum_sse2), Range(4, 65, 4)));
#endif  // HAVE_SSE2

#if HAVE_AVX2
TestSSE_SumFuncs sse_sum_avx2[] = { TestSSE_SumFuncs(
    &aom_get_blk_sse_sum_c, &aom_get_blk_sse_sum_avx2) };
INSTANTIATE_TEST_SUITE_P(AVX2, SSE_Sum_Test,
                         Combine(ValuesIn(sse_sum_avx2), Range(4, 65, 4)));
#endif  // HAVE_AVX2

//////////////////////////////////////////////////////////////////////////////
// 2D Variance test functions
//////////////////////////////////////////////////////////////////////////////

typedef uint64_t (*Var2DFunc)(uint8_t *src, int stride, int width, int height);
typedef libaom_test::FuncParam<Var2DFunc> TestFuncVar2D;

const uint16_t test_block_size[2] = { 128, 256 };

class Lowbd2dVarTest : public ::testing::TestWithParam<TestFuncVar2D> {
 public:
  virtual ~Lowbd2dVarTest() {}
  virtual void SetUp() {
    params_ = this->GetParam();
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src_ = reinterpret_cast<uint8_t *>(
        aom_memalign(16, 512 * 512 * sizeof(uint8_t)));
    ASSERT_NE(src_, nullptr);
  }

  virtual void TearDown() { aom_free(src_); }
  void RunTest(int isRandom);
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

void Lowbd2dVarTest::RunTest(int isRandom) {
  int failed = 0;
  for (int k = 0; k < kNumIterations; k++) {
    const int width = 4 * (rnd_(63) + 1);   // Up to 256x256
    const int height = 4 * (rnd_(63) + 1);  // Up to 256x256
    int stride = 4 << rnd_(8);              // Up to 512 stride
    while (stride < width) {                // Make sure it's valid
      stride = 4 << rnd_(8);
    }
    if (isRandom) {
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
  RunTest(1);  // GenRandomData
}

TEST_P(Lowbd2dVarTest, ExtremeValues) {
  RunTest(0);  // GenExtremeData
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

class Highbd2dVarTest : public ::testing::TestWithParam<TestFuncVar2D> {
 public:
  virtual ~Highbd2dVarTest() {}
  virtual void SetUp() {
    params_ = this->GetParam();
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, 512 * 512 * sizeof(uint16_t)));
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

void Highbd2dVarTest::RunTest(int isRandom) {
  int failed = 0;
  for (int k = 0; k < kNumIterations; k++) {
    const int width = 4 * (rnd_(63) + 1);   // Up to 256x256
    const int height = 4 * (rnd_(63) + 1);  // Up to 256x256
    int stride = 4 << rnd_(8);              // Up to 512 stride
    while (stride < width) {                // Make sure it's valid
      stride = 4 << rnd_(8);
    }
    if (isRandom) {
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
  RunTest(1);  // GenRandomData
}

TEST_P(Highbd2dVarTest, ExtremeValues) {
  RunTest(0);  // GenExtremeData
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
}  // namespace
