/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
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
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "vp9/common/vp9_entropy.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_integer.h"
#include "vpx_ports/mem.h"
#include "vpx_ports/msvc.h"  // for round()

using libvpx_test::ACMRandom;

namespace {

const int kNumCoeffs = 1024;
const double kPi = 3.141592653589793238462643383279502884;
void reference_32x32_dct_1d(const double in[32], double out[32]) {
  const double kInvSqrt2 = 0.707106781186547524400844362104;
  for (int k = 0; k < 32; k++) {
    out[k] = 0.0;
    for (int n = 0; n < 32; n++) {
      out[k] += in[n] * cos(kPi * (2 * n + 1) * k / 64.0);
    }
    if (k == 0) out[k] = out[k] * kInvSqrt2;
  }
}

void reference_32x32_dct_2d(const int16_t input[kNumCoeffs],
                            double output[kNumCoeffs]) {
  // First transform columns
  for (int i = 0; i < 32; ++i) {
    double temp_in[32], temp_out[32];
    for (int j = 0; j < 32; ++j) temp_in[j] = input[j * 32 + i];
    reference_32x32_dct_1d(temp_in, temp_out);
    for (int j = 0; j < 32; ++j) output[j * 32 + i] = temp_out[j];
  }
  // Then transform rows
  for (int i = 0; i < 32; ++i) {
    double temp_in[32], temp_out[32];
    for (int j = 0; j < 32; ++j) temp_in[j] = output[j + i * 32];
    reference_32x32_dct_1d(temp_in, temp_out);
    // Scale by some magic number
    for (int j = 0; j < 32; ++j) output[j + i * 32] = temp_out[j] / 4;
  }
}

typedef void (*FwdTxfmFunc)(const int16_t *in, tran_low_t *out, int stride);
typedef void (*InvTxfmFunc)(const tran_low_t *in, uint8_t *out, int stride);

typedef std::tuple<FwdTxfmFunc, InvTxfmFunc, int, vpx_bit_depth_t>
    Trans32x32Param;

#if CONFIG_VP9_HIGHBITDEPTH
void idct32x32_10(const tran_low_t *in, uint8_t *out, int stride) {
  vpx_highbd_idct32x32_1024_add_c(in, CAST_TO_SHORTPTR(out), stride, 10);
}

void idct32x32_12(const tran_low_t *in, uint8_t *out, int stride) {
  vpx_highbd_idct32x32_1024_add_c(in, CAST_TO_SHORTPTR(out), stride, 12);
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

class Trans32x32Test : public AbstractBench,
                       public ::testing::TestWithParam<Trans32x32Param> {
 public:
  virtual ~Trans32x32Test() {}
  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    version_ = GET_PARAM(2);  // 0: high precision forward transform
                              // 1: low precision version for rd loop
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
  }

  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  int version_;
  vpx_bit_depth_t bit_depth_;
  int mask_;
  FwdTxfmFunc fwd_txfm_;
  InvTxfmFunc inv_txfm_;

  int16_t *bench_in_;
  tran_low_t *bench_out_;
  virtual void Run();
};

void Trans32x32Test::Run() { fwd_txfm_(bench_in_, bench_out_, 32); }

TEST_P(Trans32x32Test, AccuracyCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  uint32_t max_error = 0;
  int64_t total_error = 0;
  const int count_test_block = 10000;
  DECLARE_ALIGNED(16, int16_t, test_input_block[kNumCoeffs]);
  DECLARE_ALIGNED(16, tran_low_t, test_temp_block[kNumCoeffs]);
  DECLARE_ALIGNED(16, uint8_t, dst[kNumCoeffs]);
  DECLARE_ALIGNED(16, uint8_t, src[kNumCoeffs]);
#if CONFIG_VP9_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, dst16[kNumCoeffs]);
  DECLARE_ALIGNED(16, uint16_t, src16[kNumCoeffs]);
#endif

  for (int i = 0; i < count_test_block; ++i) {
    // Initialize a test block with input range [-mask_, mask_].
    for (int j = 0; j < kNumCoeffs; ++j) {
      if (bit_depth_ == VPX_BITS_8) {
        src[j] = rnd.Rand8();
        dst[j] = rnd.Rand8();
        test_input_block[j] = src[j] - dst[j];
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        src16[j] = rnd.Rand16() & mask_;
        dst16[j] = rnd.Rand16() & mask_;
        test_input_block[j] = src16[j] - dst16[j];
#endif
      }
    }

    ASM_REGISTER_STATE_CHECK(fwd_txfm_(test_input_block, test_temp_block, 32));
    if (bit_depth_ == VPX_BITS_8) {
      ASM_REGISTER_STATE_CHECK(inv_txfm_(test_temp_block, dst, 32));
#if CONFIG_VP9_HIGHBITDEPTH
    } else {
      ASM_REGISTER_STATE_CHECK(
          inv_txfm_(test_temp_block, CAST_TO_BYTEPTR(dst16), 32));
#endif
    }

    for (int j = 0; j < kNumCoeffs; ++j) {
#if CONFIG_VP9_HIGHBITDEPTH
      const int32_t diff =
          bit_depth_ == VPX_BITS_8 ? dst[j] - src[j] : dst16[j] - src16[j];
#else
      const int32_t diff = dst[j] - src[j];
#endif
      const uint32_t error = diff * diff;
      if (max_error < error) max_error = error;
      total_error += error;
    }
  }

  if (version_ == 1) {
    max_error /= 2;
    total_error /= 45;
  }

  EXPECT_GE(1u << 2 * (bit_depth_ - 8), max_error)
      << "Error: 32x32 FDCT/IDCT has an individual round-trip error > 1";

  EXPECT_GE(count_test_block << 2 * (bit_depth_ - 8), total_error)
      << "Error: 32x32 FDCT/IDCT has average round-trip error > 1 per block";
}

TEST_P(Trans32x32Test, CoeffCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = 1000;

  DECLARE_ALIGNED(16, int16_t, input_block[kNumCoeffs]);
  DECLARE_ALIGNED(16, tran_low_t, output_ref_block[kNumCoeffs]);
  DECLARE_ALIGNED(16, tran_low_t, output_block[kNumCoeffs]);

  for (int i = 0; i < count_test_block; ++i) {
    for (int j = 0; j < kNumCoeffs; ++j) {
      input_block[j] = (rnd.Rand16() & mask_) - (rnd.Rand16() & mask_);
    }

    const int stride = 32;
    vpx_fdct32x32_c(input_block, output_ref_block, stride);
    ASM_REGISTER_STATE_CHECK(fwd_txfm_(input_block, output_block, stride));

    if (version_ == 0) {
      for (int j = 0; j < kNumCoeffs; ++j)
        EXPECT_EQ(output_block[j], output_ref_block[j])
            << "Error: 32x32 FDCT versions have mismatched coefficients";
    } else {
      for (int j = 0; j < kNumCoeffs; ++j)
        EXPECT_GE(6, abs(output_block[j] - output_ref_block[j]))
            << "Error: 32x32 FDCT rd has mismatched coefficients";
    }
  }
}

TEST_P(Trans32x32Test, MemCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = 2000;

  DECLARE_ALIGNED(16, int16_t, input_extreme_block[kNumCoeffs]);
  DECLARE_ALIGNED(16, tran_low_t, output_ref_block[kNumCoeffs]);
  DECLARE_ALIGNED(16, tran_low_t, output_block[kNumCoeffs]);

  for (int i = 0; i < count_test_block; ++i) {
    // Initialize a test block with input range [-mask_, mask_].
    for (int j = 0; j < kNumCoeffs; ++j) {
      input_extreme_block[j] = rnd.Rand8() & 1 ? mask_ : -mask_;
    }
    if (i == 0) {
      for (int j = 0; j < kNumCoeffs; ++j) input_extreme_block[j] = mask_;
    } else if (i == 1) {
      for (int j = 0; j < kNumCoeffs; ++j) input_extreme_block[j] = -mask_;
    }

    const int stride = 32;
    vpx_fdct32x32_c(input_extreme_block, output_ref_block, stride);
    ASM_REGISTER_STATE_CHECK(
        fwd_txfm_(input_extreme_block, output_block, stride));

    // The minimum quant value is 4.
    for (int j = 0; j < kNumCoeffs; ++j) {
      if (version_ == 0) {
        EXPECT_EQ(output_block[j], output_ref_block[j])
            << "Error: 32x32 FDCT versions have mismatched coefficients";
      } else {
        EXPECT_GE(6, abs(output_block[j] - output_ref_block[j]))
            << "Error: 32x32 FDCT rd has mismatched coefficients";
      }
      EXPECT_GE(4 * DCT_MAX_VALUE << (bit_depth_ - 8), abs(output_ref_block[j]))
          << "Error: 32x32 FDCT C has coefficient larger than 4*DCT_MAX_VALUE";
      EXPECT_GE(4 * DCT_MAX_VALUE << (bit_depth_ - 8), abs(output_block[j]))
          << "Error: 32x32 FDCT has coefficient larger than "
          << "4*DCT_MAX_VALUE";
    }
  }
}

TEST_P(Trans32x32Test, DISABLED_Speed) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());

  DECLARE_ALIGNED(16, int16_t, input_extreme_block[kNumCoeffs]);
  DECLARE_ALIGNED(16, tran_low_t, output_block[kNumCoeffs]);

  bench_in_ = input_extreme_block;
  bench_out_ = output_block;

  RunNTimes(INT16_MAX);
  PrintMedian("32x32");
}

TEST_P(Trans32x32Test, InverseAccuracy) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = 1000;
  DECLARE_ALIGNED(16, int16_t, in[kNumCoeffs]);
  DECLARE_ALIGNED(16, tran_low_t, coeff[kNumCoeffs]);
  DECLARE_ALIGNED(16, uint8_t, dst[kNumCoeffs]);
  DECLARE_ALIGNED(16, uint8_t, src[kNumCoeffs]);
#if CONFIG_VP9_HIGHBITDEPTH
  DECLARE_ALIGNED(16, uint16_t, dst16[kNumCoeffs]);
  DECLARE_ALIGNED(16, uint16_t, src16[kNumCoeffs]);
#endif

  for (int i = 0; i < count_test_block; ++i) {
    double out_r[kNumCoeffs];

    // Initialize a test block with input range [-255, 255]
    for (int j = 0; j < kNumCoeffs; ++j) {
      if (bit_depth_ == VPX_BITS_8) {
        src[j] = rnd.Rand8();
        dst[j] = rnd.Rand8();
        in[j] = src[j] - dst[j];
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        src16[j] = rnd.Rand16() & mask_;
        dst16[j] = rnd.Rand16() & mask_;
        in[j] = src16[j] - dst16[j];
#endif
      }
    }

    reference_32x32_dct_2d(in, out_r);
    for (int j = 0; j < kNumCoeffs; ++j) {
      coeff[j] = static_cast<tran_low_t>(round(out_r[j]));
    }
    if (bit_depth_ == VPX_BITS_8) {
      ASM_REGISTER_STATE_CHECK(inv_txfm_(coeff, dst, 32));
#if CONFIG_VP9_HIGHBITDEPTH
    } else {
      ASM_REGISTER_STATE_CHECK(inv_txfm_(coeff, CAST_TO_BYTEPTR(dst16), 32));
#endif
    }
    for (int j = 0; j < kNumCoeffs; ++j) {
#if CONFIG_VP9_HIGHBITDEPTH
      const int diff =
          bit_depth_ == VPX_BITS_8 ? dst[j] - src[j] : dst16[j] - src16[j];
#else
      const int diff = dst[j] - src[j];
#endif
      const int error = diff * diff;
      EXPECT_GE(1, error) << "Error: 32x32 IDCT has error " << error
                          << " at index " << j;
    }
  }
}

using std::make_tuple;

#if CONFIG_VP9_HIGHBITDEPTH
INSTANTIATE_TEST_SUITE_P(
    C, Trans32x32Test,
    ::testing::Values(
        make_tuple(&vpx_highbd_fdct32x32_c, &idct32x32_10, 0, VPX_BITS_10),
        make_tuple(&vpx_highbd_fdct32x32_rd_c, &idct32x32_10, 1, VPX_BITS_10),
        make_tuple(&vpx_highbd_fdct32x32_c, &idct32x32_12, 0, VPX_BITS_12),
        make_tuple(&vpx_highbd_fdct32x32_rd_c, &idct32x32_12, 1, VPX_BITS_12),
        make_tuple(&vpx_fdct32x32_c, &vpx_idct32x32_1024_add_c, 0, VPX_BITS_8),
        make_tuple(&vpx_fdct32x32_rd_c, &vpx_idct32x32_1024_add_c, 1,
                   VPX_BITS_8)));
#else
INSTANTIATE_TEST_SUITE_P(
    C, Trans32x32Test,
    ::testing::Values(make_tuple(&vpx_fdct32x32_c, &vpx_idct32x32_1024_add_c, 0,
                                 VPX_BITS_8),
                      make_tuple(&vpx_fdct32x32_rd_c, &vpx_idct32x32_1024_add_c,
                                 1, VPX_BITS_8)));
#endif  // CONFIG_VP9_HIGHBITDEPTH

#if HAVE_NEON && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_SUITE_P(
    NEON, Trans32x32Test,
    ::testing::Values(make_tuple(&vpx_fdct32x32_neon,
                                 &vpx_idct32x32_1024_add_neon, 0, VPX_BITS_8),
                      make_tuple(&vpx_fdct32x32_rd_neon,
                                 &vpx_idct32x32_1024_add_neon, 1, VPX_BITS_8)));
#endif  // HAVE_NEON && !CONFIG_EMULATE_HARDWARE

#if HAVE_SSE2 && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_SUITE_P(
    SSE2, Trans32x32Test,
    ::testing::Values(make_tuple(&vpx_fdct32x32_sse2,
                                 &vpx_idct32x32_1024_add_sse2, 0, VPX_BITS_8),
                      make_tuple(&vpx_fdct32x32_rd_sse2,
                                 &vpx_idct32x32_1024_add_sse2, 1, VPX_BITS_8)));
#endif  // HAVE_SSE2 && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE

#if HAVE_SSE2 && CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_SUITE_P(
    SSE2, Trans32x32Test,
    ::testing::Values(
        make_tuple(&vpx_highbd_fdct32x32_sse2, &idct32x32_10, 0, VPX_BITS_10),
        make_tuple(&vpx_highbd_fdct32x32_rd_sse2, &idct32x32_10, 1,
                   VPX_BITS_10),
        make_tuple(&vpx_highbd_fdct32x32_sse2, &idct32x32_12, 0, VPX_BITS_12),
        make_tuple(&vpx_highbd_fdct32x32_rd_sse2, &idct32x32_12, 1,
                   VPX_BITS_12),
        make_tuple(&vpx_fdct32x32_sse2, &vpx_idct32x32_1024_add_c, 0,
                   VPX_BITS_8),
        make_tuple(&vpx_fdct32x32_rd_sse2, &vpx_idct32x32_1024_add_c, 1,
                   VPX_BITS_8)));
#endif  // HAVE_SSE2 && CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE

#if HAVE_AVX2 && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_SUITE_P(
    AVX2, Trans32x32Test,
    ::testing::Values(make_tuple(&vpx_fdct32x32_avx2,
                                 &vpx_idct32x32_1024_add_sse2, 0, VPX_BITS_8),
                      make_tuple(&vpx_fdct32x32_rd_avx2,
                                 &vpx_idct32x32_1024_add_sse2, 1, VPX_BITS_8)));
#endif  // HAVE_AVX2 && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE

#if HAVE_MSA && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_SUITE_P(
    MSA, Trans32x32Test,
    ::testing::Values(make_tuple(&vpx_fdct32x32_msa,
                                 &vpx_idct32x32_1024_add_msa, 0, VPX_BITS_8),
                      make_tuple(&vpx_fdct32x32_rd_msa,
                                 &vpx_idct32x32_1024_add_msa, 1, VPX_BITS_8)));
#endif  // HAVE_MSA && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE

#if HAVE_VSX && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_SUITE_P(
    VSX, Trans32x32Test,
    ::testing::Values(make_tuple(&vpx_fdct32x32_c, &vpx_idct32x32_1024_add_vsx,
                                 0, VPX_BITS_8),
                      make_tuple(&vpx_fdct32x32_rd_vsx,
                                 &vpx_idct32x32_1024_add_vsx, 1, VPX_BITS_8)));
#endif  // HAVE_VSX && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
}  // namespace
