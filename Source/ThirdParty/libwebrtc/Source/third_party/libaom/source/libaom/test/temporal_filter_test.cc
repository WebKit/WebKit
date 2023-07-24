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

#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <tuple>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom_ports/mem.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/temporal_filter.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "test/function_equivalence_test.h"

using libaom_test::ACMRandom;
using ::testing::Combine;
using ::testing::Values;
using ::testing::ValuesIn;

#if !CONFIG_REALTIME_ONLY
namespace {
typedef enum {
  I400,  // Monochrome
  I420,  // 4:2:0
  I422,  // 4:2:2
  I444,  // 4:4:4
} ColorFormat;
static const char *color_fmt_str[] = { "I400", "I420", "I422", "I444" };
typedef void (*TemporalFilterFunc)(
    const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int num_planes, const double *noise_level, const MV *subblock_mvs,
    const int *subblock_mses, const int q_factor, const int filter_strength,
    int tf_wgt_calc_lvl, const uint8_t *pred, uint32_t *accum, uint16_t *count);
typedef libaom_test::FuncParam<TemporalFilterFunc> TemporalFilterFuncParam;

typedef std::tuple<TemporalFilterFuncParam, int> TemporalFilterWithParam;

class TemporalFilterTest
    : public ::testing::TestWithParam<TemporalFilterWithParam> {
 public:
  virtual ~TemporalFilterTest() {}
  virtual void SetUp() {
    params_ = GET_PARAM(0);
    tf_wgt_calc_lvl_ = GET_PARAM(1);
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src1_ = reinterpret_cast<uint8_t *>(
        aom_memalign(8, sizeof(uint8_t) * MAX_MB_PLANE * BH * BW));
    src2_ = reinterpret_cast<uint8_t *>(
        aom_memalign(8, sizeof(uint8_t) * MAX_MB_PLANE * BH * BW));

    ASSERT_NE(src1_, nullptr);
    ASSERT_NE(src2_, nullptr);
  }

  virtual void TearDown() {
    aom_free(src1_);
    aom_free(src2_);
  }
  void RunTest(int isRandom, int run_times, ColorFormat color_fmt);

  void GenRandomData(int width, int height, int stride, int stride2,
                     int num_planes, int subsampling_x, int subsampling_y) {
    uint8_t *src1p = src1_;
    uint8_t *src2p = src2_;
    for (int plane = 0; plane < num_planes; plane++) {
      int plane_w = plane ? width >> subsampling_x : width;
      int plane_h = plane ? height >> subsampling_y : height;
      int plane_stride = plane ? stride >> subsampling_x : stride;
      int plane_stride2 = plane ? stride2 >> subsampling_x : stride2;
      for (int ii = 0; ii < plane_h; ii++) {
        for (int jj = 0; jj < plane_w; jj++) {
          src1p[jj] = rnd_.Rand8();
          src2p[jj] = rnd_.Rand8();
        }
        src1p += plane_stride;
        src2p += plane_stride2;
      }
    }
  }

  void GenExtremeData(int width, int height, int stride, int stride2,
                      int num_planes, int subsampling_x, int subsampling_y,
                      uint8_t val) {
    uint8_t *src1p = src1_;
    uint8_t *src2p = src2_;
    for (int plane = 0; plane < num_planes; plane++) {
      int plane_w = plane ? width >> subsampling_x : width;
      int plane_h = plane ? height >> subsampling_y : height;
      int plane_stride = plane ? stride >> subsampling_x : stride;
      int plane_stride2 = plane ? stride2 >> subsampling_x : stride2;
      for (int ii = 0; ii < plane_h; ii++) {
        for (int jj = 0; jj < plane_w; jj++) {
          src1p[jj] = val;
          src2p[jj] = (255 - val);
        }
        src1p += plane_stride;
        src2p += plane_stride2;
      }
    }
  }

 protected:
  TemporalFilterFuncParam params_;
  int32_t tf_wgt_calc_lvl_;
  uint8_t *src1_;
  uint8_t *src2_;
  ACMRandom rnd_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TemporalFilterTest);

void TemporalFilterTest::RunTest(int isRandom, int run_times,
                                 ColorFormat color_fmt) {
  aom_usec_timer ref_timer, test_timer;
  const BLOCK_SIZE block_size = TF_BLOCK_SIZE;
  static_assert(block_size == BLOCK_32X32, "");
  const int width = 32;
  const int height = 32;
  int num_planes = MAX_MB_PLANE;
  int subsampling_x = 0;
  int subsampling_y = 0;
  if (color_fmt == I420) {
    subsampling_x = 1;
    subsampling_y = 1;
  } else if (color_fmt == I422) {
    subsampling_x = 1;
    subsampling_y = 0;
  } else if (color_fmt == I400) {
    num_planes = 1;
  }
  for (int k = 0; k < 3; k++) {
    const int stride = width;
    const int stride2 = width;
    if (isRandom) {
      GenRandomData(width, height, stride, stride2, num_planes, subsampling_x,
                    subsampling_y);
    } else {
      const int msb = 8;  // Up to 8 bit input
      const int limit = (1 << msb) - 1;
      if (k == 0) {
        GenExtremeData(width, height, stride, stride2, num_planes,
                       subsampling_x, subsampling_y, limit);
      } else {
        GenExtremeData(width, height, stride, stride2, num_planes,
                       subsampling_x, subsampling_y, 0);
      }
    }
    double sigma[MAX_MB_PLANE] = { 2.1002103677063437, 2.1002103677063437,
                                   2.1002103677063437 };
    DECLARE_ALIGNED(16, unsigned int, accumulator_ref[1024 * 3]);
    DECLARE_ALIGNED(16, uint16_t, count_ref[1024 * 3]);
    memset(accumulator_ref, 0, 1024 * 3 * sizeof(accumulator_ref[0]));
    memset(count_ref, 0, 1024 * 3 * sizeof(count_ref[0]));
    DECLARE_ALIGNED(16, unsigned int, accumulator_mod[1024 * 3]);
    DECLARE_ALIGNED(16, uint16_t, count_mod[1024 * 3]);
    memset(accumulator_mod, 0, 1024 * 3 * sizeof(accumulator_mod[0]));
    memset(count_mod, 0, 1024 * 3 * sizeof(count_mod[0]));

    static_assert(width == 32 && height == 32, "");
    const MV subblock_mvs[4] = { { 0, 0 }, { 5, 5 }, { 7, 8 }, { 2, 10 } };
    const int subblock_mses[4] = { 15, 16, 17, 18 };
    const int q_factor = 12;
    const int filter_strength = 5;
    const int mb_row = 0;
    const int mb_col = 0;
    std::unique_ptr<YV12_BUFFER_CONFIG> frame_to_filter(new (std::nothrow)
                                                            YV12_BUFFER_CONFIG);
    ASSERT_NE(frame_to_filter, nullptr);
    frame_to_filter->y_crop_height = 360;
    frame_to_filter->y_crop_width = 540;
    frame_to_filter->heights[PLANE_TYPE_Y] = height;
    frame_to_filter->heights[PLANE_TYPE_UV] = height >> subsampling_y;
    frame_to_filter->strides[PLANE_TYPE_Y] = stride;
    frame_to_filter->strides[PLANE_TYPE_UV] = stride >> subsampling_x;
    DECLARE_ALIGNED(16, uint8_t, src[1024 * 3]);
    frame_to_filter->buffer_alloc = src;
    frame_to_filter->flags = 0;  // Only support low bit-depth test.
    memcpy(src, src1_, 1024 * 3 * sizeof(uint8_t));

    std::unique_ptr<MACROBLOCKD> mbd(new (std::nothrow) MACROBLOCKD);
    ASSERT_NE(mbd, nullptr);
    mbd->bd = 8;
    for (int plane = AOM_PLANE_Y; plane < num_planes; plane++) {
      int plane_height = plane ? height >> subsampling_y : height;
      int plane_stride = plane ? stride >> subsampling_x : stride;
      frame_to_filter->buffers[plane] =
          frame_to_filter->buffer_alloc + plane * plane_stride * plane_height;
      mbd->plane[plane].subsampling_x = plane ? subsampling_x : 0;
      mbd->plane[plane].subsampling_y = plane ? subsampling_y : 0;
    }

    params_.ref_func(frame_to_filter.get(), mbd.get(), block_size, mb_row,
                     mb_col, num_planes, sigma, subblock_mvs, subblock_mses,
                     q_factor, filter_strength, tf_wgt_calc_lvl_, src2_,
                     accumulator_ref, count_ref);
    params_.tst_func(frame_to_filter.get(), mbd.get(), block_size, mb_row,
                     mb_col, num_planes, sigma, subblock_mvs, subblock_mses,
                     q_factor, filter_strength, tf_wgt_calc_lvl_, src2_,
                     accumulator_mod, count_mod);

    if (run_times > 1) {
      aom_usec_timer_start(&ref_timer);
      for (int j = 0; j < run_times; j++) {
        params_.ref_func(frame_to_filter.get(), mbd.get(), block_size, mb_row,
                         mb_col, num_planes, sigma, subblock_mvs, subblock_mses,
                         q_factor, filter_strength, tf_wgt_calc_lvl_, src2_,
                         accumulator_ref, count_ref);
      }
      aom_usec_timer_mark(&ref_timer);
      const int elapsed_time_c =
          static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

      aom_usec_timer_start(&test_timer);
      for (int j = 0; j < run_times; j++) {
        params_.tst_func(frame_to_filter.get(), mbd.get(), block_size, mb_row,
                         mb_col, num_planes, sigma, subblock_mvs, subblock_mses,
                         q_factor, filter_strength, tf_wgt_calc_lvl_, src2_,
                         accumulator_mod, count_mod);
      }
      aom_usec_timer_mark(&test_timer);
      const int elapsed_time_simd =
          static_cast<int>(aom_usec_timer_elapsed(&test_timer));

      printf(
          "c_time=%d \t simd_time=%d \t "
          "gain=%f\t width=%d\t height=%d\t color_format=%s\n",
          elapsed_time_c, elapsed_time_simd,
          (float)((float)elapsed_time_c / (float)elapsed_time_simd), width,
          height, color_fmt_str[color_fmt]);

    } else {
      for (int i = 0, l = 0; i < height; i++) {
        for (int j = 0; j < width; j++, l++) {
          EXPECT_EQ(accumulator_ref[l], accumulator_mod[l])
              << "Error:" << k << " SSE Sum Test [" << width << "x" << height
              << "] " << color_fmt_str[color_fmt]
              << " C accumulator does not match optimized accumulator.";
          EXPECT_EQ(count_ref[l], count_mod[l])
              << "Error:" << k << " SSE Sum Test [" << width << "x" << height
              << "] " << color_fmt_str[color_fmt]
              << " count does not match optimized count.";
        }
      }
    }
  }
}

TEST_P(TemporalFilterTest, OperationCheck) {
  RunTest(1, 1, I400);
  RunTest(1, 1, I420);
  RunTest(1, 1, I422);
  RunTest(1, 1, I444);
}

TEST_P(TemporalFilterTest, ExtremeValues) {
  RunTest(0, 1, I400);
  RunTest(0, 1, I420);
  RunTest(0, 1, I422);
  RunTest(0, 1, I444);
}

TEST_P(TemporalFilterTest, DISABLED_Speed) {
  RunTest(1, 100000, I400);
  RunTest(1, 100000, I420);
  RunTest(1, 100000, I422);
  RunTest(1, 100000, I444);
}

#if HAVE_AVX2
TemporalFilterFuncParam temporal_filter_test_avx2[] = { TemporalFilterFuncParam(
    &av1_apply_temporal_filter_c, &av1_apply_temporal_filter_avx2) };
INSTANTIATE_TEST_SUITE_P(AVX2, TemporalFilterTest,
                         Combine(ValuesIn(temporal_filter_test_avx2),
                                 Values(0, 1)));
#endif  // HAVE_AVX2

#if HAVE_SSE2
TemporalFilterFuncParam temporal_filter_test_sse2[] = { TemporalFilterFuncParam(
    &av1_apply_temporal_filter_c, &av1_apply_temporal_filter_sse2) };
INSTANTIATE_TEST_SUITE_P(SSE2, TemporalFilterTest,
                         Combine(ValuesIn(temporal_filter_test_sse2),
                                 Values(0, 1)));
#endif  // HAVE_SSE2

#if HAVE_NEON
TemporalFilterFuncParam temporal_filter_test_neon[] = { TemporalFilterFuncParam(
    &av1_apply_temporal_filter_c, &av1_apply_temporal_filter_neon) };
INSTANTIATE_TEST_SUITE_P(NEON, TemporalFilterTest,
                         Combine(ValuesIn(temporal_filter_test_neon),
                                 Values(0, 1)));
#endif  // HAVE_NEON

#if CONFIG_AV1_HIGHBITDEPTH

typedef void (*HBDTemporalFilterFunc)(
    const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int num_planes, const double *noise_level, const MV *subblock_mvs,
    const int *subblock_mses, const int q_factor, const int filter_strength,
    int tf_wgt_calc_lvl, const uint8_t *pred, uint32_t *accum, uint16_t *count);
typedef libaom_test::FuncParam<HBDTemporalFilterFunc>
    HBDTemporalFilterFuncParam;

typedef std::tuple<HBDTemporalFilterFuncParam, int> HBDTemporalFilterWithParam;

class HBDTemporalFilterTest
    : public ::testing::TestWithParam<HBDTemporalFilterWithParam> {
 public:
  virtual ~HBDTemporalFilterTest() {}
  virtual void SetUp() {
    params_ = GET_PARAM(0);
    tf_wgt_calc_lvl_ = GET_PARAM(1);
    rnd_.Reset(ACMRandom::DeterministicSeed());
    src1_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, sizeof(uint16_t) * MAX_MB_PLANE * BH * BW));
    src2_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, sizeof(uint16_t) * MAX_MB_PLANE * BH * BW));

    ASSERT_NE(src1_, nullptr);
    ASSERT_NE(src2_, nullptr);
  }

  virtual void TearDown() {
    aom_free(src1_);
    aom_free(src2_);
  }
  void RunTest(int isRandom, int run_times, int bd, ColorFormat color_fmt);

  void GenRandomData(int width, int height, int stride, int stride2, int bd,
                     int subsampling_x, int subsampling_y, int num_planes) {
    uint16_t *src1p = src1_;
    uint16_t *src2p = src2_;
    for (int plane = AOM_PLANE_Y; plane < num_planes; plane++) {
      int plane_w = plane ? width >> subsampling_x : width;
      int plane_h = plane ? height >> subsampling_y : height;
      int plane_stride = plane ? stride >> subsampling_x : stride;
      int plane_stride2 = plane ? stride2 >> subsampling_x : stride2;
      const uint16_t max_val = (1 << bd) - 1;
      for (int ii = 0; ii < plane_h; ii++) {
        for (int jj = 0; jj < plane_w; jj++) {
          src1p[jj] = rnd_.Rand16() & max_val;
          src2p[jj] = rnd_.Rand16() & max_val;
        }
        src1p += plane_stride;
        src2p += plane_stride2;
      }
    }
  }

  void GenExtremeData(int width, int height, int stride, int stride2, int bd,
                      int subsampling_x, int subsampling_y, int num_planes,
                      uint16_t val) {
    uint16_t *src1p = src1_;
    uint16_t *src2p = src2_;
    for (int plane = AOM_PLANE_Y; plane < num_planes; plane++) {
      int plane_w = plane ? width >> subsampling_x : width;
      int plane_h = plane ? height >> subsampling_y : height;
      int plane_stride = plane ? stride >> subsampling_x : stride;
      int plane_stride2 = plane ? stride2 >> subsampling_x : stride2;
      uint16_t max_val = (1 << bd) - 1;
      for (int ii = 0; ii < plane_h; ii++) {
        for (int jj = 0; jj < plane_w; jj++) {
          src1p[jj] = val;
          src2p[jj] = (max_val - val);
        }
        src1p += plane_stride;
        src2p += plane_stride2;
      }
    }
  }

 protected:
  HBDTemporalFilterFuncParam params_;
  int tf_wgt_calc_lvl_;
  uint16_t *src1_;
  uint16_t *src2_;
  ACMRandom rnd_;
};

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(HBDTemporalFilterTest);

void HBDTemporalFilterTest::RunTest(int isRandom, int run_times, int BD,
                                    ColorFormat color_fmt) {
  aom_usec_timer ref_timer, test_timer;
  const BLOCK_SIZE block_size = TF_BLOCK_SIZE;
  static_assert(block_size == BLOCK_32X32, "");
  const int width = 32;
  const int height = 32;
  int num_planes = MAX_MB_PLANE;
  int subsampling_x = 0;
  int subsampling_y = 0;
  if (color_fmt == I420) {
    subsampling_x = 1;
    subsampling_y = 1;
  } else if (color_fmt == I422) {
    subsampling_x = 1;
    subsampling_y = 0;
  } else if (color_fmt == I400) {
    num_planes = 1;
  }
  for (int k = 0; k < 3; k++) {
    const int stride = width;
    const int stride2 = width;
    if (isRandom) {
      GenRandomData(width, height, stride, stride2, BD, subsampling_x,
                    subsampling_y, num_planes);
    } else {
      const int msb = BD;
      const uint16_t limit = (1 << msb) - 1;
      if (k == 0) {
        GenExtremeData(width, height, stride, stride2, BD, subsampling_x,
                       subsampling_y, num_planes, limit);
      } else {
        GenExtremeData(width, height, stride, stride2, BD, subsampling_x,
                       subsampling_y, num_planes, 0);
      }
    }
    double sigma[MAX_MB_PLANE] = { 2.1002103677063437, 2.1002103677063437,
                                   2.1002103677063437 };
    DECLARE_ALIGNED(16, unsigned int, accumulator_ref[1024 * 3]);
    DECLARE_ALIGNED(16, uint16_t, count_ref[1024 * 3]);
    memset(accumulator_ref, 0, 1024 * 3 * sizeof(accumulator_ref[0]));
    memset(count_ref, 0, 1024 * 3 * sizeof(count_ref[0]));
    DECLARE_ALIGNED(16, unsigned int, accumulator_mod[1024 * 3]);
    DECLARE_ALIGNED(16, uint16_t, count_mod[1024 * 3]);
    memset(accumulator_mod, 0, 1024 * 3 * sizeof(accumulator_mod[0]));
    memset(count_mod, 0, 1024 * 3 * sizeof(count_mod[0]));

    static_assert(width == 32 && height == 32, "");
    const MV subblock_mvs[4] = { { 0, 0 }, { 5, 5 }, { 7, 8 }, { 2, 10 } };
    const int subblock_mses[4] = { 15, 16, 17, 18 };
    const int q_factor = 12;
    const int filter_strength = 5;
    const int mb_row = 0;
    const int mb_col = 0;
    std::unique_ptr<YV12_BUFFER_CONFIG> frame_to_filter(new (std::nothrow)
                                                            YV12_BUFFER_CONFIG);
    ASSERT_NE(frame_to_filter, nullptr);
    frame_to_filter->y_crop_height = 360;
    frame_to_filter->y_crop_width = 540;
    frame_to_filter->heights[PLANE_TYPE_Y] = height;
    frame_to_filter->heights[PLANE_TYPE_UV] = height >> subsampling_y;
    frame_to_filter->strides[PLANE_TYPE_Y] = stride;
    frame_to_filter->strides[PLANE_TYPE_UV] = stride >> subsampling_x;
    DECLARE_ALIGNED(16, uint16_t, src[1024 * 3]);
    frame_to_filter->buffer_alloc = CONVERT_TO_BYTEPTR(src);
    frame_to_filter->flags =
        YV12_FLAG_HIGHBITDEPTH;  // Only Hihgbd bit-depth test.
    memcpy(src, src1_, 1024 * 3 * sizeof(uint16_t));

    std::unique_ptr<MACROBLOCKD> mbd(new (std::nothrow) MACROBLOCKD);
    ASSERT_NE(mbd, nullptr);
    mbd->bd = BD;
    for (int plane = AOM_PLANE_Y; plane < num_planes; plane++) {
      int plane_height = plane ? height >> subsampling_y : height;
      int plane_stride = plane ? stride >> subsampling_x : stride;
      frame_to_filter->buffers[plane] =
          frame_to_filter->buffer_alloc + plane * plane_stride * plane_height;
      mbd->plane[plane].subsampling_x = plane ? subsampling_x : 0;
      mbd->plane[plane].subsampling_y = plane ? subsampling_y : 0;
    }

    params_.ref_func(frame_to_filter.get(), mbd.get(), block_size, mb_row,
                     mb_col, num_planes, sigma, subblock_mvs, subblock_mses,
                     q_factor, filter_strength, tf_wgt_calc_lvl_,
                     CONVERT_TO_BYTEPTR(src2_), accumulator_ref, count_ref);
    params_.tst_func(frame_to_filter.get(), mbd.get(), block_size, mb_row,
                     mb_col, num_planes, sigma, subblock_mvs, subblock_mses,
                     q_factor, filter_strength, tf_wgt_calc_lvl_,
                     CONVERT_TO_BYTEPTR(src2_), accumulator_mod, count_mod);

    if (run_times > 1) {
      aom_usec_timer_start(&ref_timer);
      for (int j = 0; j < run_times; j++) {
        params_.ref_func(frame_to_filter.get(), mbd.get(), block_size, mb_row,
                         mb_col, num_planes, sigma, subblock_mvs, subblock_mses,
                         q_factor, filter_strength, tf_wgt_calc_lvl_,
                         CONVERT_TO_BYTEPTR(src2_), accumulator_ref, count_ref);
      }
      aom_usec_timer_mark(&ref_timer);
      const int elapsed_time_c =
          static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

      aom_usec_timer_start(&test_timer);
      for (int j = 0; j < run_times; j++) {
        params_.tst_func(frame_to_filter.get(), mbd.get(), block_size, mb_row,
                         mb_col, num_planes, sigma, subblock_mvs, subblock_mses,
                         q_factor, filter_strength, tf_wgt_calc_lvl_,
                         CONVERT_TO_BYTEPTR(src2_), accumulator_mod, count_mod);
      }
      aom_usec_timer_mark(&test_timer);
      const int elapsed_time_simd =
          static_cast<int>(aom_usec_timer_elapsed(&test_timer));

      printf(
          "c_time=%d \t simd_time=%d \t "
          "gain=%f\t width=%d\t height=%d\t color_format=%s\n",
          elapsed_time_c, elapsed_time_simd,
          (float)((float)elapsed_time_c / (float)elapsed_time_simd), width,
          height, color_fmt_str[color_fmt]);

    } else {
      for (int i = 0, l = 0; i < height; i++) {
        for (int j = 0; j < width; j++, l++) {
          EXPECT_EQ(accumulator_ref[l], accumulator_mod[l])
              << "Error:" << k << " SSE Sum Test [" << width << "x" << height
              << "] " << color_fmt_str[color_fmt]
              << " C accumulator does not match optimized accumulator.";
          EXPECT_EQ(count_ref[l], count_mod[l])
              << "Error:" << k << " SSE Sum Test [" << width << "x" << height
              << "] " << color_fmt_str[color_fmt]
              << " C count does not match optimized count.";
        }
      }
    }
  }
}

TEST_P(HBDTemporalFilterTest, OperationCheck) {
  RunTest(1, 1, 10, I400);
  RunTest(1, 1, 10, I420);
  RunTest(1, 1, 10, I422);
  RunTest(1, 1, 10, I444);
}

TEST_P(HBDTemporalFilterTest, ExtremeValues) {
  RunTest(0, 1, 10, I400);
  RunTest(0, 1, 10, I420);
  RunTest(0, 1, 10, I422);
  RunTest(0, 1, 10, I444);
}

TEST_P(HBDTemporalFilterTest, DISABLED_Speed) {
  RunTest(1, 100000, 10, I400);
  RunTest(1, 100000, 10, I420);
  RunTest(1, 100000, 10, I422);
  RunTest(1, 100000, 10, I444);
}
#if HAVE_SSE2
HBDTemporalFilterFuncParam HBDtemporal_filter_test_sse2[] = {
  HBDTemporalFilterFuncParam(&av1_highbd_apply_temporal_filter_c,
                             &av1_highbd_apply_temporal_filter_sse2)
};
INSTANTIATE_TEST_SUITE_P(SSE2, HBDTemporalFilterTest,
                         Combine(ValuesIn(HBDtemporal_filter_test_sse2),
                                 Values(0, 1)));
#endif  // HAVE_SSE2
#if HAVE_AVX2
HBDTemporalFilterFuncParam HBDtemporal_filter_test_avx2[] = {
  HBDTemporalFilterFuncParam(&av1_highbd_apply_temporal_filter_c,
                             &av1_highbd_apply_temporal_filter_avx2)
};
INSTANTIATE_TEST_SUITE_P(AVX2, HBDTemporalFilterTest,
                         Combine(ValuesIn(HBDtemporal_filter_test_avx2),
                                 Values(0, 1)));
#endif  // HAVE_AVX2
#endif  // CONFIG_AV1_HIGHBITDEPTH
}  // namespace
#endif
