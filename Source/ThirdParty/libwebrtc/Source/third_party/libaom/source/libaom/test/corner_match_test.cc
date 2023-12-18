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

typedef double (*ComputeCrossCorrFunc)(const unsigned char *im1, int stride1,
                                       int x1, int y1, const unsigned char *im2,
                                       int stride2, int x2, int y2);

using std::make_tuple;
using std::tuple;
typedef tuple<int, ComputeCrossCorrFunc> CornerMatchParam;

class AV1CornerMatchTest : public ::testing::TestWithParam<CornerMatchParam> {
 public:
  ~AV1CornerMatchTest() override;
  void SetUp() override;

 protected:
  void RunCheckOutput(int run_times);
  ComputeCrossCorrFunc target_func;

  libaom_test::ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1CornerMatchTest);

AV1CornerMatchTest::~AV1CornerMatchTest() = default;
void AV1CornerMatchTest::SetUp() {
  rnd_.Reset(ACMRandom::DeterministicSeed());
  target_func = GET_PARAM(1);
}

void AV1CornerMatchTest::RunCheckOutput(int run_times) {
  const int w = 128, h = 128;
  const int num_iters = 10000;
  int i, j;
  aom_usec_timer ref_timer, test_timer;

  std::unique_ptr<uint8_t[]> input1(new (std::nothrow) uint8_t[w * h]);
  std::unique_ptr<uint8_t[]> input2(new (std::nothrow) uint8_t[w * h]);
  ASSERT_NE(input1, nullptr);
  ASSERT_NE(input2, nullptr);

  // Test the two extreme cases:
  // i) Random data, should have correlation close to 0
  // ii) Linearly related data + noise, should have correlation close to 1
  int mode = GET_PARAM(0);
  if (mode == 0) {
    for (i = 0; i < h; ++i)
      for (j = 0; j < w; ++j) {
        input1[i * w + j] = rnd_.Rand8();
        input2[i * w + j] = rnd_.Rand8();
      }
  } else if (mode == 1) {
    for (i = 0; i < h; ++i)
      for (j = 0; j < w; ++j) {
        int v = rnd_.Rand8();
        input1[i * w + j] = v;
        input2[i * w + j] = (v / 2) + (rnd_.Rand8() & 15);
      }
  }

  for (i = 0; i < num_iters; ++i) {
    int x1 = MATCH_SZ_BY2 + rnd_.PseudoUniform(w - 2 * MATCH_SZ_BY2);
    int y1 = MATCH_SZ_BY2 + rnd_.PseudoUniform(h - 2 * MATCH_SZ_BY2);
    int x2 = MATCH_SZ_BY2 + rnd_.PseudoUniform(w - 2 * MATCH_SZ_BY2);
    int y2 = MATCH_SZ_BY2 + rnd_.PseudoUniform(h - 2 * MATCH_SZ_BY2);

    double res_c = av1_compute_cross_correlation_c(input1.get(), w, x1, y1,
                                                   input2.get(), w, x2, y2);
    double res_simd =
        target_func(input1.get(), w, x1, y1, input2.get(), w, x2, y2);

    if (run_times > 1) {
      aom_usec_timer_start(&ref_timer);
      for (j = 0; j < run_times; j++) {
        av1_compute_cross_correlation_c(input1.get(), w, x1, y1, input2.get(),
                                        w, x2, y2);
      }
      aom_usec_timer_mark(&ref_timer);
      const int elapsed_time_c =
          static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

      aom_usec_timer_start(&test_timer);
      for (j = 0; j < run_times; j++) {
        target_func(input1.get(), w, x1, y1, input2.get(), w, x2, y2);
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
      ASSERT_EQ(res_simd, res_c);
    }
  }
}

TEST_P(AV1CornerMatchTest, CheckOutput) { RunCheckOutput(1); }
TEST_P(AV1CornerMatchTest, DISABLED_Speed) { RunCheckOutput(100000); }

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, AV1CornerMatchTest,
    ::testing::Values(make_tuple(0, &av1_compute_cross_correlation_sse4_1),
                      make_tuple(1, &av1_compute_cross_correlation_sse4_1)));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1CornerMatchTest,
    ::testing::Values(make_tuple(0, &av1_compute_cross_correlation_avx2),
                      make_tuple(1, &av1_compute_cross_correlation_avx2)));
#endif
}  // namespace AV1CornerMatch

}  // namespace test_libaom
