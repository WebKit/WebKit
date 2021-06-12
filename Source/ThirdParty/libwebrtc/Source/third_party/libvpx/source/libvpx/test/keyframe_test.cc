/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <climits>
#include <vector>
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

class KeyframeTest
    : public ::libvpx_test::EncoderTest,
      public ::libvpx_test::CodecTestWithParam<libvpx_test::TestMode> {
 protected:
  KeyframeTest() : EncoderTest(GET_PARAM(0)) {}
  virtual ~KeyframeTest() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    kf_count_ = 0;
    kf_count_max_ = INT_MAX;
    kf_do_force_kf_ = false;
    set_cpu_used_ = 0;
  }

  virtual void PreEncodeFrameHook(::libvpx_test::VideoSource *video,
                                  ::libvpx_test::Encoder *encoder) {
    if (kf_do_force_kf_) {
      frame_flags_ = (video->frame() % 3) ? 0 : VPX_EFLAG_FORCE_KF;
    }
    if (set_cpu_used_ && video->frame() == 0) {
      encoder->Control(VP8E_SET_CPUUSED, set_cpu_used_);
    }
  }

  virtual void FramePktHook(const vpx_codec_cx_pkt_t *pkt) {
    if (pkt->data.frame.flags & VPX_FRAME_IS_KEY) {
      kf_pts_list_.push_back(pkt->data.frame.pts);
      kf_count_++;
      abort_ |= kf_count_ > kf_count_max_;
    }
  }

  bool kf_do_force_kf_;
  int kf_count_;
  int kf_count_max_;
  std::vector<vpx_codec_pts_t> kf_pts_list_;
  int set_cpu_used_;
};

TEST_P(KeyframeTest, TestRandomVideoSource) {
  // Validate that encoding the RandomVideoSource produces multiple keyframes.
  // This validates the results of the TestDisableKeyframes test.
  kf_count_max_ = 2;  // early exit successful tests.

  ::libvpx_test::RandomVideoSource video;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  // In realtime mode - auto placed keyframes are exceedingly rare,  don't
  // bother with this check   if(GetParam() > 0)
  if (GET_PARAM(1) > 0) {
    EXPECT_GT(kf_count_, 1);
  }
}

TEST_P(KeyframeTest, TestDisableKeyframes) {
  cfg_.kf_mode = VPX_KF_DISABLED;
  kf_count_max_ = 1;  // early exit failed tests.

  ::libvpx_test::RandomVideoSource video;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  EXPECT_EQ(1, kf_count_);
}

TEST_P(KeyframeTest, TestForceKeyframe) {
  cfg_.kf_mode = VPX_KF_DISABLED;
  kf_do_force_kf_ = true;

  ::libvpx_test::DummyVideoSource video;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  // verify that every third frame is a keyframe.
  for (std::vector<vpx_codec_pts_t>::const_iterator iter = kf_pts_list_.begin();
       iter != kf_pts_list_.end(); ++iter) {
    ASSERT_EQ(0, *iter % 3) << "Unexpected keyframe at frame " << *iter;
  }
}

TEST_P(KeyframeTest, TestKeyframeMaxDistance) {
  cfg_.kf_max_dist = 25;

  ::libvpx_test::DummyVideoSource video;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  // verify that keyframe interval matches kf_max_dist
  for (std::vector<vpx_codec_pts_t>::const_iterator iter = kf_pts_list_.begin();
       iter != kf_pts_list_.end(); ++iter) {
    ASSERT_EQ(0, *iter % 25) << "Unexpected keyframe at frame " << *iter;
  }
}

TEST_P(KeyframeTest, TestAutoKeyframe) {
  cfg_.kf_mode = VPX_KF_AUTO;
  kf_do_force_kf_ = false;

  // Force a deterministic speed step in Real Time mode, as the faster modes
  // may not produce a keyframe like we expect. This is necessary when running
  // on very slow environments (like Valgrind). The step -11 was determined
  // experimentally as the fastest mode that still throws the keyframe.
  if (deadline_ == VPX_DL_REALTIME) set_cpu_used_ = -11;

  // This clip has a cut scene every 30 frames -> Frame 0, 30, 60, 90, 120.
  // I check only the first 40 frames to make sure there's a keyframe at frame
  // 0 and 30.
  ::libvpx_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 40);

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  // In realtime mode - auto placed keyframes are exceedingly rare,  don't
  // bother with this check
  if (GET_PARAM(1) > 0) {
    EXPECT_EQ(2u, kf_pts_list_.size()) << " Not the right number of keyframes ";
  }

  // Verify that keyframes match the file keyframes in the file.
  for (std::vector<vpx_codec_pts_t>::const_iterator iter = kf_pts_list_.begin();
       iter != kf_pts_list_.end(); ++iter) {
    if (deadline_ == VPX_DL_REALTIME && *iter > 0)
      EXPECT_EQ(0, (*iter - 1) % 30)
          << "Unexpected keyframe at frame " << *iter;
    else
      EXPECT_EQ(0, *iter % 30) << "Unexpected keyframe at frame " << *iter;
  }
}

VP8_INSTANTIATE_TEST_SUITE(KeyframeTest, ALL_TEST_MODES);
}  // namespace
