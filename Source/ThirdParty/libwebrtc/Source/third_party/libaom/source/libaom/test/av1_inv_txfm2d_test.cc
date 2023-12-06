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

#include "aom_ports/aom_timer.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/scan.h"
#include "test/acm_random.h"
#include "test/av1_txfm_test.h"
#include "test/util.h"

using libaom_test::ACMRandom;
using libaom_test::bd;
using libaom_test::compute_avg_abs_error;
using libaom_test::input_base;
using libaom_test::InvTxfm2dFunc;
using libaom_test::LbdInvTxfm2dFunc;
using libaom_test::tx_type_name;

using ::testing::Combine;
using ::testing::Range;
using ::testing::Values;

using std::vector;

typedef TX_TYPE TxType;
typedef TX_SIZE TxSize;

namespace {

// AV1InvTxfm2dParam argument list:
// tx_type_, tx_size_, max_error_, max_avg_error_
typedef std::tuple<TxType, TxSize, int, double> AV1InvTxfm2dParam;

class AV1InvTxfm2d : public ::testing::TestWithParam<AV1InvTxfm2dParam> {
 public:
  void SetUp() override {
    tx_type_ = GET_PARAM(0);
    tx_size_ = GET_PARAM(1);
    max_error_ = GET_PARAM(2);
    max_avg_error_ = GET_PARAM(3);
  }

  void RunRoundtripCheck() {
    int tx_w = tx_size_wide[tx_size_];
    int tx_h = tx_size_high[tx_size_];
    int txfm2d_size = tx_w * tx_h;
    const FwdTxfm2dFunc fwd_txfm_func = libaom_test::fwd_txfm_func_ls[tx_size_];
    const InvTxfm2dFunc inv_txfm_func = libaom_test::inv_txfm_func_ls[tx_size_];
    double avg_abs_error = 0;
    ACMRandom rnd(ACMRandom::DeterministicSeed());

    const int count = 500;

    for (int ci = 0; ci < count; ci++) {
      DECLARE_ALIGNED(16, int16_t, input[64 * 64]) = { 0 };
      ASSERT_LE(txfm2d_size, NELEMENTS(input));

      for (int ni = 0; ni < txfm2d_size; ++ni) {
        if (ci == 0) {
          int extreme_input = input_base - 1;
          input[ni] = extreme_input;  // extreme case
        } else {
          input[ni] = rnd.Rand16() % input_base;
        }
      }

      DECLARE_ALIGNED(16, uint16_t, expected[64 * 64]) = { 0 };
      ASSERT_LE(txfm2d_size, NELEMENTS(expected));
      if (TxfmUsesApproximation()) {
        // Compare reference forward HT + inverse HT vs forward HT + inverse HT.
        double ref_input[64 * 64];
        ASSERT_LE(txfm2d_size, NELEMENTS(ref_input));
        for (int ni = 0; ni < txfm2d_size; ++ni) {
          ref_input[ni] = input[ni];
        }
        double ref_coeffs[64 * 64] = { 0 };
        ASSERT_LE(txfm2d_size, NELEMENTS(ref_coeffs));
        ASSERT_EQ(tx_type_, static_cast<TxType>(DCT_DCT));
        libaom_test::reference_hybrid_2d(ref_input, ref_coeffs, tx_type_,
                                         tx_size_);
        DECLARE_ALIGNED(16, int32_t, ref_coeffs_int[64 * 64]) = { 0 };
        ASSERT_LE(txfm2d_size, NELEMENTS(ref_coeffs_int));
        for (int ni = 0; ni < txfm2d_size; ++ni) {
          ref_coeffs_int[ni] = (int32_t)round(ref_coeffs[ni]);
        }
        inv_txfm_func(ref_coeffs_int, expected, tx_w, tx_type_, bd);
      } else {
        // Compare original input vs forward HT + inverse HT.
        for (int ni = 0; ni < txfm2d_size; ++ni) {
          expected[ni] = input[ni];
        }
      }

      DECLARE_ALIGNED(16, int32_t, coeffs[64 * 64]) = { 0 };
      ASSERT_LE(txfm2d_size, NELEMENTS(coeffs));
      fwd_txfm_func(input, coeffs, tx_w, tx_type_, bd);

      DECLARE_ALIGNED(16, uint16_t, actual[64 * 64]) = { 0 };
      ASSERT_LE(txfm2d_size, NELEMENTS(actual));
      inv_txfm_func(coeffs, actual, tx_w, tx_type_, bd);

      double actual_max_error = 0;
      for (int ni = 0; ni < txfm2d_size; ++ni) {
        const double this_error = abs(expected[ni] - actual[ni]);
        actual_max_error = AOMMAX(actual_max_error, this_error);
      }
      EXPECT_GE(max_error_, actual_max_error)
          << " tx_w: " << tx_w << " tx_h " << tx_h
          << " tx_type: " << tx_type_name[tx_type_];
      if (actual_max_error > max_error_) {  // exit early.
        break;
      }
      avg_abs_error += compute_avg_abs_error<uint16_t, uint16_t>(
          expected, actual, txfm2d_size);
    }

    avg_abs_error /= count;
    EXPECT_GE(max_avg_error_, avg_abs_error)
        << " tx_w: " << tx_w << " tx_h " << tx_h
        << " tx_type: " << tx_type_name[tx_type_];
  }

 private:
  bool TxfmUsesApproximation() {
    if (tx_size_wide[tx_size_] == 64 || tx_size_high[tx_size_] == 64) {
      return true;
    }
    return false;
  }

  int max_error_;
  double max_avg_error_;
  TxType tx_type_;
  TxSize tx_size_;
};

static int max_error_ls[TX_SIZES_ALL] = {
  2,  // 4x4 transform
  2,  // 8x8 transform
  2,  // 16x16 transform
  4,  // 32x32 transform
  3,  // 64x64 transform
  2,  // 4x8 transform
  2,  // 8x4 transform
  2,  // 8x16 transform
  2,  // 16x8 transform
  3,  // 16x32 transform
  3,  // 32x16 transform
  5,  // 32x64 transform
  5,  // 64x32 transform
  2,  // 4x16 transform
  2,  // 16x4 transform
  2,  // 8x32 transform
  2,  // 32x8 transform
  3,  // 16x64 transform
  3,  // 64x16 transform
};

static double avg_error_ls[TX_SIZES_ALL] = {
  0.002,  // 4x4 transform
  0.05,   // 8x8 transform
  0.07,   // 16x16 transform
  0.4,    // 32x32 transform
  0.3,    // 64x64 transform
  0.02,   // 4x8 transform
  0.02,   // 8x4 transform
  0.04,   // 8x16 transform
  0.07,   // 16x8 transform
  0.4,    // 16x32 transform
  0.5,    // 32x16 transform
  0.38,   // 32x64 transform
  0.39,   // 64x32 transform
  0.2,    // 4x16 transform
  0.2,    // 16x4 transform
  0.2,    // 8x32 transform
  0.2,    // 32x8 transform
  0.38,   // 16x64 transform
  0.38,   // 64x16 transform
};

vector<AV1InvTxfm2dParam> GetInvTxfm2dParamList() {
  vector<AV1InvTxfm2dParam> param_list;
  for (int s = 0; s < TX_SIZES; ++s) {
    const int max_error = max_error_ls[s];
    const double avg_error = avg_error_ls[s];
    for (int t = 0; t < TX_TYPES; ++t) {
      const TxType tx_type = static_cast<TxType>(t);
      const TxSize tx_size = static_cast<TxSize>(s);
      if (libaom_test::IsTxSizeTypeValid(tx_size, tx_type)) {
        param_list.push_back(
            AV1InvTxfm2dParam(tx_type, tx_size, max_error, avg_error));
      }
    }
  }
  return param_list;
}

INSTANTIATE_TEST_SUITE_P(C, AV1InvTxfm2d,
                         ::testing::ValuesIn(GetInvTxfm2dParamList()));

TEST_P(AV1InvTxfm2d, RunRoundtripCheck) { RunRoundtripCheck(); }

TEST(AV1InvTxfm2d, CfgTest) {
  for (int bd_idx = 0; bd_idx < BD_NUM; ++bd_idx) {
    int bd = libaom_test::bd_arr[bd_idx];
    int8_t low_range = libaom_test::low_range_arr[bd_idx];
    int8_t high_range = libaom_test::high_range_arr[bd_idx];
    for (int tx_size = 0; tx_size < TX_SIZES_ALL; ++tx_size) {
      for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
        if (libaom_test::IsTxSizeTypeValid(static_cast<TxSize>(tx_size),
                                           static_cast<TxType>(tx_type)) ==
            false) {
          continue;
        }
        TXFM_2D_FLIP_CFG cfg;
        av1_get_inv_txfm_cfg(static_cast<TxType>(tx_type),
                             static_cast<TxSize>(tx_size), &cfg);
        int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
        int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
        av1_gen_inv_stage_range(stage_range_col, stage_range_row, &cfg,
                                static_cast<TxSize>(tx_size), bd);
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

typedef std::tuple<const LbdInvTxfm2dFunc> AV1LbdInvTxfm2dParam;
class AV1LbdInvTxfm2d : public ::testing::TestWithParam<AV1LbdInvTxfm2dParam> {
 public:
  void SetUp() override { target_func_ = GET_PARAM(0); }
  void RunAV1InvTxfm2dTest(TxType tx_type, TxSize tx_size, int run_times,
                           int gt_int16 = 0);

 private:
  LbdInvTxfm2dFunc target_func_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1LbdInvTxfm2d);

void AV1LbdInvTxfm2d::RunAV1InvTxfm2dTest(TxType tx_type, TxSize tx_size,
                                          int run_times, int gt_int16) {
  FwdTxfm2dFunc fwd_func_ = libaom_test::fwd_txfm_func_ls[tx_size];
  InvTxfm2dFunc ref_func_ = libaom_test::inv_txfm_func_ls[tx_size];
  if (fwd_func_ == nullptr || ref_func_ == nullptr || target_func_ == nullptr) {
    return;
  }
  const int bd = 8;
  const int BLK_WIDTH = 64;
  const int BLK_SIZE = BLK_WIDTH * BLK_WIDTH;
  DECLARE_ALIGNED(16, int16_t, input[BLK_SIZE]) = { 0 };
  DECLARE_ALIGNED(32, int32_t, inv_input[BLK_SIZE]) = { 0 };
  DECLARE_ALIGNED(16, uint8_t, output[BLK_SIZE]) = { 0 };
  DECLARE_ALIGNED(16, uint16_t, ref_output[BLK_SIZE]) = { 0 };
  int stride = BLK_WIDTH;
  int rows = tx_size_high[tx_size];
  int cols = tx_size_wide[tx_size];
  const int rows_nonezero = AOMMIN(32, rows);
  const int cols_nonezero = AOMMIN(32, cols);
  run_times /= (rows * cols);
  run_times = AOMMAX(1, run_times);
  const SCAN_ORDER *scan_order = get_default_scan(tx_size, tx_type);
  const int16_t *scan = scan_order->scan;
  const int16_t eobmax = rows_nonezero * cols_nonezero;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  int randTimes = run_times == 1 ? (eobmax + 500) : 1;

  for (int cnt = 0; cnt < randTimes; ++cnt) {
    const int16_t max_in = (1 << (bd)) - 1;
    for (int r = 0; r < BLK_WIDTH; ++r) {
      for (int c = 0; c < BLK_WIDTH; ++c) {
        input[r * cols + c] = (cnt == 0) ? max_in : rnd.Rand8Extremes();
        output[r * stride + c] = (cnt == 0) ? 128 : rnd.Rand8();
        ref_output[r * stride + c] = output[r * stride + c];
      }
    }
    fwd_func_(input, inv_input, stride, tx_type, bd);

    // produce eob input by setting high freq coeffs to zero
    const int eob = AOMMIN(cnt + 1, eobmax);
    for (int i = eob; i < eobmax; i++) {
      inv_input[scan[i]] = 0;
    }
    if (gt_int16) {
      inv_input[scan[eob - 1]] = ((int32_t)INT16_MAX * 100 / 141);
    }
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      ref_func_(inv_input, ref_output, stride, tx_type, bd);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      target_func_(inv_input, output, stride, tx_type, tx_size, eob);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    if (run_times > 10) {
      printf("txfm[%d] %3dx%-3d:%7.2f/%7.2fns", tx_type, cols, rows, time1,
             time2);
      printf("(%3.2f)\n", time1 / time2);
    }
    for (int r = 0; r < rows; ++r) {
      for (int c = 0; c < cols; ++c) {
        uint8_t ref_value = static_cast<uint8_t>(ref_output[r * stride + c]);
        if (ref_value != output[r * stride + c]) {
          printf(" ");
        }
        ASSERT_EQ(ref_value, output[r * stride + c])
            << "[" << r << "," << c << "] " << cnt << " tx_size: " << cols
            << "x" << rows << " tx_type: " << tx_type_name[tx_type] << " eob "
            << eob;
      }
    }
  }
}

TEST_P(AV1LbdInvTxfm2d, match) {
  for (int j = 0; j < (int)(TX_SIZES_ALL); ++j) {
    for (int i = 0; i < (int)TX_TYPES; ++i) {
      if (libaom_test::IsTxSizeTypeValid(static_cast<TxSize>(j),
                                         static_cast<TxType>(i))) {
        RunAV1InvTxfm2dTest(static_cast<TxType>(i), static_cast<TxSize>(j), 1);
      }
    }
  }
}

TEST_P(AV1LbdInvTxfm2d, gt_int16) {
  static const TxType types[] = { DCT_DCT, ADST_DCT, FLIPADST_DCT, IDTX,
                                  V_DCT,   H_DCT,    H_ADST,       H_FLIPADST };
  for (int j = 0; j < (int)(TX_SIZES_ALL); ++j) {
    const TxSize sz = static_cast<TxSize>(j);
    for (uint8_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
      const TxType tp = types[i];
      if (libaom_test::IsTxSizeTypeValid(sz, tp)) {
        RunAV1InvTxfm2dTest(tp, sz, 1, 1);
      }
    }
  }
}

TEST_P(AV1LbdInvTxfm2d, DISABLED_Speed) {
  for (int j = 1; j < (int)(TX_SIZES_ALL); ++j) {
    for (int i = 0; i < (int)TX_TYPES; ++i) {
      if (libaom_test::IsTxSizeTypeValid(static_cast<TxSize>(j),
                                         static_cast<TxType>(i))) {
        RunAV1InvTxfm2dTest(static_cast<TxType>(i), static_cast<TxSize>(j),
                            10000000);
      }
    }
  }
}

#if HAVE_SSSE3
extern "C" void av1_lowbd_inv_txfm2d_add_ssse3(const int32_t *input,
                                               uint8_t *output, int stride,
                                               TxType tx_type, TxSize tx_size,
                                               int eob);
INSTANTIATE_TEST_SUITE_P(SSSE3, AV1LbdInvTxfm2d,
                         ::testing::Values(av1_lowbd_inv_txfm2d_add_ssse3));
#endif  // HAVE_SSSE3

#if HAVE_AVX2
extern "C" void av1_lowbd_inv_txfm2d_add_avx2(const int32_t *input,
                                              uint8_t *output, int stride,
                                              TxType tx_type, TxSize tx_size,
                                              int eob);

INSTANTIATE_TEST_SUITE_P(AVX2, AV1LbdInvTxfm2d,
                         ::testing::Values(av1_lowbd_inv_txfm2d_add_avx2));
#endif  // HAVE_AVX2

#if HAVE_NEON
extern "C" void av1_lowbd_inv_txfm2d_add_neon(const int32_t *input,
                                              uint8_t *output, int stride,
                                              TX_TYPE tx_type, TX_SIZE tx_size,
                                              int eob);

INSTANTIATE_TEST_SUITE_P(NEON, AV1LbdInvTxfm2d,
                         ::testing::Values(av1_lowbd_inv_txfm2d_add_neon));
#endif  // HAVE_NEON

}  // namespace
