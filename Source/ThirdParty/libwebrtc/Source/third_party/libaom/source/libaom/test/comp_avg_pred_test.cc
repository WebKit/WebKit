/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <tuple>

#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "test/register_state_check.h"
#include "av1/common/common_data.h"
#include "aom_ports/aom_timer.h"

using libaom_test::ACMRandom;
using std::make_tuple;
using std::tuple;

namespace {

const int kMaxSize = 128 + 32;  // padding

typedef void (*distwtdcompavg_func)(uint8_t *comp_pred, const uint8_t *pred,
                                    int width, int height, const uint8_t *ref,
                                    int ref_stride,
                                    const DIST_WTD_COMP_PARAMS *jcp_param);

typedef void (*distwtdcompavgupsampled_func)(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
    int ref_stride, const DIST_WTD_COMP_PARAMS *jcp_param, int subpel_search);

typedef void (*DistWtdCompAvgFunc)(uint8_t *comp_pred, const uint8_t *pred,
                                   int width, int height, const uint8_t *ref,
                                   int ref_stride,
                                   const DIST_WTD_COMP_PARAMS *jcp_param);

typedef std::tuple<distwtdcompavg_func, BLOCK_SIZE> AV1DistWtdCompAvgParam;

typedef std::tuple<distwtdcompavgupsampled_func, BLOCK_SIZE>
    AV1DistWtdCompAvgUpsampledParam;

typedef std::tuple<int, int, DistWtdCompAvgFunc, int> DistWtdCompAvgParam;

#if CONFIG_AV1_HIGHBITDEPTH
typedef void (*highbddistwtdcompavgupsampled_func)(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
    int ref_stride, int bd, const DIST_WTD_COMP_PARAMS *jcp_param,
    int subpel_search);

typedef std::tuple<int, highbddistwtdcompavgupsampled_func, BLOCK_SIZE>
    HighbdDistWtdCompAvgUpsampledParam;

typedef std::tuple<int, distwtdcompavg_func, BLOCK_SIZE>
    HighbdDistWtdCompAvgParam;

#if HAVE_SSE2 || HAVE_NEON
::testing::internal::ParamGenerator<HighbdDistWtdCompAvgParam> BuildParams(
    distwtdcompavg_func filter, int is_hbd) {
  (void)is_hbd;
  return ::testing::Combine(::testing::Range(8, 13, 2),
                            ::testing::Values(filter),
                            ::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL));
}

::testing::internal::ParamGenerator<HighbdDistWtdCompAvgUpsampledParam>
BuildParams(highbddistwtdcompavgupsampled_func filter) {
  return ::testing::Combine(::testing::Range(8, 13, 2),
                            ::testing::Values(filter),
                            ::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL));
}
#endif  // HAVE_SSE2 || HAVE_NEON
#endif  // CONFIG_AV1_HIGHBITDEPTH

#if HAVE_SSSE3
::testing::internal::ParamGenerator<AV1DistWtdCompAvgParam> BuildParams(
    distwtdcompavg_func filter) {
  return ::testing::Combine(::testing::Values(filter),
                            ::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL));
}
#endif  // HAVE_SSSE3

#if HAVE_SSSE3 || HAVE_NEON
::testing::internal::ParamGenerator<AV1DistWtdCompAvgUpsampledParam>
BuildParams(distwtdcompavgupsampled_func filter) {
  return ::testing::Combine(::testing::Values(filter),
                            ::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL));
}
#endif  // HAVE_SSSE3 || HAVE_NEON

class AV1DistWtdCompAvgTest
    : public ::testing::TestWithParam<AV1DistWtdCompAvgParam> {
 public:
  ~AV1DistWtdCompAvgTest() override = default;
  void SetUp() override { rnd_.Reset(ACMRandom::DeterministicSeed()); }

 protected:
  void RunCheckOutput(distwtdcompavg_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(1);

    uint8_t pred8[kMaxSize * kMaxSize];
    uint8_t ref8[kMaxSize * kMaxSize];
    uint8_t output[kMaxSize * kMaxSize];
    uint8_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand8();
        ref8[i * w + j] = rnd_.Rand8();
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;

    for (int ii = 0; ii < 2; ii++) {
      for (int jj = 0; jj < 4; jj++) {
        dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[jj][ii];
        dist_wtd_comp_params.bck_offset = quant_dist_lookup_table[jj][1 - ii];

        const int offset_r = 3 + rnd_.PseudoUniform(h - in_h - 7);
        const int offset_c = 3 + rnd_.PseudoUniform(w - in_w - 7);
        aom_dist_wtd_comp_avg_pred_c(output, pred8 + offset_r * w + offset_c,
                                     in_w, in_h, ref8 + offset_r * w + offset_c,
                                     in_w, &dist_wtd_comp_params);
        test_impl(output2, pred8 + offset_r * w + offset_c, in_w, in_h,
                  ref8 + offset_r * w + offset_c, in_w, &dist_wtd_comp_params);

        for (int i = 0; i < in_h; ++i) {
          for (int j = 0; j < in_w; ++j) {
            int idx = i * in_w + j;
            ASSERT_EQ(output[idx], output2[idx])
                << "Mismatch at unit tests for AV1DistWtdCompAvgTest\n"
                << in_w << "x" << in_h << " Pixel mismatch at index " << idx
                << " = (" << i << ", " << j << ")";
          }
        }
      }
    }
  }
  void RunSpeedTest(distwtdcompavg_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(1);

    uint8_t pred8[kMaxSize * kMaxSize];
    uint8_t ref8[kMaxSize * kMaxSize];
    uint8_t output[kMaxSize * kMaxSize];
    uint8_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand8();
        ref8[i * w + j] = rnd_.Rand8();
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;

    dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[0][0];
    dist_wtd_comp_params.bck_offset = quant_dist_lookup_table[0][1];

    const int num_loops = 1000000000 / (in_w + in_h);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);

    for (int i = 0; i < num_loops; ++i)
      aom_dist_wtd_comp_avg_pred_c(output, pred8, in_w, in_h, ref8, in_w,
                                   &dist_wtd_comp_params);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("distwtdcompavg c_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);

    for (int i = 0; i < num_loops; ++i)
      test_impl(output2, pred8, in_w, in_h, ref8, in_w, &dist_wtd_comp_params);

    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("distwtdcompavg test_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time1 / num_loops);
  }

  libaom_test::ACMRandom rnd_;
};  // class AV1DistWtdCompAvgTest

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1DistWtdCompAvgTest);

class AV1DistWtdCompAvgUpsampledTest
    : public ::testing::TestWithParam<AV1DistWtdCompAvgUpsampledParam> {
 public:
  ~AV1DistWtdCompAvgUpsampledTest() override = default;
  void SetUp() override { rnd_.Reset(ACMRandom::DeterministicSeed()); }

 protected:
  void RunCheckOutput(distwtdcompavgupsampled_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(1);

    uint8_t pred8[kMaxSize * kMaxSize];
    uint8_t ref8[kMaxSize * kMaxSize];
    DECLARE_ALIGNED(16, uint8_t, output[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint8_t, output2[MAX_SB_SQUARE]);

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand8();
        ref8[i * w + j] = rnd_.Rand8();
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;
    int sub_x_q3, sub_y_q3;
    int subpel_search;
    for (subpel_search = USE_4_TAPS; subpel_search <= USE_8_TAPS;
         ++subpel_search) {
      for (sub_x_q3 = 0; sub_x_q3 < 8; ++sub_x_q3) {
        for (sub_y_q3 = 0; sub_y_q3 < 8; ++sub_y_q3) {
          for (int ii = 0; ii < 2; ii++) {
            for (int jj = 0; jj < 4; jj++) {
              dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[jj][ii];
              dist_wtd_comp_params.bck_offset =
                  quant_dist_lookup_table[jj][1 - ii];

              const int offset_r = 3 + rnd_.PseudoUniform(h - in_h - 7);
              const int offset_c = 3 + rnd_.PseudoUniform(w - in_w - 7);

              aom_dist_wtd_comp_avg_upsampled_pred_c(
                  nullptr, nullptr, 0, 0, nullptr, output,
                  pred8 + offset_r * w + offset_c, in_w, in_h, sub_x_q3,
                  sub_y_q3, ref8 + offset_r * w + offset_c, in_w,
                  &dist_wtd_comp_params, subpel_search);
              test_impl(nullptr, nullptr, 0, 0, nullptr, output2,
                        pred8 + offset_r * w + offset_c, in_w, in_h, sub_x_q3,
                        sub_y_q3, ref8 + offset_r * w + offset_c, in_w,
                        &dist_wtd_comp_params, subpel_search);

              for (int i = 0; i < in_h; ++i) {
                for (int j = 0; j < in_w; ++j) {
                  int idx = i * in_w + j;
                  ASSERT_EQ(output[idx], output2[idx])
                      << "Mismatch at unit tests for "
                         "AV1DistWtdCompAvgUpsampledTest\n"
                      << in_w << "x" << in_h << " Pixel mismatch at index "
                      << idx << " = (" << i << ", " << j
                      << "), sub pixel offset = (" << sub_y_q3 << ", "
                      << sub_x_q3 << ")";
                }
              }
            }
          }
        }
      }
    }
  }
  void RunSpeedTest(distwtdcompavgupsampled_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(1);

    uint8_t pred8[kMaxSize * kMaxSize];
    uint8_t ref8[kMaxSize * kMaxSize];
    DECLARE_ALIGNED(16, uint8_t, output[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint8_t, output2[MAX_SB_SQUARE]);

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand8();
        ref8[i * w + j] = rnd_.Rand8();
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;

    dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[0][0];
    dist_wtd_comp_params.bck_offset = quant_dist_lookup_table[0][1];

    int sub_x_q3 = 0;
    int sub_y_q3 = 0;

    const int num_loops = 1000000000 / (in_w + in_h);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    int subpel_search = USE_8_TAPS;  // set to USE_4_TAPS to test 4-tap filter.

    for (int i = 0; i < num_loops; ++i)
      aom_dist_wtd_comp_avg_upsampled_pred_c(
          nullptr, nullptr, 0, 0, nullptr, output, pred8, in_w, in_h, sub_x_q3,
          sub_y_q3, ref8, in_w, &dist_wtd_comp_params, subpel_search);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("distwtdcompavgupsampled c_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);

    for (int i = 0; i < num_loops; ++i)
      test_impl(nullptr, nullptr, 0, 0, nullptr, output2, pred8, in_w, in_h,
                sub_x_q3, sub_y_q3, ref8, in_w, &dist_wtd_comp_params,
                subpel_search);

    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("distwtdcompavgupsampled test_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time1 / num_loops);
  }

  libaom_test::ACMRandom rnd_;
};  // class AV1DistWtdCompAvgUpsampledTest

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1DistWtdCompAvgUpsampledTest);

class DistWtdCompAvgTest
    : public ::testing::WithParamInterface<DistWtdCompAvgParam>,
      public ::testing::Test {
 public:
  DistWtdCompAvgTest()
      : width_(GET_PARAM(0)), height_(GET_PARAM(1)), bd_(GET_PARAM(3)) {}

  static void SetUpTestSuite() {
    reference_data8_ = reinterpret_cast<uint8_t *>(
        aom_memalign(kDataAlignment, kDataBufferSize));
    ASSERT_NE(reference_data8_, nullptr);
    second_pred8_ =
        reinterpret_cast<uint8_t *>(aom_memalign(kDataAlignment, 128 * 128));
    ASSERT_NE(second_pred8_, nullptr);
    comp_pred8_ =
        reinterpret_cast<uint8_t *>(aom_memalign(kDataAlignment, 128 * 128));
    ASSERT_NE(comp_pred8_, nullptr);
    comp_pred8_test_ =
        reinterpret_cast<uint8_t *>(aom_memalign(kDataAlignment, 128 * 128));
    ASSERT_NE(comp_pred8_test_, nullptr);
    reference_data16_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, kDataBufferSize * sizeof(uint16_t)));
    ASSERT_NE(reference_data16_, nullptr);
    second_pred16_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, 128 * 128 * sizeof(uint16_t)));
    ASSERT_NE(second_pred16_, nullptr);
    comp_pred16_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, 128 * 128 * sizeof(uint16_t)));
    ASSERT_NE(comp_pred16_, nullptr);
    comp_pred16_test_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, 128 * 128 * sizeof(uint16_t)));
    ASSERT_NE(comp_pred16_test_, nullptr);
  }

  static void TearDownTestSuite() {
    aom_free(reference_data8_);
    reference_data8_ = nullptr;
    aom_free(second_pred8_);
    second_pred8_ = nullptr;
    aom_free(comp_pred8_);
    comp_pred8_ = nullptr;
    aom_free(comp_pred8_test_);
    comp_pred8_test_ = nullptr;
    aom_free(reference_data16_);
    reference_data16_ = nullptr;
    aom_free(second_pred16_);
    second_pred16_ = nullptr;
    aom_free(comp_pred16_);
    comp_pred16_ = nullptr;
    aom_free(comp_pred16_test_);
    comp_pred16_test_ = nullptr;
  }

 protected:
  // Handle up to 4 128x128 blocks, with stride up to 256
  static const int kDataAlignment = 16;
  static const int kDataBlockSize = 128 * 256;
  static const int kDataBufferSize = 4 * kDataBlockSize;

  void SetUp() override {
    if (bd_ == -1) {
      use_high_bit_depth_ = false;
      bit_depth_ = AOM_BITS_8;
      reference_data_ = reference_data8_;
      second_pred_ = second_pred8_;
      comp_pred_ = comp_pred8_;
      comp_pred_test_ = comp_pred8_test_;
    } else {
      use_high_bit_depth_ = true;
      bit_depth_ = static_cast<aom_bit_depth_t>(bd_);
      reference_data_ = CONVERT_TO_BYTEPTR(reference_data16_);
      second_pred_ = CONVERT_TO_BYTEPTR(second_pred16_);
      comp_pred_ = CONVERT_TO_BYTEPTR(comp_pred16_);
      comp_pred_test_ = CONVERT_TO_BYTEPTR(comp_pred16_test_);
    }
    mask_ = (1 << bit_depth_) - 1;
    reference_stride_ = width_ * 2;
    rnd_.Reset(ACMRandom::DeterministicSeed());
  }

  virtual uint8_t *GetReference(int block_idx) {
    if (use_high_bit_depth_)
      return CONVERT_TO_BYTEPTR(CONVERT_TO_SHORTPTR(reference_data_) +
                                block_idx * kDataBlockSize);
    return reference_data_ + block_idx * kDataBlockSize;
  }

  void ReferenceDistWtdCompAvg(int block_idx) {
    const uint8_t *const reference8 = GetReference(block_idx);
    const uint8_t *const second_pred8 = second_pred_;
    uint8_t *const comp_pred8 = comp_pred_;
    const uint16_t *const reference16 =
        CONVERT_TO_SHORTPTR(GetReference(block_idx));
    const uint16_t *const second_pred16 = CONVERT_TO_SHORTPTR(second_pred_);
    uint16_t *const comp_pred16 = CONVERT_TO_SHORTPTR(comp_pred_);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          const int tmp =
              second_pred8[h * width_ + w] * jcp_param_.bck_offset +
              reference8[h * reference_stride_ + w] * jcp_param_.fwd_offset;
          comp_pred8[h * width_ + w] = ROUND_POWER_OF_TWO(tmp, 4);
        } else {
          const int tmp =
              second_pred16[h * width_ + w] * jcp_param_.bck_offset +
              reference16[h * reference_stride_ + w] * jcp_param_.fwd_offset;
          comp_pred16[h * width_ + w] = ROUND_POWER_OF_TWO(tmp, 4);
        }
      }
    }
  }

  void FillConstant(uint8_t *data, int stride, uint16_t fill_constant) {
    uint8_t *data8 = data;
    uint16_t *data16 = CONVERT_TO_SHORTPTR(data);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          data8[h * stride + w] = static_cast<uint8_t>(fill_constant);
        } else {
          data16[h * stride + w] = fill_constant;
        }
      }
    }
  }

  void FillRandom(uint8_t *data, int stride) {
    uint8_t *data8 = data;
    uint16_t *data16 = CONVERT_TO_SHORTPTR(data);
    for (int h = 0; h < height_; ++h) {
      for (int w = 0; w < width_; ++w) {
        if (!use_high_bit_depth_) {
          data8[h * stride + w] = rnd_.Rand8();
        } else {
          data16[h * stride + w] = rnd_.Rand16() & mask_;
        }
      }
    }
  }

  void dist_wtd_comp_avg(int block_idx) {
    const uint8_t *const reference = GetReference(block_idx);

    API_REGISTER_STATE_CHECK(GET_PARAM(2)(comp_pred_test_, second_pred_, width_,
                                          height_, reference, reference_stride_,
                                          &jcp_param_));
  }

  void CheckCompAvg() {
    for (int j = 0; j < 2; ++j) {
      for (int i = 0; i < 4; ++i) {
        jcp_param_.fwd_offset = quant_dist_lookup_table[i][j];
        jcp_param_.bck_offset = quant_dist_lookup_table[i][1 - j];

        ReferenceDistWtdCompAvg(0);
        dist_wtd_comp_avg(0);

        for (int y = 0; y < height_; ++y)
          for (int x = 0; x < width_; ++x)
            ASSERT_EQ(comp_pred_[y * width_ + x],
                      comp_pred_test_[y * width_ + x]);
      }
    }
  }

  int width_, height_, mask_, bd_;
  aom_bit_depth_t bit_depth_;
  static uint8_t *reference_data_;
  static uint8_t *second_pred_;
  bool use_high_bit_depth_;
  static uint8_t *reference_data8_;
  static uint8_t *second_pred8_;
  static uint16_t *reference_data16_;
  static uint16_t *second_pred16_;
  int reference_stride_;
  static uint8_t *comp_pred_;
  static uint8_t *comp_pred8_;
  static uint16_t *comp_pred16_;
  static uint8_t *comp_pred_test_;
  static uint8_t *comp_pred8_test_;
  static uint16_t *comp_pred16_test_;
  DIST_WTD_COMP_PARAMS jcp_param_;

  ACMRandom rnd_;
};

uint8_t *DistWtdCompAvgTest::reference_data_ = nullptr;
uint8_t *DistWtdCompAvgTest::second_pred_ = nullptr;
uint8_t *DistWtdCompAvgTest::comp_pred_ = nullptr;
uint8_t *DistWtdCompAvgTest::comp_pred_test_ = nullptr;
uint8_t *DistWtdCompAvgTest::reference_data8_ = nullptr;
uint8_t *DistWtdCompAvgTest::second_pred8_ = nullptr;
uint8_t *DistWtdCompAvgTest::comp_pred8_ = nullptr;
uint8_t *DistWtdCompAvgTest::comp_pred8_test_ = nullptr;
uint16_t *DistWtdCompAvgTest::reference_data16_ = nullptr;
uint16_t *DistWtdCompAvgTest::second_pred16_ = nullptr;
uint16_t *DistWtdCompAvgTest::comp_pred16_ = nullptr;
uint16_t *DistWtdCompAvgTest::comp_pred16_test_ = nullptr;

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DistWtdCompAvgTest);

#if CONFIG_AV1_HIGHBITDEPTH
class AV1HighBDDistWtdCompAvgTest
    : public ::testing::TestWithParam<HighbdDistWtdCompAvgParam> {
 public:
  ~AV1HighBDDistWtdCompAvgTest() override = default;
  void SetUp() override { rnd_.Reset(ACMRandom::DeterministicSeed()); }

 protected:
  void RunCheckOutput(distwtdcompavg_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    uint16_t pred8[kMaxSize * kMaxSize];
    uint16_t ref8[kMaxSize * kMaxSize];
    uint16_t output[kMaxSize * kMaxSize];
    uint16_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
        ref8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;

    for (int ii = 0; ii < 2; ii++) {
      for (int jj = 0; jj < 4; jj++) {
        dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[jj][ii];
        dist_wtd_comp_params.bck_offset = quant_dist_lookup_table[jj][1 - ii];

        const int offset_r = 3 + rnd_.PseudoUniform(h - in_h - 7);
        const int offset_c = 3 + rnd_.PseudoUniform(w - in_w - 7);
        aom_highbd_dist_wtd_comp_avg_pred_c(
            CONVERT_TO_BYTEPTR(output),
            CONVERT_TO_BYTEPTR(pred8) + offset_r * w + offset_c, in_w, in_h,
            CONVERT_TO_BYTEPTR(ref8) + offset_r * w + offset_c, in_w,
            &dist_wtd_comp_params);
        test_impl(CONVERT_TO_BYTEPTR(output2),
                  CONVERT_TO_BYTEPTR(pred8) + offset_r * w + offset_c, in_w,
                  in_h, CONVERT_TO_BYTEPTR(ref8) + offset_r * w + offset_c,
                  in_w, &dist_wtd_comp_params);

        for (int i = 0; i < in_h; ++i) {
          for (int j = 0; j < in_w; ++j) {
            int idx = i * in_w + j;
            ASSERT_EQ(output[idx], output2[idx])
                << "Mismatch at unit tests for AV1HighBDDistWtdCompAvgTest\n"
                << in_w << "x" << in_h << " Pixel mismatch at index " << idx
                << " = (" << i << ", " << j << ")";
          }
        }
      }
    }
  }
  void RunSpeedTest(distwtdcompavg_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    uint16_t pred8[kMaxSize * kMaxSize];
    uint16_t ref8[kMaxSize * kMaxSize];
    uint16_t output[kMaxSize * kMaxSize];
    uint16_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
        ref8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;

    dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[0][0];
    dist_wtd_comp_params.bck_offset = quant_dist_lookup_table[0][1];

    const int num_loops = 1000000000 / (in_w + in_h);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);

    for (int i = 0; i < num_loops; ++i)
      aom_highbd_dist_wtd_comp_avg_pred_c(
          CONVERT_TO_BYTEPTR(output), CONVERT_TO_BYTEPTR(pred8), in_w, in_h,
          CONVERT_TO_BYTEPTR(ref8), in_w, &dist_wtd_comp_params);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("highbddistwtdcompavg c_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);

    for (int i = 0; i < num_loops; ++i)
      test_impl(CONVERT_TO_BYTEPTR(output2), CONVERT_TO_BYTEPTR(pred8), in_w,
                in_h, CONVERT_TO_BYTEPTR(ref8), in_w, &dist_wtd_comp_params);

    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("highbddistwtdcompavg test_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time1 / num_loops);
  }

  libaom_test::ACMRandom rnd_;
};  // class AV1HighBDDistWtdCompAvgTest

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HighBDDistWtdCompAvgTest);

class AV1HighBDDistWtdCompAvgUpsampledTest
    : public ::testing::TestWithParam<HighbdDistWtdCompAvgUpsampledParam> {
 public:
  ~AV1HighBDDistWtdCompAvgUpsampledTest() override = default;
  void SetUp() override { rnd_.Reset(ACMRandom::DeterministicSeed()); }

 protected:
  void RunCheckOutput(highbddistwtdcompavgupsampled_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    uint16_t pred8[kMaxSize * kMaxSize];
    uint16_t ref8[kMaxSize * kMaxSize];
    DECLARE_ALIGNED(16, uint16_t, output[kMaxSize * kMaxSize]);
    DECLARE_ALIGNED(16, uint16_t, output2[kMaxSize * kMaxSize]);

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
        ref8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;
    int sub_x_q3, sub_y_q3;
    int subpel_search;
    for (subpel_search = USE_4_TAPS; subpel_search <= USE_8_TAPS;
         ++subpel_search) {
      for (sub_x_q3 = 0; sub_x_q3 < 8; ++sub_x_q3) {
        for (sub_y_q3 = 0; sub_y_q3 < 8; ++sub_y_q3) {
          for (int ii = 0; ii < 2; ii++) {
            for (int jj = 0; jj < 4; jj++) {
              dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[jj][ii];
              dist_wtd_comp_params.bck_offset =
                  quant_dist_lookup_table[jj][1 - ii];

              const int offset_r = 3 + rnd_.PseudoUniform(h - in_h - 7);
              const int offset_c = 3 + rnd_.PseudoUniform(w - in_w - 7);

              aom_highbd_dist_wtd_comp_avg_upsampled_pred_c(
                  nullptr, nullptr, 0, 0, nullptr, CONVERT_TO_BYTEPTR(output),
                  CONVERT_TO_BYTEPTR(pred8) + offset_r * w + offset_c, in_w,
                  in_h, sub_x_q3, sub_y_q3,
                  CONVERT_TO_BYTEPTR(ref8) + offset_r * w + offset_c, in_w, bd,
                  &dist_wtd_comp_params, subpel_search);
              test_impl(nullptr, nullptr, 0, 0, nullptr,
                        CONVERT_TO_BYTEPTR(output2),
                        CONVERT_TO_BYTEPTR(pred8) + offset_r * w + offset_c,
                        in_w, in_h, sub_x_q3, sub_y_q3,
                        CONVERT_TO_BYTEPTR(ref8) + offset_r * w + offset_c,
                        in_w, bd, &dist_wtd_comp_params, subpel_search);

              for (int i = 0; i < in_h; ++i) {
                for (int j = 0; j < in_w; ++j) {
                  int idx = i * in_w + j;
                  ASSERT_EQ(output[idx], output2[idx])
                      << "Mismatch at unit tests for "
                         "AV1HighBDDistWtdCompAvgUpsampledTest\n"
                      << in_w << "x" << in_h << " Pixel mismatch at index "
                      << idx << " = (" << i << ", " << j
                      << "), sub pixel offset = (" << sub_y_q3 << ", "
                      << sub_x_q3 << ")";
                }
              }
            }
          }
        }
      }
    }
  }
  void RunSpeedTest(highbddistwtdcompavgupsampled_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    uint16_t pred8[kMaxSize * kMaxSize];
    uint16_t ref8[kMaxSize * kMaxSize];
    DECLARE_ALIGNED(16, uint16_t, output[kMaxSize * kMaxSize]);
    DECLARE_ALIGNED(16, uint16_t, output2[kMaxSize * kMaxSize]);

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
        ref8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    DIST_WTD_COMP_PARAMS dist_wtd_comp_params;
    dist_wtd_comp_params.use_dist_wtd_comp_avg = 1;

    dist_wtd_comp_params.fwd_offset = quant_dist_lookup_table[0][0];
    dist_wtd_comp_params.bck_offset = quant_dist_lookup_table[0][1];
    int sub_x_q3 = 0;
    int sub_y_q3 = 0;
    const int num_loops = 1000000000 / (in_w + in_h);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    int subpel_search = USE_8_TAPS;  // set to USE_4_TAPS to test 4-tap filter.
    for (int i = 0; i < num_loops; ++i)
      aom_highbd_dist_wtd_comp_avg_upsampled_pred_c(
          nullptr, nullptr, 0, 0, nullptr, CONVERT_TO_BYTEPTR(output),
          CONVERT_TO_BYTEPTR(pred8), in_w, in_h, sub_x_q3, sub_y_q3,
          CONVERT_TO_BYTEPTR(ref8), in_w, bd, &dist_wtd_comp_params,
          subpel_search);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("highbddistwtdcompavgupsampled c_code %3dx%-3d: %7.2f us\n", in_w,
           in_h, 1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);

    for (int i = 0; i < num_loops; ++i)
      test_impl(nullptr, nullptr, 0, 0, nullptr, CONVERT_TO_BYTEPTR(output2),
                CONVERT_TO_BYTEPTR(pred8), in_w, in_h, sub_x_q3, sub_y_q3,
                CONVERT_TO_BYTEPTR(ref8), in_w, bd, &dist_wtd_comp_params,
                subpel_search);

    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("highbddistwtdcompavgupsampled test_code %3dx%-3d: %7.2f us\n", in_w,
           in_h, 1000.0 * elapsed_time1 / num_loops);
  }

  libaom_test::ACMRandom rnd_;
};  // class AV1HighBDDistWtdCompAvgUpsampledTest

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    AV1HighBDDistWtdCompAvgUpsampledTest);
#endif  // CONFIG_AV1_HIGHBITDEPTH

TEST_P(AV1DistWtdCompAvgTest, DISABLED_Speed) { RunSpeedTest(GET_PARAM(0)); }

TEST_P(AV1DistWtdCompAvgTest, CheckOutput) { RunCheckOutput(GET_PARAM(0)); }

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(SSSE3, AV1DistWtdCompAvgTest,
                         BuildParams(aom_dist_wtd_comp_avg_pred_ssse3));
#endif

TEST_P(AV1DistWtdCompAvgUpsampledTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(0));
}

TEST_P(AV1DistWtdCompAvgUpsampledTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(0));
}

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(
    SSSE3, AV1DistWtdCompAvgUpsampledTest,
    BuildParams(aom_dist_wtd_comp_avg_upsampled_pred_ssse3));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1DistWtdCompAvgUpsampledTest,
    BuildParams(aom_dist_wtd_comp_avg_upsampled_pred_neon));
#endif  // HAVE_NEON

TEST_P(DistWtdCompAvgTest, MaxRef) {
  FillConstant(reference_data_, reference_stride_, mask_);
  FillConstant(second_pred_, width_, 0);
  CheckCompAvg();
}

TEST_P(DistWtdCompAvgTest, MaxSecondPred) {
  FillConstant(reference_data_, reference_stride_, 0);
  FillConstant(second_pred_, width_, mask_);
  CheckCompAvg();
}

TEST_P(DistWtdCompAvgTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckCompAvg();
  reference_stride_ = tmp_stride;
}

TEST_P(DistWtdCompAvgTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, width_);
  CheckCompAvg();
  reference_stride_ = tmp_stride;
}

// TODO(chengchen): add highbd tests
const DistWtdCompAvgParam dist_wtd_comp_avg_c_tests[] = {
  make_tuple(128, 128, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(128, 64, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(64, 128, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(64, 64, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(64, 32, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(32, 64, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(32, 32, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(32, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 32, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 8, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(8, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(8, 8, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(8, 4, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(4, 8, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(4, 4, &aom_dist_wtd_comp_avg_pred_c, -1),

#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 64, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(32, 8, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(8, 32, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(16, 4, &aom_dist_wtd_comp_avg_pred_c, -1),
  make_tuple(4, 16, &aom_dist_wtd_comp_avg_pred_c, -1),
#endif
};

INSTANTIATE_TEST_SUITE_P(C, DistWtdCompAvgTest,
                         ::testing::ValuesIn(dist_wtd_comp_avg_c_tests));

#if HAVE_SSSE3
const DistWtdCompAvgParam dist_wtd_comp_avg_ssse3_tests[] = {
  make_tuple(128, 128, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(128, 64, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(64, 128, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(64, 64, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(64, 32, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(32, 64, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(32, 32, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(32, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 32, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 8, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(8, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(8, 8, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(8, 4, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(4, 8, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(4, 4, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 64, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(32, 8, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(8, 32, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(16, 4, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
  make_tuple(4, 16, &aom_dist_wtd_comp_avg_pred_ssse3, -1),
#endif
};

INSTANTIATE_TEST_SUITE_P(SSSE3, DistWtdCompAvgTest,
                         ::testing::ValuesIn(dist_wtd_comp_avg_ssse3_tests));
#endif  // HAVE_SSSE3

#if HAVE_NEON
const DistWtdCompAvgParam dist_wtd_comp_avg_neon_tests[] = {
  make_tuple(128, 128, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(128, 64, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(64, 128, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(64, 64, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(64, 32, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(32, 64, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(32, 32, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(32, 16, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(16, 32, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(16, 16, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(16, 8, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(8, 16, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(8, 8, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(8, 4, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(4, 8, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(4, 4, &aom_dist_wtd_comp_avg_pred_neon, -1),
#if !CONFIG_REALTIME_ONLY
  make_tuple(64, 16, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(16, 64, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(32, 8, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(8, 32, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(16, 4, &aom_dist_wtd_comp_avg_pred_neon, -1),
  make_tuple(4, 16, &aom_dist_wtd_comp_avg_pred_neon, -1),
#endif  // !CONFIG_REALTIME_ONLY
};

INSTANTIATE_TEST_SUITE_P(NEON, DistWtdCompAvgTest,
                         ::testing::ValuesIn(dist_wtd_comp_avg_neon_tests));
#endif  // HAVE_NEON

#if CONFIG_AV1_HIGHBITDEPTH
TEST_P(AV1HighBDDistWtdCompAvgTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(1));
}

TEST_P(AV1HighBDDistWtdCompAvgTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(1));
}

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, AV1HighBDDistWtdCompAvgTest,
                         BuildParams(aom_highbd_dist_wtd_comp_avg_pred_sse2,
                                     1));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1HighBDDistWtdCompAvgTest,
                         BuildParams(aom_highbd_dist_wtd_comp_avg_pred_neon,
                                     1));
#endif

TEST_P(AV1HighBDDistWtdCompAvgUpsampledTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(1));
}

TEST_P(AV1HighBDDistWtdCompAvgUpsampledTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(1));
}

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, AV1HighBDDistWtdCompAvgUpsampledTest,
    BuildParams(aom_highbd_dist_wtd_comp_avg_upsampled_pred_sse2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1HighBDDistWtdCompAvgUpsampledTest,
    BuildParams(aom_highbd_dist_wtd_comp_avg_upsampled_pred_neon));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

}  // namespace
