/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <tuple>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vp9_rtcd.h"
#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/bench.h"
#include "test/buffer.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "vp9/common/vp9_entropy.h"
#include "vp9/common/vp9_scan.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_integer.h"
#include "vpx_ports/msvc.h"
#include "vpx_ports/vpx_timer.h"

using libvpx_test::ACMRandom;
using libvpx_test::Buffer;

namespace {
const int number_of_iterations = 100;

typedef void (*QuantizeFunc)(const tran_low_t *coeff, intptr_t count,
                             int skip_block, const int16_t *zbin,
                             const int16_t *round, const int16_t *quant,
                             const int16_t *quant_shift, tran_low_t *qcoeff,
                             tran_low_t *dqcoeff, const int16_t *dequant,
                             uint16_t *eob, const int16_t *scan,
                             const int16_t *iscan);
typedef std::tuple<QuantizeFunc, QuantizeFunc, vpx_bit_depth_t,
                   int /*max_size*/, bool /*is_fp*/>
    QuantizeParam;

// Wrapper for FP version which does not use zbin or quant_shift.
typedef void (*QuantizeFPFunc)(const tran_low_t *coeff, intptr_t count,
                               int skip_block, const int16_t *round,
                               const int16_t *quant, tran_low_t *qcoeff,
                               tran_low_t *dqcoeff, const int16_t *dequant,
                               uint16_t *eob, const int16_t *scan,
                               const int16_t *iscan);

template <QuantizeFPFunc fn>
void QuantFPWrapper(const tran_low_t *coeff, intptr_t count, int skip_block,
                    const int16_t *zbin, const int16_t *round,
                    const int16_t *quant, const int16_t *quant_shift,
                    tran_low_t *qcoeff, tran_low_t *dqcoeff,
                    const int16_t *dequant, uint16_t *eob, const int16_t *scan,
                    const int16_t *iscan) {
  (void)zbin;
  (void)quant_shift;

  fn(coeff, count, skip_block, round, quant, qcoeff, dqcoeff, dequant, eob,
     scan, iscan);
}

class VP9QuantizeBase : public AbstractBench {
 public:
  VP9QuantizeBase(vpx_bit_depth_t bit_depth, int max_size, bool is_fp)
      : bit_depth_(bit_depth), max_size_(max_size), is_fp_(is_fp),
        coeff_(Buffer<tran_low_t>(max_size_, max_size_, 0, 16)),
        qcoeff_(Buffer<tran_low_t>(max_size_, max_size_, 0, 32)),
        dqcoeff_(Buffer<tran_low_t>(max_size_, max_size_, 0, 32)) {
    // TODO(jianj): SSSE3 and AVX2 tests fail on extreme values.
#if HAVE_NEON
    max_value_ = (1 << (7 + bit_depth_)) - 1;
#else
    max_value_ = (1 << bit_depth_) - 1;
#endif
    zbin_ptr_ =
        reinterpret_cast<int16_t *>(vpx_memalign(16, 8 * sizeof(*zbin_ptr_)));
    round_fp_ptr_ = reinterpret_cast<int16_t *>(
        vpx_memalign(16, 8 * sizeof(*round_fp_ptr_)));
    quant_fp_ptr_ = reinterpret_cast<int16_t *>(
        vpx_memalign(16, 8 * sizeof(*quant_fp_ptr_)));
    round_ptr_ =
        reinterpret_cast<int16_t *>(vpx_memalign(16, 8 * sizeof(*round_ptr_)));
    quant_ptr_ =
        reinterpret_cast<int16_t *>(vpx_memalign(16, 8 * sizeof(*quant_ptr_)));
    quant_shift_ptr_ = reinterpret_cast<int16_t *>(
        vpx_memalign(16, 8 * sizeof(*quant_shift_ptr_)));
    dequant_ptr_ = reinterpret_cast<int16_t *>(
        vpx_memalign(16, 8 * sizeof(*dequant_ptr_)));

    r_ptr_ = (is_fp_) ? round_fp_ptr_ : round_ptr_;
    q_ptr_ = (is_fp_) ? quant_fp_ptr_ : quant_ptr_;
  }

  ~VP9QuantizeBase() {
    vpx_free(zbin_ptr_);
    vpx_free(round_fp_ptr_);
    vpx_free(quant_fp_ptr_);
    vpx_free(round_ptr_);
    vpx_free(quant_ptr_);
    vpx_free(quant_shift_ptr_);
    vpx_free(dequant_ptr_);
    zbin_ptr_ = NULL;
    round_fp_ptr_ = NULL;
    quant_fp_ptr_ = NULL;
    round_ptr_ = NULL;
    quant_ptr_ = NULL;
    quant_shift_ptr_ = NULL;
    dequant_ptr_ = NULL;
    libvpx_test::ClearSystemState();
  }

 protected:
  int16_t *zbin_ptr_;
  int16_t *round_fp_ptr_;
  int16_t *quant_fp_ptr_;
  int16_t *round_ptr_;
  int16_t *quant_ptr_;
  int16_t *quant_shift_ptr_;
  int16_t *dequant_ptr_;
  const vpx_bit_depth_t bit_depth_;
  int max_value_;
  const int max_size_;
  const bool is_fp_;
  Buffer<tran_low_t> coeff_;
  Buffer<tran_low_t> qcoeff_;
  Buffer<tran_low_t> dqcoeff_;
  int16_t *r_ptr_;
  int16_t *q_ptr_;
  int count_;
  int skip_block_;
  const scan_order *scan_;
  uint16_t eob_;
};

class VP9QuantizeTest : public VP9QuantizeBase,
                        public ::testing::TestWithParam<QuantizeParam> {
 public:
  VP9QuantizeTest()
      : VP9QuantizeBase(GET_PARAM(2), GET_PARAM(3), GET_PARAM(4)),
        quantize_op_(GET_PARAM(0)), ref_quantize_op_(GET_PARAM(1)) {}

 protected:
  virtual void Run();
  const QuantizeFunc quantize_op_;
  const QuantizeFunc ref_quantize_op_;
};

void VP9QuantizeTest::Run() {
  quantize_op_(coeff_.TopLeftPixel(), count_, skip_block_, zbin_ptr_, r_ptr_,
               q_ptr_, quant_shift_ptr_, qcoeff_.TopLeftPixel(),
               dqcoeff_.TopLeftPixel(), dequant_ptr_, &eob_, scan_->scan,
               scan_->iscan);
}

// This quantizer compares the AC coefficients to the quantization step size to
// determine if further multiplication operations are needed.
// Based on vp9_quantize_fp_sse2().
inline void quant_fp_nz(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                        int skip_block, const int16_t *round_ptr,
                        const int16_t *quant_ptr, tran_low_t *qcoeff_ptr,
                        tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                        uint16_t *eob_ptr, const int16_t *scan,
                        const int16_t *iscan, int is_32x32) {
  int i, eob = -1;
  const int thr = dequant_ptr[1] >> (1 + is_32x32);
  (void)iscan;
  (void)skip_block;
  assert(!skip_block);

  // Quantization pass: All coefficients with index >= zero_flag are
  // skippable. Note: zero_flag can be zero.
  for (i = 0; i < n_coeffs; i += 16) {
    int y;
    int nzflag_cnt = 0;
    int abs_coeff[16];
    int coeff_sign[16];

    // count nzflag for each row (16 tran_low_t)
    for (y = 0; y < 16; ++y) {
      const int rc = i + y;
      const int coeff = coeff_ptr[rc];
      coeff_sign[y] = (coeff >> 31);
      abs_coeff[y] = (coeff ^ coeff_sign[y]) - coeff_sign[y];
      // The first 16 are skipped in the sse2 code.  Do the same here to match.
      if (i >= 16 && (abs_coeff[y] <= thr)) {
        nzflag_cnt++;
      }
    }

    for (y = 0; y < 16; ++y) {
      const int rc = i + y;
      // If all of the AC coeffs in a row has magnitude less than the
      // quantization step_size/2, quantize to zero.
      if (nzflag_cnt < 16) {
        int tmp;
        int _round;

        if (is_32x32) {
          _round = ROUND_POWER_OF_TWO(round_ptr[rc != 0], 1);
        } else {
          _round = round_ptr[rc != 0];
        }
        tmp = clamp(abs_coeff[y] + _round, INT16_MIN, INT16_MAX);
        tmp = (tmp * quant_ptr[rc != 0]) >> (16 - is_32x32);
        qcoeff_ptr[rc] = (tmp ^ coeff_sign[y]) - coeff_sign[y];
        dqcoeff_ptr[rc] =
            static_cast<tran_low_t>(qcoeff_ptr[rc] * dequant_ptr[rc != 0]);

        if (is_32x32) {
          dqcoeff_ptr[rc] = static_cast<tran_low_t>(qcoeff_ptr[rc] *
                                                    dequant_ptr[rc != 0] / 2);
        } else {
          dqcoeff_ptr[rc] =
              static_cast<tran_low_t>(qcoeff_ptr[rc] * dequant_ptr[rc != 0]);
        }
      } else {
        qcoeff_ptr[rc] = 0;
        dqcoeff_ptr[rc] = 0;
      }
    }
  }

  // Scan for eob.
  for (i = 0; i < n_coeffs; i++) {
    // Use the scan order to find the correct eob.
    const int rc = scan[i];
    if (qcoeff_ptr[rc]) {
      eob = i;
    }
  }
  *eob_ptr = eob + 1;
}

void quantize_fp_nz_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                      int skip_block, const int16_t *round_ptr,
                      const int16_t *quant_ptr, tran_low_t *qcoeff_ptr,
                      tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                      uint16_t *eob_ptr, const int16_t *scan,
                      const int16_t *iscan) {
  quant_fp_nz(coeff_ptr, n_coeffs, skip_block, round_ptr, quant_ptr, qcoeff_ptr,
              dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan, 0);
}

void quantize_fp_32x32_nz_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                            int skip_block, const int16_t *round_ptr,
                            const int16_t *quant_ptr, tran_low_t *qcoeff_ptr,
                            tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                            uint16_t *eob_ptr, const int16_t *scan,
                            const int16_t *iscan) {
  quant_fp_nz(coeff_ptr, n_coeffs, skip_block, round_ptr, quant_ptr, qcoeff_ptr,
              dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan, 1);
}

void GenerateHelperArrays(ACMRandom *rnd, int16_t *zbin, int16_t *round,
                          int16_t *quant, int16_t *quant_shift,
                          int16_t *dequant, int16_t *round_fp,
                          int16_t *quant_fp) {
  // Max when q == 0.  Otherwise, it is 48 for Y and 42 for U/V.
  const int max_qrounding_factor_fp = 64;

  for (int j = 0; j < 2; j++) {
    // The range is 4 to 1828 in the VP9 tables.
    const int qlookup = rnd->RandRange(1825) + 4;
    round_fp[j] = (max_qrounding_factor_fp * qlookup) >> 7;
    quant_fp[j] = (1 << 16) / qlookup;

    // Values determined by deconstructing vp9_init_quantizer().
    // zbin may be up to 1143 for 8 and 10 bit Y values, or 1200 for 12 bit Y
    // values or U/V values of any bit depth. This is because y_delta is not
    // factored into the vp9_ac_quant() call.
    zbin[j] = rnd->RandRange(1200);

    // round may be up to 685 for Y values or 914 for U/V.
    round[j] = rnd->RandRange(914);
    // quant ranges from 1 to -32703
    quant[j] = static_cast<int>(rnd->RandRange(32704)) - 32703;
    // quant_shift goes up to 1 << 16.
    quant_shift[j] = rnd->RandRange(16384);
    // dequant maxes out at 1828 for all cases.
    dequant[j] = rnd->RandRange(1828);
  }
  for (int j = 2; j < 8; j++) {
    zbin[j] = zbin[1];
    round_fp[j] = round_fp[1];
    quant_fp[j] = quant_fp[1];
    round[j] = round[1];
    quant[j] = quant[1];
    quant_shift[j] = quant_shift[1];
    dequant[j] = dequant[1];
  }
}

TEST_P(VP9QuantizeTest, OperationCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  ASSERT_TRUE(coeff_.Init());
  ASSERT_TRUE(qcoeff_.Init());
  ASSERT_TRUE(dqcoeff_.Init());
  Buffer<tran_low_t> ref_qcoeff =
      Buffer<tran_low_t>(max_size_, max_size_, 0, 32);
  ASSERT_TRUE(ref_qcoeff.Init());
  Buffer<tran_low_t> ref_dqcoeff =
      Buffer<tran_low_t>(max_size_, max_size_, 0, 32);
  ASSERT_TRUE(ref_dqcoeff.Init());
  uint16_t ref_eob = 0;
  eob_ = 0;

  for (int i = 0; i < number_of_iterations; ++i) {
    // Test skip block for the first three iterations to catch all the different
    // sizes.
    const int skip_block = 0;
    TX_SIZE sz;
    if (max_size_ == 16) {
      sz = static_cast<TX_SIZE>(i % 3);  // TX_4X4, TX_8X8 TX_16X16
    } else {
      sz = TX_32X32;
    }
    const TX_TYPE tx_type = static_cast<TX_TYPE>((i >> 2) % 3);
    scan_ = &vp9_scan_orders[sz][tx_type];
    count_ = (4 << sz) * (4 << sz);
    coeff_.Set(&rnd, -max_value_, max_value_);
    GenerateHelperArrays(&rnd, zbin_ptr_, round_ptr_, quant_ptr_,
                         quant_shift_ptr_, dequant_ptr_, round_fp_ptr_,
                         quant_fp_ptr_);
    ref_quantize_op_(coeff_.TopLeftPixel(), count_, skip_block, zbin_ptr_,
                     r_ptr_, q_ptr_, quant_shift_ptr_,
                     ref_qcoeff.TopLeftPixel(), ref_dqcoeff.TopLeftPixel(),
                     dequant_ptr_, &ref_eob, scan_->scan, scan_->iscan);

    ASM_REGISTER_STATE_CHECK(quantize_op_(
        coeff_.TopLeftPixel(), count_, skip_block, zbin_ptr_, r_ptr_, q_ptr_,
        quant_shift_ptr_, qcoeff_.TopLeftPixel(), dqcoeff_.TopLeftPixel(),
        dequant_ptr_, &eob_, scan_->scan, scan_->iscan));

    EXPECT_TRUE(qcoeff_.CheckValues(ref_qcoeff));
    EXPECT_TRUE(dqcoeff_.CheckValues(ref_dqcoeff));

    EXPECT_EQ(eob_, ref_eob);

    if (HasFailure()) {
      printf("Failure on iteration %d.\n", i);
      qcoeff_.PrintDifference(ref_qcoeff);
      dqcoeff_.PrintDifference(ref_dqcoeff);
      return;
    }
  }
}

TEST_P(VP9QuantizeTest, EOBCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  ASSERT_TRUE(coeff_.Init());
  ASSERT_TRUE(qcoeff_.Init());
  ASSERT_TRUE(dqcoeff_.Init());
  Buffer<tran_low_t> ref_qcoeff =
      Buffer<tran_low_t>(max_size_, max_size_, 0, 32);
  ASSERT_TRUE(ref_qcoeff.Init());
  Buffer<tran_low_t> ref_dqcoeff =
      Buffer<tran_low_t>(max_size_, max_size_, 0, 32);
  ASSERT_TRUE(ref_dqcoeff.Init());
  uint16_t ref_eob = 0;
  eob_ = 0;
  const uint32_t max_index = max_size_ * max_size_ - 1;

  for (int i = 0; i < number_of_iterations; ++i) {
    skip_block_ = 0;
    TX_SIZE sz;
    if (max_size_ == 16) {
      sz = static_cast<TX_SIZE>(i % 3);  // TX_4X4, TX_8X8 TX_16X16
    } else {
      sz = TX_32X32;
    }
    const TX_TYPE tx_type = static_cast<TX_TYPE>((i >> 2) % 3);
    scan_ = &vp9_scan_orders[sz][tx_type];
    count_ = (4 << sz) * (4 << sz);
    // Two random entries
    coeff_.Set(0);
    coeff_.TopLeftPixel()[rnd.RandRange(count_) & max_index] =
        static_cast<int>(rnd.RandRange(max_value_ * 2)) - max_value_;
    coeff_.TopLeftPixel()[rnd.RandRange(count_) & max_index] =
        static_cast<int>(rnd.RandRange(max_value_ * 2)) - max_value_;
    GenerateHelperArrays(&rnd, zbin_ptr_, round_ptr_, quant_ptr_,
                         quant_shift_ptr_, dequant_ptr_, round_fp_ptr_,
                         quant_fp_ptr_);
    ref_quantize_op_(coeff_.TopLeftPixel(), count_, skip_block_, zbin_ptr_,
                     r_ptr_, q_ptr_, quant_shift_ptr_,
                     ref_qcoeff.TopLeftPixel(), ref_dqcoeff.TopLeftPixel(),
                     dequant_ptr_, &ref_eob, scan_->scan, scan_->iscan);

    ASM_REGISTER_STATE_CHECK(quantize_op_(
        coeff_.TopLeftPixel(), count_, skip_block_, zbin_ptr_, r_ptr_, q_ptr_,
        quant_shift_ptr_, qcoeff_.TopLeftPixel(), dqcoeff_.TopLeftPixel(),
        dequant_ptr_, &eob_, scan_->scan, scan_->iscan));

    EXPECT_TRUE(qcoeff_.CheckValues(ref_qcoeff));
    EXPECT_TRUE(dqcoeff_.CheckValues(ref_dqcoeff));

    EXPECT_EQ(eob_, ref_eob);

    if (HasFailure()) {
      printf("Failure on iteration %d.\n", i);
      qcoeff_.PrintDifference(ref_qcoeff);
      dqcoeff_.PrintDifference(ref_dqcoeff);
      return;
    }
  }
}

TEST_P(VP9QuantizeTest, DISABLED_Speed) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  ASSERT_TRUE(coeff_.Init());
  ASSERT_TRUE(qcoeff_.Init());
  ASSERT_TRUE(dqcoeff_.Init());
  TX_SIZE starting_sz, ending_sz;

  if (max_size_ == 16) {
    starting_sz = TX_4X4;
    ending_sz = TX_16X16;
  } else {
    starting_sz = TX_32X32;
    ending_sz = TX_32X32;
  }

  for (TX_SIZE sz = starting_sz; sz <= ending_sz; ++sz) {
    // zbin > coeff, zbin < coeff.
    for (int i = 0; i < 2; ++i) {
      skip_block_ = 0;
      // TX_TYPE defines the scan order. That is not relevant to the speed test.
      // Pick the first one.
      const TX_TYPE tx_type = DCT_DCT;
      count_ = (4 << sz) * (4 << sz);
      scan_ = &vp9_scan_orders[sz][tx_type];

      GenerateHelperArrays(&rnd, zbin_ptr_, round_ptr_, quant_ptr_,
                           quant_shift_ptr_, dequant_ptr_, round_fp_ptr_,
                           quant_fp_ptr_);

      if (i == 0) {
        // When |coeff values| are less than zbin the results are 0.
        int threshold = 100;
        if (max_size_ == 32) {
          // For 32x32, the threshold is halved. Double it to keep the values
          // from clearing it.
          threshold = 200;
        }
        for (int j = 0; j < 8; ++j) zbin_ptr_[j] = threshold;
        coeff_.Set(&rnd, -99, 99);
      } else if (i == 1) {
        for (int j = 0; j < 8; ++j) zbin_ptr_[j] = 50;
        coeff_.Set(&rnd, -500, 500);
      }

      RunNTimes(10000000 / count_);
      const char *type =
          (i == 0) ? "Bypass calculations " : "Full calculations ";
      char block_size[16];
      snprintf(block_size, sizeof(block_size), "%dx%d", 4 << sz, 4 << sz);
      char title[100];
      snprintf(title, sizeof(title), "%25s %8s ", type, block_size);
      PrintMedian(title);
    }
  }
}

using std::make_tuple;

#if HAVE_SSE2
#if CONFIG_VP9_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    SSE2, VP9QuantizeTest,
    ::testing::Values(
        make_tuple(&vpx_quantize_b_sse2, &vpx_quantize_b_c, VPX_BITS_8, 16,
                   false),
        make_tuple(&vpx_highbd_quantize_b_sse2, &vpx_highbd_quantize_b_c,
                   VPX_BITS_8, 16, false),
        make_tuple(&vpx_highbd_quantize_b_sse2, &vpx_highbd_quantize_b_c,
                   VPX_BITS_10, 16, false),
        make_tuple(&vpx_highbd_quantize_b_sse2, &vpx_highbd_quantize_b_c,
                   VPX_BITS_12, 16, false),
        make_tuple(&vpx_highbd_quantize_b_32x32_sse2,
                   &vpx_highbd_quantize_b_32x32_c, VPX_BITS_8, 32, false),
        make_tuple(&vpx_highbd_quantize_b_32x32_sse2,
                   &vpx_highbd_quantize_b_32x32_c, VPX_BITS_10, 32, false),
        make_tuple(&vpx_highbd_quantize_b_32x32_sse2,
                   &vpx_highbd_quantize_b_32x32_c, VPX_BITS_12, 32, false)));

#else
INSTANTIATE_TEST_CASE_P(
    SSE2, VP9QuantizeTest,
    ::testing::Values(make_tuple(&vpx_quantize_b_sse2, &vpx_quantize_b_c,
                                 VPX_BITS_8, 16, false),
                      make_tuple(&QuantFPWrapper<vp9_quantize_fp_sse2>,
                                 &QuantFPWrapper<quantize_fp_nz_c>, VPX_BITS_8,
                                 16, true)));
#endif  // CONFIG_VP9_HIGHBITDEPTH
#endif  // HAVE_SSE2

#if HAVE_SSSE3
#if VPX_ARCH_X86_64
INSTANTIATE_TEST_CASE_P(
    SSSE3, VP9QuantizeTest,
    ::testing::Values(make_tuple(&vpx_quantize_b_ssse3, &vpx_quantize_b_c,
                                 VPX_BITS_8, 16, false),
                      make_tuple(&vpx_quantize_b_32x32_ssse3,
                                 &vpx_quantize_b_32x32_c, VPX_BITS_8, 32,
                                 false),
                      make_tuple(&QuantFPWrapper<vp9_quantize_fp_ssse3>,
                                 &QuantFPWrapper<quantize_fp_nz_c>, VPX_BITS_8,
                                 16, true),
                      make_tuple(&QuantFPWrapper<vp9_quantize_fp_32x32_ssse3>,
                                 &QuantFPWrapper<quantize_fp_32x32_nz_c>,
                                 VPX_BITS_8, 32, true)));
#else
INSTANTIATE_TEST_CASE_P(
    SSSE3, VP9QuantizeTest,
    ::testing::Values(make_tuple(&vpx_quantize_b_ssse3, &vpx_quantize_b_c,
                                 VPX_BITS_8, 16, false),
                      make_tuple(&vpx_quantize_b_32x32_ssse3,
                                 &vpx_quantize_b_32x32_c, VPX_BITS_8, 32,
                                 false)));

#endif  // VPX_ARCH_X86_64
#endif  // HAVE_SSSE3

#if HAVE_AVX
INSTANTIATE_TEST_CASE_P(AVX, VP9QuantizeTest,
                        ::testing::Values(make_tuple(&vpx_quantize_b_avx,
                                                     &vpx_quantize_b_c,
                                                     VPX_BITS_8, 16, false),
                                          make_tuple(&vpx_quantize_b_32x32_avx,
                                                     &vpx_quantize_b_32x32_c,
                                                     VPX_BITS_8, 32, false)));
#endif  // HAVE_AVX

#if VPX_ARCH_X86_64 && HAVE_AVX2
INSTANTIATE_TEST_CASE_P(
    AVX2, VP9QuantizeTest,
    ::testing::Values(make_tuple(&QuantFPWrapper<vp9_quantize_fp_avx2>,
                                 &QuantFPWrapper<quantize_fp_nz_c>, VPX_BITS_8,
                                 16, true)));
#endif  // HAVE_AVX2

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, VP9QuantizeTest,
    ::testing::Values(make_tuple(&vpx_quantize_b_neon, &vpx_quantize_b_c,
                                 VPX_BITS_8, 16, false),
                      make_tuple(&vpx_quantize_b_32x32_neon,
                                 &vpx_quantize_b_32x32_c, VPX_BITS_8, 32,
                                 false),
                      make_tuple(&QuantFPWrapper<vp9_quantize_fp_neon>,
                                 &QuantFPWrapper<vp9_quantize_fp_c>, VPX_BITS_8,
                                 16, true),
                      make_tuple(&QuantFPWrapper<vp9_quantize_fp_32x32_neon>,
                                 &QuantFPWrapper<vp9_quantize_fp_32x32_c>,
                                 VPX_BITS_8, 32, true)));
#endif  // HAVE_NEON

#if HAVE_VSX && !CONFIG_VP9_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    VSX, VP9QuantizeTest,
    ::testing::Values(make_tuple(&vpx_quantize_b_vsx, &vpx_quantize_b_c,
                                 VPX_BITS_8, 16, false),
                      make_tuple(&vpx_quantize_b_32x32_vsx,
                                 &vpx_quantize_b_32x32_c, VPX_BITS_8, 32,
                                 false),
                      make_tuple(&QuantFPWrapper<vp9_quantize_fp_vsx>,
                                 &QuantFPWrapper<vp9_quantize_fp_c>, VPX_BITS_8,
                                 16, true),
                      make_tuple(&QuantFPWrapper<vp9_quantize_fp_32x32_vsx>,
                                 &QuantFPWrapper<vp9_quantize_fp_32x32_c>,
                                 VPX_BITS_8, 32, true)));
#endif  // HAVE_VSX && !CONFIG_VP9_HIGHBITDEPTH

// Only useful to compare "Speed" test results.
INSTANTIATE_TEST_CASE_P(
    DISABLED_C, VP9QuantizeTest,
    ::testing::Values(
        make_tuple(&vpx_quantize_b_c, &vpx_quantize_b_c, VPX_BITS_8, 16, false),
        make_tuple(&vpx_quantize_b_32x32_c, &vpx_quantize_b_32x32_c, VPX_BITS_8,
                   32, false),
        make_tuple(&QuantFPWrapper<vp9_quantize_fp_c>,
                   &QuantFPWrapper<vp9_quantize_fp_c>, VPX_BITS_8, 16, true),
        make_tuple(&QuantFPWrapper<quantize_fp_nz_c>,
                   &QuantFPWrapper<quantize_fp_nz_c>, VPX_BITS_8, 16, true),
        make_tuple(&QuantFPWrapper<quantize_fp_32x32_nz_c>,
                   &QuantFPWrapper<quantize_fp_32x32_nz_c>, VPX_BITS_8, 32,
                   true),
        make_tuple(&QuantFPWrapper<vp9_quantize_fp_32x32_c>,
                   &QuantFPWrapper<vp9_quantize_fp_32x32_c>, VPX_BITS_8, 32,
                   true)));
}  // namespace
