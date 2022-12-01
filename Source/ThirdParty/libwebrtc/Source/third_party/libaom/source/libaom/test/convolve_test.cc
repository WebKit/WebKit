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

#include <string.h>
#include <tuple>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "av1/common/filter.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"

namespace {

static const unsigned int kMaxDimension = MAX_SB_SIZE;

typedef void (*ConvolveFunc)(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride,
                             const int16_t *filter_x, int filter_x_stride,
                             const int16_t *filter_y, int filter_y_stride,
                             int w, int h);

struct ConvolveFunctions {
  ConvolveFunctions(ConvolveFunc h8, ConvolveFunc v8, int bd)
      : h8_(h8), v8_(v8), use_highbd_(bd) {}

  ConvolveFunc h8_;
  ConvolveFunc v8_;
  int use_highbd_;  // 0 if high bitdepth not used, else the actual bit depth.
};

typedef std::tuple<int, int, const ConvolveFunctions *> ConvolveParam;

#define ALL_SIZES_64(convolve_fn)                                         \
  make_tuple(4, 4, &convolve_fn), make_tuple(8, 4, &convolve_fn),         \
      make_tuple(4, 8, &convolve_fn), make_tuple(8, 8, &convolve_fn),     \
      make_tuple(16, 8, &convolve_fn), make_tuple(8, 16, &convolve_fn),   \
      make_tuple(16, 16, &convolve_fn), make_tuple(32, 16, &convolve_fn), \
      make_tuple(16, 32, &convolve_fn), make_tuple(32, 32, &convolve_fn), \
      make_tuple(64, 32, &convolve_fn), make_tuple(32, 64, &convolve_fn), \
      make_tuple(64, 64, &convolve_fn)

#define ALL_SIZES(convolve_fn)                                          \
  make_tuple(128, 64, &convolve_fn), make_tuple(64, 128, &convolve_fn), \
      make_tuple(128, 128, &convolve_fn), ALL_SIZES_64(convolve_fn)

// Reference 8-tap subpixel filter, slightly modified to fit into this test.
#define AV1_FILTER_WEIGHT 128
#define AV1_FILTER_SHIFT 7
uint8_t clip_pixel(int x) { return x < 0 ? 0 : x > 255 ? 255 : x; }

void filter_block2d_8_c(const uint8_t *src_ptr, unsigned int src_stride,
                        const int16_t *HFilter, const int16_t *VFilter,
                        uint8_t *dst_ptr, unsigned int dst_stride,
                        unsigned int output_width, unsigned int output_height) {
  // Between passes, we use an intermediate buffer whose height is extended to
  // have enough horizontally filtered values as input for the vertical pass.
  // This buffer is allocated to be big enough for the largest block type we
  // support.
  const int kInterp_Extend = 4;
  const unsigned int intermediate_height =
      (kInterp_Extend - 1) + output_height + kInterp_Extend;
  unsigned int i, j;

  assert(intermediate_height > 7);

  // Size of intermediate_buffer is max_intermediate_height * filter_max_width,
  // where max_intermediate_height = (kInterp_Extend - 1) + filter_max_height
  //                                 + kInterp_Extend
  //                               = 3 + 16 + 4
  //                               = 23
  // and filter_max_width          = 16
  //
  uint8_t intermediate_buffer[(kMaxDimension + 8) * kMaxDimension];
  const int intermediate_next_stride =
      1 - static_cast<int>(intermediate_height * output_width);

  // Horizontal pass (src -> transposed intermediate).
  uint8_t *output_ptr = intermediate_buffer;
  const int src_next_row_stride = src_stride - output_width;
  src_ptr -= (kInterp_Extend - 1) * src_stride + (kInterp_Extend - 1);
  for (i = 0; i < intermediate_height; ++i) {
    for (j = 0; j < output_width; ++j) {
      // Apply filter...
      const int temp = (src_ptr[0] * HFilter[0]) + (src_ptr[1] * HFilter[1]) +
                       (src_ptr[2] * HFilter[2]) + (src_ptr[3] * HFilter[3]) +
                       (src_ptr[4] * HFilter[4]) + (src_ptr[5] * HFilter[5]) +
                       (src_ptr[6] * HFilter[6]) + (src_ptr[7] * HFilter[7]) +
                       (AV1_FILTER_WEIGHT >> 1);  // Rounding

      // Normalize back to 0-255...
      *output_ptr = clip_pixel(temp >> AV1_FILTER_SHIFT);
      ++src_ptr;
      output_ptr += intermediate_height;
    }
    src_ptr += src_next_row_stride;
    output_ptr += intermediate_next_stride;
  }

  // Vertical pass (transposed intermediate -> dst).
  src_ptr = intermediate_buffer;
  const int dst_next_row_stride = dst_stride - output_width;
  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; ++j) {
      // Apply filter...
      const int temp = (src_ptr[0] * VFilter[0]) + (src_ptr[1] * VFilter[1]) +
                       (src_ptr[2] * VFilter[2]) + (src_ptr[3] * VFilter[3]) +
                       (src_ptr[4] * VFilter[4]) + (src_ptr[5] * VFilter[5]) +
                       (src_ptr[6] * VFilter[6]) + (src_ptr[7] * VFilter[7]) +
                       (AV1_FILTER_WEIGHT >> 1);  // Rounding

      // Normalize back to 0-255...
      *dst_ptr++ = clip_pixel(temp >> AV1_FILTER_SHIFT);
      src_ptr += intermediate_height;
    }
    src_ptr += intermediate_next_stride;
    dst_ptr += dst_next_row_stride;
  }
}

void block2d_average_c(uint8_t *src, unsigned int src_stride,
                       uint8_t *output_ptr, unsigned int output_stride,
                       unsigned int output_width, unsigned int output_height) {
  unsigned int i, j;
  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; ++j) {
      output_ptr[j] = (output_ptr[j] + src[i * src_stride + j] + 1) >> 1;
    }
    output_ptr += output_stride;
  }
}

void filter_average_block2d_8_c(const uint8_t *src_ptr,
                                const unsigned int src_stride,
                                const int16_t *HFilter, const int16_t *VFilter,
                                uint8_t *dst_ptr, unsigned int dst_stride,
                                unsigned int output_width,
                                unsigned int output_height) {
  uint8_t tmp[kMaxDimension * kMaxDimension];

  assert(output_width <= kMaxDimension);
  assert(output_height <= kMaxDimension);
  filter_block2d_8_c(src_ptr, src_stride, HFilter, VFilter, tmp, kMaxDimension,
                     output_width, output_height);
  block2d_average_c(tmp, kMaxDimension, dst_ptr, dst_stride, output_width,
                    output_height);
}

void highbd_filter_block2d_8_c(const uint16_t *src_ptr,
                               const unsigned int src_stride,
                               const int16_t *HFilter, const int16_t *VFilter,
                               uint16_t *dst_ptr, unsigned int dst_stride,
                               unsigned int output_width,
                               unsigned int output_height, int bd) {
  // Between passes, we use an intermediate buffer whose height is extended to
  // have enough horizontally filtered values as input for the vertical pass.
  // This buffer is allocated to be big enough for the largest block type we
  // support.
  const int kInterp_Extend = 4;
  const unsigned int intermediate_height =
      (kInterp_Extend - 1) + output_height + kInterp_Extend;

  /* Size of intermediate_buffer is max_intermediate_height * filter_max_width,
   * where max_intermediate_height = (kInterp_Extend - 1) + filter_max_height
   *                                 + kInterp_Extend
   *                               = 3 + 16 + 4
   *                               = 23
   * and filter_max_width = 16
   */
  uint16_t intermediate_buffer[(kMaxDimension + 8) * kMaxDimension] = { 0 };
  const int intermediate_next_stride =
      1 - static_cast<int>(intermediate_height * output_width);

  // Horizontal pass (src -> transposed intermediate).
  {
    uint16_t *output_ptr = intermediate_buffer;
    const int src_next_row_stride = src_stride - output_width;
    unsigned int i, j;
    src_ptr -= (kInterp_Extend - 1) * src_stride + (kInterp_Extend - 1);
    for (i = 0; i < intermediate_height; ++i) {
      for (j = 0; j < output_width; ++j) {
        // Apply filter...
        const int temp = (src_ptr[0] * HFilter[0]) + (src_ptr[1] * HFilter[1]) +
                         (src_ptr[2] * HFilter[2]) + (src_ptr[3] * HFilter[3]) +
                         (src_ptr[4] * HFilter[4]) + (src_ptr[5] * HFilter[5]) +
                         (src_ptr[6] * HFilter[6]) + (src_ptr[7] * HFilter[7]) +
                         (AV1_FILTER_WEIGHT >> 1);  // Rounding

        // Normalize back to 0-255...
        *output_ptr = clip_pixel_highbd(temp >> AV1_FILTER_SHIFT, bd);
        ++src_ptr;
        output_ptr += intermediate_height;
      }
      src_ptr += src_next_row_stride;
      output_ptr += intermediate_next_stride;
    }
  }

  // Vertical pass (transposed intermediate -> dst).
  {
    const uint16_t *interm_ptr = intermediate_buffer;
    const int dst_next_row_stride = dst_stride - output_width;
    unsigned int i, j;
    for (i = 0; i < output_height; ++i) {
      for (j = 0; j < output_width; ++j) {
        // Apply filter...
        const int temp =
            (interm_ptr[0] * VFilter[0]) + (interm_ptr[1] * VFilter[1]) +
            (interm_ptr[2] * VFilter[2]) + (interm_ptr[3] * VFilter[3]) +
            (interm_ptr[4] * VFilter[4]) + (interm_ptr[5] * VFilter[5]) +
            (interm_ptr[6] * VFilter[6]) + (interm_ptr[7] * VFilter[7]) +
            (AV1_FILTER_WEIGHT >> 1);  // Rounding

        // Normalize back to 0-255...
        *dst_ptr++ = clip_pixel_highbd(temp >> AV1_FILTER_SHIFT, bd);
        interm_ptr += intermediate_height;
      }
      interm_ptr += intermediate_next_stride;
      dst_ptr += dst_next_row_stride;
    }
  }
}

void highbd_block2d_average_c(uint16_t *src, unsigned int src_stride,
                              uint16_t *output_ptr, unsigned int output_stride,
                              unsigned int output_width,
                              unsigned int output_height) {
  unsigned int i, j;
  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; ++j) {
      output_ptr[j] = (output_ptr[j] + src[i * src_stride + j] + 1) >> 1;
    }
    output_ptr += output_stride;
  }
}

void highbd_filter_average_block2d_8_c(
    const uint16_t *src_ptr, unsigned int src_stride, const int16_t *HFilter,
    const int16_t *VFilter, uint16_t *dst_ptr, unsigned int dst_stride,
    unsigned int output_width, unsigned int output_height, int bd) {
  uint16_t tmp[kMaxDimension * kMaxDimension];

  assert(output_width <= kMaxDimension);
  assert(output_height <= kMaxDimension);
  highbd_filter_block2d_8_c(src_ptr, src_stride, HFilter, VFilter, tmp,
                            kMaxDimension, output_width, output_height, bd);
  highbd_block2d_average_c(tmp, kMaxDimension, dst_ptr, dst_stride,
                           output_width, output_height);
}

class ConvolveTest : public ::testing::TestWithParam<ConvolveParam> {
 public:
  static void SetUpTestSuite() {
    // Force input_ to be unaligned, output to be 16 byte aligned.
    input_ = reinterpret_cast<uint8_t *>(
                 aom_memalign(kDataAlignment, kInputBufferSize + 1)) +
             1;
    ASSERT_NE(input_, nullptr);
    ref8_ = reinterpret_cast<uint8_t *>(
        aom_memalign(kDataAlignment, kOutputStride * kMaxDimension));
    ASSERT_NE(ref8_, nullptr);
    output_ = reinterpret_cast<uint8_t *>(
        aom_memalign(kDataAlignment, kOutputBufferSize));
    ASSERT_NE(output_, nullptr);
    output_ref_ = reinterpret_cast<uint8_t *>(
        aom_memalign(kDataAlignment, kOutputBufferSize));
    ASSERT_NE(output_ref_, nullptr);
    input16_ = reinterpret_cast<uint16_t *>(aom_memalign(
                   kDataAlignment, (kInputBufferSize + 1) * sizeof(uint16_t))) +
               1;
    ASSERT_NE(input16_, nullptr);
    ref16_ = reinterpret_cast<uint16_t *>(aom_memalign(
        kDataAlignment, kOutputStride * kMaxDimension * sizeof(uint16_t)));
    ASSERT_NE(ref16_, nullptr);
    output16_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, (kOutputBufferSize) * sizeof(uint16_t)));
    ASSERT_NE(output16_, nullptr);
    output16_ref_ = reinterpret_cast<uint16_t *>(
        aom_memalign(kDataAlignment, (kOutputBufferSize) * sizeof(uint16_t)));
    ASSERT_NE(output16_ref_, nullptr);
  }

  virtual void TearDown() {}

  static void TearDownTestSuite() {
    aom_free(input_ - 1);
    input_ = nullptr;
    aom_free(ref8_);
    ref8_ = nullptr;
    aom_free(output_);
    output_ = nullptr;
    aom_free(output_ref_);
    output_ref_ = nullptr;
    aom_free(input16_ - 1);
    input16_ = nullptr;
    aom_free(ref16_);
    ref16_ = nullptr;
    aom_free(output16_);
    output16_ = nullptr;
    aom_free(output16_ref_);
    output16_ref_ = nullptr;
  }

 protected:
  static const int kDataAlignment = 16;
  static const int kOuterBlockSize = 4 * kMaxDimension;
  static const int kInputStride = kOuterBlockSize;
  static const int kOutputStride = kOuterBlockSize;
  static const int kInputBufferSize = kOuterBlockSize * kOuterBlockSize;
  static const int kOutputBufferSize = kOuterBlockSize * kOuterBlockSize;

  int Width() const { return GET_PARAM(0); }
  int Height() const { return GET_PARAM(1); }
  int BorderLeft() const {
    const int center = (kOuterBlockSize - Width()) / 2;
    return (center + (kDataAlignment - 1)) & ~(kDataAlignment - 1);
  }
  int BorderTop() const { return (kOuterBlockSize - Height()) / 2; }

  bool IsIndexInBorder(int i) {
    return (i < BorderTop() * kOuterBlockSize ||
            i >= (BorderTop() + Height()) * kOuterBlockSize ||
            i % kOuterBlockSize < BorderLeft() ||
            i % kOuterBlockSize >= (BorderLeft() + Width()));
  }

  virtual void SetUp() {
    UUT_ = GET_PARAM(2);
    if (UUT_->use_highbd_ != 0)
      mask_ = (1 << UUT_->use_highbd_) - 1;
    else
      mask_ = 255;
    /* Set up guard blocks for an inner block centered in the outer block */
    for (int i = 0; i < kOutputBufferSize; ++i) {
      if (IsIndexInBorder(i)) {
        output_[i] = 255;
        output16_[i] = mask_;
      } else {
        output_[i] = 0;
        output16_[i] = 0;
      }
    }

    ::libaom_test::ACMRandom prng;
    for (int i = 0; i < kInputBufferSize; ++i) {
      if (i & 1) {
        input_[i] = 255;
        input16_[i] = mask_;
      } else {
        input_[i] = prng.Rand8Extremes();
        input16_[i] = prng.Rand16() & mask_;
      }
    }
  }

  void SetConstantInput(int value) {
    memset(input_, value, kInputBufferSize);
    aom_memset16(input16_, value, kInputBufferSize);
  }

  void CopyOutputToRef() {
    memcpy(output_ref_, output_, kOutputBufferSize);
    // Copy 16-bit pixels values. The effective number of bytes is double.
    memcpy(output16_ref_, output16_, sizeof(output16_[0]) * kOutputBufferSize);
  }

  void CheckGuardBlocks() {
    for (int i = 0; i < kOutputBufferSize; ++i) {
      if (IsIndexInBorder(i)) {
        EXPECT_EQ(255, output_[i]);
      }
    }
  }

  uint8_t *input() const {
    const int offset = BorderTop() * kOuterBlockSize + BorderLeft();
    if (UUT_->use_highbd_ == 0) {
      return input_ + offset;
    } else {
      return CONVERT_TO_BYTEPTR(input16_) + offset;
    }
  }

  uint8_t *output() const {
    const int offset = BorderTop() * kOuterBlockSize + BorderLeft();
    if (UUT_->use_highbd_ == 0) {
      return output_ + offset;
    } else {
      return CONVERT_TO_BYTEPTR(output16_) + offset;
    }
  }

  uint8_t *output_ref() const {
    const int offset = BorderTop() * kOuterBlockSize + BorderLeft();
    if (UUT_->use_highbd_ == 0) {
      return output_ref_ + offset;
    } else {
      return CONVERT_TO_BYTEPTR(output16_ref_) + offset;
    }
  }

  uint16_t lookup(uint8_t *list, int index) const {
    if (UUT_->use_highbd_ == 0) {
      return list[index];
    } else {
      return CONVERT_TO_SHORTPTR(list)[index];
    }
  }

  void assign_val(uint8_t *list, int index, uint16_t val) const {
    if (UUT_->use_highbd_ == 0) {
      list[index] = (uint8_t)val;
    } else {
      CONVERT_TO_SHORTPTR(list)[index] = val;
    }
  }

  void wrapper_filter_average_block2d_8_c(
      const uint8_t *src_ptr, unsigned int src_stride, const int16_t *HFilter,
      const int16_t *VFilter, uint8_t *dst_ptr, unsigned int dst_stride,
      unsigned int output_width, unsigned int output_height) {
    if (UUT_->use_highbd_ == 0) {
      filter_average_block2d_8_c(src_ptr, src_stride, HFilter, VFilter, dst_ptr,
                                 dst_stride, output_width, output_height);
    } else {
      highbd_filter_average_block2d_8_c(
          CONVERT_TO_SHORTPTR(src_ptr), src_stride, HFilter, VFilter,
          CONVERT_TO_SHORTPTR(dst_ptr), dst_stride, output_width, output_height,
          UUT_->use_highbd_);
    }
  }

  void wrapper_filter_block2d_8_c(
      const uint8_t *src_ptr, unsigned int src_stride, const int16_t *HFilter,
      const int16_t *VFilter, uint8_t *dst_ptr, unsigned int dst_stride,
      unsigned int output_width, unsigned int output_height) {
    if (UUT_->use_highbd_ == 0) {
      filter_block2d_8_c(src_ptr, src_stride, HFilter, VFilter, dst_ptr,
                         dst_stride, output_width, output_height);
    } else {
      highbd_filter_block2d_8_c(CONVERT_TO_SHORTPTR(src_ptr), src_stride,
                                HFilter, VFilter, CONVERT_TO_SHORTPTR(dst_ptr),
                                dst_stride, output_width, output_height,
                                UUT_->use_highbd_);
    }
  }

  const ConvolveFunctions *UUT_;
  static uint8_t *input_;
  static uint8_t *ref8_;
  static uint8_t *output_;
  static uint8_t *output_ref_;
  static uint16_t *input16_;
  static uint16_t *ref16_;
  static uint16_t *output16_;
  static uint16_t *output16_ref_;
  int mask_;
};

uint8_t *ConvolveTest::input_ = nullptr;
uint8_t *ConvolveTest::ref8_ = nullptr;
uint8_t *ConvolveTest::output_ = nullptr;
uint8_t *ConvolveTest::output_ref_ = nullptr;
uint16_t *ConvolveTest::input16_ = nullptr;
uint16_t *ConvolveTest::ref16_ = nullptr;
uint16_t *ConvolveTest::output16_ = nullptr;
uint16_t *ConvolveTest::output16_ref_ = nullptr;

TEST_P(ConvolveTest, GuardBlocks) { CheckGuardBlocks(); }

const int kNumFilterBanks = SWITCHABLE_FILTERS;
const int kNumFilters = 16;

TEST(ConvolveTest, FiltersWontSaturateWhenAddedPairwise) {
  int subpel_search;
  for (subpel_search = USE_4_TAPS; subpel_search <= USE_8_TAPS;
       ++subpel_search) {
    for (int filter_bank = 0; filter_bank < kNumFilterBanks; ++filter_bank) {
      const InterpFilter filter = (InterpFilter)filter_bank;
      const InterpKernel *filters =
          (const InterpKernel *)av1_get_interp_filter_kernel(filter,
                                                             subpel_search);
      for (int i = 0; i < kNumFilters; i++) {
        const int p0 = filters[i][0] + filters[i][1];
        const int p1 = filters[i][2] + filters[i][3];
        const int p2 = filters[i][4] + filters[i][5];
        const int p3 = filters[i][6] + filters[i][7];
        EXPECT_LE(p0, 128);
        EXPECT_LE(p1, 128);
        EXPECT_LE(p2, 128);
        EXPECT_LE(p3, 128);
        EXPECT_LE(p0 + p3, 128);
        EXPECT_LE(p0 + p3 + p1, 128);
        EXPECT_LE(p0 + p3 + p1 + p2, 128);
        EXPECT_EQ(p0 + p1 + p2 + p3, 128);
      }
    }
  }
}

const int16_t kInvalidFilter[8] = { 0 };

TEST_P(ConvolveTest, MatchesReferenceSubpixelFilter) {
  uint8_t *const in = input();
  uint8_t *const out = output();
  uint8_t *ref;
  if (UUT_->use_highbd_ == 0) {
    ref = ref8_;
  } else {
    ref = CONVERT_TO_BYTEPTR(ref16_);
  }
  int subpel_search;
  for (subpel_search = USE_4_TAPS; subpel_search <= USE_8_TAPS;
       ++subpel_search) {
    for (int filter_bank = 0; filter_bank < kNumFilterBanks; ++filter_bank) {
      const InterpFilter filter = (InterpFilter)filter_bank;
      const InterpKernel *filters =
          (const InterpKernel *)av1_get_interp_filter_kernel(filter,
                                                             subpel_search);
      for (int filter_x = 0; filter_x < kNumFilters; ++filter_x) {
        for (int filter_y = 0; filter_y < kNumFilters; ++filter_y) {
          wrapper_filter_block2d_8_c(in, kInputStride, filters[filter_x],
                                     filters[filter_y], ref, kOutputStride,
                                     Width(), Height());

          if (filter_x && filter_y)
            continue;
          else if (filter_y)
            API_REGISTER_STATE_CHECK(
                UUT_->v8_(in, kInputStride, out, kOutputStride, kInvalidFilter,
                          16, filters[filter_y], 16, Width(), Height()));
          else if (filter_x)
            API_REGISTER_STATE_CHECK(UUT_->h8_(
                in, kInputStride, out, kOutputStride, filters[filter_x], 16,
                kInvalidFilter, 16, Width(), Height()));
          else
            continue;

          CheckGuardBlocks();

          for (int y = 0; y < Height(); ++y)
            for (int x = 0; x < Width(); ++x)
              ASSERT_EQ(lookup(ref, y * kOutputStride + x),
                        lookup(out, y * kOutputStride + x))
                  << "mismatch at (" << x << "," << y << "), "
                  << "filters (" << filter_bank << "," << filter_x << ","
                  << filter_y << ")";
        }
      }
    }
  }
}

TEST_P(ConvolveTest, FilterExtremes) {
  uint8_t *const in = input();
  uint8_t *const out = output();
  uint8_t *ref;
  if (UUT_->use_highbd_ == 0) {
    ref = ref8_;
  } else {
    ref = CONVERT_TO_BYTEPTR(ref16_);
  }

  // Populate ref and out with some random data
  ::libaom_test::ACMRandom prng;
  for (int y = 0; y < Height(); ++y) {
    for (int x = 0; x < Width(); ++x) {
      uint16_t r;
      if (UUT_->use_highbd_ == 0 || UUT_->use_highbd_ == 8) {
        r = prng.Rand8Extremes();
      } else {
        r = prng.Rand16() & mask_;
      }
      assign_val(out, y * kOutputStride + x, r);
      assign_val(ref, y * kOutputStride + x, r);
    }
  }

  for (int axis = 0; axis < 2; axis++) {
    int seed_val = 0;
    while (seed_val < 256) {
      for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
          assign_val(in, y * kOutputStride + x - SUBPEL_TAPS / 2 + 1,
                     ((seed_val >> (axis ? y : x)) & 1) * mask_);
          if (axis) seed_val++;
        }
        if (axis)
          seed_val -= 8;
        else
          seed_val++;
      }
      if (axis) seed_val += 8;
      int subpel_search;
      for (subpel_search = USE_4_TAPS; subpel_search <= USE_8_TAPS;
           ++subpel_search) {
        for (int filter_bank = 0; filter_bank < kNumFilterBanks;
             ++filter_bank) {
          const InterpFilter filter = (InterpFilter)filter_bank;
          const InterpKernel *filters =
              (const InterpKernel *)av1_get_interp_filter_kernel(filter,
                                                                 subpel_search);
          for (int filter_x = 0; filter_x < kNumFilters; ++filter_x) {
            for (int filter_y = 0; filter_y < kNumFilters; ++filter_y) {
              wrapper_filter_block2d_8_c(in, kInputStride, filters[filter_x],
                                         filters[filter_y], ref, kOutputStride,
                                         Width(), Height());
              if (filter_x && filter_y)
                continue;
              else if (filter_y)
                API_REGISTER_STATE_CHECK(UUT_->v8_(
                    in, kInputStride, out, kOutputStride, kInvalidFilter, 16,
                    filters[filter_y], 16, Width(), Height()));
              else if (filter_x)
                API_REGISTER_STATE_CHECK(UUT_->h8_(
                    in, kInputStride, out, kOutputStride, filters[filter_x], 16,
                    kInvalidFilter, 16, Width(), Height()));
              else
                continue;

              for (int y = 0; y < Height(); ++y)
                for (int x = 0; x < Width(); ++x)
                  ASSERT_EQ(lookup(ref, y * kOutputStride + x),
                            lookup(out, y * kOutputStride + x))
                      << "mismatch at (" << x << "," << y << "), "
                      << "filters (" << filter_bank << "," << filter_x << ","
                      << filter_y << ")";
            }
          }
        }
      }
    }
  }
}

TEST_P(ConvolveTest, DISABLED_Speed) {
  uint8_t *const in = input();
  uint8_t *const out = output();
  uint8_t *ref;
  if (UUT_->use_highbd_ == 0) {
    ref = ref8_;
  } else {
    ref = CONVERT_TO_BYTEPTR(ref16_);
  }

  // Populate ref and out with some random data
  ::libaom_test::ACMRandom prng;
  for (int y = 0; y < Height(); ++y) {
    for (int x = 0; x < Width(); ++x) {
      uint16_t r;
      if (UUT_->use_highbd_ == 0 || UUT_->use_highbd_ == 8) {
        r = prng.Rand8Extremes();
      } else {
        r = prng.Rand16() & mask_;
      }
      assign_val(out, y * kOutputStride + x, r);
      assign_val(ref, y * kOutputStride + x, r);
    }
  }

  const InterpFilter filter = (InterpFilter)1;
  const InterpKernel *filters =
      (const InterpKernel *)av1_get_interp_filter_kernel(filter, USE_8_TAPS);
  wrapper_filter_average_block2d_8_c(in, kInputStride, filters[1], filters[1],
                                     out, kOutputStride, Width(), Height());

  aom_usec_timer timer;
  int tests_num = 1000;

  aom_usec_timer_start(&timer);
  while (tests_num > 0) {
    for (int filter_bank = 0; filter_bank < kNumFilterBanks; ++filter_bank) {
      const InterpFilter filter = (InterpFilter)filter_bank;
      const InterpKernel *filters =
          (const InterpKernel *)av1_get_interp_filter_kernel(filter,
                                                             USE_8_TAPS);
      for (int filter_x = 0; filter_x < kNumFilters; ++filter_x) {
        for (int filter_y = 0; filter_y < kNumFilters; ++filter_y) {
          if (filter_x && filter_y) continue;
          if (filter_y)
            API_REGISTER_STATE_CHECK(
                UUT_->v8_(in, kInputStride, out, kOutputStride, kInvalidFilter,
                          16, filters[filter_y], 16, Width(), Height()));
          else if (filter_x)
            API_REGISTER_STATE_CHECK(UUT_->h8_(
                in, kInputStride, out, kOutputStride, filters[filter_x], 16,
                kInvalidFilter, 16, Width(), Height()));
        }
      }
    }
    tests_num--;
  }
  aom_usec_timer_mark(&timer);

  const int elapsed_time =
      static_cast<int>(aom_usec_timer_elapsed(&timer) / 1000);
  printf("%dx%d (bitdepth %d) time: %5d ms\n", Width(), Height(),
         UUT_->use_highbd_, elapsed_time);
}

using std::make_tuple;

// WRAP macro is only used for high bitdepth build.
#if CONFIG_AV1_HIGHBITDEPTH
#define WRAP(func, bd)                                                       \
  static void wrap_##func##_##bd(                                            \
      const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,                \
      ptrdiff_t dst_stride, const int16_t *filter_x, int filter_x_stride,    \
      const int16_t *filter_y, int filter_y_stride, int w, int h) {          \
    aom_highbd_##func(src, src_stride, dst, dst_stride, filter_x,            \
                      filter_x_stride, filter_y, filter_y_stride, w, h, bd); \
  }
#if HAVE_SSE2 && ARCH_X86_64
WRAP(convolve8_horiz_sse2, 8)
WRAP(convolve8_vert_sse2, 8)
WRAP(convolve8_horiz_sse2, 10)
WRAP(convolve8_vert_sse2, 10)
WRAP(convolve8_horiz_sse2, 12)
WRAP(convolve8_vert_sse2, 12)
#endif  // HAVE_SSE2 && ARCH_X86_64

WRAP(convolve8_horiz_c, 8)
WRAP(convolve8_vert_c, 8)
WRAP(convolve8_horiz_c, 10)
WRAP(convolve8_vert_c, 10)
WRAP(convolve8_horiz_c, 12)
WRAP(convolve8_vert_c, 12)

#if HAVE_AVX2
WRAP(convolve8_horiz_avx2, 8)
WRAP(convolve8_vert_avx2, 8)

WRAP(convolve8_horiz_avx2, 10)
WRAP(convolve8_vert_avx2, 10)

WRAP(convolve8_horiz_avx2, 12)
WRAP(convolve8_vert_avx2, 12)
#endif  // HAVE_AVX2
#endif  // CONFIG_AV1_HIGHBITDEPTH

#undef WRAP

#if CONFIG_AV1_HIGHBITDEPTH
const ConvolveFunctions wrap_convolve8_c(wrap_convolve8_horiz_c_8,
                                         wrap_convolve8_vert_c_8, 8);
const ConvolveFunctions wrap_convolve10_c(wrap_convolve8_horiz_c_10,
                                          wrap_convolve8_vert_c_10, 10);
const ConvolveFunctions wrap_convolve12_c(wrap_convolve8_horiz_c_12,
                                          wrap_convolve8_vert_c_12, 12);
const ConvolveParam kArrayConvolve_c[] = { ALL_SIZES(wrap_convolve8_c),
                                           ALL_SIZES(wrap_convolve10_c),
                                           ALL_SIZES(wrap_convolve12_c) };
#else
const ConvolveFunctions convolve8_c(aom_convolve8_horiz_c, aom_convolve8_vert_c,
                                    0);
const ConvolveParam kArrayConvolve_c[] = { ALL_SIZES(convolve8_c) };
#endif

INSTANTIATE_TEST_SUITE_P(C, ConvolveTest,
                         ::testing::ValuesIn(kArrayConvolve_c));

#if HAVE_SSE2 && ARCH_X86_64
#if CONFIG_AV1_HIGHBITDEPTH
const ConvolveFunctions wrap_convolve8_sse2(wrap_convolve8_horiz_sse2_8,
                                            wrap_convolve8_vert_sse2_8, 8);
const ConvolveFunctions wrap_convolve10_sse2(wrap_convolve8_horiz_sse2_10,
                                             wrap_convolve8_vert_sse2_10, 10);
const ConvolveFunctions wrap_convolve12_sse2(wrap_convolve8_horiz_sse2_12,
                                             wrap_convolve8_vert_sse2_12, 12);
const ConvolveParam kArrayConvolve_sse2[] = { ALL_SIZES(wrap_convolve8_sse2),
                                              ALL_SIZES(wrap_convolve10_sse2),
                                              ALL_SIZES(wrap_convolve12_sse2) };
#else
const ConvolveFunctions convolve8_sse2(aom_convolve8_horiz_sse2,
                                       aom_convolve8_vert_sse2, 0);
const ConvolveParam kArrayConvolve_sse2[] = { ALL_SIZES(convolve8_sse2) };
#endif
INSTANTIATE_TEST_SUITE_P(SSE2, ConvolveTest,
                         ::testing::ValuesIn(kArrayConvolve_sse2));
#endif

#if HAVE_SSSE3
const ConvolveFunctions convolve8_ssse3(aom_convolve8_horiz_ssse3,
                                        aom_convolve8_vert_ssse3, 0);

const ConvolveParam kArrayConvolve8_ssse3[] = { ALL_SIZES(convolve8_ssse3) };
INSTANTIATE_TEST_SUITE_P(SSSE3, ConvolveTest,
                         ::testing::ValuesIn(kArrayConvolve8_ssse3));
#endif

#if HAVE_AVX2
#if CONFIG_AV1_HIGHBITDEPTH
const ConvolveFunctions wrap_convolve8_avx2(wrap_convolve8_horiz_avx2_8,
                                            wrap_convolve8_vert_avx2_8, 8);
const ConvolveFunctions wrap_convolve10_avx2(wrap_convolve8_horiz_avx2_10,
                                             wrap_convolve8_vert_avx2_10, 10);
const ConvolveFunctions wrap_convolve12_avx2(wrap_convolve8_horiz_avx2_12,
                                             wrap_convolve8_vert_avx2_12, 12);
const ConvolveParam kArray_Convolve8_avx2[] = {
  ALL_SIZES_64(wrap_convolve8_avx2), ALL_SIZES_64(wrap_convolve10_avx2),
  ALL_SIZES_64(wrap_convolve12_avx2)
};
#else
const ConvolveFunctions convolve8_avx2(aom_convolve8_horiz_avx2,
                                       aom_convolve8_vert_avx2, 0);
const ConvolveParam kArray_Convolve8_avx2[] = { ALL_SIZES(convolve8_avx2) };
#endif

INSTANTIATE_TEST_SUITE_P(AVX2, ConvolveTest,
                         ::testing::ValuesIn(kArray_Convolve8_avx2));
#endif  // HAVE_AVX2

}  // namespace
