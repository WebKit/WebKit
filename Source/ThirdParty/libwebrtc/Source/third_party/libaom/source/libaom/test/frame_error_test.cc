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

namespace frame_error_test {
typedef int64_t (*frame_error_func)(const uint8_t *const ref, int ref_stride,
                                    const uint8_t *const dst, int dst_stride,
                                    int p_width, int p_height);
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
  ~AV1FrameErrorTest() override = default;
  void SetUp() override {
    rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());
  }

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
      av1_calc_frame_error_c(ref, stride, dst, stride, width, height);
  const int64_t test_error = test_impl(ref, stride, dst, stride, width, height);
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
        av1_calc_frame_error_c(ref, stride, dst, stride, width, height);
    const int64_t test_error =
        test_impl(ref, stride, dst, stride, width, height);
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
      func(ref, stride, dst, stride, width, height);
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
}  // namespace frame_error_test

#if CONFIG_AV1_HIGHBITDEPTH
namespace highbd_frame_error_test {
typedef int64_t (*highbd_frame_error_func)(const uint16_t *const ref,
                                           int ref_stride,
                                           const uint16_t *const dst,
                                           int dst_stride, int p_width,
                                           int p_height, int bd);
const int kBlockWidth[] = {
  832, 834, 640, 1280, 1920,
};
const int kBlockHeight[] = {
  480, 482, 360, 720, 1080,
};
#if HAVE_AVX2 || HAVE_SSE2
const int kBitDepths[] = { 8, 10, 12 };
#endif
typedef std::tuple<highbd_frame_error_func, int, int, int>
    HighbdFrameErrorParam;

class AV1HighbdFrameErrorTest
    : public ::testing::TestWithParam<HighbdFrameErrorParam> {
 public:
  ~AV1HighbdFrameErrorTest() override = default;
  void SetUp() override {
    rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());
  }

 protected:
  void RandomValues(highbd_frame_error_func test_impl, int width, int height,
                    int bd);
  void ExtremeValues(highbd_frame_error_func test_impl, int width, int height,
                     int bd);
  void RunSpeedTest(highbd_frame_error_func test_impl, int width, int height,
                    int bd);
  libaom_test::ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HighbdFrameErrorTest);

void AV1HighbdFrameErrorTest::RandomValues(highbd_frame_error_func test_impl,
                                           int width, int height, int bd) {
  const int stride = (((width * 3) / 2) + 15) & ~15;
  const int max_blk_size = stride * height;
  uint16_t *const dst =
      static_cast<uint16_t *>(aom_memalign(32, max_blk_size * sizeof(*dst)));
  uint16_t *const ref =
      static_cast<uint16_t *>(aom_memalign(32, max_blk_size * sizeof(*ref)));
  ASSERT_NE(dst, nullptr);
  ASSERT_NE(ref, nullptr);
  int mask = (1 << bd) - 1;
  for (int i = 0; i < max_blk_size; ++i) {
    dst[i] = rnd_.Rand16() & mask;
    ref[i] = rnd_.Rand16() & mask;
  }
  const int64_t ref_error = av1_calc_highbd_frame_error_c(
      ref, stride, dst, stride, width, height, bd);
  const int64_t test_error =
      test_impl(ref, stride, dst, stride, width, height, bd);
  ASSERT_EQ(test_error, ref_error) << width << "x" << height << " bd=" << bd;
  aom_free(dst);
  aom_free(ref);
}

void AV1HighbdFrameErrorTest::ExtremeValues(highbd_frame_error_func test_impl,
                                            int width, int height, int bd) {
  const int stride = (((width * 3) / 2) + 15) & ~15;
  const int max_blk_size = stride * height;
  uint16_t *const dst =
      static_cast<uint16_t *>(aom_memalign(32, max_blk_size * sizeof(*dst)));
  uint16_t *const ref =
      static_cast<uint16_t *>(aom_memalign(32, max_blk_size * sizeof(*ref)));
  ASSERT_NE(dst, nullptr);
  ASSERT_NE(ref, nullptr);
  int mask = (1 << bd) - 1;
  for (int r = 0; r < 2; r++) {
    // Silence static analysis warnings
    assert(dst);
    assert(ref);
    if (r == 0) {
      aom_memset16(dst, 0, max_blk_size);
      aom_memset16(ref, mask, max_blk_size);
    } else if (r == 1) {
      aom_memset16(dst, mask, max_blk_size);
      aom_memset16(ref, 0, max_blk_size);
    }
    const int64_t ref_error = av1_calc_highbd_frame_error_c(
        ref, stride, dst, stride, width, height, bd);
    const int64_t test_error =
        test_impl(ref, stride, dst, stride, width, height, bd);
    ASSERT_EQ(test_error, ref_error) << width << "x" << height << " bd=" << bd;
  }
  aom_free(dst);
  aom_free(ref);
}

void AV1HighbdFrameErrorTest::RunSpeedTest(highbd_frame_error_func test_impl,
                                           int width, int height, int bd) {
  const int stride = (((width * 3) / 2) + 15) & ~15;
  const int max_blk_size = stride * height;
  uint16_t *const dst =
      static_cast<uint16_t *>(aom_memalign(32, max_blk_size * sizeof(*dst)));
  uint16_t *const ref =
      static_cast<uint16_t *>(aom_memalign(32, max_blk_size * sizeof(*ref)));
  ASSERT_NE(dst, nullptr);
  ASSERT_NE(ref, nullptr);
  int mask = (1 << bd) - 1;
  for (int i = 0; i < max_blk_size; ++i) {
    dst[i] = ref[i] = rnd_.Rand16() & mask;
  }
  const int num_loops = 10000000 / (width + height);
  highbd_frame_error_func funcs[2] = { av1_calc_highbd_frame_error_c,
                                       test_impl };
  double elapsed_time[2] = { 0 };
  for (int i = 0; i < 2; ++i) {
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    highbd_frame_error_func func = funcs[i];
    for (int j = 0; j < num_loops; ++j) {
      func(ref, stride, dst, stride, width, height, bd);
    }
    aom_usec_timer_mark(&timer);
    double time = static_cast<double>(aom_usec_timer_elapsed(&timer));
    elapsed_time[i] = 1000.0 * time / num_loops;
  }
  aom_free(dst);
  aom_free(ref);
  printf("av1_calc_highbd_frame_error %3dx%-3d bd=%2d: %7.2f/%7.2fns", width,
         height, bd, elapsed_time[0], elapsed_time[1]);
  printf("(%3.2f)\n", elapsed_time[0] / elapsed_time[1]);
}

TEST_P(AV1HighbdFrameErrorTest, CheckOutput) {
  RandomValues(GET_PARAM(0), GET_PARAM(1), GET_PARAM(2), GET_PARAM(3));
  ExtremeValues(GET_PARAM(0), GET_PARAM(1), GET_PARAM(2), GET_PARAM(3));
}

TEST_P(AV1HighbdFrameErrorTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(0), GET_PARAM(1), GET_PARAM(2), GET_PARAM(3));
}

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, AV1HighbdFrameErrorTest,
    ::testing::Combine(::testing::Values(&av1_calc_highbd_frame_error_sse2),
                       ::testing::ValuesIn(kBlockWidth),
                       ::testing::ValuesIn(kBlockHeight),
                       ::testing::ValuesIn(kBitDepths)));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1HighbdFrameErrorTest,
    ::testing::Combine(::testing::Values(&av1_calc_highbd_frame_error_avx2),
                       ::testing::ValuesIn(kBlockWidth),
                       ::testing::ValuesIn(kBlockHeight),
                       ::testing::ValuesIn(kBitDepths)));
#endif

// Check that 8-bit and 16-bit code paths give the same results for
// 8-bit content
typedef std::tuple<int, int> HighbdFrameErrorConsistencyParam;

class AV1HighbdFrameErrorConsistencyTest
    : public ::testing::TestWithParam<HighbdFrameErrorConsistencyParam> {
 public:
  ~AV1HighbdFrameErrorConsistencyTest() override = default;
  void SetUp() override {
    rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());
  }

 protected:
  void RandomValues(int width, int height);
  void ExtremeValues(int width, int height);
  libaom_test::ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    AV1HighbdFrameErrorConsistencyTest);

void AV1HighbdFrameErrorConsistencyTest::RandomValues(int width, int height) {
  const int stride = (((width * 3) / 2) + 15) & ~15;
  const int max_blk_size = stride * height;
  uint8_t *const dst8 =
      static_cast<uint8_t *>(aom_memalign(16, max_blk_size * sizeof(*dst8)));
  uint8_t *const ref8 =
      static_cast<uint8_t *>(aom_memalign(16, max_blk_size * sizeof(*ref8)));
  uint16_t *const dst16 =
      static_cast<uint16_t *>(aom_memalign(32, max_blk_size * sizeof(*dst16)));
  uint16_t *const ref16 =
      static_cast<uint16_t *>(aom_memalign(32, max_blk_size * sizeof(*ref16)));
  ASSERT_NE(dst8, nullptr);
  ASSERT_NE(ref8, nullptr);
  ASSERT_NE(dst16, nullptr);
  ASSERT_NE(ref16, nullptr);
  // Set up parallel 8-bit and 16-bit buffers with the same content
  for (int i = 0; i < max_blk_size; ++i) {
    dst16[i] = dst8[i] = rnd_.Rand8();
    ref16[i] = ref8[i] = rnd_.Rand8();
  }
  const int64_t error8 =
      av1_calc_frame_error_c(ref8, stride, dst8, stride, width, height);
  const int64_t error16 = av1_calc_highbd_frame_error_c(
      ref16, stride, dst16, stride, width, height, 8);
  ASSERT_EQ(error8, error16) << width << "x" << height;
  aom_free(dst8);
  aom_free(ref8);
  aom_free(dst16);
  aom_free(ref16);
}

void AV1HighbdFrameErrorConsistencyTest::ExtremeValues(int width, int height) {
  const int stride = (((width * 3) / 2) + 15) & ~15;
  const int max_blk_size = stride * height;
  uint8_t *const dst8 =
      static_cast<uint8_t *>(aom_memalign(16, max_blk_size * sizeof(*dst8)));
  uint8_t *const ref8 =
      static_cast<uint8_t *>(aom_memalign(16, max_blk_size * sizeof(*ref8)));
  uint16_t *const dst16 =
      static_cast<uint16_t *>(aom_memalign(32, max_blk_size * sizeof(*dst16)));
  uint16_t *const ref16 =
      static_cast<uint16_t *>(aom_memalign(32, max_blk_size * sizeof(*ref16)));
  ASSERT_NE(dst8, nullptr);
  ASSERT_NE(ref8, nullptr);
  ASSERT_NE(dst16, nullptr);
  ASSERT_NE(ref16, nullptr);
  for (int r = 0; r < 2; r++) {
    // Silence static analysis warnings
    assert(dst16);
    assert(ref16);
    // Set up parallel 8-bit and 16-bit buffers with the same content
    if (r == 0) {
      memset(dst8, 0, max_blk_size);
      aom_memset16(dst16, 0, max_blk_size);
      memset(ref8, 255, max_blk_size);
      aom_memset16(ref16, 255, max_blk_size);
    } else if (r == 1) {
      memset(dst8, 255, max_blk_size);
      aom_memset16(dst16, 255, max_blk_size);
      memset(ref8, 0, max_blk_size);
      aom_memset16(ref16, 0, max_blk_size);
    }
    const int64_t error8 =
        av1_calc_frame_error_c(ref8, stride, dst8, stride, width, height);
    const int64_t error16 = av1_calc_highbd_frame_error_c(
        ref16, stride, dst16, stride, width, height, 8);
    ASSERT_EQ(error8, error16) << width << "x" << height;
  }
  aom_free(dst8);
  aom_free(ref8);
  aom_free(dst16);
  aom_free(ref16);
}

TEST_P(AV1HighbdFrameErrorConsistencyTest, CheckOutput) {
  RandomValues(GET_PARAM(0), GET_PARAM(1));
  ExtremeValues(GET_PARAM(0), GET_PARAM(1));
}

INSTANTIATE_TEST_SUITE_P(C, AV1HighbdFrameErrorConsistencyTest,
                         ::testing::Combine(::testing::ValuesIn(kBlockWidth),
                                            ::testing::ValuesIn(kBlockHeight)));
}  // namespace highbd_frame_error_test
#endif  // CONFIG_AV1_HIGHBITDEPTH
