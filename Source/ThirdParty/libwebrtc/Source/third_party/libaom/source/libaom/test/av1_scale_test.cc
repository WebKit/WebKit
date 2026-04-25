/*
 *  Copyright (c) 2017 The WebM project authors. All Rights Reserved.
 *  Copyright (c) 2024, Alliance for Open Media. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <string.h>

#include <tuple>

#include "gtest/gtest.h"

#include "common/av1_config.h"
#include "config/av1_rtcd.h"

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

using ResizeFrameFunc = void (*)(const YV12_BUFFER_CONFIG *src,
                                 YV12_BUFFER_CONFIG *dst,
                                 const InterpFilter filter, const int phase,
                                 const int num_planes);

class ResizeAndExtendTest : public ::testing::TestWithParam<ResizeFrameFunc> {
 public:
  ResizeAndExtendTest() { resize_fn_ = GetParam(); }
  ~ResizeAndExtendTest() override = default;

 protected:
  const int kBufFiller = 123;
  const int kBufMax = kBufFiller - 1;

  void FillPlane(uint8_t *const buf, const int width, const int height,
                 const int stride) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        buf[x + (y * stride)] = (x + (width * y)) % kBufMax;
      }
    }
  }

  void ResetResizeImage(YV12_BUFFER_CONFIG *const img, const int width,
                        const int height, const int border) {
    memset(img, 0, sizeof(*img));
    ASSERT_EQ(0, aom_alloc_frame_buffer(img, width, height, 1, 1, 0, border, 16,
                                        false, 0));
    memset(img->buffer_alloc, kBufFiller, img->frame_size);
  }

  void ResetResizeImages(const int src_width, const int src_height,
                         const int dst_width, const int dst_height,
                         const int dst_border) {
    ResetResizeImage(&img_, src_width, src_height, AOM_BORDER_IN_PIXELS);
    ResetResizeImage(&ref_img_, dst_width, dst_height, dst_border);
    ResetResizeImage(&dst_img_, dst_width, dst_height, dst_border);
    FillPlane(img_.y_buffer, img_.y_crop_width, img_.y_crop_height,
              img_.y_stride);
    FillPlane(img_.u_buffer, img_.uv_crop_width, img_.uv_crop_height,
              img_.uv_stride);
    FillPlane(img_.v_buffer, img_.uv_crop_width, img_.uv_crop_height,
              img_.uv_stride);
  }

  void DeallocResizeImages() {
    aom_free_frame_buffer(&img_);
    aom_free_frame_buffer(&ref_img_);
    aom_free_frame_buffer(&dst_img_);
  }

  void RunTest(InterpFilter filter_type) {
    static const int kNumSizesToTest = 22;
    static const int kNumScaleFactorsToTest = 4;
    static const int kNumDstBordersToTest = 2;
    static const int kSizesToTest[] = { 1,  2,  3,  4,  6,   8,  10, 12,
                                        14, 16, 18, 20, 22,  24, 26, 28,
                                        30, 32, 34, 68, 128, 134 };
    static const int kScaleFactors[] = { 1, 2, 3, 4 };
    static const int kDstBorders[] = { 0, AOM_BORDER_IN_PIXELS };
    for (int border = 0; border < kNumDstBordersToTest; ++border) {
      const int dst_border = kDstBorders[border];
      for (int phase_scaler = 0; phase_scaler < 16; ++phase_scaler) {
        for (int h = 0; h < kNumSizesToTest; ++h) {
          const int src_height = kSizesToTest[h];
          for (int w = 0; w < kNumSizesToTest; ++w) {
            const int src_width = kSizesToTest[w];
            for (int sf_up_idx = 0; sf_up_idx < kNumScaleFactorsToTest;
                 ++sf_up_idx) {
              const int sf_up = kScaleFactors[sf_up_idx];
              for (int sf_down_idx = 0; sf_down_idx < kNumScaleFactorsToTest;
                   ++sf_down_idx) {
                const int sf_down = kScaleFactors[sf_down_idx];
                const int dst_width = src_width * sf_up / sf_down;
                const int dst_height = src_height * sf_up / sf_down;
                // TODO: bug aomedia:363916152 - Enable unit tests for 4 to 3
                // scaling when Neon and SSSE3 implementation of
                // av1_resize_and_extend_frame do not differ from scalar version
                if (sf_down == 4 && sf_up == 3) {
                  continue;
                }

                if (sf_up == sf_down && sf_up != 1) {
                  continue;
                }
                // I420 frame width and height must be even.
                if (!dst_width || !dst_height || dst_width & 1 ||
                    dst_height & 1) {
                  continue;
                }
                // aom_convolve8_c() has restriction on the step which cannot
                // exceed 64 (ratio 1 to 4).
                if (src_width > 4 * dst_width || src_height > 4 * dst_height) {
                  continue;
                }
                ASSERT_NO_FATAL_FAILURE(ResetResizeImages(
                    src_width, src_height, dst_width, dst_height, dst_border));

                av1_resize_and_extend_frame_c(&img_, &ref_img_, filter_type,
                                              phase_scaler, 1);
                resize_fn_(&img_, &dst_img_, filter_type, phase_scaler, 1);

                if (memcmp(dst_img_.buffer_alloc, ref_img_.buffer_alloc,
                           ref_img_.frame_size)) {
                  printf(
                      "filter_type = %d, phase_scaler = %d, src_width = %4d, "
                      "src_height = %4d, dst_width = %4d, dst_height = %4d, "
                      "scale factor = %d:%d\n",
                      filter_type, phase_scaler, src_width, src_height,
                      dst_width, dst_height, sf_down, sf_up);
                  PrintDiff();
                }

                EXPECT_EQ(ref_img_.frame_size, dst_img_.frame_size);
                EXPECT_EQ(0,
                          memcmp(ref_img_.buffer_alloc, dst_img_.buffer_alloc,
                                 ref_img_.frame_size));

                DeallocResizeImages();
              }
            }
          }
        }
      }
    }
  }

  void PrintDiffComponent(const uint8_t *const ref, const uint8_t *const opt,
                          const int stride, const int width, const int height,
                          const int plane_idx) const {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        if (ref[y * stride + x] != opt[y * stride + x]) {
          printf("Plane %d pixel[%d][%d] diff:%6d (ref),%6d (opt)\n", plane_idx,
                 y, x, ref[y * stride + x], opt[y * stride + x]);
          break;
        }
      }
    }
  }

  void PrintDiff() const {
    assert(ref_img_.y_stride == dst_img_.y_stride);
    assert(ref_img_.y_width == dst_img_.y_width);
    assert(ref_img_.y_height == dst_img_.y_height);
    assert(ref_img_.uv_stride == dst_img_.uv_stride);
    assert(ref_img_.uv_width == dst_img_.uv_width);
    assert(ref_img_.uv_height == dst_img_.uv_height);

    if (memcmp(dst_img_.buffer_alloc, ref_img_.buffer_alloc,
               ref_img_.frame_size)) {
      PrintDiffComponent(ref_img_.y_buffer, dst_img_.y_buffer,
                         ref_img_.y_stride, ref_img_.y_width, ref_img_.y_height,
                         0);
      PrintDiffComponent(ref_img_.u_buffer, dst_img_.u_buffer,
                         ref_img_.uv_stride, ref_img_.uv_width,
                         ref_img_.uv_height, 1);
      PrintDiffComponent(ref_img_.v_buffer, dst_img_.v_buffer,
                         ref_img_.uv_stride, ref_img_.uv_width,
                         ref_img_.uv_height, 2);
    }
  }

  void SpeedTest() {
    static const int kCountSpeedTestBlock = 100;
    static const int kNumScaleFactorsToTest = 4;
    static const int kNumInterpFiltersToTest = 3;
    static const int kScaleFactors[] = { 1, 2, 3, 4 };
    static const int kInterpFilters[] = { 0, 1, 3 };
    const int src_width = 1280;
    const int src_height = 720;
    for (int filter = 0; filter < kNumInterpFiltersToTest; ++filter) {
      const InterpFilter filter_type =
          static_cast<InterpFilter>(kInterpFilters[filter]);
      for (int phase_scaler = 0; phase_scaler < 2; ++phase_scaler) {
        for (int sf_up_idx = 0; sf_up_idx < kNumScaleFactorsToTest;
             ++sf_up_idx) {
          const int sf_up = kScaleFactors[sf_up_idx];
          for (int sf_down_idx = 0; sf_down_idx < kNumScaleFactorsToTest;
               ++sf_down_idx) {
            const int sf_down = kScaleFactors[sf_down_idx];
            const int dst_width = src_width * sf_up / sf_down;
            const int dst_height = src_height * sf_up / sf_down;
            // TODO: bug aomedia:363916152 - Enable unit tests for 4 to 3
            // scaling when Neon and SSSE3 implementation of
            // av1_resize_and_extend_frame do not differ from scalar version
            if (sf_down == 4 && sf_up == 3) {
              continue;
            }

            if (sf_up == sf_down && sf_up != 1) {
              continue;
            }
            // I420 frame width and height must be even.
            if (dst_width & 1 || dst_height & 1) {
              continue;
            }
            ASSERT_NO_FATAL_FAILURE(ResetResizeImages(src_width, src_height,
                                                      dst_width, dst_height,
                                                      AOM_BORDER_IN_PIXELS));

            aom_usec_timer ref_timer;
            aom_usec_timer_start(&ref_timer);
            for (int i = 0; i < kCountSpeedTestBlock; ++i)
              av1_resize_and_extend_frame_c(&img_, &ref_img_, filter_type,
                                            phase_scaler, 1);
            aom_usec_timer_mark(&ref_timer);
            const int64_t ref_time = aom_usec_timer_elapsed(&ref_timer);

            aom_usec_timer tst_timer;
            aom_usec_timer_start(&tst_timer);
            for (int i = 0; i < kCountSpeedTestBlock; ++i)
              resize_fn_(&img_, &dst_img_, filter_type, phase_scaler, 1);
            aom_usec_timer_mark(&tst_timer);
            const int64_t tst_time = aom_usec_timer_elapsed(&tst_timer);
            DeallocResizeImages();

            std::cout << "[          ] C time = " << ref_time / 1000
                      << " ms, SIMD time = " << tst_time / 1000 << " ms\n";
          }
        }
      }
    }
  }

  YV12_BUFFER_CONFIG img_;
  YV12_BUFFER_CONFIG ref_img_;
  YV12_BUFFER_CONFIG dst_img_;
  ResizeFrameFunc resize_fn_;
};

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ResizeAndExtendTest);

TEST_P(ResizeAndExtendTest, ResizeFrame_EightTap) { RunTest(EIGHTTAP_REGULAR); }
TEST_P(ResizeAndExtendTest, ResizeFrame_EightTapSmooth) {
  RunTest(EIGHTTAP_SMOOTH);
}
TEST_P(ResizeAndExtendTest, ResizeFrame_Bilinear) { RunTest(BILINEAR); }
TEST_P(ResizeAndExtendTest, DISABLED_Speed) { SpeedTest(); }

// TODO: bug aomedia:363916152 - Enable SSSE3 unit tests when implementation of
// av1_resize_and_extend_frame does not differ from scalar version
#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(DISABLED_SSSE3, ResizeAndExtendTest,
                         ::testing::Values(av1_resize_and_extend_frame_ssse3));
#endif  // HAVE_SSSE3

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, ResizeAndExtendTest,
                         ::testing::Values(av1_resize_and_extend_frame_neon));
#endif  // HAVE_NEON

#if HAVE_NEON_DOTPROD
INSTANTIATE_TEST_SUITE_P(
    NEON_DOTPROD, ResizeAndExtendTest,
    ::testing::Values(av1_resize_and_extend_frame_neon_dotprod));

#endif  // HAVE_NEON_DOTPROD

#if HAVE_NEON_I8MM
INSTANTIATE_TEST_SUITE_P(
    NEON_I8MM, ResizeAndExtendTest,
    ::testing::Values(av1_resize_and_extend_frame_neon_i8mm));

#endif  // HAVE_NEON_I8MM

}  // namespace
