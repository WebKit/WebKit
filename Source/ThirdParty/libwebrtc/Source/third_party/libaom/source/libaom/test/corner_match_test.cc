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
#include <memory>
#include <new>
#include <tuple>

#include "config/aom_dsp_rtcd.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "test/register_state_check.h"

#include "aom_dsp/flow_estimation/corner_match.h"

namespace test_libaom {

namespace AV1CornerMatch {

using libaom_test::ACMRandom;

typedef bool (*ComputeMeanStddevFunc)(const unsigned char *frame, int stride,
                                      int x, int y, double *mean,
                                      double *one_over_stddev);
typedef double (*ComputeCorrFunc)(const unsigned char *frame1, int stride1,
                                  int x1, int y1, double mean1,
                                  double one_over_stddev1,
                                  const unsigned char *frame2, int stride2,
                                  int x2, int y2, double mean2,
                                  double one_over_stddev2);

using std::make_tuple;
using std::tuple;
typedef tuple<int, ComputeMeanStddevFunc, ComputeCorrFunc> CornerMatchParam;

class AV1CornerMatchTest : public ::testing::TestWithParam<CornerMatchParam> {
 public:
  ~AV1CornerMatchTest() override;
  void SetUp() override;

 protected:
  void GenerateInput(uint8_t *input1, uint8_t *input2, int w, int h, int mode);
  void RunCheckOutput();
  void RunSpeedTest();
  ComputeMeanStddevFunc target_compute_mean_stddev_func;
  ComputeCorrFunc target_compute_corr_func;

  libaom_test::ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1CornerMatchTest);

AV1CornerMatchTest::~AV1CornerMatchTest() = default;
void AV1CornerMatchTest::SetUp() {
  rnd_.Reset(ACMRandom::DeterministicSeed());
  target_compute_mean_stddev_func = GET_PARAM(1);
  target_compute_corr_func = GET_PARAM(2);
}

void AV1CornerMatchTest::GenerateInput(uint8_t *input1, uint8_t *input2, int w,
                                       int h, int mode) {
  if (mode == 0) {
    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        input1[i * w + j] = rnd_.Rand8();
        input2[i * w + j] = rnd_.Rand8();
      }
  } else if (mode == 1) {
    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        int v = rnd_.Rand8();
        input1[i * w + j] = v;
        input2[i * w + j] = (v / 2) + (rnd_.Rand8() & 15);
      }
  }
}

void AV1CornerMatchTest::RunCheckOutput() {
  const int w = 128, h = 128;
  const int num_iters = 1000;

  std::unique_ptr<uint8_t[]> input1(new (std::nothrow) uint8_t[w * h]);
  std::unique_ptr<uint8_t[]> input2(new (std::nothrow) uint8_t[w * h]);
  ASSERT_NE(input1, nullptr);
  ASSERT_NE(input2, nullptr);

  // Test the two extreme cases:
  // i) Random data, should have correlation close to 0
  // ii) Linearly related data + noise, should have correlation close to 1
  int mode = GET_PARAM(0);
  GenerateInput(&input1[0], &input2[0], w, h, mode);

  for (int i = 0; i < num_iters; ++i) {
    int x1 = MATCH_SZ_BY2 + rnd_.PseudoUniform(w + 1 - MATCH_SZ);
    int y1 = MATCH_SZ_BY2 + rnd_.PseudoUniform(h + 1 - MATCH_SZ);
    int x2 = MATCH_SZ_BY2 + rnd_.PseudoUniform(w + 1 - MATCH_SZ);
    int y2 = MATCH_SZ_BY2 + rnd_.PseudoUniform(h + 1 - MATCH_SZ);

    double c_mean1, c_one_over_stddev1, c_mean2, c_one_over_stddev2;
    bool c_valid1 = aom_compute_mean_stddev_c(input1.get(), w, x1, y1, &c_mean1,
                                              &c_one_over_stddev1);
    bool c_valid2 = aom_compute_mean_stddev_c(input2.get(), w, x2, y2, &c_mean2,
                                              &c_one_over_stddev2);

    double simd_mean1, simd_one_over_stddev1, simd_mean2, simd_one_over_stddev2;
    bool simd_valid1 = target_compute_mean_stddev_func(
        input1.get(), w, x1, y1, &simd_mean1, &simd_one_over_stddev1);
    bool simd_valid2 = target_compute_mean_stddev_func(
        input2.get(), w, x2, y2, &simd_mean2, &simd_one_over_stddev2);

    // Run the correlation calculation even if one of the "valid" flags is
    // false, i.e. if one of the patches doesn't have enough variance. This is
    // safe because any potential division by 0 is caught in
    // aom_compute_mean_stddev(), and one_over_stddev is set to 0 instead.
    // This causes aom_compute_correlation() to return 0, without causing a
    // division by 0.
    const double c_corr = aom_compute_correlation_c(
        input1.get(), w, x1, y1, c_mean1, c_one_over_stddev1, input2.get(), w,
        x2, y2, c_mean2, c_one_over_stddev2);
    const double simd_corr = target_compute_corr_func(
        input1.get(), w, x1, y1, c_mean1, c_one_over_stddev1, input2.get(), w,
        x2, y2, c_mean2, c_one_over_stddev2);

    ASSERT_EQ(simd_valid1, c_valid1);
    ASSERT_EQ(simd_valid2, c_valid2);
    ASSERT_EQ(simd_mean1, c_mean1);
    ASSERT_EQ(simd_one_over_stddev1, c_one_over_stddev1);
    ASSERT_EQ(simd_mean2, c_mean2);
    ASSERT_EQ(simd_one_over_stddev2, c_one_over_stddev2);
    ASSERT_EQ(simd_corr, c_corr);
  }
}

void AV1CornerMatchTest::RunSpeedTest() {
  const int w = 16, h = 16;
  const int num_iters = 1000000;
  aom_usec_timer ref_timer, test_timer;

  std::unique_ptr<uint8_t[]> input1(new (std::nothrow) uint8_t[w * h]);
  std::unique_ptr<uint8_t[]> input2(new (std::nothrow) uint8_t[w * h]);
  ASSERT_NE(input1, nullptr);
  ASSERT_NE(input2, nullptr);

  // Test the two extreme cases:
  // i) Random data, should have correlation close to 0
  // ii) Linearly related data + noise, should have correlation close to 1
  int mode = GET_PARAM(0);
  GenerateInput(&input1[0], &input2[0], w, h, mode);

  // Time aom_compute_mean_stddev()
  double c_mean1, c_one_over_stddev1, c_mean2, c_one_over_stddev2;
  aom_usec_timer_start(&ref_timer);
  for (int i = 0; i < num_iters; i++) {
    aom_compute_mean_stddev_c(input1.get(), w, 0, 0, &c_mean1,
                              &c_one_over_stddev1);
    aom_compute_mean_stddev_c(input2.get(), w, 0, 0, &c_mean2,
                              &c_one_over_stddev2);
  }
  aom_usec_timer_mark(&ref_timer);
  int elapsed_time_c = static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

  double simd_mean1, simd_one_over_stddev1, simd_mean2, simd_one_over_stddev2;
  aom_usec_timer_start(&test_timer);
  for (int i = 0; i < num_iters; i++) {
    target_compute_mean_stddev_func(input1.get(), w, 0, 0, &simd_mean1,
                                    &simd_one_over_stddev1);
    target_compute_mean_stddev_func(input2.get(), w, 0, 0, &simd_mean2,
                                    &simd_one_over_stddev2);
  }
  aom_usec_timer_mark(&test_timer);
  int elapsed_time_simd = static_cast<int>(aom_usec_timer_elapsed(&test_timer));

  printf(
      "aom_compute_mean_stddev(): c_time=%6d   simd_time=%6d   "
      "gain=%.3f\n",
      elapsed_time_c, elapsed_time_simd,
      (elapsed_time_c / (double)elapsed_time_simd));

  // Time aom_compute_correlation
  aom_usec_timer_start(&ref_timer);
  for (int i = 0; i < num_iters; i++) {
    aom_compute_correlation_c(input1.get(), w, 0, 0, c_mean1,
                              c_one_over_stddev1, input2.get(), w, 0, 0,
                              c_mean2, c_one_over_stddev2);
  }
  aom_usec_timer_mark(&ref_timer);
  elapsed_time_c = static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

  aom_usec_timer_start(&test_timer);
  for (int i = 0; i < num_iters; i++) {
    target_compute_corr_func(input1.get(), w, 0, 0, c_mean1, c_one_over_stddev1,
                             input2.get(), w, 0, 0, c_mean2,
                             c_one_over_stddev2);
  }
  aom_usec_timer_mark(&test_timer);
  elapsed_time_simd = static_cast<int>(aom_usec_timer_elapsed(&test_timer));

  printf(
      "aom_compute_correlation(): c_time=%6d   simd_time=%6d   "
      "gain=%.3f\n",
      elapsed_time_c, elapsed_time_simd,
      (elapsed_time_c / (double)elapsed_time_simd));
}

TEST_P(AV1CornerMatchTest, CheckOutput) { RunCheckOutput(); }
TEST_P(AV1CornerMatchTest, DISABLED_Speed) { RunSpeedTest(); }

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, AV1CornerMatchTest,
    ::testing::Values(make_tuple(0, &aom_compute_mean_stddev_sse4_1,
                                 &aom_compute_correlation_sse4_1),
                      make_tuple(1, &aom_compute_mean_stddev_sse4_1,
                                 &aom_compute_correlation_sse4_1)));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1CornerMatchTest,
    ::testing::Values(make_tuple(0, &aom_compute_mean_stddev_avx2,
                                 &aom_compute_correlation_avx2),
                      make_tuple(1, &aom_compute_mean_stddev_avx2,
                                 &aom_compute_correlation_avx2)));
#endif
}  // namespace AV1CornerMatch

}  // namespace test_libaom
