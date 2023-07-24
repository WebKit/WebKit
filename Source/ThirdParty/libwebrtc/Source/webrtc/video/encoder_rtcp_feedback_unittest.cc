/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/encoder_rtcp_feedback.h"

#include <memory>

#include "test/gmock.h"
#include "test/gtest.h"
#include "video/test/mock_video_stream_encoder.h"

using ::testing::_;

namespace webrtc {

class VieKeyRequestTest : public ::testing::Test {
 public:
  VieKeyRequestTest()
      : simulated_clock_(123456789),
        encoder_(),
        encoder_rtcp_feedback_(
            &simulated_clock_,
            std::vector<uint32_t>(1, VieKeyRequestTest::kSsrc),
            &encoder_,
            nullptr) {}

 protected:
  const uint32_t kSsrc = 1234;

  SimulatedClock simulated_clock_;
  ::testing::StrictMock<MockVideoStreamEncoder> encoder_;
  EncoderRtcpFeedback encoder_rtcp_feedback_;
};

TEST_F(VieKeyRequestTest, CreateAndTriggerRequests) {
  EXPECT_CALL(encoder_, SendKeyFrame(_)).Times(1);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);
}

TEST_F(VieKeyRequestTest, TooManyOnReceivedIntraFrameRequest) {
  EXPECT_CALL(encoder_, SendKeyFrame(_)).Times(1);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);
  simulated_clock_.AdvanceTimeMilliseconds(10);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);

  EXPECT_CALL(encoder_, SendKeyFrame(_)).Times(1);
  simulated_clock_.AdvanceTimeMilliseconds(300);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);
}

}  // namespace webrtc
