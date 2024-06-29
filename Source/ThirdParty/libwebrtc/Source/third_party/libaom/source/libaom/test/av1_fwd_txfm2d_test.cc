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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <tuple>
#include <vector>

#include "config/av1_rtcd.h"

#include "test/acm_random.h"
#include "test/util.h"
#include "test/av1_txfm_test.h"
#include "av1/common/av1_txfm.h"
#include "av1/encoder/hybrid_fwd_txfm.h"

using libaom_test::ACMRandom;
using libaom_test::bd;
using libaom_test::compute_avg_abs_error;
using libaom_test::input_base;
using libaom_test::tx_type_name;
using libaom_test::TYPE_TXFM;

using std::vector;

namespace {
// tx_type_, tx_size_, max_error_, max_avg_error_
typedef std::tuple<TX_TYPE, TX_SIZE, double, double> AV1FwdTxfm2dParam;

class AV1FwdTxfm2d : public ::testing::TestWithParam<AV1FwdTxfm2dParam> {
 public:
  void SetUp() override {
    tx_type_ = GET_PARAM(0);
    tx_size_ = GET_PARAM(1);
    max_error_ = GET_PARAM(2);
    max_avg_error_ = GET_PARAM(3);
    count_ = 500;
    TXFM_2D_FLIP_CFG fwd_txfm_flip_cfg;
    av1_get_fwd_txfm_cfg(tx_type_, tx_size_, &fwd_txfm_flip_cfg);
    amplify_factor_ = libaom_test::get_amplification_factor(tx_type_, tx_size_);
    tx_width_ = tx_size_wide[fwd_txfm_flip_cfg.tx_size];
    tx_height_ = tx_size_high[fwd_txfm_flip_cfg.tx_size];
    ud_flip_ = fwd_txfm_flip_cfg.ud_flip;
    lr_flip_ = fwd_txfm_flip_cfg.lr_flip;

    fwd_txfm_ = libaom_test::fwd_txfm_func_ls[tx_size_];
    txfm2d_size_ = tx_width_ * tx_height_;
    input_ = reinterpret_cast<int16_t *>(
        aom_memalign(16, sizeof(input_[0]) * txfm2d_size_));
    ASSERT_NE(input_, nullptr);
    output_ = reinterpret_cast<int32_t *>(
        aom_memalign(16, sizeof(output_[0]) * txfm2d_size_));
    ASSERT_NE(output_, nullptr);
    ref_input_ = reinterpret_cast<double *>(
        aom_memalign(16, sizeof(ref_input_[0]) * txfm2d_size_));
    ASSERT_NE(ref_input_, nullptr);
    ref_output_ = reinterpret_cast<double *>(
        aom_memalign(16, sizeof(ref_output_[0]) * txfm2d_size_));
    ASSERT_NE(ref_output_, nullptr);
  }

  void RunFwdAccuracyCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    double avg_abs_error = 0;
    for (int ci = 0; ci < count_; ci++) {
      for (int ni = 0; ni < txfm2d_size_; ++ni) {
        input_[ni] = rnd.Rand16() % input_base;
        ref_input_[ni] = static_cast<double>(input_[ni]);
        output_[ni] = 0;
        ref_output_[ni] = 0;
      }

      fwd_txfm_(input_, output_, tx_width_, tx_type_, bd);

      if (lr_flip_ && ud_flip_) {
        libaom_test::fliplrud(ref_input_, tx_width_, tx_height_, tx_width_);
      } else if (lr_flip_) {
        libaom_test::fliplr(ref_input_, tx_width_, tx_height_, tx_width_);
      } else if (ud_flip_) {
        libaom_test::flipud(ref_input_, tx_width_, tx_height_, tx_width_);
      }

      libaom_test::reference_hybrid_2d(ref_input_, ref_output_, tx_type_,
                                       tx_size_);

      double actual_max_error = 0;
      for (int ni = 0; ni < txfm2d_size_; ++ni) {
        ref_output_[ni] = round(ref_output_[ni]);
        const double this_error =
            fabs(output_[ni] - ref_output_[ni]) / amplify_factor_;
        actual_max_error = AOMMAX(actual_max_error, this_error);
      }
      EXPECT_GE(max_error_, actual_max_error)
          << "tx_w: " << tx_width_ << " tx_h: " << tx_height_
          << ", tx_type = " << (int)tx_type_;
      if (actual_max_error > max_error_) {  // exit early.
        break;
      }

      avg_abs_error += compute_avg_abs_error<int32_t, double>(
          output_, ref_output_, txfm2d_size_);
    }

    avg_abs_error /= amplify_factor_;
    avg_abs_error /= count_;
    EXPECT_GE(max_avg_error_, avg_abs_error)
        << "tx_size = " << tx_size_ << ", tx_type = " << tx_type_;
  }

  void TearDown() override {
    aom_free(input_);
    aom_free(output_);
    aom_free(ref_input_);
    aom_free(ref_output_);
  }

 private:
  double max_error_;
  double max_avg_error_;
  int count_;
  double amplify_factor_;
  TX_TYPE tx_type_;
  TX_SIZE tx_size_;
  int tx_width_;
  int tx_height_;
  int txfm2d_size_;
  FwdTxfm2dFunc fwd_txfm_;
  int16_t *input_;
  int32_t *output_;
  double *ref_input_;
  double *ref_output_;
  int ud_flip_;  // flip upside down
  int lr_flip_;  // flip left to right
};

static double avg_error_ls[TX_SIZES_ALL] = {
  0.5,   // 4x4 transform
  0.5,   // 8x8 transform
  1.2,   // 16x16 transform
  6.1,   // 32x32 transform
  3.4,   // 64x64 transform
  0.57,  // 4x8 transform
  0.68,  // 8x4 transform
  0.92,  // 8x16 transform
  1.1,   // 16x8 transform
  4.1,   // 16x32 transform
  6,     // 32x16 transform
  3.5,   // 32x64 transform
  5.7,   // 64x32 transform
  0.6,   // 4x16 transform
  0.9,   // 16x4 transform
  1.2,   // 8x32 transform
  1.7,   // 32x8 transform
  2.0,   // 16x64 transform
  4.7,   // 64x16 transform
};

static double max_error_ls[TX_SIZES_ALL] = {
  3,    // 4x4 transform
  5,    // 8x8 transform
  11,   // 16x16 transform
  70,   // 32x32 transform
  64,   // 64x64 transform
  3.9,  // 4x8 transform
  4.3,  // 8x4 transform
  12,   // 8x16 transform
  12,   // 16x8 transform
  32,   // 16x32 transform
  46,   // 32x16 transform
  136,  // 32x64 transform
  136,  // 64x32 transform
  5,    // 4x16 transform
  6,    // 16x4 transform
  21,   // 8x32 transform
  13,   // 32x8 transform
  30,   // 16x64 transform
  36,   // 64x16 transform
};

vector<AV1FwdTxfm2dParam> GetTxfm2dParamList() {
  vector<AV1FwdTxfm2dParam> param_list;
  for (int s = 0; s < TX_SIZES; ++s) {
    const double max_error = max_error_ls[s];
    const double avg_error = avg_error_ls[s];
    for (int t = 0; t < TX_TYPES; ++t) {
      const TX_TYPE tx_type = static_cast<TX_TYPE>(t);
      const TX_SIZE tx_size = static_cast<TX_SIZE>(s);
      if (libaom_test::IsTxSizeTypeValid(tx_size, tx_type)) {
        param_list.push_back(
            AV1FwdTxfm2dParam(tx_type, tx_size, max_error, avg_error));
      }
    }
  }
  return param_list;
}

INSTANTIATE_TEST_SUITE_P(C, AV1FwdTxfm2d,
                         ::testing::ValuesIn(GetTxfm2dParamList()));

TEST_P(AV1FwdTxfm2d, RunFwdAccuracyCheck) { RunFwdAccuracyCheck(); }

TEST(AV1FwdTxfm2d, CfgTest) {
  for (int bd_idx = 0; bd_idx < BD_NUM; ++bd_idx) {
    int bd = libaom_test::bd_arr[bd_idx];
    int8_t low_range = libaom_test::low_range_arr[bd_idx];
    int8_t high_range = libaom_test::high_range_arr[bd_idx];
    for (int tx_size = 0; tx_size < TX_SIZES_ALL; ++tx_size) {
      for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
        if (libaom_test::IsTxSizeTypeValid(static_cast<TX_SIZE>(tx_size),
                                           static_cast<TX_TYPE>(tx_type)) ==
            false) {
          continue;
        }
        TXFM_2D_FLIP_CFG cfg;
        av1_get_fwd_txfm_cfg(static_cast<TX_TYPE>(tx_type),
                             static_cast<TX_SIZE>(tx_size), &cfg);
        int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
        int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
        av1_gen_fwd_stage_range(stage_range_col, stage_range_row, &cfg, bd);
        libaom_test::txfm_stage_range_check(stage_range_col, cfg.stage_num_col,
                                            cfg.cos_bit_col, low_range,
                                            high_range);
        libaom_test::txfm_stage_range_check(stage_range_row, cfg.stage_num_row,
                                            cfg.cos_bit_row, low_range,
                                            high_range);
      }
    }
  }
}

typedef void (*lowbd_fwd_txfm_func)(const int16_t *src_diff, tran_low_t *coeff,
                                    int diff_stride, TxfmParam *txfm_param);

void AV1FwdTxfm2dMatchTest(TX_SIZE tx_size, lowbd_fwd_txfm_func target_func) {
  const int bd = 8;
  TxfmParam param;
  memset(&param, 0, sizeof(param));
  const int rows = tx_size_high[tx_size];
  const int cols = tx_size_wide[tx_size];
  // printf("%d x %d\n", cols, rows);
  for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
    if (libaom_test::IsTxSizeTypeValid(
            tx_size, static_cast<TX_TYPE>(tx_type)) == false) {
      continue;
    }

    FwdTxfm2dFunc ref_func = libaom_test::fwd_txfm_func_ls[tx_size];
    if (ref_func != nullptr) {
      DECLARE_ALIGNED(32, int16_t, input[64 * 64]) = { 0 };
      DECLARE_ALIGNED(32, int32_t, output[64 * 64]);
      DECLARE_ALIGNED(32, int32_t, ref_output[64 * 64]);
      int input_stride = 64;
      ACMRandom rnd(ACMRandom::DeterministicSeed());
      for (int cnt = 0; cnt < 500; ++cnt) {
        if (cnt == 0) {
          for (int c = 0; c < cols; ++c) {
            for (int r = 0; r < rows; ++r) {
              input[r * input_stride + c] = (1 << bd) - 1;
            }
          }
        } else {
          for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
              input[r * input_stride + c] = rnd.Rand16() % (1 << bd);
            }
          }
        }
        param.tx_type = (TX_TYPE)tx_type;
        param.tx_size = (TX_SIZE)tx_size;
        param.tx_set_type = EXT_TX_SET_ALL16;
        param.bd = bd;
        ref_func(input, ref_output, input_stride, (TX_TYPE)tx_type, bd);
        target_func(input, output, input_stride, &param);
        const int check_cols = AOMMIN(32, cols);
        const int check_rows = AOMMIN(32, rows * cols / check_cols);
        for (int r = 0; r < check_rows; ++r) {
          for (int c = 0; c < check_cols; ++c) {
            ASSERT_EQ(ref_output[r * check_cols + c],
                      output[r * check_cols + c])
                << "[" << r << "," << c << "] cnt:" << cnt
                << " tx_size: " << cols << "x" << rows
                << " tx_type: " << tx_type_name[tx_type];
          }
        }
      }
    }
  }
}

void AV1FwdTxfm2dSpeedTest(TX_SIZE tx_size, lowbd_fwd_txfm_func target_func) {
  TxfmParam param;
  memset(&param, 0, sizeof(param));
  const int rows = tx_size_high[tx_size];
  const int cols = tx_size_wide[tx_size];
  const int num_loops = 1000000 / (rows * cols);

  const int bd = 8;
  for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
    if (libaom_test::IsTxSizeTypeValid(
            tx_size, static_cast<TX_TYPE>(tx_type)) == false) {
      continue;
    }

    FwdTxfm2dFunc ref_func = libaom_test::fwd_txfm_func_ls[tx_size];
    if (ref_func != nullptr) {
      DECLARE_ALIGNED(32, int16_t, input[64 * 64]) = { 0 };
      DECLARE_ALIGNED(32, int32_t, output[64 * 64]);
      DECLARE_ALIGNED(32, int32_t, ref_output[64 * 64]);
      int input_stride = 64;
      ACMRandom rnd(ACMRandom::DeterministicSeed());

      for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
          input[r * input_stride + c] = rnd.Rand16() % (1 << bd);
        }
      }

      param.tx_type = (TX_TYPE)tx_type;
      param.tx_size = (TX_SIZE)tx_size;
      param.tx_set_type = EXT_TX_SET_ALL16;
      param.bd = bd;

      aom_usec_timer ref_timer, test_timer;

      aom_usec_timer_start(&ref_timer);
      for (int i = 0; i < num_loops; ++i) {
        ref_func(input, ref_output, input_stride, (TX_TYPE)tx_type, bd);
      }
      aom_usec_timer_mark(&ref_timer);
      const int elapsed_time_c =
          static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

      aom_usec_timer_start(&test_timer);
      for (int i = 0; i < num_loops; ++i) {
        target_func(input, output, input_stride, &param);
      }
      aom_usec_timer_mark(&test_timer);
      const int elapsed_time_simd =
          static_cast<int>(aom_usec_timer_elapsed(&test_timer));

      printf(
          "txfm_size[%2dx%-2d] \t txfm_type[%d] \t c_time=%d \t"
          "simd_time=%d \t gain=%d \n",
          rows, cols, tx_type, elapsed_time_c, elapsed_time_simd,
          (elapsed_time_c / elapsed_time_simd));
    }
  }
}

typedef std::tuple<TX_SIZE, lowbd_fwd_txfm_func> LbdFwdTxfm2dParam;

class AV1FwdTxfm2dTest : public ::testing::TestWithParam<LbdFwdTxfm2dParam> {};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1FwdTxfm2dTest);

TEST_P(AV1FwdTxfm2dTest, match) {
  AV1FwdTxfm2dMatchTest(GET_PARAM(0), GET_PARAM(1));
}
TEST_P(AV1FwdTxfm2dTest, DISABLED_Speed) {
  AV1FwdTxfm2dSpeedTest(GET_PARAM(0), GET_PARAM(1));
}
TEST(AV1FwdTxfm2dTest, DCTScaleTest) {
  BitDepthInfo bd_info;
  bd_info.bit_depth = 8;
  bd_info.use_highbitdepth_buf = 0;
  DECLARE_ALIGNED(32, int16_t, src_diff[1024]);
  DECLARE_ALIGNED(32, tran_low_t, coeff[1024]);

  const TX_SIZE tx_size_list[4] = { TX_4X4, TX_8X8, TX_16X16, TX_32X32 };
  const int stride_list[4] = { 4, 8, 16, 32 };
  const int ref_scale_list[4] = { 64, 64, 64, 16 };

  for (int i = 0; i < 4; i++) {
    TX_SIZE tx_size = tx_size_list[i];
    int stride = stride_list[i];
    int array_size = stride * stride;

    for (int j = 0; j < array_size; j++) {
      src_diff[j] = 8;
      coeff[j] = 0;
    }

    av1_quick_txfm(/*use_hadamard=*/0, tx_size, bd_info, src_diff, stride,
                   coeff);

    double input_sse = 0;
    double output_sse = 0;
    for (int j = 0; j < array_size; j++) {
      input_sse += pow(src_diff[j], 2);
      output_sse += pow(coeff[j], 2);
    }

    double scale = output_sse / input_sse;

    EXPECT_NEAR(scale, ref_scale_list[i], 5);
  }
}
TEST(AV1FwdTxfm2dTest, HadamardScaleTest) {
  BitDepthInfo bd_info;
  bd_info.bit_depth = 8;
  bd_info.use_highbitdepth_buf = 0;
  DECLARE_ALIGNED(32, int16_t, src_diff[1024]);
  DECLARE_ALIGNED(32, tran_low_t, coeff[1024]);

  const TX_SIZE tx_size_list[4] = { TX_4X4, TX_8X8, TX_16X16, TX_32X32 };
  const int stride_list[4] = { 4, 8, 16, 32 };
  const int ref_scale_list[4] = { 1, 64, 64, 16 };

  for (int i = 0; i < 4; i++) {
    TX_SIZE tx_size = tx_size_list[i];
    int stride = stride_list[i];
    int array_size = stride * stride;

    for (int j = 0; j < array_size; j++) {
      src_diff[j] = 8;
      coeff[j] = 0;
    }

    av1_quick_txfm(/*use_hadamard=*/1, tx_size, bd_info, src_diff, stride,
                   coeff);

    double input_sse = 0;
    double output_sse = 0;
    for (int j = 0; j < array_size; j++) {
      input_sse += pow(src_diff[j], 2);
      output_sse += pow(coeff[j], 2);
    }

    double scale = output_sse / input_sse;

    EXPECT_NEAR(scale, ref_scale_list[i], 5);
  }
}
using ::testing::Combine;
using ::testing::Values;
using ::testing::ValuesIn;

#if AOM_ARCH_X86 && HAVE_SSE2
static TX_SIZE fwd_txfm_for_sse2[] = {
  TX_4X4,
  TX_8X8,
  TX_16X16,
  TX_32X32,
  // TX_64X64,
  TX_4X8,
  TX_8X4,
  TX_8X16,
  TX_16X8,
  TX_16X32,
  TX_32X16,
  // TX_32X64,
  // TX_64X32,
  TX_4X16,
  TX_16X4,
  TX_8X32,
  TX_32X8,
  TX_16X64,
  TX_64X16,
};

INSTANTIATE_TEST_SUITE_P(SSE2, AV1FwdTxfm2dTest,
                         Combine(ValuesIn(fwd_txfm_for_sse2),
                                 Values(av1_lowbd_fwd_txfm_sse2)));
#endif  // AOM_ARCH_X86 && HAVE_SSE2

#if HAVE_SSE4_1
static TX_SIZE fwd_txfm_for_sse41[] = { TX_4X4,   TX_8X8,   TX_16X16, TX_32X32,
                                        TX_64X64, TX_4X8,   TX_8X4,   TX_8X16,
                                        TX_16X8,  TX_16X32, TX_32X16, TX_32X64,
                                        TX_64X32, TX_4X16,  TX_16X4,  TX_8X32,
                                        TX_32X8,  TX_16X64, TX_64X16 };

INSTANTIATE_TEST_SUITE_P(SSE4_1, AV1FwdTxfm2dTest,
                         Combine(ValuesIn(fwd_txfm_for_sse41),
                                 Values(av1_lowbd_fwd_txfm_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
static TX_SIZE fwd_txfm_for_avx2[] = {
  TX_4X4,  TX_8X8,  TX_16X16, TX_32X32, TX_64X64, TX_4X8,   TX_8X4,
  TX_8X16, TX_16X8, TX_16X32, TX_32X16, TX_32X64, TX_64X32, TX_4X16,
  TX_16X4, TX_8X32, TX_32X8,  TX_16X64, TX_64X16,
};

INSTANTIATE_TEST_SUITE_P(AVX2, AV1FwdTxfm2dTest,
                         Combine(ValuesIn(fwd_txfm_for_avx2),
                                 Values(av1_lowbd_fwd_txfm_avx2)));
#endif  // HAVE_AVX2

#if HAVE_NEON

static TX_SIZE fwd_txfm_for_neon[] = { TX_4X4,   TX_8X8,   TX_16X16, TX_32X32,
                                       TX_64X64, TX_4X8,   TX_8X4,   TX_8X16,
                                       TX_16X8,  TX_16X32, TX_32X16, TX_32X64,
                                       TX_64X32, TX_4X16,  TX_16X4,  TX_8X32,
                                       TX_32X8,  TX_16X64, TX_64X16 };

INSTANTIATE_TEST_SUITE_P(NEON, AV1FwdTxfm2dTest,
                         Combine(ValuesIn(fwd_txfm_for_neon),
                                 Values(av1_lowbd_fwd_txfm_neon)));

#endif  // HAVE_NEON

typedef void (*Highbd_fwd_txfm_func)(const int16_t *src_diff, tran_low_t *coeff,
                                     int diff_stride, TxfmParam *txfm_param);

void AV1HighbdFwdTxfm2dMatchTest(TX_SIZE tx_size,
                                 Highbd_fwd_txfm_func target_func) {
  const int bd_ar[2] = { 10, 12 };
  TxfmParam param;
  memset(&param, 0, sizeof(param));
  const int rows = tx_size_high[tx_size];
  const int cols = tx_size_wide[tx_size];
  for (int i = 0; i < 2; ++i) {
    const int bd = bd_ar[i];
    for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
      if (libaom_test::IsTxSizeTypeValid(
              tx_size, static_cast<TX_TYPE>(tx_type)) == false) {
        continue;
      }

      FwdTxfm2dFunc ref_func = libaom_test::fwd_txfm_func_ls[tx_size];
      if (ref_func != nullptr) {
        DECLARE_ALIGNED(32, int16_t, input[64 * 64]) = { 0 };
        DECLARE_ALIGNED(32, int32_t, output[64 * 64]);
        DECLARE_ALIGNED(32, int32_t, ref_output[64 * 64]);
        int input_stride = 64;
        ACMRandom rnd(ACMRandom::DeterministicSeed());
        for (int cnt = 0; cnt < 500; ++cnt) {
          if (cnt == 0) {
            for (int r = 0; r < rows; ++r) {
              for (int c = 0; c < cols; ++c) {
                input[r * input_stride + c] = (1 << bd) - 1;
              }
            }
          } else {
            for (int r = 0; r < rows; ++r) {
              for (int c = 0; c < cols; ++c) {
                input[r * input_stride + c] = rnd.Rand16() % (1 << bd);
              }
            }
          }
          param.tx_type = (TX_TYPE)tx_type;
          param.tx_size = (TX_SIZE)tx_size;
          param.tx_set_type = EXT_TX_SET_ALL16;
          param.bd = bd;

          ref_func(input, ref_output, input_stride, (TX_TYPE)tx_type, bd);
          target_func(input, output, input_stride, &param);
          const int check_cols = AOMMIN(32, cols);
          const int check_rows = AOMMIN(32, rows * cols / check_cols);
          for (int r = 0; r < check_rows; ++r) {
            for (int c = 0; c < check_cols; ++c) {
              ASSERT_EQ(ref_output[c * check_rows + r],
                        output[c * check_rows + r])
                  << "[" << r << "," << c << "] cnt:" << cnt
                  << " tx_size: " << cols << "x" << rows
                  << " tx_type: " << tx_type;
            }
          }
        }
      }
    }
  }
}

void AV1HighbdFwdTxfm2dSpeedTest(TX_SIZE tx_size,
                                 Highbd_fwd_txfm_func target_func) {
  const int bd_ar[2] = { 10, 12 };
  TxfmParam param;
  memset(&param, 0, sizeof(param));
  const int rows = tx_size_high[tx_size];
  const int cols = tx_size_wide[tx_size];
  const int num_loops = 1000000 / (rows * cols);

  for (int i = 0; i < 2; ++i) {
    const int bd = bd_ar[i];
    for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
      if (libaom_test::IsTxSizeTypeValid(
              tx_size, static_cast<TX_TYPE>(tx_type)) == false) {
        continue;
      }

      FwdTxfm2dFunc ref_func = libaom_test::fwd_txfm_func_ls[tx_size];
      if (ref_func != nullptr) {
        DECLARE_ALIGNED(32, int16_t, input[64 * 64]) = { 0 };
        DECLARE_ALIGNED(32, int32_t, output[64 * 64]);
        DECLARE_ALIGNED(32, int32_t, ref_output[64 * 64]);
        int input_stride = 64;
        ACMRandom rnd(ACMRandom::DeterministicSeed());

        for (int r = 0; r < rows; ++r) {
          for (int c = 0; c < cols; ++c) {
            input[r * input_stride + c] = rnd.Rand16() % (1 << bd);
          }
        }

        param.tx_type = (TX_TYPE)tx_type;
        param.tx_size = (TX_SIZE)tx_size;
        param.tx_set_type = EXT_TX_SET_ALL16;
        param.bd = bd;

        aom_usec_timer ref_timer, test_timer;

        aom_usec_timer_start(&ref_timer);
        for (int j = 0; j < num_loops; ++j) {
          ref_func(input, ref_output, input_stride, (TX_TYPE)tx_type, bd);
        }
        aom_usec_timer_mark(&ref_timer);
        const int elapsed_time_c =
            static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

        aom_usec_timer_start(&test_timer);
        for (int j = 0; j < num_loops; ++j) {
          target_func(input, output, input_stride, &param);
        }
        aom_usec_timer_mark(&test_timer);
        const int elapsed_time_simd =
            static_cast<int>(aom_usec_timer_elapsed(&test_timer));

        printf(
            "txfm_size[%2dx%-2d] \t txfm_type[%d] \t c_time=%d \t"
            "simd_time=%d \t gain=%d \n",
            cols, rows, tx_type, elapsed_time_c, elapsed_time_simd,
            (elapsed_time_c / elapsed_time_simd));
      }
    }
  }
}

typedef std::tuple<TX_SIZE, Highbd_fwd_txfm_func> HighbdFwdTxfm2dParam;

class AV1HighbdFwdTxfm2dTest
    : public ::testing::TestWithParam<HighbdFwdTxfm2dParam> {};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HighbdFwdTxfm2dTest);

TEST_P(AV1HighbdFwdTxfm2dTest, match) {
  AV1HighbdFwdTxfm2dMatchTest(GET_PARAM(0), GET_PARAM(1));
}

TEST_P(AV1HighbdFwdTxfm2dTest, DISABLED_Speed) {
  AV1HighbdFwdTxfm2dSpeedTest(GET_PARAM(0), GET_PARAM(1));
}

using ::testing::Combine;
using ::testing::Values;
using ::testing::ValuesIn;

#if HAVE_SSE4_1
static TX_SIZE Highbd_fwd_txfm_for_sse4_1[] = {
  TX_4X4,  TX_8X8,  TX_16X16, TX_32X32, TX_64X64, TX_4X8,   TX_8X4,
  TX_8X16, TX_16X8, TX_16X32, TX_32X16, TX_32X64, TX_64X32,
#if !CONFIG_REALTIME_ONLY
  TX_4X16, TX_16X4, TX_8X32,  TX_32X8,  TX_16X64, TX_64X16,
#endif
};

INSTANTIATE_TEST_SUITE_P(SSE4_1, AV1HighbdFwdTxfm2dTest,
                         Combine(ValuesIn(Highbd_fwd_txfm_for_sse4_1),
                                 Values(av1_highbd_fwd_txfm)));
#endif  // HAVE_SSE4_1
#if HAVE_AVX2
static TX_SIZE Highbd_fwd_txfm_for_avx2[] = { TX_8X8,   TX_16X16, TX_32X32,
                                              TX_64X64, TX_8X16,  TX_16X8 };

INSTANTIATE_TEST_SUITE_P(AVX2, AV1HighbdFwdTxfm2dTest,
                         Combine(ValuesIn(Highbd_fwd_txfm_for_avx2),
                                 Values(av1_highbd_fwd_txfm)));
#endif  // HAVE_AVX2

#if HAVE_NEON
static TX_SIZE Highbd_fwd_txfm_for_neon[] = {
  TX_4X4,  TX_8X8,  TX_16X16, TX_32X32, TX_64X64, TX_4X8,   TX_8X4,
  TX_8X16, TX_16X8, TX_16X32, TX_32X16, TX_32X64, TX_64X32, TX_4X16,
  TX_16X4, TX_8X32, TX_32X8,  TX_16X64, TX_64X16
};

INSTANTIATE_TEST_SUITE_P(NEON, AV1HighbdFwdTxfm2dTest,
                         Combine(ValuesIn(Highbd_fwd_txfm_for_neon),
                                 Values(av1_highbd_fwd_txfm)));
#endif  // HAVE_NEON

}  // namespace
