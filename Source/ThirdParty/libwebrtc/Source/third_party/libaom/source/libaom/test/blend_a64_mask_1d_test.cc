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

#include "gtest/gtest.h"
#include "test/register_state_check.h"
#include "test/function_equivalence_test.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"

#include "av1/common/enums.h"

#include "aom_dsp/blend.h"

using libaom_test::FunctionEquivalenceTest;

namespace {

template <typename F, typename T>
class BlendA64Mask1DTest : public FunctionEquivalenceTest<F> {
 public:
  static const int kIterations = 10000;
  static const int kMaxWidth = MAX_SB_SIZE * 5;  // * 5 to cover longer strides
  static const int kMaxHeight = MAX_SB_SIZE;
  static const int kBufSize = kMaxWidth * kMaxHeight;
  static const int kMaxMaskWidth = 2 * MAX_SB_SIZE;
  static const int kMaxMaskSize = kMaxMaskWidth;

  ~BlendA64Mask1DTest() override = default;

  virtual void Execute(const T *p_src0, const T *p_src1) = 0;

  void Common(int block_size) {
    w_ = block_size_wide[block_size];
    h_ = block_size_high[block_size];

    dst_offset_ = this->rng_(33);
    dst_stride_ = this->rng_(kMaxWidth + 1 - w_) + w_;

    src0_offset_ = this->rng_(33);
    src0_stride_ = this->rng_(kMaxWidth + 1 - w_) + w_;

    src1_offset_ = this->rng_(33);
    src1_stride_ = this->rng_(kMaxWidth + 1 - w_) + w_;

    T *p_src0;
    T *p_src1;

    switch (this->rng_(3)) {
      case 0:  // Separate sources
        p_src0 = src0_;
        p_src1 = src1_;
        break;
      case 1:  // src0 == dst
        p_src0 = dst_tst_;
        src0_stride_ = dst_stride_;
        src0_offset_ = dst_offset_;
        p_src1 = src1_;
        break;
      case 2:  // src1 == dst
        p_src0 = src0_;
        p_src1 = dst_tst_;
        src1_stride_ = dst_stride_;
        src1_offset_ = dst_offset_;
        break;
      default: FAIL();
    }

    Execute(p_src0, p_src1);

    for (int r = 0; r < h_; ++r) {
      for (int c = 0; c < w_; ++c) {
        ASSERT_EQ(dst_ref_[dst_offset_ + r * dst_stride_ + c],
                  dst_tst_[dst_offset_ + r * dst_stride_ + c]);
      }
    }
  }

  T dst_ref_[kBufSize];
  T dst_tst_[kBufSize];
  uint32_t dst_stride_;
  uint32_t dst_offset_;

  T src0_[kBufSize];
  uint32_t src0_stride_;
  uint32_t src0_offset_;

  T src1_[kBufSize];
  uint32_t src1_stride_;
  uint32_t src1_offset_;

  uint8_t mask_[kMaxMaskSize];

  int w_;
  int h_;
};

//////////////////////////////////////////////////////////////////////////////
// 8 bit version
//////////////////////////////////////////////////////////////////////////////

typedef void (*F8B)(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
                    uint32_t src0_stride, const uint8_t *src1,
                    uint32_t src1_stride, const uint8_t *mask, int w, int h);
typedef libaom_test::FuncParam<F8B> TestFuncs;

class BlendA64Mask1DTest8B : public BlendA64Mask1DTest<F8B, uint8_t> {
 protected:
  void Execute(const uint8_t *p_src0, const uint8_t *p_src1) override {
    params_.ref_func(dst_ref_ + dst_offset_, dst_stride_, p_src0 + src0_offset_,
                     src0_stride_, p_src1 + src1_offset_, src1_stride_, mask_,
                     w_, h_);
    API_REGISTER_STATE_CHECK(params_.tst_func(
        dst_tst_ + dst_offset_, dst_stride_, p_src0 + src0_offset_,
        src0_stride_, p_src1 + src1_offset_, src1_stride_, mask_, w_, h_));
  }
};

TEST_P(BlendA64Mask1DTest8B, RandomValues) {
  for (int bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
    for (int i = 0; i < kBufSize; ++i) {
      dst_ref_[i] = rng_.Rand8();
      dst_tst_[i] = rng_.Rand8();

      src0_[i] = rng_.Rand8();
      src1_[i] = rng_.Rand8();
    }

    for (int i = 0; i < kMaxMaskSize; ++i)
      mask_[i] = rng_(AOM_BLEND_A64_MAX_ALPHA + 1);

    Common(bsize);
  }
}

TEST_P(BlendA64Mask1DTest8B, ExtremeValues) {
  for (int i = 0; i < kBufSize; ++i) {
    dst_ref_[i] = rng_(2) + 254;
    dst_tst_[i] = rng_(2) + 254;
    src0_[i] = rng_(2) + 254;
    src1_[i] = rng_(2) + 254;
  }

  for (int i = 0; i < kMaxMaskSize; ++i)
    mask_[i] = rng_(2) + AOM_BLEND_A64_MAX_ALPHA - 1;

  for (int bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
    Common(bsize);
  }
}

static void blend_a64_hmask_ref(uint8_t *dst, uint32_t dst_stride,
                                const uint8_t *src0, uint32_t src0_stride,
                                const uint8_t *src1, uint32_t src1_stride,
                                const uint8_t *mask, int w, int h) {
  uint8_t mask2d[BlendA64Mask1DTest8B::kMaxMaskSize]
                [BlendA64Mask1DTest8B::kMaxMaskSize];

  for (int row = 0; row < h; ++row)
    for (int col = 0; col < w; ++col) mask2d[row][col] = mask[col];

  aom_blend_a64_mask_c(dst, dst_stride, src0, src0_stride, src1, src1_stride,
                       &mask2d[0][0], BlendA64Mask1DTest8B::kMaxMaskSize, w, h,
                       0, 0);
}

static void blend_a64_vmask_ref(uint8_t *dst, uint32_t dst_stride,
                                const uint8_t *src0, uint32_t src0_stride,
                                const uint8_t *src1, uint32_t src1_stride,
                                const uint8_t *mask, int w, int h) {
  uint8_t mask2d[BlendA64Mask1DTest8B::kMaxMaskSize]
                [BlendA64Mask1DTest8B::kMaxMaskSize];

  for (int row = 0; row < h; ++row)
    for (int col = 0; col < w; ++col) mask2d[row][col] = mask[row];

  aom_blend_a64_mask_c(dst, dst_stride, src0, src0_stride, src1, src1_stride,
                       &mask2d[0][0], BlendA64Mask1DTest8B::kMaxMaskSize, w, h,
                       0, 0);
}

INSTANTIATE_TEST_SUITE_P(
    C, BlendA64Mask1DTest8B,
    ::testing::Values(TestFuncs(blend_a64_hmask_ref, aom_blend_a64_hmask_c),
                      TestFuncs(blend_a64_vmask_ref, aom_blend_a64_vmask_c)));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, BlendA64Mask1DTest8B,
    ::testing::Values(
        TestFuncs(blend_a64_hmask_ref, aom_blend_a64_hmask_sse4_1),
        TestFuncs(blend_a64_vmask_ref, aom_blend_a64_vmask_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, BlendA64Mask1DTest8B,
    ::testing::Values(TestFuncs(blend_a64_hmask_ref, aom_blend_a64_hmask_neon),
                      TestFuncs(blend_a64_vmask_ref,
                                aom_blend_a64_vmask_neon)));
#endif  // HAVE_NEON

//////////////////////////////////////////////////////////////////////////////
// High bit-depth version
//////////////////////////////////////////////////////////////////////////////
#if CONFIG_AV1_HIGHBITDEPTH
typedef void (*FHBD)(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
                     uint32_t src0_stride, const uint8_t *src1,
                     uint32_t src1_stride, const uint8_t *mask, int w, int h,
                     int bd);
typedef libaom_test::FuncParam<FHBD> TestFuncsHBD;

class BlendA64Mask1DTestHBD : public BlendA64Mask1DTest<FHBD, uint16_t> {
 protected:
  void Execute(const uint16_t *p_src0, const uint16_t *p_src1) override {
    params_.ref_func(CONVERT_TO_BYTEPTR(dst_ref_ + dst_offset_), dst_stride_,
                     CONVERT_TO_BYTEPTR(p_src0 + src0_offset_), src0_stride_,
                     CONVERT_TO_BYTEPTR(p_src1 + src1_offset_), src1_stride_,
                     mask_, w_, h_, bit_depth_);
    API_REGISTER_STATE_CHECK(params_.tst_func(
        CONVERT_TO_BYTEPTR(dst_tst_ + dst_offset_), dst_stride_,
        CONVERT_TO_BYTEPTR(p_src0 + src0_offset_), src0_stride_,
        CONVERT_TO_BYTEPTR(p_src1 + src1_offset_), src1_stride_, mask_, w_, h_,
        bit_depth_));
  }

  int bit_depth_;
};

TEST_P(BlendA64Mask1DTestHBD, RandomValues) {
  for (bit_depth_ = 8; bit_depth_ <= 12; bit_depth_ += 2) {
    for (int bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
      const int hi = 1 << bit_depth_;

      for (int i = 0; i < kBufSize; ++i) {
        dst_ref_[i] = rng_(hi);
        dst_tst_[i] = rng_(hi);
        src0_[i] = rng_(hi);
        src1_[i] = rng_(hi);
      }

      for (int i = 0; i < kMaxMaskSize; ++i)
        mask_[i] = rng_(AOM_BLEND_A64_MAX_ALPHA + 1);

      Common(bsize);
    }
  }
}

TEST_P(BlendA64Mask1DTestHBD, ExtremeValues) {
  for (bit_depth_ = 8; bit_depth_ <= 12; bit_depth_ += 2) {
    const int hi = 1 << bit_depth_;
    const int lo = hi - 2;

    for (int i = 0; i < kBufSize; ++i) {
      dst_ref_[i] = rng_(hi - lo) + lo;
      dst_tst_[i] = rng_(hi - lo) + lo;
      src0_[i] = rng_(hi - lo) + lo;
      src1_[i] = rng_(hi - lo) + lo;
    }

    for (int i = 0; i < kMaxMaskSize; ++i)
      mask_[i] = rng_(2) + AOM_BLEND_A64_MAX_ALPHA - 1;

    for (int bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
      Common(bsize);
    }
  }
}

static void highbd_blend_a64_hmask_ref(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, int w, int h, int bd) {
  uint8_t mask2d[BlendA64Mask1DTestHBD::kMaxMaskSize]
                [BlendA64Mask1DTestHBD::kMaxMaskSize];

  for (int row = 0; row < h; ++row)
    for (int col = 0; col < w; ++col) mask2d[row][col] = mask[col];

  aom_highbd_blend_a64_mask_c(
      dst, dst_stride, src0, src0_stride, src1, src1_stride, &mask2d[0][0],
      BlendA64Mask1DTestHBD::kMaxMaskSize, w, h, 0, 0, bd);
}

static void highbd_blend_a64_vmask_ref(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, int w, int h, int bd) {
  uint8_t mask2d[BlendA64Mask1DTestHBD::kMaxMaskSize]
                [BlendA64Mask1DTestHBD::kMaxMaskSize];

  for (int row = 0; row < h; ++row)
    for (int col = 0; col < w; ++col) mask2d[row][col] = mask[row];

  aom_highbd_blend_a64_mask_c(
      dst, dst_stride, src0, src0_stride, src1, src1_stride, &mask2d[0][0],
      BlendA64Mask1DTestHBD::kMaxMaskSize, w, h, 0, 0, bd);
}

INSTANTIATE_TEST_SUITE_P(
    C, BlendA64Mask1DTestHBD,
    ::testing::Values(TestFuncsHBD(highbd_blend_a64_hmask_ref,
                                   aom_highbd_blend_a64_hmask_c),
                      TestFuncsHBD(highbd_blend_a64_vmask_ref,
                                   aom_highbd_blend_a64_vmask_c)));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, BlendA64Mask1DTestHBD,
    ::testing::Values(TestFuncsHBD(highbd_blend_a64_hmask_ref,
                                   aom_highbd_blend_a64_hmask_sse4_1),
                      TestFuncsHBD(highbd_blend_a64_vmask_ref,
                                   aom_highbd_blend_a64_vmask_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, BlendA64Mask1DTestHBD,
    ::testing::Values(TestFuncsHBD(highbd_blend_a64_hmask_ref,
                                   aom_highbd_blend_a64_hmask_neon),
                      TestFuncsHBD(highbd_blend_a64_vmask_ref,
                                   aom_highbd_blend_a64_vmask_neon)));
#endif  // HAVE_NEON

#endif  // CONFIG_AV1_HIGHBITDEPTH
}  // namespace
