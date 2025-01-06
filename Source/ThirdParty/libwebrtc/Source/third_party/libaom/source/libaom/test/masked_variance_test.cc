/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <tuple>

#include "gtest/gtest.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_codec.h"
#include "aom/aom_integer.h"
#include "aom_dsp/aom_filter.h"
#include "aom_mem/aom_mem.h"

using libaom_test::ACMRandom;

namespace {
const int number_of_iterations = 200;

typedef unsigned int (*MaskedSubPixelVarianceFunc)(
    const uint8_t *src, int src_stride, int xoffset, int yoffset,
    const uint8_t *ref, int ref_stride, const uint8_t *second_pred,
    const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

typedef std::tuple<MaskedSubPixelVarianceFunc, MaskedSubPixelVarianceFunc>
    MaskedSubPixelVarianceParam;

class MaskedSubPixelVarianceTest
    : public ::testing::TestWithParam<MaskedSubPixelVarianceParam> {
 public:
  ~MaskedSubPixelVarianceTest() override = default;
  void SetUp() override {
    opt_func_ = GET_PARAM(0);
    ref_func_ = GET_PARAM(1);
  }

 protected:
  MaskedSubPixelVarianceFunc opt_func_;
  MaskedSubPixelVarianceFunc ref_func_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(MaskedSubPixelVarianceTest);

TEST_P(MaskedSubPixelVarianceTest, OperationCheck) {
  unsigned int ref_ret, opt_ret;
  unsigned int ref_sse, opt_sse;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  // Note: We pad out the input array to a multiple of 16 bytes wide, so that
  // consecutive rows keep the 16-byte alignment.
  DECLARE_ALIGNED(16, uint8_t, src_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 16)]);
  DECLARE_ALIGNED(16, uint8_t, ref_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 16)]);
  DECLARE_ALIGNED(16, uint8_t,
                  second_pred_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 16)]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 16)]);
  int err_count = 0;
  int first_failure = -1;
  int src_stride = (MAX_SB_SIZE + 16);
  int ref_stride = (MAX_SB_SIZE + 16);
  int msk_stride = (MAX_SB_SIZE + 16);
  int xoffset;
  int yoffset;

  for (int i = 0; i < number_of_iterations; ++i) {
    int xoffsets[] = { 0, 4, rnd(BIL_SUBPEL_SHIFTS) };
    int yoffsets[] = { 0, 4, rnd(BIL_SUBPEL_SHIFTS) };
    for (int j = 0; j < (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 16); j++) {
      src_ptr[j] = rnd.Rand8();
      ref_ptr[j] = rnd.Rand8();
      second_pred_ptr[j] = rnd.Rand8();
      msk_ptr[j] = rnd(65);
    }
    for (int k = 0; k < 3; k++) {
      for (int l = 0; l < 3; l++) {
        xoffset = xoffsets[k];
        yoffset = yoffsets[l];
        for (int invert_mask = 0; invert_mask < 2; ++invert_mask) {
          ref_ret = ref_func_(src_ptr, src_stride, xoffset, yoffset, ref_ptr,
                              ref_stride, second_pred_ptr, msk_ptr, msk_stride,
                              invert_mask, &ref_sse);
          API_REGISTER_STATE_CHECK(
              opt_ret = opt_func_(src_ptr, src_stride, xoffset, yoffset,
                                  ref_ptr, ref_stride, second_pred_ptr, msk_ptr,
                                  msk_stride, invert_mask, &opt_sse));

          if (opt_ret != ref_ret || opt_sse != ref_sse) {
            err_count++;
            if (first_failure == -1) first_failure = i;
          }
        }
      }
    }
  }

  EXPECT_EQ(0, err_count)
      << "Error: Masked Sub Pixel Variance Test OperationCheck,"
      << "C output doesn't match SSSE3 output. "
      << "First failed at test case " << first_failure;
}

TEST_P(MaskedSubPixelVarianceTest, ExtremeValues) {
  unsigned int ref_ret, opt_ret;
  unsigned int ref_sse, opt_sse;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint8_t, src_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 16)]);
  DECLARE_ALIGNED(16, uint8_t, ref_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 16)]);
  DECLARE_ALIGNED(16, uint8_t,
                  second_pred_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 16)]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 16)]);
  int first_failure_x = -1;
  int first_failure_y = -1;
  int err_count = 0;
  int first_failure = -1;
  int src_stride = (MAX_SB_SIZE + 16);
  int ref_stride = (MAX_SB_SIZE + 16);
  int msk_stride = (MAX_SB_SIZE + 16);

  for (int xoffset = 0; xoffset < BIL_SUBPEL_SHIFTS; xoffset++) {
    for (int yoffset = 0; yoffset < BIL_SUBPEL_SHIFTS; yoffset++) {
      for (int i = 0; i < 16; ++i) {
        memset(src_ptr, (i & 0x1) ? 255 : 0,
               (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 16));
        memset(ref_ptr, (i & 0x2) ? 255 : 0,
               (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 16));
        memset(second_pred_ptr, (i & 0x4) ? 255 : 0,
               (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 16));
        memset(msk_ptr, (i & 0x8) ? 64 : 0,
               (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 16));

        for (int invert_mask = 0; invert_mask < 2; ++invert_mask) {
          ref_ret = ref_func_(src_ptr, src_stride, xoffset, yoffset, ref_ptr,
                              ref_stride, second_pred_ptr, msk_ptr, msk_stride,
                              invert_mask, &ref_sse);
          API_REGISTER_STATE_CHECK(
              opt_ret = opt_func_(src_ptr, src_stride, xoffset, yoffset,
                                  ref_ptr, ref_stride, second_pred_ptr, msk_ptr,
                                  msk_stride, invert_mask, &opt_sse));

          if (opt_ret != ref_ret || opt_sse != ref_sse) {
            err_count++;
            if (first_failure == -1) {
              first_failure = i;
              first_failure_x = xoffset;
              first_failure_y = yoffset;
            }
          }
        }
      }
    }
  }

  EXPECT_EQ(0, err_count) << "Error: Masked Variance Test ExtremeValues,"
                          << "C output doesn't match SSSE3 output. "
                          << "First failed at test case " << first_failure
                          << " x_offset = " << first_failure_x
                          << " y_offset = " << first_failure_y;
}

#if CONFIG_AV1_HIGHBITDEPTH
typedef std::tuple<MaskedSubPixelVarianceFunc, MaskedSubPixelVarianceFunc,
                   aom_bit_depth_t>
    HighbdMaskedSubPixelVarianceParam;

class HighbdMaskedSubPixelVarianceTest
    : public ::testing::TestWithParam<HighbdMaskedSubPixelVarianceParam> {
 public:
  ~HighbdMaskedSubPixelVarianceTest() override = default;
  void SetUp() override {
    opt_func_ = GET_PARAM(0);
    ref_func_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
  }

 protected:
  MaskedSubPixelVarianceFunc opt_func_;
  MaskedSubPixelVarianceFunc ref_func_;
  aom_bit_depth_t bit_depth_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(HighbdMaskedSubPixelVarianceTest);

TEST_P(HighbdMaskedSubPixelVarianceTest, OperationCheck) {
  unsigned int ref_ret, opt_ret;
  unsigned int ref_sse, opt_sse;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, src_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 8)]);
  DECLARE_ALIGNED(16, uint16_t, ref_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 8)]);
  DECLARE_ALIGNED(16, uint16_t,
                  second_pred_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 8)]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 8)]);
  uint8_t *src8_ptr = CONVERT_TO_BYTEPTR(src_ptr);
  uint8_t *ref8_ptr = CONVERT_TO_BYTEPTR(ref_ptr);
  uint8_t *second_pred8_ptr = CONVERT_TO_BYTEPTR(second_pred_ptr);
  int err_count = 0;
  int first_failure = -1;
  int first_failure_x = -1;
  int first_failure_y = -1;
  int src_stride = (MAX_SB_SIZE + 8);
  int ref_stride = (MAX_SB_SIZE + 8);
  int msk_stride = (MAX_SB_SIZE + 8);
  int xoffset, yoffset;

  for (int i = 0; i < number_of_iterations; ++i) {
    for (int j = 0; j < (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 8); j++) {
      src_ptr[j] = rnd.Rand16() & ((1 << bit_depth_) - 1);
      ref_ptr[j] = rnd.Rand16() & ((1 << bit_depth_) - 1);
      second_pred_ptr[j] = rnd.Rand16() & ((1 << bit_depth_) - 1);
      msk_ptr[j] = rnd(65);
    }
    for (xoffset = 0; xoffset < BIL_SUBPEL_SHIFTS; xoffset++) {
      for (yoffset = 0; yoffset < BIL_SUBPEL_SHIFTS; yoffset++) {
        for (int invert_mask = 0; invert_mask < 2; ++invert_mask) {
          ref_ret = ref_func_(src8_ptr, src_stride, xoffset, yoffset, ref8_ptr,
                              ref_stride, second_pred8_ptr, msk_ptr, msk_stride,
                              invert_mask, &ref_sse);
          API_REGISTER_STATE_CHECK(
              opt_ret = opt_func_(src8_ptr, src_stride, xoffset, yoffset,
                                  ref8_ptr, ref_stride, second_pred8_ptr,
                                  msk_ptr, msk_stride, invert_mask, &opt_sse));

          if (opt_ret != ref_ret || opt_sse != ref_sse) {
            err_count++;
            if (first_failure == -1) {
              first_failure = i;
              first_failure_x = xoffset;
              first_failure_y = yoffset;
            }
          }
        }
      }
    }
  }

  EXPECT_EQ(0, err_count)
      << "Error: Masked Sub Pixel Variance Test OperationCheck,"
      << "C output doesn't match SSSE3 output. "
      << "First failed at test case " << first_failure
      << " x_offset = " << first_failure_x << " y_offset = " << first_failure_y;
}

TEST_P(HighbdMaskedSubPixelVarianceTest, ExtremeValues) {
  unsigned int ref_ret, opt_ret;
  unsigned int ref_sse, opt_sse;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, src_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 8)]);
  DECLARE_ALIGNED(16, uint16_t, ref_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 8)]);
  DECLARE_ALIGNED(16, uint8_t, msk_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 8)]);
  DECLARE_ALIGNED(16, uint16_t,
                  second_pred_ptr[(MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 8)]);
  uint8_t *src8_ptr = CONVERT_TO_BYTEPTR(src_ptr);
  uint8_t *ref8_ptr = CONVERT_TO_BYTEPTR(ref_ptr);
  uint8_t *second_pred8_ptr = CONVERT_TO_BYTEPTR(second_pred_ptr);
  int first_failure_x = -1;
  int first_failure_y = -1;
  int err_count = 0;
  int first_failure = -1;
  int src_stride = (MAX_SB_SIZE + 8);
  int ref_stride = (MAX_SB_SIZE + 8);
  int msk_stride = (MAX_SB_SIZE + 8);

  for (int xoffset = 0; xoffset < BIL_SUBPEL_SHIFTS; xoffset++) {
    for (int yoffset = 0; yoffset < BIL_SUBPEL_SHIFTS; yoffset++) {
      for (int i = 0; i < 16; ++i) {
        aom_memset16(src_ptr, (i & 0x1) ? ((1 << bit_depth_) - 1) : 0,
                     (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 8));
        aom_memset16(ref_ptr, (i & 0x2) ? ((1 << bit_depth_) - 1) : 0,
                     (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 8));
        aom_memset16(second_pred_ptr, (i & 0x4) ? ((1 << bit_depth_) - 1) : 0,
                     (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 8));
        memset(msk_ptr, (i & 0x8) ? 64 : 0,
               (MAX_SB_SIZE + 1) * (MAX_SB_SIZE + 8));

        for (int invert_mask = 0; invert_mask < 2; ++invert_mask) {
          ref_ret = ref_func_(src8_ptr, src_stride, xoffset, yoffset, ref8_ptr,
                              ref_stride, second_pred8_ptr, msk_ptr, msk_stride,
                              invert_mask, &ref_sse);
          API_REGISTER_STATE_CHECK(
              opt_ret = opt_func_(src8_ptr, src_stride, xoffset, yoffset,
                                  ref8_ptr, ref_stride, second_pred8_ptr,
                                  msk_ptr, msk_stride, invert_mask, &opt_sse));

          if (opt_ret != ref_ret || opt_sse != ref_sse) {
            err_count++;
            if (first_failure == -1) {
              first_failure = i;
              first_failure_x = xoffset;
              first_failure_y = yoffset;
            }
          }
        }
      }
    }
  }

  EXPECT_EQ(0, err_count) << "Error: Masked Variance Test ExtremeValues,"
                          << "C output doesn't match SSSE3 output. "
                          << "First failed at test case " << first_failure
                          << " x_offset = " << first_failure_x
                          << " y_offset = " << first_failure_y;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

using std::make_tuple;

#if HAVE_SSSE3

const MaskedSubPixelVarianceParam sub_pel_var_test[] = {
  make_tuple(&aom_masked_sub_pixel_variance128x128_ssse3,
             &aom_masked_sub_pixel_variance128x128_c),
  make_tuple(&aom_masked_sub_pixel_variance128x64_ssse3,
             &aom_masked_sub_pixel_variance128x64_c),
  make_tuple(&aom_masked_sub_pixel_variance64x128_ssse3,
             &aom_masked_sub_pixel_variance64x128_c),
  make_tuple(&aom_masked_sub_pixel_variance64x64_ssse3,
             &aom_masked_sub_pixel_variance64x64_c),
  make_tuple(&aom_masked_sub_pixel_variance64x32_ssse3,
             &aom_masked_sub_pixel_variance64x32_c),
  make_tuple(&aom_masked_sub_pixel_variance32x64_ssse3,
             &aom_masked_sub_pixel_variance32x64_c),
  make_tuple(&aom_masked_sub_pixel_variance32x32_ssse3,
             &aom_masked_sub_pixel_variance32x32_c),
  make_tuple(&aom_masked_sub_pixel_variance32x16_ssse3,
             &aom_masked_sub_pixel_variance32x16_c),
  make_tuple(&aom_masked_sub_pixel_variance16x32_ssse3,
             &aom_masked_sub_pixel_variance16x32_c),
  make_tuple(&aom_masked_sub_pixel_variance16x16_ssse3,
             &aom_masked_sub_pixel_variance16x16_c),
  make_tuple(&aom_masked_sub_pixel_variance16x8_ssse3,
             &aom_masked_sub_pixel_variance16x8_c),
  make_tuple(&aom_masked_sub_pixel_variance8x16_ssse3,
             &aom_masked_sub_pixel_variance8x16_c),
  make_tuple(&aom_masked_sub_pixel_variance8x8_ssse3,
             &aom_masked_sub_pixel_variance8x8_c),
  make_tuple(&aom_masked_sub_pixel_variance8x4_ssse3,
             &aom_masked_sub_pixel_variance8x4_c),
  make_tuple(&aom_masked_sub_pixel_variance4x8_ssse3,
             &aom_masked_sub_pixel_variance4x8_c),
  make_tuple(&aom_masked_sub_pixel_variance4x4_ssse3,
             &aom_masked_sub_pixel_variance4x4_c),
#if !CONFIG_REALTIME_ONLY
  make_tuple(&aom_masked_sub_pixel_variance64x16_ssse3,
             &aom_masked_sub_pixel_variance64x16_c),
  make_tuple(&aom_masked_sub_pixel_variance16x64_ssse3,
             &aom_masked_sub_pixel_variance16x64_c),
  make_tuple(&aom_masked_sub_pixel_variance32x8_ssse3,
             &aom_masked_sub_pixel_variance32x8_c),
  make_tuple(&aom_masked_sub_pixel_variance8x32_ssse3,
             &aom_masked_sub_pixel_variance8x32_c),
  make_tuple(&aom_masked_sub_pixel_variance16x4_ssse3,
             &aom_masked_sub_pixel_variance16x4_c),
  make_tuple(&aom_masked_sub_pixel_variance4x16_ssse3,
             &aom_masked_sub_pixel_variance4x16_c),
#endif
};

INSTANTIATE_TEST_SUITE_P(SSSE3_C_COMPARE, MaskedSubPixelVarianceTest,
                         ::testing::ValuesIn(sub_pel_var_test));

#if CONFIG_AV1_HIGHBITDEPTH
const HighbdMaskedSubPixelVarianceParam hbd_sub_pel_var_test[] = {
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance128x128_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance128x128_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance128x64_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance128x64_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance64x128_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance64x128_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance64x64_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance64x64_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance64x32_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance64x32_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance32x64_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance32x64_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance32x32_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance32x32_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance32x16_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance32x16_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance16x32_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance16x32_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance16x16_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance16x16_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance16x8_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance16x8_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance8x16_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance8x16_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance8x8_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance8x8_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance8x4_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance8x4_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance4x8_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance4x8_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance4x4_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance4x4_c, AOM_BITS_8),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance128x128_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance128x128_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance128x64_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance128x64_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance64x128_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance64x128_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance64x64_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance64x64_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance64x32_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance64x32_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance32x64_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance32x64_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance32x32_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance32x32_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance32x16_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance32x16_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance16x32_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance16x32_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance16x16_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance16x16_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance16x8_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance16x8_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance8x16_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance8x16_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance8x8_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance8x8_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance8x4_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance8x4_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance4x8_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance4x8_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance4x4_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance4x4_c, AOM_BITS_10),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance128x128_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance128x128_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance128x64_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance128x64_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance64x128_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance64x128_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance64x64_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance64x64_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance64x32_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance64x32_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance32x64_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance32x64_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance32x32_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance32x32_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance32x16_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance32x16_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance16x32_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance16x32_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance16x16_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance16x16_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance16x8_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance16x8_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance8x16_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance8x16_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance8x8_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance8x8_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance8x4_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance8x4_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance4x8_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance4x8_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance4x4_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance4x4_c, AOM_BITS_12),
#if !CONFIG_REALTIME_ONLY
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance64x16_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance64x16_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance16x64_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance16x64_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance32x8_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance32x8_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance8x32_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance8x32_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance16x4_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance16x4_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance4x16_ssse3,
             &aom_highbd_8_masked_sub_pixel_variance4x16_c, AOM_BITS_8),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance64x16_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance64x16_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance16x64_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance16x64_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance32x8_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance32x8_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance8x32_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance8x32_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance16x4_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance16x4_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance4x16_ssse3,
             &aom_highbd_10_masked_sub_pixel_variance4x16_c, AOM_BITS_10),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance64x16_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance64x16_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance16x64_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance16x64_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance32x8_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance32x8_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance8x32_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance8x32_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance16x4_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance16x4_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance4x16_ssse3,
             &aom_highbd_12_masked_sub_pixel_variance4x16_c, AOM_BITS_12),
#endif
};

INSTANTIATE_TEST_SUITE_P(SSSE3_C_COMPARE, HighbdMaskedSubPixelVarianceTest,
                         ::testing::ValuesIn(hbd_sub_pel_var_test));
#endif  // CONFIG_AV1_HIGHBITDEPTH
#endif  // HAVE_SSSE3

#if HAVE_NEON

const MaskedSubPixelVarianceParam sub_pel_var_test[] = {
  make_tuple(&aom_masked_sub_pixel_variance128x128_neon,
             &aom_masked_sub_pixel_variance128x128_c),
  make_tuple(&aom_masked_sub_pixel_variance128x64_neon,
             &aom_masked_sub_pixel_variance128x64_c),
  make_tuple(&aom_masked_sub_pixel_variance64x128_neon,
             &aom_masked_sub_pixel_variance64x128_c),
  make_tuple(&aom_masked_sub_pixel_variance64x64_neon,
             &aom_masked_sub_pixel_variance64x64_c),
  make_tuple(&aom_masked_sub_pixel_variance64x32_neon,
             &aom_masked_sub_pixel_variance64x32_c),
  make_tuple(&aom_masked_sub_pixel_variance32x64_neon,
             &aom_masked_sub_pixel_variance32x64_c),
  make_tuple(&aom_masked_sub_pixel_variance32x32_neon,
             &aom_masked_sub_pixel_variance32x32_c),
  make_tuple(&aom_masked_sub_pixel_variance32x16_neon,
             &aom_masked_sub_pixel_variance32x16_c),
  make_tuple(&aom_masked_sub_pixel_variance16x32_neon,
             &aom_masked_sub_pixel_variance16x32_c),
  make_tuple(&aom_masked_sub_pixel_variance16x16_neon,
             &aom_masked_sub_pixel_variance16x16_c),
  make_tuple(&aom_masked_sub_pixel_variance16x8_neon,
             &aom_masked_sub_pixel_variance16x8_c),
  make_tuple(&aom_masked_sub_pixel_variance8x16_neon,
             &aom_masked_sub_pixel_variance8x16_c),
  make_tuple(&aom_masked_sub_pixel_variance8x8_neon,
             &aom_masked_sub_pixel_variance8x8_c),
  make_tuple(&aom_masked_sub_pixel_variance8x4_neon,
             &aom_masked_sub_pixel_variance8x4_c),
  make_tuple(&aom_masked_sub_pixel_variance4x8_neon,
             &aom_masked_sub_pixel_variance4x8_c),
  make_tuple(&aom_masked_sub_pixel_variance4x4_neon,
             &aom_masked_sub_pixel_variance4x4_c),
#if !CONFIG_REALTIME_ONLY
  make_tuple(&aom_masked_sub_pixel_variance64x16_neon,
             &aom_masked_sub_pixel_variance64x16_c),
  make_tuple(&aom_masked_sub_pixel_variance16x64_neon,
             &aom_masked_sub_pixel_variance16x64_c),
  make_tuple(&aom_masked_sub_pixel_variance32x8_neon,
             &aom_masked_sub_pixel_variance32x8_c),
  make_tuple(&aom_masked_sub_pixel_variance8x32_neon,
             &aom_masked_sub_pixel_variance8x32_c),
  make_tuple(&aom_masked_sub_pixel_variance16x4_neon,
             &aom_masked_sub_pixel_variance16x4_c),
  make_tuple(&aom_masked_sub_pixel_variance4x16_neon,
             &aom_masked_sub_pixel_variance4x16_c),
#endif
};

INSTANTIATE_TEST_SUITE_P(NEON_C_COMPARE, MaskedSubPixelVarianceTest,
                         ::testing::ValuesIn(sub_pel_var_test));

#if CONFIG_AV1_HIGHBITDEPTH
const HighbdMaskedSubPixelVarianceParam hbd_sub_pel_var_test_neon[] = {
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance128x128_neon,
             &aom_highbd_8_masked_sub_pixel_variance128x128_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance128x64_neon,
             &aom_highbd_8_masked_sub_pixel_variance128x64_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance64x128_neon,
             &aom_highbd_8_masked_sub_pixel_variance64x128_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance64x64_neon,
             &aom_highbd_8_masked_sub_pixel_variance64x64_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance64x32_neon,
             &aom_highbd_8_masked_sub_pixel_variance64x32_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance32x64_neon,
             &aom_highbd_8_masked_sub_pixel_variance32x64_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance32x32_neon,
             &aom_highbd_8_masked_sub_pixel_variance32x32_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance32x16_neon,
             &aom_highbd_8_masked_sub_pixel_variance32x16_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance16x32_neon,
             &aom_highbd_8_masked_sub_pixel_variance16x32_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance16x16_neon,
             &aom_highbd_8_masked_sub_pixel_variance16x16_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance16x8_neon,
             &aom_highbd_8_masked_sub_pixel_variance16x8_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance8x16_neon,
             &aom_highbd_8_masked_sub_pixel_variance8x16_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance8x8_neon,
             &aom_highbd_8_masked_sub_pixel_variance8x8_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance8x4_neon,
             &aom_highbd_8_masked_sub_pixel_variance8x4_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance4x8_neon,
             &aom_highbd_8_masked_sub_pixel_variance4x8_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance4x4_neon,
             &aom_highbd_8_masked_sub_pixel_variance4x4_c, AOM_BITS_8),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance128x128_neon,
             &aom_highbd_10_masked_sub_pixel_variance128x128_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance128x64_neon,
             &aom_highbd_10_masked_sub_pixel_variance128x64_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance64x128_neon,
             &aom_highbd_10_masked_sub_pixel_variance64x128_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance64x64_neon,
             &aom_highbd_10_masked_sub_pixel_variance64x64_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance64x32_neon,
             &aom_highbd_10_masked_sub_pixel_variance64x32_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance32x64_neon,
             &aom_highbd_10_masked_sub_pixel_variance32x64_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance32x32_neon,
             &aom_highbd_10_masked_sub_pixel_variance32x32_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance32x16_neon,
             &aom_highbd_10_masked_sub_pixel_variance32x16_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance16x32_neon,
             &aom_highbd_10_masked_sub_pixel_variance16x32_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance16x16_neon,
             &aom_highbd_10_masked_sub_pixel_variance16x16_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance16x8_neon,
             &aom_highbd_10_masked_sub_pixel_variance16x8_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance8x16_neon,
             &aom_highbd_10_masked_sub_pixel_variance8x16_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance8x8_neon,
             &aom_highbd_10_masked_sub_pixel_variance8x8_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance8x4_neon,
             &aom_highbd_10_masked_sub_pixel_variance8x4_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance4x8_neon,
             &aom_highbd_10_masked_sub_pixel_variance4x8_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance4x4_neon,
             &aom_highbd_10_masked_sub_pixel_variance4x4_c, AOM_BITS_10),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance128x128_neon,
             &aom_highbd_12_masked_sub_pixel_variance128x128_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance128x64_neon,
             &aom_highbd_12_masked_sub_pixel_variance128x64_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance64x128_neon,
             &aom_highbd_12_masked_sub_pixel_variance64x128_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance64x64_neon,
             &aom_highbd_12_masked_sub_pixel_variance64x64_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance64x32_neon,
             &aom_highbd_12_masked_sub_pixel_variance64x32_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance32x64_neon,
             &aom_highbd_12_masked_sub_pixel_variance32x64_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance32x32_neon,
             &aom_highbd_12_masked_sub_pixel_variance32x32_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance32x16_neon,
             &aom_highbd_12_masked_sub_pixel_variance32x16_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance16x32_neon,
             &aom_highbd_12_masked_sub_pixel_variance16x32_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance16x16_neon,
             &aom_highbd_12_masked_sub_pixel_variance16x16_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance16x8_neon,
             &aom_highbd_12_masked_sub_pixel_variance16x8_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance8x16_neon,
             &aom_highbd_12_masked_sub_pixel_variance8x16_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance8x8_neon,
             &aom_highbd_12_masked_sub_pixel_variance8x8_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance8x4_neon,
             &aom_highbd_12_masked_sub_pixel_variance8x4_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance4x8_neon,
             &aom_highbd_12_masked_sub_pixel_variance4x8_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance4x4_neon,
             &aom_highbd_12_masked_sub_pixel_variance4x4_c, AOM_BITS_12),
#if !CONFIG_REALTIME_ONLY
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance64x16_neon,
             &aom_highbd_8_masked_sub_pixel_variance64x16_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance16x64_neon,
             &aom_highbd_8_masked_sub_pixel_variance16x64_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance32x8_neon,
             &aom_highbd_8_masked_sub_pixel_variance32x8_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance8x32_neon,
             &aom_highbd_8_masked_sub_pixel_variance8x32_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance16x4_neon,
             &aom_highbd_8_masked_sub_pixel_variance16x4_c, AOM_BITS_8),
  make_tuple(&aom_highbd_8_masked_sub_pixel_variance4x16_neon,
             &aom_highbd_8_masked_sub_pixel_variance4x16_c, AOM_BITS_8),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance64x16_neon,
             &aom_highbd_10_masked_sub_pixel_variance64x16_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance16x64_neon,
             &aom_highbd_10_masked_sub_pixel_variance16x64_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance32x8_neon,
             &aom_highbd_10_masked_sub_pixel_variance32x8_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance8x32_neon,
             &aom_highbd_10_masked_sub_pixel_variance8x32_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance16x4_neon,
             &aom_highbd_10_masked_sub_pixel_variance16x4_c, AOM_BITS_10),
  make_tuple(&aom_highbd_10_masked_sub_pixel_variance4x16_neon,
             &aom_highbd_10_masked_sub_pixel_variance4x16_c, AOM_BITS_10),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance64x16_neon,
             &aom_highbd_12_masked_sub_pixel_variance64x16_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance16x64_neon,
             &aom_highbd_12_masked_sub_pixel_variance16x64_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance32x8_neon,
             &aom_highbd_12_masked_sub_pixel_variance32x8_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance8x32_neon,
             &aom_highbd_12_masked_sub_pixel_variance8x32_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance16x4_neon,
             &aom_highbd_12_masked_sub_pixel_variance16x4_c, AOM_BITS_12),
  make_tuple(&aom_highbd_12_masked_sub_pixel_variance4x16_neon,
             &aom_highbd_12_masked_sub_pixel_variance4x16_c, AOM_BITS_12),
#endif
};

INSTANTIATE_TEST_SUITE_P(NEON_C_COMPARE, HighbdMaskedSubPixelVarianceTest,
                         ::testing::ValuesIn(hbd_sub_pel_var_test_neon));

#endif  // CONFIG_AV1_HIGHBITDEPTH

#endif  // HAVE_NEON
}  // namespace
