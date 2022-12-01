/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <tuple>

#include "config/av1_rtcd.h"

#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {
typedef int64_t (*frame_error_func)(const uint8_t *const ref, int stride,
                                    const uint8_t *const dst, int p_width,
                                    int p_height, int p_stride);
#if HAVE_AVX2 || HAVE_SSE2
const int kBlockWidth[] = {
  832, 834, 640, 1280, 1920,
};
const int kBlockHeight[] = {
  480, 482, 360, 720, 1080,
};
#endif
typedef std::tuple<frame_error_func, int, int> FrameErrorParam;

class AV1FrameErrorTest : public ::testing::TestWithParam<FrameErrorParam> {
 public:
  virtual ~AV1FrameErrorTest() {}
  virtual void SetUp() {
    rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());
  }
  virtual void TearDown() {}

 protected:
  void RandomValues(frame_error_func test_impl, int width, int height);
  void ExtremeValues(frame_error_func test_impl, int width, int height);
  void RunSpeedTest(frame_error_func test_impl, int width, int height);
  libaom_test::ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1FrameErrorTest);

void AV1FrameErrorTest::RandomValues(frame_error_func test_impl, int width,
                                     int height) {
  const int stride = (((width * 3) / 2) + 15) & ~15;
  const int max_blk_size = stride * height;
  uint8_t *const dst =
      static_cast<uint8_t *>(aom_memalign(16, max_blk_size * sizeof(*dst)));
  uint8_t *const ref =
      static_cast<uint8_t *>(aom_memalign(16, max_blk_size * sizeof(*ref)));
  ASSERT_NE(dst, nullptr);
  ASSERT_NE(ref, nullptr);
  for (int i = 0; i < max_blk_size; ++i) {
    dst[i] = rnd_.Rand8();
    ref[i] = rnd_.Rand8();
  }
  const int64_t ref_error =
      av1_calc_frame_error_c(ref, stride, dst, width, height, stride);
  const int64_t test_error = test_impl(ref, stride, dst, width, height, stride);
  ASSERT_EQ(test_error, ref_error) << width << "x" << height;
  aom_free(dst);
  aom_free(ref);
}

void AV1FrameErrorTest::ExtremeValues(frame_error_func test_impl, int width,
                                      int height) {
  const int stride = (((width * 3) / 2) + 15) & ~15;
  const int max_blk_size = stride * height;
  uint8_t *const dst =
      static_cast<uint8_t *>(aom_memalign(16, max_blk_size * sizeof(*dst)));
  uint8_t *const ref =
      static_cast<uint8_t *>(aom_memalign(16, max_blk_size * sizeof(*ref)));
  ASSERT_NE(dst, nullptr);
  ASSERT_NE(ref, nullptr);
  for (int r = 0; r < 2; r++) {
    if (r == 0) {
      memset(dst, 0, max_blk_size);
      memset(ref, 255, max_blk_size);
    } else if (r == 1) {
      memset(dst, 255, max_blk_size);
      memset(ref, 0, max_blk_size);
    }
    const int64_t ref_error =
        av1_calc_frame_error_c(ref, stride, dst, width, height, stride);
    const int64_t test_error =
        test_impl(ref, stride, dst, width, height, stride);
    ASSERT_EQ(test_error, ref_error) << width << "x" << height;
  }
  aom_free(dst);
  aom_free(ref);
}

void AV1FrameErrorTest::RunSpeedTest(frame_error_func test_impl, int width,
                                     int height) {
  const int stride = (((width * 3) / 2) + 15) & ~15;
  const int max_blk_size = stride * height;
  uint8_t *const dst =
      static_cast<uint8_t *>(aom_memalign(16, max_blk_size * sizeof(*dst)));
  uint8_t *const ref =
      static_cast<uint8_t *>(aom_memalign(16, max_blk_size * sizeof(*ref)));
  ASSERT_NE(dst, nullptr);
  ASSERT_NE(ref, nullptr);
  for (int i = 0; i < max_blk_size; ++i) {
    dst[i] = ref[i] = rnd_.Rand8();
  }
  const int num_loops = 10000000 / (width + height);
  frame_error_func funcs[2] = { av1_calc_frame_error_c, test_impl };
  double elapsed_time[2] = { 0 };
  for (int i = 0; i < 2; ++i) {
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    frame_error_func func = funcs[i];
    for (int j = 0; j < num_loops; ++j) {
      func(ref, stride, dst, width, height, stride);
    }
    aom_usec_timer_mark(&timer);
    double time = static_cast<double>(aom_usec_timer_elapsed(&timer));
    elapsed_time[i] = 1000.0 * time / num_loops;
  }
  aom_free(dst);
  aom_free(ref);
  printf("av1_calc_frame_error %3dx%-3d: %7.2f/%7.2fns", width, height,
         elapsed_time[0], elapsed_time[1]);
  printf("(%3.2f)\n", elapsed_time[0] / elapsed_time[1]);
}

TEST_P(AV1FrameErrorTest, CheckOutput) {
  RandomValues(GET_PARAM(0), GET_PARAM(1), GET_PARAM(2));
  ExtremeValues(GET_PARAM(0), GET_PARAM(1), GET_PARAM(2));
}

TEST_P(AV1FrameErrorTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), GET_PARAM(2));
}

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, AV1FrameErrorTest,
    ::testing::Combine(::testing::Values(&av1_calc_frame_error_sse2),
                       ::testing::ValuesIn(kBlockWidth),
                       ::testing::ValuesIn(kBlockHeight)));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1FrameErrorTest,
    ::testing::Combine(::testing::Values(&av1_calc_frame_error_avx2),
                       ::testing::ValuesIn(kBlockWidth),
                       ::testing::ValuesIn(kBlockHeight)));
#endif
}  // namespace
