/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/peerconnection_quality_test_fixture.h"

#include <vector>

#include "absl/types/optional.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using VideoResolution = ::webrtc::webrtc_pc_e2e::
    PeerConnectionE2EQualityTestFixture::VideoResolution;
using VideoConfig =
    ::webrtc::webrtc_pc_e2e::PeerConnectionE2EQualityTestFixture::VideoConfig;
using VideoSubscription = ::webrtc::webrtc_pc_e2e::
    PeerConnectionE2EQualityTestFixture::VideoSubscription;

TEST(PclfVideoSubscription, MaxFromSenderSpecEqualIndependentOfOtherFields) {
  VideoResolution r1(VideoResolution::Spec::kMaxFromSender);
  r1.set_width(1);
  r1.set_height(2);
  r1.set_fps(3);
  VideoResolution r2(VideoResolution::Spec::kMaxFromSender);
  r1.set_width(4);
  r1.set_height(5);
  r1.set_fps(6);
  EXPECT_EQ(r1, r2);
}

TEST(PclfVideoSubscription, WhenSpecIsNotSetFieldsAreCompared) {
  VideoResolution test_resolution(/*width=*/1, /*height=*/2,
                                  /*fps=*/3);
  VideoResolution equal_resolution(/*width=*/1, /*height=*/2,
                                   /*fps=*/3);
  VideoResolution different_width(/*width=*/10, /*height=*/2,
                                  /*fps=*/3);
  VideoResolution different_height(/*width=*/1, /*height=*/20,
                                   /*fps=*/3);
  VideoResolution different_fps(/*width=*/1, /*height=*/20,
                                /*fps=*/30);

  EXPECT_EQ(test_resolution, equal_resolution);
  EXPECT_NE(test_resolution, different_width);
  EXPECT_NE(test_resolution, different_height);
  EXPECT_NE(test_resolution, different_fps);
}

TEST(PclfVideoSubscription, GetMaxResolutionForEmptyReturnsNullopt) {
  absl::optional<VideoResolution> resolution =
      VideoSubscription::GetMaxResolution(std::vector<VideoConfig>{});
  ASSERT_FALSE(resolution.has_value());
}

TEST(PclfVideoSubscription, GetMaxResolutionSelectMaxForEachDimention) {
  VideoConfig max_width(/*width=*/1000, /*height=*/1, /*fps=*/1);
  VideoConfig max_height(/*width=*/1, /*height=*/100, /*fps=*/1);
  VideoConfig max_fps(/*width=*/1, /*height=*/1, /*fps=*/10);

  absl::optional<VideoResolution> resolution =
      VideoSubscription::GetMaxResolution(
          std::vector<VideoConfig>{max_width, max_height, max_fps});
  ASSERT_TRUE(resolution.has_value());
  EXPECT_EQ(resolution->width(), static_cast<size_t>(1000));
  EXPECT_EQ(resolution->height(), static_cast<size_t>(100));
  EXPECT_EQ(resolution->fps(), 10);
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
