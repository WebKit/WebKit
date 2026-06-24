/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cstdio>
#include <cstdlib>

#include "gtest/gtest.h"

#include "config/av1_rtcd.h"
#include "aom_ports/aom_timer.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"

namespace {

using InterpCubicRateDistFunc = void (*)(const double *p1, const double *p2,
                                         double x, double rate_dist_f[2]);

class InterpCubicTest
    : public ::testing::TestWithParam<InterpCubicRateDistFunc> {
 public:
  InterpCubicTest()
      : target_func_(GetParam()),
        rnd_(libaom_test::ACMRandom::DeterministicSeed()) {}
  double GenerateRandomDouble(double min, double max) {
    return min + (static_cast<double>(rnd_.Rand31()) / ((1U << 31) - 1)) *
                     (max - min);
  }

 protected:
  void CheckOutput();
  void SpeedTest();
  InterpCubicRateDistFunc target_func_;
  libaom_test::ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(InterpCubicTest);

#if AOM_ARCH_X86 || AOM_ARCH_X86_64
/* On x86, disable the x87 unit's internal 80 bit precision for better
 * consistency with the SSE unit's 64 bit precision.
 */
#include "aom_ports/x86.h"
#define FLOATING_POINT_SET_PRECISION \
  unsigned short x87_orig_mode = x87_set_double_precision();
#define FLOATING_POINT_RESTORE_PRECISION x87_set_control_word(x87_orig_mode);
#else
#define FLOATING_POINT_SET_PRECISION
#define FLOATING_POINT_RESTORE_PRECISION
#endif  // AOM_ARCH_X86 || AOM_ARCH_X86_64

void InterpCubicTest::CheckOutput() {
  double p1[4], p2[4], out_ref[2], out_mod[2];
  constexpr int kNumIters = 10000;
  for (int iter = 0; iter < kNumIters; ++iter) {
    for (int i = 0; i < 4; ++i) {
      p1[i] = GenerateRandomDouble(0.0000, 4096.000000);
      p2[i] = GenerateRandomDouble(0.0000, 16.0000);
    }
    const double x = GenerateRandomDouble(0.0000, 1.0000);

    FLOATING_POINT_SET_PRECISION
    av1_interp_cubic_rate_dist_c(p1, p2, x, out_ref);
    FLOATING_POINT_RESTORE_PRECISION

    API_REGISTER_STATE_CHECK(target_func_(p1, p2, x, out_mod));

    EXPECT_EQ(out_ref[0], out_mod[0]) << "Error: rate_f value mismatch";
    EXPECT_EQ(out_ref[1], out_mod[1]) << "Error: distbysse_f value mismatch";
  }
}

void InterpCubicTest::SpeedTest() {
  double p1[4], p2[4], out_ref[2], out_mod[2];

  for (int i = 0; i < 4; ++i) {
    p1[i] = GenerateRandomDouble(0.0000, 4096.0000);
    p2[i] = GenerateRandomDouble(0.0000, 16.0000);
  }
  const double x = GenerateRandomDouble(0.0000, 1.0000);

  constexpr int kNumIters = 100000000;

  aom_usec_timer ref_timer, test_timer;
  FLOATING_POINT_SET_PRECISION
  aom_usec_timer_start(&ref_timer);
  for (int iter = 0; iter < kNumIters; ++iter) {
    av1_interp_cubic_rate_dist_c(p1, p2, x, out_ref);
  }
  aom_usec_timer_mark(&ref_timer);
  FLOATING_POINT_RESTORE_PRECISION
  const int elapsed_time_c =
      static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

  aom_usec_timer_start(&test_timer);
  for (int iter = 0; iter < kNumIters; ++iter) {
    API_REGISTER_STATE_CHECK(target_func_(p1, p2, x, out_mod));
  }
  aom_usec_timer_mark(&test_timer);
  const int elapsed_time_simd =
      static_cast<int>(aom_usec_timer_elapsed(&test_timer));

  printf(
      "c_time=%d \t simd_time=%d \t "
      "Scaling=%lf \n",
      elapsed_time_c, elapsed_time_simd,
      (static_cast<double>(elapsed_time_c) / elapsed_time_simd));
}

TEST_P(InterpCubicTest, CheckOutput) { CheckOutput(); }

TEST_P(InterpCubicTest, DISABLED_Speed) { SpeedTest(); }

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, InterpCubicTest,
                         ::testing::Values(av1_interp_cubic_rate_dist_sse2));
#endif  // HAVE_SSE2

}  // namespace
