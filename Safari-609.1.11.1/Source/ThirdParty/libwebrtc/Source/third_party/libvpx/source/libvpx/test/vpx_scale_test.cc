/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vpx_config.h"
#include "./vpx_scale_rtcd.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/vpx_scale_test.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/vpx_timer.h"
#include "vpx_scale/yv12config.h"

namespace libvpx_test {

typedef void (*ExtendFrameBorderFunc)(YV12_BUFFER_CONFIG *ybf);
typedef void (*CopyFrameFunc)(const YV12_BUFFER_CONFIG *src_ybf,
                              YV12_BUFFER_CONFIG *dst_ybf);

class ExtendBorderTest
    : public VpxScaleBase,
      public ::testing::TestWithParam<ExtendFrameBorderFunc> {
 public:
  virtual ~ExtendBorderTest() {}

 protected:
  virtual void SetUp() { extend_fn_ = GetParam(); }

  void ExtendBorder() { ASM_REGISTER_STATE_CHECK(extend_fn_(&img_)); }

  void RunTest() {
#if ARCH_ARM
    // Some arm devices OOM when trying to allocate the largest buffers.
    static const int kNumSizesToTest = 6;
#else
    static const int kNumSizesToTest = 7;
#endif
    static const int kSizesToTest[] = { 1, 15, 33, 145, 512, 1025, 16383 };
    for (int h = 0; h < kNumSizesToTest; ++h) {
      for (int w = 0; w < kNumSizesToTest; ++w) {
        ASSERT_NO_FATAL_FAILURE(ResetImages(kSizesToTest[w], kSizesToTest[h]));
        ReferenceCopyFrame();
        ExtendBorder();
        CompareImages(img_);
        DeallocImages();
      }
    }
  }

  ExtendFrameBorderFunc extend_fn_;
};

TEST_P(ExtendBorderTest, ExtendBorder) { ASSERT_NO_FATAL_FAILURE(RunTest()); }

INSTANTIATE_TEST_CASE_P(C, ExtendBorderTest,
                        ::testing::Values(vp8_yv12_extend_frame_borders_c));

class CopyFrameTest : public VpxScaleBase,
                      public ::testing::TestWithParam<CopyFrameFunc> {
 public:
  virtual ~CopyFrameTest() {}

 protected:
  virtual void SetUp() { copy_frame_fn_ = GetParam(); }

  void CopyFrame() {
    ASM_REGISTER_STATE_CHECK(copy_frame_fn_(&img_, &dst_img_));
  }

  void RunTest() {
#if ARCH_ARM
    // Some arm devices OOM when trying to allocate the largest buffers.
    static const int kNumSizesToTest = 6;
#else
    static const int kNumSizesToTest = 7;
#endif
    static const int kSizesToTest[] = { 1, 15, 33, 145, 512, 1025, 16383 };
    for (int h = 0; h < kNumSizesToTest; ++h) {
      for (int w = 0; w < kNumSizesToTest; ++w) {
        ASSERT_NO_FATAL_FAILURE(ResetImages(kSizesToTest[w], kSizesToTest[h]));
        ReferenceCopyFrame();
        CopyFrame();
        CompareImages(dst_img_);
        DeallocImages();
      }
    }
  }

  CopyFrameFunc copy_frame_fn_;
};

TEST_P(CopyFrameTest, CopyFrame) { ASSERT_NO_FATAL_FAILURE(RunTest()); }

INSTANTIATE_TEST_CASE_P(C, CopyFrameTest,
                        ::testing::Values(vp8_yv12_copy_frame_c));

}  // namespace libvpx_test
