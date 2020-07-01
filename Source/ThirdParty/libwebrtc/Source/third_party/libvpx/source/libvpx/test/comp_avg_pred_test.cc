/*
 *  Copyright (c) 2017 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vpx_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/buffer.h"
#include "test/register_state_check.h"
#include "vpx_ports/vpx_timer.h"

namespace {

using ::libvpx_test::ACMRandom;
using ::libvpx_test::Buffer;

typedef void (*AvgPredFunc)(uint8_t *a, const uint8_t *b, int w, int h,
                            const uint8_t *c, int c_stride);

uint8_t avg_with_rounding(uint8_t a, uint8_t b) { return (a + b + 1) >> 1; }

void reference_pred(const Buffer<uint8_t> &pred, const Buffer<uint8_t> &ref,
                    int width, int height, Buffer<uint8_t> *avg) {
  ASSERT_TRUE(avg->TopLeftPixel() != NULL);
  ASSERT_TRUE(pred.TopLeftPixel() != NULL);
  ASSERT_TRUE(ref.TopLeftPixel() != NULL);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      avg->TopLeftPixel()[y * avg->stride() + x] =
          avg_with_rounding(pred.TopLeftPixel()[y * pred.stride() + x],
                            ref.TopLeftPixel()[y * ref.stride() + x]);
    }
  }
}

class AvgPredTest : public ::testing::TestWithParam<AvgPredFunc> {
 public:
  virtual void SetUp() {
    avg_pred_func_ = GetParam();
    rnd_.Reset(ACMRandom::DeterministicSeed());
  }

 protected:
  AvgPredFunc avg_pred_func_;
  ACMRandom rnd_;
};

TEST_P(AvgPredTest, SizeCombinations) {
  // This is called as part of the sub pixel variance. As such it must be one of
  // the variance block sizes.

  for (int width_pow = 2; width_pow <= 6; ++width_pow) {
    for (int height_pow = width_pow - 1; height_pow <= width_pow + 1;
         ++height_pow) {
      // Don't test 4x2 or 64x128
      if (height_pow == 1 || height_pow == 7) continue;

      // The sse2 special-cases when ref width == stride, so make sure to test
      // it.
      for (int ref_padding = 0; ref_padding < 2; ref_padding++) {
        const int width = 1 << width_pow;
        const int height = 1 << height_pow;
        // Only the reference buffer may have a stride not equal to width.
        Buffer<uint8_t> ref =
            Buffer<uint8_t>(width, height, ref_padding ? 8 : 0);
        ASSERT_TRUE(ref.Init());
        Buffer<uint8_t> pred = Buffer<uint8_t>(width, height, 0, 16);
        ASSERT_TRUE(pred.Init());
        Buffer<uint8_t> avg_ref = Buffer<uint8_t>(width, height, 0, 16);
        ASSERT_TRUE(avg_ref.Init());
        Buffer<uint8_t> avg_chk = Buffer<uint8_t>(width, height, 0, 16);
        ASSERT_TRUE(avg_chk.Init());

        ref.Set(&rnd_, &ACMRandom::Rand8);
        pred.Set(&rnd_, &ACMRandom::Rand8);

        reference_pred(pred, ref, width, height, &avg_ref);
        ASM_REGISTER_STATE_CHECK(
            avg_pred_func_(avg_chk.TopLeftPixel(), pred.TopLeftPixel(), width,
                           height, ref.TopLeftPixel(), ref.stride()));

        EXPECT_TRUE(avg_chk.CheckValues(avg_ref));
        if (HasFailure()) {
          printf("Width: %d Height: %d\n", width, height);
          avg_chk.PrintDifference(avg_ref);
          return;
        }
      }
    }
  }
}

TEST_P(AvgPredTest, CompareReferenceRandom) {
  const int width = 64;
  const int height = 32;
  Buffer<uint8_t> ref = Buffer<uint8_t>(width, height, 8);
  ASSERT_TRUE(ref.Init());
  Buffer<uint8_t> pred = Buffer<uint8_t>(width, height, 0, 16);
  ASSERT_TRUE(pred.Init());
  Buffer<uint8_t> avg_ref = Buffer<uint8_t>(width, height, 0, 16);
  ASSERT_TRUE(avg_ref.Init());
  Buffer<uint8_t> avg_chk = Buffer<uint8_t>(width, height, 0, 16);
  ASSERT_TRUE(avg_chk.Init());

  for (int i = 0; i < 500; ++i) {
    ref.Set(&rnd_, &ACMRandom::Rand8);
    pred.Set(&rnd_, &ACMRandom::Rand8);

    reference_pred(pred, ref, width, height, &avg_ref);
    ASM_REGISTER_STATE_CHECK(avg_pred_func_(avg_chk.TopLeftPixel(),
                                            pred.TopLeftPixel(), width, height,
                                            ref.TopLeftPixel(), ref.stride()));
    EXPECT_TRUE(avg_chk.CheckValues(avg_ref));
    if (HasFailure()) {
      printf("Width: %d Height: %d\n", width, height);
      avg_chk.PrintDifference(avg_ref);
      return;
    }
  }
}

TEST_P(AvgPredTest, DISABLED_Speed) {
  for (int width_pow = 2; width_pow <= 6; ++width_pow) {
    for (int height_pow = width_pow - 1; height_pow <= width_pow + 1;
         ++height_pow) {
      // Don't test 4x2 or 64x128
      if (height_pow == 1 || height_pow == 7) continue;

      for (int ref_padding = 0; ref_padding < 2; ref_padding++) {
        const int width = 1 << width_pow;
        const int height = 1 << height_pow;
        Buffer<uint8_t> ref =
            Buffer<uint8_t>(width, height, ref_padding ? 8 : 0);
        ASSERT_TRUE(ref.Init());
        Buffer<uint8_t> pred = Buffer<uint8_t>(width, height, 0, 16);
        ASSERT_TRUE(pred.Init());
        Buffer<uint8_t> avg = Buffer<uint8_t>(width, height, 0, 16);
        ASSERT_TRUE(avg.Init());

        ref.Set(&rnd_, &ACMRandom::Rand8);
        pred.Set(&rnd_, &ACMRandom::Rand8);

        vpx_usec_timer timer;
        vpx_usec_timer_start(&timer);
        for (int i = 0; i < 10000000 / (width * height); ++i) {
          avg_pred_func_(avg.TopLeftPixel(), pred.TopLeftPixel(), width, height,
                         ref.TopLeftPixel(), ref.stride());
        }
        vpx_usec_timer_mark(&timer);

        const int elapsed_time =
            static_cast<int>(vpx_usec_timer_elapsed(&timer));
        printf("Average Test (ref_padding: %d) %dx%d time: %5d us\n",
               ref_padding, width, height, elapsed_time);
      }
    }
  }
}

INSTANTIATE_TEST_CASE_P(C, AvgPredTest,
                        ::testing::Values(&vpx_comp_avg_pred_c));

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, AvgPredTest,
                        ::testing::Values(&vpx_comp_avg_pred_sse2));
#endif  // HAVE_SSE2

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(NEON, AvgPredTest,
                        ::testing::Values(&vpx_comp_avg_pred_neon));
#endif  // HAVE_NEON

#if HAVE_VSX
INSTANTIATE_TEST_CASE_P(VSX, AvgPredTest,
                        ::testing::Values(&vpx_comp_avg_pred_vsx));
#endif  // HAVE_VSX
}  // namespace
