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

template <typename BlendA64Func, typename SrcPixel, typename DstPixel>
class BlendA64MaskTest : public FunctionEquivalenceTest<BlendA64Func> {
 protected:
  static const int kIterations = 10000;
  static const int kMaxWidth = MAX_SB_SIZE * 5;  // * 5 to cover longer strides
  static const int kMaxHeight = MAX_SB_SIZE;
  static const int kBufSize = kMaxWidth * kMaxHeight;
  static const int kMaxMaskWidth = 2 * MAX_SB_SIZE;
  static const int kMaxMaskSize = kMaxMaskWidth * kMaxMaskWidth;

  ~BlendA64MaskTest() override = default;

  virtual void Execute(const SrcPixel *p_src0, const SrcPixel *p_src1,
                       int run_times) = 0;

  template <typename Pixel>
  void GetSources(Pixel **src0, Pixel **src1, Pixel * /*dst*/, int run_times) {
    if (run_times > 1) {
      *src0 = src0_;
      *src1 = src1_;
      return;
    }
    switch (this->rng_(3)) {
      case 0:  // Separate sources
        *src0 = src0_;
        *src1 = src1_;
        break;
      case 1:  // src0 == dst
        *src0 = dst_tst_;
        src0_stride_ = dst_stride_;
        src0_offset_ = dst_offset_;
        *src1 = src1_;
        break;
      case 2:  // src1 == dst
        *src0 = src0_;
        *src1 = dst_tst_;
        src1_stride_ = dst_stride_;
        src1_offset_ = dst_offset_;
        break;
      default: FAIL();
    }
  }

  void GetSources(uint16_t **src0, uint16_t **src1, uint8_t * /*dst*/,
                  int /*run_times*/) {
    *src0 = src0_;
    *src1 = src1_;
  }

  uint8_t Rand1() { return this->rng_.Rand8() & 1; }

  void RunOneTest(int block_size, int subx, int suby, int run_times) {
    w_ = block_size_wide[block_size];
    h_ = block_size_high[block_size];
    run_times = run_times > 1 ? run_times / w_ : 1;
    ASSERT_GT(run_times, 0);
    subx_ = subx;
    suby_ = suby;

    dst_offset_ = this->rng_(33);
    dst_stride_ = this->rng_(kMaxWidth + 1 - w_) + w_;

    src0_offset_ = this->rng_(33);
    src0_stride_ = this->rng_(kMaxWidth + 1 - w_) + w_;

    src1_offset_ = this->rng_(33);
    src1_stride_ = this->rng_(kMaxWidth + 1 - w_) + w_;

    mask_stride_ =
        this->rng_(kMaxWidth + 1 - w_ * (subx_ ? 2 : 1)) + w_ * (subx_ ? 2 : 1);

    SrcPixel *p_src0;
    SrcPixel *p_src1;

    p_src0 = src0_;
    p_src1 = src1_;

    GetSources(&p_src0, &p_src1, &dst_ref_[0], run_times);

    Execute(p_src0, p_src1, run_times);

    for (int r = 0; r < h_; ++r) {
      for (int c = 0; c < w_; ++c) {
        ASSERT_EQ(dst_ref_[dst_offset_ + r * dst_stride_ + c],
                  dst_tst_[dst_offset_ + r * dst_stride_ + c])
            << w_ << "x" << h_ << " subx " << subx_ << " suby " << suby_
            << " r: " << r << " c: " << c;
      }
    }
  }

  void RunTest(int block_size, int run_times) {
    for (subx_ = 0; subx_ <= 1; subx_++) {
      for (suby_ = 0; suby_ <= 1; suby_++) {
        RunOneTest(block_size, subx_, suby_, run_times);
      }
    }
  }

  DstPixel dst_ref_[kBufSize];
  DstPixel dst_tst_[kBufSize];
  uint32_t dst_stride_;
  uint32_t dst_offset_;

  SrcPixel src0_[kBufSize];
  uint32_t src0_stride_;
  uint32_t src0_offset_;

  SrcPixel src1_[kBufSize];
  uint32_t src1_stride_;
  uint32_t src1_offset_;

  uint8_t mask_[kMaxMaskSize];
  size_t mask_stride_;

  int w_;
  int h_;

  int suby_;
  int subx_;
};

//////////////////////////////////////////////////////////////////////////////
// 8 bit version
//////////////////////////////////////////////////////////////////////////////

typedef void (*F8B)(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
                    uint32_t src0_stride, const uint8_t *src1,
                    uint32_t src1_stride, const uint8_t *mask,
                    uint32_t mask_stride, int w, int h, int subx, int suby);
typedef libaom_test::FuncParam<F8B> TestFuncs;

class BlendA64MaskTest8B : public BlendA64MaskTest<F8B, uint8_t, uint8_t> {
 protected:
  void Execute(const uint8_t *p_src0, const uint8_t *p_src1,
               int run_times) override {
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      params_.ref_func(dst_ref_ + dst_offset_, dst_stride_,
                       p_src0 + src0_offset_, src0_stride_,
                       p_src1 + src1_offset_, src1_stride_, mask_,
                       kMaxMaskWidth, w_, h_, subx_, suby_);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      params_.tst_func(dst_tst_ + dst_offset_, dst_stride_,
                       p_src0 + src0_offset_, src0_stride_,
                       p_src1 + src1_offset_, src1_stride_, mask_,
                       kMaxMaskWidth, w_, h_, subx_, suby_);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    if (run_times > 1) {
      printf("%3dx%-3d subx %d suby %d :%7.2f/%7.2fns", w_, h_, subx_, suby_,
             time1, time2);
      printf("(%3.2f)\n", time1 / time2);
    }
  }
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BlendA64MaskTest8B);

TEST_P(BlendA64MaskTest8B, RandomValues) {
  for (int bsize = 0; bsize < BLOCK_SIZES_ALL && !HasFatalFailure(); ++bsize) {
    for (int i = 0; i < kBufSize; ++i) {
      dst_ref_[i] = rng_.Rand8();
      dst_tst_[i] = rng_.Rand8();

      src0_[i] = rng_.Rand8();
      src1_[i] = rng_.Rand8();
    }

    for (int i = 0; i < kMaxMaskSize; ++i)
      mask_[i] = rng_(AOM_BLEND_A64_MAX_ALPHA + 1);

    RunTest(bsize, 1);
  }
}

TEST_P(BlendA64MaskTest8B, ExtremeValues) {
  for (int i = 0; i < kBufSize; ++i) {
    dst_ref_[i] = rng_(2) + 254;
    dst_tst_[i] = rng_(2) + 254;
    src0_[i] = rng_(2) + 254;
    src1_[i] = rng_(2) + 254;
  }

  for (int i = 0; i < kMaxMaskSize; ++i)
    mask_[i] = rng_(2) + AOM_BLEND_A64_MAX_ALPHA - 1;

  for (int bsize = 0; bsize < BLOCK_SIZES_ALL && !HasFatalFailure(); ++bsize)
    RunTest(bsize, 1);
}

TEST_P(BlendA64MaskTest8B, DISABLED_Speed) {
  const int kRunTimes = 10000000;
  for (int bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
    for (int i = 0; i < kBufSize; ++i) {
      dst_ref_[i] = rng_.Rand8();
      dst_tst_[i] = rng_.Rand8();

      src0_[i] = rng_.Rand8();
      src1_[i] = rng_.Rand8();
    }

    for (int i = 0; i < kMaxMaskSize; ++i)
      mask_[i] = rng_(AOM_BLEND_A64_MAX_ALPHA + 1);

    RunTest(bsize, kRunTimes);
  }
}
#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE4_1, BlendA64MaskTest8B,
                         ::testing::Values(TestFuncs(
                             aom_blend_a64_mask_c, aom_blend_a64_mask_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, BlendA64MaskTest8B,
                         ::testing::Values(TestFuncs(aom_blend_a64_mask_sse4_1,
                                                     aom_blend_a64_mask_avx2)));
#endif  // HAVE_AVX2

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, BlendA64MaskTest8B,
                         ::testing::Values(TestFuncs(aom_blend_a64_mask_c,
                                                     aom_blend_a64_mask_neon)));
#endif  // HAVE_NEON

//////////////////////////////////////////////////////////////////////////////
// 8 bit _d16 version
//////////////////////////////////////////////////////////////////////////////

typedef void (*F8B_D16)(uint8_t *dst, uint32_t dst_stride, const uint16_t *src0,
                        uint32_t src0_stride, const uint16_t *src1,
                        uint32_t src1_stride, const uint8_t *mask,
                        uint32_t mask_stride, int w, int h, int subx, int suby,
                        ConvolveParams *conv_params);
typedef libaom_test::FuncParam<F8B_D16> TestFuncs_d16;

class BlendA64MaskTest8B_d16
    : public BlendA64MaskTest<F8B_D16, uint16_t, uint8_t> {
 protected:
  // max number of bits used by the source
  static const int kSrcMaxBitsMask = 0x3fff;

  void Execute(const uint16_t *p_src0, const uint16_t *p_src1,
               int run_times) override {
    ConvolveParams conv_params;
    conv_params.round_0 = ROUND0_BITS;
    conv_params.round_1 = COMPOUND_ROUND1_BITS;
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      params_.ref_func(dst_ref_ + dst_offset_, dst_stride_,
                       p_src0 + src0_offset_, src0_stride_,
                       p_src1 + src1_offset_, src1_stride_, mask_,
                       kMaxMaskWidth, w_, h_, subx_, suby_, &conv_params);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      params_.tst_func(dst_tst_ + dst_offset_, dst_stride_,
                       p_src0 + src0_offset_, src0_stride_,
                       p_src1 + src1_offset_, src1_stride_, mask_,
                       kMaxMaskWidth, w_, h_, subx_, suby_, &conv_params);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    if (run_times > 1) {
      printf("%3dx%-3d subx %d suby %d :%7.2f/%7.2fns", w_, h_, subx_, suby_,
             time1, time2);
      printf("(%3.2f)\n", time1 / time2);
    }
  }
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BlendA64MaskTest8B_d16);

TEST_P(BlendA64MaskTest8B_d16, RandomValues) {
  for (int bsize = 0; bsize < BLOCK_SIZES_ALL && !HasFatalFailure(); ++bsize) {
    for (int i = 0; i < kBufSize; ++i) {
      dst_ref_[i] = rng_.Rand8();
      dst_tst_[i] = rng_.Rand8();

      src0_[i] = rng_.Rand16() & kSrcMaxBitsMask;
      src1_[i] = rng_.Rand16() & kSrcMaxBitsMask;
    }

    for (int i = 0; i < kMaxMaskSize; ++i)
      mask_[i] = rng_(AOM_BLEND_A64_MAX_ALPHA + 1);

    RunTest(bsize, 1);
  }
}

TEST_P(BlendA64MaskTest8B_d16, ExtremeValues) {
  for (int i = 0; i < kBufSize; ++i) {
    dst_ref_[i] = 255;
    dst_tst_[i] = 255;

    src0_[i] = kSrcMaxBitsMask;
    src1_[i] = kSrcMaxBitsMask;
  }

  for (int i = 0; i < kMaxMaskSize; ++i) mask_[i] = AOM_BLEND_A64_MAX_ALPHA - 1;

  for (int bsize = 0; bsize < BLOCK_SIZES_ALL && !HasFatalFailure(); ++bsize)
    RunTest(bsize, 1);
}

TEST_P(BlendA64MaskTest8B_d16, DISABLED_Speed) {
  const int kRunTimes = 10000000;
  for (int bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
    for (int i = 0; i < kBufSize; ++i) {
      dst_ref_[i] = rng_.Rand8();
      dst_tst_[i] = rng_.Rand8();

      src0_[i] = rng_.Rand16() & kSrcMaxBitsMask;
      src1_[i] = rng_.Rand16() & kSrcMaxBitsMask;
    }

    for (int i = 0; i < kMaxMaskSize; ++i)
      mask_[i] = rng_(AOM_BLEND_A64_MAX_ALPHA + 1);

    RunTest(bsize, kRunTimes);
  }
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, BlendA64MaskTest8B_d16,
    ::testing::Values(TestFuncs_d16(aom_lowbd_blend_a64_d16_mask_c,
                                    aom_lowbd_blend_a64_d16_mask_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, BlendA64MaskTest8B_d16,
    ::testing::Values(TestFuncs_d16(aom_lowbd_blend_a64_d16_mask_c,
                                    aom_lowbd_blend_a64_d16_mask_avx2)));
#endif  // HAVE_AVX2

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, BlendA64MaskTest8B_d16,
    ::testing::Values(TestFuncs_d16(aom_lowbd_blend_a64_d16_mask_c,
                                    aom_lowbd_blend_a64_d16_mask_neon)));
#endif  // HAVE_NEON

//////////////////////////////////////////////////////////////////////////////
// High bit-depth version
//////////////////////////////////////////////////////////////////////////////
#if CONFIG_AV1_HIGHBITDEPTH
typedef void (*FHBD)(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
                     uint32_t src0_stride, const uint8_t *src1,
                     uint32_t src1_stride, const uint8_t *mask,
                     uint32_t mask_stride, int w, int h, int subx, int suby,
                     int bd);
typedef libaom_test::FuncParam<FHBD> TestFuncsHBD;

class BlendA64MaskTestHBD : public BlendA64MaskTest<FHBD, uint16_t, uint16_t> {
 protected:
  void Execute(const uint16_t *p_src0, const uint16_t *p_src1,
               int run_times) override {
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      params_.ref_func(CONVERT_TO_BYTEPTR(dst_ref_ + dst_offset_), dst_stride_,
                       CONVERT_TO_BYTEPTR(p_src0 + src0_offset_), src0_stride_,
                       CONVERT_TO_BYTEPTR(p_src1 + src1_offset_), src1_stride_,
                       mask_, kMaxMaskWidth, w_, h_, subx_, suby_, bit_depth_);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      params_.tst_func(CONVERT_TO_BYTEPTR(dst_tst_ + dst_offset_), dst_stride_,
                       CONVERT_TO_BYTEPTR(p_src0 + src0_offset_), src0_stride_,
                       CONVERT_TO_BYTEPTR(p_src1 + src1_offset_), src1_stride_,
                       mask_, kMaxMaskWidth, w_, h_, subx_, suby_, bit_depth_);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    if (run_times > 1) {
      printf("%3dx%-3d subx %d suby %d :%7.2f/%7.2fns", w_, h_, subx_, suby_,
             time1, time2);
      printf("(%3.2f)\n", time1 / time2);
    }
  }

  int bit_depth_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BlendA64MaskTestHBD);

TEST_P(BlendA64MaskTestHBD, RandomValues) {
  for (bit_depth_ = 8; bit_depth_ <= 12 && !HasFatalFailure();
       bit_depth_ += 2) {
    const int hi = 1 << bit_depth_;

    for (int bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
      for (int i = 0; i < kBufSize; ++i) {
        dst_ref_[i] = rng_(hi);
        dst_tst_[i] = rng_(hi);
        src0_[i] = rng_(hi);
        src1_[i] = rng_(hi);
      }

      for (int i = 0; i < kMaxMaskSize; ++i)
        mask_[i] = rng_(AOM_BLEND_A64_MAX_ALPHA + 1);

      RunTest(bsize, 1);
    }
  }
}

TEST_P(BlendA64MaskTestHBD, ExtremeValues) {
  for (bit_depth_ = 8; bit_depth_ <= 12 && !HasFatalFailure();
       bit_depth_ += 2) {
    const int hi = 1 << bit_depth_;
    const int lo = hi - 2;

    for (int bsize = 0; bsize < BLOCK_SIZES_ALL && !HasFatalFailure();
         ++bsize) {
      for (int i = 0; i < kBufSize; ++i) {
        dst_ref_[i] = rng_(hi - lo) + lo;
        dst_tst_[i] = rng_(hi - lo) + lo;
        src0_[i] = rng_(hi - lo) + lo;
        src1_[i] = rng_(hi - lo) + lo;
      }

      for (int i = 0; i < kMaxMaskSize; ++i)
        mask_[i] = rng_(2) + AOM_BLEND_A64_MAX_ALPHA - 1;

      RunTest(bsize, 1);
    }
  }
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, BlendA64MaskTestHBD,
    ::testing::Values(TestFuncsHBD(aom_highbd_blend_a64_mask_c,
                                   aom_highbd_blend_a64_mask_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, BlendA64MaskTestHBD,
    ::testing::Values(TestFuncsHBD(aom_highbd_blend_a64_mask_c,
                                   aom_highbd_blend_a64_mask_neon)));
#endif  // HAVE_NEON

//////////////////////////////////////////////////////////////////////////////
// HBD _d16 version
//////////////////////////////////////////////////////////////////////////////

typedef void (*FHBD_D16)(uint8_t *dst, uint32_t dst_stride,
                         const CONV_BUF_TYPE *src0, uint32_t src0_stride,
                         const CONV_BUF_TYPE *src1, uint32_t src1_stride,
                         const uint8_t *mask, uint32_t mask_stride, int w,
                         int h, int subx, int suby, ConvolveParams *conv_params,
                         const int bd);
typedef libaom_test::FuncParam<FHBD_D16> TestFuncsHBD_d16;

class BlendA64MaskTestHBD_d16
    : public BlendA64MaskTest<FHBD_D16, uint16_t, uint16_t> {
 protected:
  // max number of bits used by the source
  static const int kSrcMaxBitsMask = (1 << 14) - 1;
  static const int kSrcMaxBitsMaskHBD = (1 << 16) - 1;

  void Execute(const uint16_t *p_src0, const uint16_t *p_src1,
               int run_times) override {
    ASSERT_GT(run_times, 0) << "Cannot run 0 iterations of the test.";
    ConvolveParams conv_params;
    conv_params.round_0 = (bit_depth_ == 12) ? ROUND0_BITS + 2 : ROUND0_BITS;
    conv_params.round_1 = COMPOUND_ROUND1_BITS;
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      params_.ref_func(CONVERT_TO_BYTEPTR(dst_ref_ + dst_offset_), dst_stride_,
                       p_src0 + src0_offset_, src0_stride_,
                       p_src1 + src1_offset_, src1_stride_, mask_,
                       kMaxMaskWidth, w_, h_, subx_, suby_, &conv_params,
                       bit_depth_);
    }
    if (params_.tst_func) {
      aom_usec_timer_mark(&timer);
      const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
      aom_usec_timer_start(&timer);
      for (int i = 0; i < run_times; ++i) {
        params_.tst_func(CONVERT_TO_BYTEPTR(dst_tst_ + dst_offset_),
                         dst_stride_, p_src0 + src0_offset_, src0_stride_,
                         p_src1 + src1_offset_, src1_stride_, mask_,
                         kMaxMaskWidth, w_, h_, subx_, suby_, &conv_params,
                         bit_depth_);
      }
      aom_usec_timer_mark(&timer);
      const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
      if (run_times > 1) {
        printf("%3dx%-3d subx %d suby %d :%7.2f/%7.2fns", w_, h_, subx_, suby_,
               time1, time2);
        printf("(%3.2f)\n", time1 / time2);
      }
    }
  }

  int bit_depth_;
  int src_max_bits_mask_;
};

TEST_P(BlendA64MaskTestHBD_d16, RandomValues) {
  if (params_.tst_func == nullptr) return;
  for (bit_depth_ = 8; bit_depth_ <= 12 && !HasFatalFailure();
       bit_depth_ += 2) {
    src_max_bits_mask_ =
        (bit_depth_ == 8) ? kSrcMaxBitsMask : kSrcMaxBitsMaskHBD;

    for (int bsize = 0; bsize < BLOCK_SIZES_ALL && !HasFatalFailure();
         ++bsize) {
      for (int i = 0; i < kBufSize; ++i) {
        dst_ref_[i] = rng_.Rand8();
        dst_tst_[i] = rng_.Rand8();

        src0_[i] = rng_.Rand16() & src_max_bits_mask_;
        src1_[i] = rng_.Rand16() & src_max_bits_mask_;
      }

      for (int i = 0; i < kMaxMaskSize; ++i)
        mask_[i] = rng_(AOM_BLEND_A64_MAX_ALPHA + 1);

      RunTest(bsize, 1);
    }
  }
}

TEST_P(BlendA64MaskTestHBD_d16, ExtremeValues) {
  for (bit_depth_ = 8; bit_depth_ <= 12; bit_depth_ += 2) {
    src_max_bits_mask_ =
        (bit_depth_ == 8) ? kSrcMaxBitsMask : kSrcMaxBitsMaskHBD;

    for (int i = 0; i < kBufSize; ++i) {
      dst_ref_[i] = 0;
      dst_tst_[i] = (1 << bit_depth_) - 1;

      src0_[i] = src_max_bits_mask_;
      src1_[i] = src_max_bits_mask_;
    }

    for (int i = 0; i < kMaxMaskSize; ++i) mask_[i] = AOM_BLEND_A64_MAX_ALPHA;
    for (int bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
      RunTest(bsize, 1);
    }
  }
}

TEST_P(BlendA64MaskTestHBD_d16, DISABLED_Speed) {
  const int kRunTimes = 10000000;
  for (int bsize = 0; bsize < BLOCK_SIZES_ALL; ++bsize) {
    for (bit_depth_ = 8; bit_depth_ <= 12; bit_depth_ += 2) {
      for (int i = 0; i < kBufSize; ++i) {
        dst_ref_[i] = rng_.Rand12() % (1 << bit_depth_);
        dst_tst_[i] = rng_.Rand12() % (1 << bit_depth_);

        src0_[i] = rng_.Rand16();
        src1_[i] = rng_.Rand16();
      }

      for (int i = 0; i < kMaxMaskSize; ++i)
        mask_[i] = rng_(AOM_BLEND_A64_MAX_ALPHA + 1);

      RunTest(bsize, kRunTimes);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    C, BlendA64MaskTestHBD_d16,
    ::testing::Values(TestFuncsHBD_d16(aom_highbd_blend_a64_d16_mask_c,
                                       aom_highbd_blend_a64_d16_mask_c)));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, BlendA64MaskTestHBD_d16,
    ::testing::Values(TestFuncsHBD_d16(aom_highbd_blend_a64_d16_mask_c,
                                       aom_highbd_blend_a64_d16_mask_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, BlendA64MaskTestHBD_d16,
    ::testing::Values(TestFuncsHBD_d16(aom_highbd_blend_a64_d16_mask_c,
                                       aom_highbd_blend_a64_d16_mask_avx2)));
#endif  // HAVE_AVX2

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, BlendA64MaskTestHBD_d16,
    ::testing::Values(TestFuncsHBD_d16(aom_highbd_blend_a64_d16_mask_c,
                                       aom_highbd_blend_a64_d16_mask_neon)));
#endif  // HAVE_NEON

// TODO(slavarnway): Enable the following in the avx2 commit. (56501)
#if 0
#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, BlendA64MaskTestHBD,
    ::testing::Values(TestFuncsHBD(aom_highbd_blend_a64_mask_c,
                                   aom_highbd_blend_a64_mask_avx2)));
#endif  // HAVE_AVX2
#endif
#endif  // CONFIG_AV1_HIGHBITDEPTH
}  // namespace
