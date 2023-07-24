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

#include <cstdlib>
#include <new>
#include <tuple>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom/aom_codec.h"
#include "aom/aom_integer.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "test/acm_random.h"
#include "av1/encoder/palette.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace AV1Kmeans {
typedef void (*av1_calc_indices_dim1_func)(const int16_t *data,
                                           const int16_t *centroids,
                                           uint8_t *indices,
                                           int64_t *total_dist, int n, int k);
typedef void (*av1_calc_indices_dim2_func)(const int16_t *data,
                                           const int16_t *centroids,
                                           uint8_t *indices,
                                           int64_t *total_dist, int n, int k);

typedef std::tuple<av1_calc_indices_dim1_func, BLOCK_SIZE>
    av1_calc_indices_dim1Param;

typedef std::tuple<av1_calc_indices_dim2_func, BLOCK_SIZE>
    av1_calc_indices_dim2Param;

class AV1KmeansTest1
    : public ::testing::TestWithParam<av1_calc_indices_dim1Param> {
 public:
  ~AV1KmeansTest1();
  void SetUp();

  void TearDown();

 protected:
  void RunCheckOutput(av1_calc_indices_dim1_func test_impl, BLOCK_SIZE bsize,
                      int centroids);
  void RunSpeedTest(av1_calc_indices_dim1_func test_impl, BLOCK_SIZE bsize,
                    int centroids);
  bool CheckResult(int n) {
    for (int idx = 0; idx < n; ++idx) {
      if (indices1_[idx] != indices2_[idx]) {
        printf("%d ", idx);
        printf("%d != %d ", indices1_[idx], indices2_[idx]);
        return false;
      }
    }
    return true;
  }

  libaom_test::ACMRandom rnd_;
  int16_t data_[4096];
  int16_t centroids_[8];
  uint8_t indices1_[4096];
  uint8_t indices2_[4096];
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1KmeansTest1);

AV1KmeansTest1::~AV1KmeansTest1() {}

void AV1KmeansTest1::SetUp() {
  rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());
  for (int i = 0; i < 4096; ++i) {
    data_[i] = (int)rnd_.Rand8() << 4;
  }
  for (int i = 0; i < 8; i++) {
    centroids_[i] = (int)rnd_.Rand8() << 4;
  }
}

void AV1KmeansTest1::TearDown() {}

void AV1KmeansTest1::RunCheckOutput(av1_calc_indices_dim1_func test_impl,
                                    BLOCK_SIZE bsize, int k) {
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int n = w * h;
  int64_t total_dist_dim1, total_dist_impl;
  av1_calc_indices_dim1_c(data_, centroids_, indices1_, &total_dist_dim1, n, k);
  test_impl(data_, centroids_, indices2_, &total_dist_impl, n, k);

  ASSERT_EQ(total_dist_dim1, total_dist_impl);
  ASSERT_EQ(CheckResult(n), true)
      << " block " << bsize << " index " << n << " Centroids " << k;
}

void AV1KmeansTest1::RunSpeedTest(av1_calc_indices_dim1_func test_impl,
                                  BLOCK_SIZE bsize, int k) {
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int n = w * h;
  const int num_loops = 1000000000 / n;

  av1_calc_indices_dim1_func funcs[2] = { av1_calc_indices_dim1_c, test_impl };
  double elapsed_time[2] = { 0 };
  for (int i = 0; i < 2; ++i) {
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    av1_calc_indices_dim1_func func = funcs[i];
    for (int j = 0; j < num_loops; ++j) {
      func(data_, centroids_, indices1_, /*total_dist=*/nullptr, n, k);
    }
    aom_usec_timer_mark(&timer);
    double time = static_cast<double>(aom_usec_timer_elapsed(&timer));
    elapsed_time[i] = 1000.0 * time / num_loops;
  }
  printf("av1_calc_indices_dim1 indices= %d centroids=%d: %7.2f/%7.2fns", n, k,
         elapsed_time[0], elapsed_time[1]);
  printf("(%3.2f)\n", elapsed_time[0] / elapsed_time[1]);
}

TEST_P(AV1KmeansTest1, CheckOutput) {
  // centroids = 2..8
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 2);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 3);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 4);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 5);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 6);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 7);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 8);
}

TEST_P(AV1KmeansTest1, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 2);
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 3);
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 4);
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 5);
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 6);
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 7);
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 8);
}

class AV1KmeansTest2
    : public ::testing::TestWithParam<av1_calc_indices_dim2Param> {
 public:
  ~AV1KmeansTest2();
  void SetUp();

  void TearDown();

 protected:
  void RunCheckOutput(av1_calc_indices_dim2_func test_impl, BLOCK_SIZE bsize,
                      int centroids);
  void RunSpeedTest(av1_calc_indices_dim2_func test_impl, BLOCK_SIZE bsize,
                    int centroids);
  bool CheckResult(int n) {
    bool flag = true;
    for (int idx = 0; idx < n; ++idx) {
      if (indices1_[idx] != indices2_[idx]) {
        printf("%d ", idx);
        printf("%d != %d ", indices1_[idx], indices2_[idx]);
        flag = false;
      }
    }
    if (flag == false) {
      return false;
    }
    return true;
  }

  libaom_test::ACMRandom rnd_;
  int16_t data_[4096 * 2];
  int16_t centroids_[8 * 2];
  uint8_t indices1_[4096];
  uint8_t indices2_[4096];
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1KmeansTest2);

AV1KmeansTest2::~AV1KmeansTest2() {}

void AV1KmeansTest2::SetUp() {
  rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());
  for (int i = 0; i < 4096 * 2; ++i) {
    data_[i] = (int)rnd_.Rand8();
  }
  for (int i = 0; i < 8 * 2; i++) {
    centroids_[i] = (int)rnd_.Rand8();
  }
}

void AV1KmeansTest2::TearDown() {}

void AV1KmeansTest2::RunCheckOutput(av1_calc_indices_dim2_func test_impl,
                                    BLOCK_SIZE bsize, int k) {
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int n = w * h;
  int64_t total_dist_dim2, total_dist_impl;
  av1_calc_indices_dim2_c(data_, centroids_, indices1_, &total_dist_dim2, n, k);
  test_impl(data_, centroids_, indices2_, &total_dist_impl, n, k);

  ASSERT_EQ(total_dist_dim2, total_dist_impl);
  ASSERT_EQ(CheckResult(n), true)
      << " block " << bsize << " index " << n << " Centroids " << k;
}

void AV1KmeansTest2::RunSpeedTest(av1_calc_indices_dim2_func test_impl,
                                  BLOCK_SIZE bsize, int k) {
  const int w = block_size_wide[bsize];
  const int h = block_size_high[bsize];
  const int n = w * h;
  const int num_loops = 1000000000 / n;

  av1_calc_indices_dim2_func funcs[2] = { av1_calc_indices_dim2_c, test_impl };
  double elapsed_time[2] = { 0 };
  for (int i = 0; i < 2; ++i) {
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    av1_calc_indices_dim2_func func = funcs[i];
    for (int j = 0; j < num_loops; ++j) {
      func(data_, centroids_, indices1_, /*total_dist=*/nullptr, n, k);
    }
    aom_usec_timer_mark(&timer);
    double time = static_cast<double>(aom_usec_timer_elapsed(&timer));
    elapsed_time[i] = 1000.0 * time / num_loops;
  }
  printf("av1_calc_indices_dim2 indices= %d centroids=%d: %7.2f/%7.2fns", n, k,
         elapsed_time[0], elapsed_time[1]);
  printf("(%3.2f)\n", elapsed_time[0] / elapsed_time[1]);
}

TEST_P(AV1KmeansTest2, CheckOutput) {
  // centroids = 2..8
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 2);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 3);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 4);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 5);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 6);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 7);
  RunCheckOutput(GET_PARAM(0), GET_PARAM(1), 8);
}

TEST_P(AV1KmeansTest2, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 2);
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 3);
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 4);
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 5);
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 6);
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 7);
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), 8);
}

#if HAVE_SSE2 || HAVE_AVX2 || HAVE_NEON
const BLOCK_SIZE kValidBlockSize[] = { BLOCK_8X8,   BLOCK_8X16,  BLOCK_8X32,
                                       BLOCK_16X8,  BLOCK_16X16, BLOCK_16X32,
                                       BLOCK_32X8,  BLOCK_32X16, BLOCK_32X32,
                                       BLOCK_32X64, BLOCK_64X32, BLOCK_64X64,
                                       BLOCK_16X64, BLOCK_64X16 };
#endif

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, AV1KmeansTest1,
    ::testing::Combine(::testing::Values(&av1_calc_indices_dim1_sse2),
                       ::testing::ValuesIn(kValidBlockSize)));
INSTANTIATE_TEST_SUITE_P(
    SSE2, AV1KmeansTest2,
    ::testing::Combine(::testing::Values(&av1_calc_indices_dim2_sse2),
                       ::testing::ValuesIn(kValidBlockSize)));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1KmeansTest1,
    ::testing::Combine(::testing::Values(&av1_calc_indices_dim1_avx2),
                       ::testing::ValuesIn(kValidBlockSize)));
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1KmeansTest2,
    ::testing::Combine(::testing::Values(&av1_calc_indices_dim2_avx2),
                       ::testing::ValuesIn(kValidBlockSize)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1KmeansTest1,
    ::testing::Combine(::testing::Values(&av1_calc_indices_dim1_neon),
                       ::testing::ValuesIn(kValidBlockSize)));
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1KmeansTest2,
    ::testing::Combine(::testing::Values(&av1_calc_indices_dim2_neon),
                       ::testing::ValuesIn(kValidBlockSize)));
#endif

}  // namespace AV1Kmeans
