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
#include <stdlib.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "av1/common/scan.h"
#include "av1/encoder/av1_quantize.h"

namespace {

typedef void (*QuantizeFpFunc)(
    const tran_low_t *coeff_ptr, intptr_t count, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan, int log_scale);

struct QuantizeFuncParams {
  QuantizeFuncParams(QuantizeFpFunc qF = nullptr,
                     QuantizeFpFunc qRefF = nullptr, int count = 16)
      : qFunc(qF), qFuncRef(qRefF), coeffCount(count) {}
  QuantizeFpFunc qFunc;
  QuantizeFpFunc qFuncRef;
  int coeffCount;
};

using libaom_test::ACMRandom;

const int numTests = 1000;
const int maxSize = 1024;
const int roundFactorRange = 127;
const int dequantRange = 32768;
const int coeffRange = (1 << 20) - 1;

class AV1QuantizeTest : public ::testing::TestWithParam<QuantizeFuncParams> {
 public:
  void RunQuantizeTest() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    DECLARE_ALIGNED(16, tran_low_t, coeff_ptr[maxSize]);
    DECLARE_ALIGNED(16, int16_t, zbin_ptr[8]);
    DECLARE_ALIGNED(16, int16_t, round_ptr[8]);
    DECLARE_ALIGNED(16, int16_t, quant_ptr[8]);
    DECLARE_ALIGNED(16, int16_t, quant_shift_ptr[8]);
    DECLARE_ALIGNED(16, tran_low_t, qcoeff_ptr[maxSize]);
    DECLARE_ALIGNED(16, tran_low_t, dqcoeff_ptr[maxSize]);
    DECLARE_ALIGNED(16, tran_low_t, ref_qcoeff_ptr[maxSize]);
    DECLARE_ALIGNED(16, tran_low_t, ref_dqcoeff_ptr[maxSize]);
    DECLARE_ALIGNED(16, int16_t, dequant_ptr[8]);
    uint16_t eob;
    uint16_t ref_eob;
    int err_count_total = 0;
    int first_failure = -1;
    int count = params_.coeffCount;
    const TX_SIZE txSize = getTxSize(count);
    int log_scale = (txSize == TX_32X32);
    QuantizeFpFunc quanFunc = params_.qFunc;
    QuantizeFpFunc quanFuncRef = params_.qFuncRef;

    const SCAN_ORDER scanOrder = av1_scan_orders[txSize][DCT_DCT];
    for (int i = 0; i < numTests; i++) {
      int err_count = 0;
      ref_eob = eob = UINT16_MAX;
      for (int j = 0; j < count; j++) {
        coeff_ptr[j] = rnd(coeffRange);
      }

      for (int j = 0; j < 2; j++) {
        zbin_ptr[j] = rnd.Rand16();
        quant_shift_ptr[j] = rnd.Rand16();
        // int16_t positive
        dequant_ptr[j] = abs(rnd(dequantRange));
        quant_ptr[j] = static_cast<int16_t>((1 << 16) / dequant_ptr[j]);
        round_ptr[j] = (abs(rnd(roundFactorRange)) * dequant_ptr[j]) >> 7;
      }
      for (int j = 2; j < 8; ++j) {
        zbin_ptr[j] = zbin_ptr[1];
        quant_shift_ptr[j] = quant_shift_ptr[1];
        dequant_ptr[j] = dequant_ptr[1];
        quant_ptr[j] = quant_ptr[1];
        round_ptr[j] = round_ptr[1];
      }
      quanFuncRef(coeff_ptr, count, zbin_ptr, round_ptr, quant_ptr,
                  quant_shift_ptr, ref_qcoeff_ptr, ref_dqcoeff_ptr, dequant_ptr,
                  &ref_eob, scanOrder.scan, scanOrder.iscan, log_scale);

      API_REGISTER_STATE_CHECK(
          quanFunc(coeff_ptr, count, zbin_ptr, round_ptr, quant_ptr,
                   quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr, &eob,
                   scanOrder.scan, scanOrder.iscan, log_scale));

      for (int j = 0; j < count; ++j) {
        err_count += (ref_qcoeff_ptr[j] != qcoeff_ptr[j]) |
                     (ref_dqcoeff_ptr[j] != dqcoeff_ptr[j]);
        ASSERT_EQ(ref_qcoeff_ptr[j], qcoeff_ptr[j])
            << "qcoeff error: i = " << i << " j = " << j << "\n";
        EXPECT_EQ(ref_dqcoeff_ptr[j], dqcoeff_ptr[j])
            << "dqcoeff error: i = " << i << " j = " << j << "\n";
      }
      EXPECT_EQ(ref_eob, eob) << "eob error: "
                              << "i = " << i << "\n";
      err_count += (ref_eob != eob);
      if (err_count && !err_count_total) {
        first_failure = i;
      }
      err_count_total += err_count;
    }
    EXPECT_EQ(0, err_count_total)
        << "Error: Quantization Test, C output doesn't match SSE2 output. "
        << "First failed at test case " << first_failure;
  }

  void RunEobTest() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    DECLARE_ALIGNED(16, tran_low_t, coeff_ptr[maxSize]);
    DECLARE_ALIGNED(16, int16_t, zbin_ptr[8]);
    DECLARE_ALIGNED(16, int16_t, round_ptr[8]);
    DECLARE_ALIGNED(16, int16_t, quant_ptr[8]);
    DECLARE_ALIGNED(16, int16_t, quant_shift_ptr[8]);
    DECLARE_ALIGNED(16, tran_low_t, qcoeff_ptr[maxSize]);
    DECLARE_ALIGNED(16, tran_low_t, dqcoeff_ptr[maxSize]);
    DECLARE_ALIGNED(16, tran_low_t, ref_qcoeff_ptr[maxSize]);
    DECLARE_ALIGNED(16, tran_low_t, ref_dqcoeff_ptr[maxSize]);
    DECLARE_ALIGNED(16, int16_t, dequant_ptr[8]);
    uint16_t eob;
    uint16_t ref_eob;
    int count = params_.coeffCount;
    const TX_SIZE txSize = getTxSize(count);
    int log_scale = (txSize == TX_32X32);
    QuantizeFpFunc quanFunc = params_.qFunc;
    QuantizeFpFunc quanFuncRef = params_.qFuncRef;
    const SCAN_ORDER scanOrder = av1_scan_orders[txSize][DCT_DCT];

    for (int i = 0; i < numTests; i++) {
      ref_eob = eob = UINT16_MAX;
      for (int j = 0; j < count; j++) {
        coeff_ptr[j] = 0;
      }

      coeff_ptr[rnd(count)] = rnd(coeffRange);
      coeff_ptr[rnd(count)] = rnd(coeffRange);
      coeff_ptr[rnd(count)] = rnd(coeffRange);

      for (int j = 0; j < 2; j++) {
        zbin_ptr[j] = rnd.Rand16();
        quant_shift_ptr[j] = rnd.Rand16();
        // int16_t positive
        dequant_ptr[j] = abs(rnd(dequantRange));
        quant_ptr[j] = (1 << 16) / dequant_ptr[j];
        round_ptr[j] = (abs(rnd(roundFactorRange)) * dequant_ptr[j]) >> 7;
      }
      for (int j = 2; j < 8; ++j) {
        zbin_ptr[j] = zbin_ptr[1];
        quant_shift_ptr[j] = quant_shift_ptr[1];
        dequant_ptr[j] = dequant_ptr[1];
        quant_ptr[j] = quant_ptr[1];
        round_ptr[j] = round_ptr[1];
      }

      quanFuncRef(coeff_ptr, count, zbin_ptr, round_ptr, quant_ptr,
                  quant_shift_ptr, ref_qcoeff_ptr, ref_dqcoeff_ptr, dequant_ptr,
                  &ref_eob, scanOrder.scan, scanOrder.iscan, log_scale);

      API_REGISTER_STATE_CHECK(
          quanFunc(coeff_ptr, count, zbin_ptr, round_ptr, quant_ptr,
                   quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr, &eob,
                   scanOrder.scan, scanOrder.iscan, log_scale));
      EXPECT_EQ(ref_eob, eob) << "eob error: "
                              << "i = " << i << "\n";
    }
  }

  virtual void SetUp() { params_ = GetParam(); }

  virtual void TearDown() {}

  virtual ~AV1QuantizeTest() {}

 private:
  TX_SIZE getTxSize(int count) {
    switch (count) {
      case 16: return TX_4X4;
      case 64: return TX_8X8;
      case 256: return TX_16X16;
      case 1024: return TX_32X32;
      default: return TX_4X4;
    }
  }

  QuantizeFuncParams params_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1QuantizeTest);

TEST_P(AV1QuantizeTest, BitExactCheck) { RunQuantizeTest(); }
TEST_P(AV1QuantizeTest, EobVerify) { RunEobTest(); }

TEST(AV1QuantizeTest, QuantizeFpNoQmatrix) {
  // Here we use a uniform quantizer as an example
  const int16_t dequant_ptr[2] = { 78, 93 };  // quantize step
  const int16_t round_ptr[2] = { 39, 46 };    // round ~= dequant / 2

  // quant ~= 2^16 / dequant. This is a 16-bit fixed point representation of the
  // inverse of quantize step.
  const int16_t quant_ptr[2] = { 840, 704 };
  int log_scale = 0;
  int coeff_count = 4;
  const tran_low_t coeff_ptr[4] = { -449, 624, -14, 24 };
  const tran_low_t ref_qcoeff_ptr[4] = { -6, 7, 0, 0 };
  const tran_low_t ref_dqcoeff_ptr[4] = { -468, 651, 0, 0 };
  const int16_t scan[4] = { 0, 1, 2, 3 };
  tran_low_t qcoeff_ptr[4];
  tran_low_t dqcoeff_ptr[4];
  int eob = av1_quantize_fp_no_qmatrix(quant_ptr, dequant_ptr, round_ptr,
                                       log_scale, scan, coeff_count, coeff_ptr,
                                       qcoeff_ptr, dqcoeff_ptr);
  EXPECT_EQ(eob, 2);
  for (int i = 0; i < coeff_count; ++i) {
    EXPECT_EQ(qcoeff_ptr[i], ref_qcoeff_ptr[i]);
    EXPECT_EQ(dqcoeff_ptr[i], ref_dqcoeff_ptr[i]);
  }
}

#if HAVE_SSE4_1
const QuantizeFuncParams qfps[4] = {
  QuantizeFuncParams(&av1_highbd_quantize_fp_sse4_1, &av1_highbd_quantize_fp_c,
                     16),
  QuantizeFuncParams(&av1_highbd_quantize_fp_sse4_1, &av1_highbd_quantize_fp_c,
                     64),
  QuantizeFuncParams(&av1_highbd_quantize_fp_sse4_1, &av1_highbd_quantize_fp_c,
                     256),
  QuantizeFuncParams(&av1_highbd_quantize_fp_sse4_1, &av1_highbd_quantize_fp_c,
                     1024),
};

INSTANTIATE_TEST_SUITE_P(SSE4_1, AV1QuantizeTest, ::testing::ValuesIn(qfps));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
const QuantizeFuncParams qfps_avx2[4] = {
  QuantizeFuncParams(&av1_highbd_quantize_fp_avx2, &av1_highbd_quantize_fp_c,
                     16),
  QuantizeFuncParams(&av1_highbd_quantize_fp_avx2, &av1_highbd_quantize_fp_c,
                     64),
  QuantizeFuncParams(&av1_highbd_quantize_fp_avx2, &av1_highbd_quantize_fp_c,
                     256),
  QuantizeFuncParams(&av1_highbd_quantize_fp_avx2, &av1_highbd_quantize_fp_c,
                     1024),
};

INSTANTIATE_TEST_SUITE_P(AVX2, AV1QuantizeTest, ::testing::ValuesIn(qfps_avx2));
#endif  // HAVE_AVX2

}  // namespace
