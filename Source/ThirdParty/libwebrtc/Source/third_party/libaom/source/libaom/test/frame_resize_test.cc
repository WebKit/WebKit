/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved.
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

#include "config/av1_rtcd.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/bitops.h"
#include "gtest/gtest.h"
#include "test/acm_random.h"
#include "test/util.h"

namespace {

using ::testing::Combine;
using ::testing::Values;
using ::testing::ValuesIn;

using std::make_tuple;
using std::tuple;

const int kIters = 1000;

typedef tuple<int, int> FrameDimension;

// Check that two 8-bit output buffers are identical.
void AssertOutputBufferEq(const uint8_t *p1, const uint8_t *p2, int width,
                          int height) {
  ASSERT_TRUE(p1 != p2) << "Buffers must be at different memory locations";
  for (int j = 0; j < height; ++j) {
    if (memcmp(p1, p2, sizeof(*p1) * width) == 0) {
      p1 += width;
      p2 += width;
      continue;
    }
    for (int i = 0; i < width; ++i) {
      ASSERT_EQ(p1[i], p2[i])
          << width << "x" << height << " Pixel mismatch at (" << i << ", " << j
          << ")";
    }
  }
}

typedef bool (*LowBDResizeFunc)(uint8_t *intbuf, uint8_t *output,
                                int out_stride, int height, int height2,
                                int stride, int start_wd);
// Test parameter list:
//  <tst_fun, dims>
typedef tuple<LowBDResizeFunc, FrameDimension> ResizeTestParams;

class AV1ResizeYTest : public ::testing::TestWithParam<ResizeTestParams> {
 public:
  void SetUp() {
    test_fun_ = GET_PARAM(0);
    frame_dim_ = GET_PARAM(1);
    width_ = std::get<0>(frame_dim_);
    height_ = std::get<1>(frame_dim_);
    const int msb = get_msb(AOMMIN(width_, height_));
    n_levels_ = AOMMAX(msb - MIN_PYRAMID_SIZE_LOG2, 1);
    const int src_buf_size = (width_ / 2) * height_;
    const int dest_buf_size = (width_ * height_) / 4;
    src_ = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[src_buf_size]);
    ASSERT_NE(src_, nullptr);

    ref_dest_ =
        std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[dest_buf_size]);
    ASSERT_NE(ref_dest_, nullptr);

    test_dest_ =
        std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[dest_buf_size]);
    ASSERT_NE(test_dest_, nullptr);
  }

  void RunTest() {
    for (int i = 0; i < (width_ / 2) * height_; i++) src_[i] = rng_.Rand8();
    for (int level = 1; level < n_levels_; level++) {
      const int width2 = (width_ >> level);
      const int height2 = (height_ >> level);
      av1_resize_vert_dir_c(src_.get(), ref_dest_.get(), width2, height2 << 1,
                            height2, width2, 0);
      test_fun_(src_.get(), test_dest_.get(), width2, height2 << 1, height2,
                width2, 0);

      AssertOutputBufferEq(ref_dest_.get(), test_dest_.get(), width2, height2);
    }
  }

  void SpeedTest() {
    for (int i = 0; i < (width_ / 2) * height_; i++) src_[i] = rng_.Rand8();
    for (int level = 1; level < n_levels_; level++) {
      const int width2 = (width_ >> level);
      const int height2 = (height_ >> level);
      aom_usec_timer ref_timer;
      aom_usec_timer_start(&ref_timer);
      for (int j = 0; j < kIters; j++) {
        av1_resize_vert_dir_c(src_.get(), ref_dest_.get(), width2, height2 << 1,
                              height2, width2, 0);
      }
      aom_usec_timer_mark(&ref_timer);
      const int64_t ref_time = aom_usec_timer_elapsed(&ref_timer);

      aom_usec_timer tst_timer;
      aom_usec_timer_start(&tst_timer);
      for (int j = 0; j < kIters; j++) {
        test_fun_(src_.get(), test_dest_.get(), width2, height2 << 1, height2,
                  width2, 0);
      }
      aom_usec_timer_mark(&tst_timer);
      const int64_t tst_time = aom_usec_timer_elapsed(&tst_timer);

      std::cout << "level: " << level << " [" << width2 << " x " << height2
                << "] C time = " << ref_time << " , SIMD time = " << tst_time
                << " scaling=" << float(1.00) * ref_time / tst_time << "x \n";
    }
  }

 private:
  LowBDResizeFunc test_fun_;
  FrameDimension frame_dim_;
  int width_;
  int height_;
  int n_levels_;
  std::unique_ptr<uint8_t[]> src_;
  std::unique_ptr<uint8_t[]> ref_dest_;
  std::unique_ptr<uint8_t[]> test_dest_;
  libaom_test::ACMRandom rng_;
};

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1ResizeYTest);

TEST_P(AV1ResizeYTest, RunTest) { RunTest(); }

TEST_P(AV1ResizeYTest, DISABLED_SpeedTest) { SpeedTest(); }

#if HAVE_AVX2 || HAVE_SSE2
// Resolutions (width x height) to be tested for resizing.
const FrameDimension kFrameDim[] = {
  make_tuple(3840, 2160), make_tuple(2560, 1440), make_tuple(1920, 1080),
  make_tuple(1280, 720),  make_tuple(640, 480),   make_tuple(640, 360),
  make_tuple(286, 286),   make_tuple(284, 284),   make_tuple(282, 282),
  make_tuple(280, 280),   make_tuple(262, 262),   make_tuple(258, 258),
  make_tuple(256, 256),   make_tuple(34, 34),
};
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1ResizeYTest,
    ::testing::Combine(::testing::Values(av1_resize_vert_dir_avx2),
                       ::testing::ValuesIn(kFrameDim)));
#endif

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, AV1ResizeYTest,
    ::testing::Combine(::testing::Values(av1_resize_vert_dir_sse2),
                       ::testing::ValuesIn(kFrameDim)));
#endif

typedef void (*LowBDResize_x_Func)(const uint8_t *const input, int in_stride,
                                   uint8_t *intbuf, int height,
                                   int filtered_length, int width2);

typedef tuple<LowBDResize_x_Func, FrameDimension> Resize_x_TestParams;

class AV1ResizeXTest : public ::testing::TestWithParam<Resize_x_TestParams> {
 public:
  void SetUp() {
    test_fun_ = GET_PARAM(0);
    frame_dim_ = GET_PARAM(1);
    width_ = std::get<0>(frame_dim_);
    height_ = std::get<1>(frame_dim_);
    const int msb = get_msb(AOMMIN(width_, height_));
    n_levels_ = AOMMAX(msb - MIN_PYRAMID_SIZE_LOG2, 1);
    const int src_buf_size = width_ * height_;
    const int dest_buf_size = (width_ * height_) / 2;
    src_ = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[src_buf_size]);
    ASSERT_NE(src_, nullptr);

    ref_dest_ =
        std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[dest_buf_size]);
    ASSERT_NE(ref_dest_, nullptr);

    test_dest_ =
        std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[dest_buf_size]);
    ASSERT_NE(test_dest_, nullptr);
  }

  void RunTest() {
    for (int i = 0; i < width_ * height_; ++i) src_[i] = rng_.Rand8();

    for (int level = 1; level < n_levels_; ++level) {
      const int width2 = (width_ >> level);
      av1_resize_horz_dir_c(src_.get(), width_, ref_dest_.get(), height_,
                            width2 << 1, width2);
      test_fun_(src_.get(), width_, test_dest_.get(), height_, width2 << 1,
                width2);
      AssertOutputBufferEq(ref_dest_.get(), test_dest_.get(), width2, height_);
    }
  }

  void SpeedTest() {
    for (int i = 0; i < width_ * height_; ++i) src_[i] = rng_.Rand8();

    for (int level = 1; level < n_levels_; ++level) {
      const int width2 = (width_ >> level);
      aom_usec_timer ref_timer;
      aom_usec_timer_start(&ref_timer);
      for (int j = 0; j < kIters; ++j) {
        av1_resize_horz_dir_c(src_.get(), width_, ref_dest_.get(), height_,
                              width2 << 1, width2);
      }
      aom_usec_timer_mark(&ref_timer);
      const int64_t ref_time = aom_usec_timer_elapsed(&ref_timer);

      aom_usec_timer tst_timer;
      aom_usec_timer_start(&tst_timer);
      for (int j = 0; j < kIters; ++j) {
        test_fun_(src_.get(), width_, test_dest_.get(), height_, width2 << 1,
                  width2);
      }
      aom_usec_timer_mark(&tst_timer);
      const int64_t tst_time = aom_usec_timer_elapsed(&tst_timer);

      std::cout << "level: " << level << " [" << width2 << " x " << height_
                << "] C time = " << ref_time << " , SIMD time = " << tst_time
                << " scaling=" << float(1.00) * ref_time / tst_time << "x \n";
    }
  }

 private:
  LowBDResize_x_Func test_fun_;
  FrameDimension frame_dim_;
  int width_;
  int height_;
  int n_levels_;
  std::unique_ptr<uint8_t[]> src_;
  std::unique_ptr<uint8_t[]> ref_dest_;
  std::unique_ptr<uint8_t[]> test_dest_;
  libaom_test::ACMRandom rng_;
};

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1ResizeXTest);

TEST_P(AV1ResizeXTest, RunTest) { RunTest(); }

TEST_P(AV1ResizeXTest, DISABLED_SpeedTest) { SpeedTest(); }

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, AV1ResizeXTest,
    ::testing::Combine(::testing::Values(av1_resize_horz_dir_sse2),
                       ::testing::ValuesIn(kFrameDim)));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1ResizeXTest,
    ::testing::Combine(::testing::Values(av1_resize_horz_dir_avx2),
                       ::testing::ValuesIn(kFrameDim)));
#endif

}  // namespace
