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
#include "video/send_statistics_proxy.h"
#include "video/video_stream_encoder.h"

using ::testing::NiceMock;

namespace webrtc {

class MockVideoStreamEncoder : public VideoStreamEncoder {
 public:
  explicit MockVideoStreamEncoder(SendStatisticsProxy* send_stats_proxy)
      : VideoStreamEncoder(1,
                           send_stats_proxy,
                           VideoSendStream::Config::EncoderSettings("fake", 0,
                                                                    nullptr),
                           nullptr,
                           std::unique_ptr<OveruseFrameDetector>()) {}
  ~MockVideoStreamEncoder() { Stop(); }

  MOCK_METHOD1(OnReceivedIntraFrameRequest, void(size_t));
};

class VieKeyRequestTest : public ::testing::Test {
 public:
  VieKeyRequestTest()
      : simulated_clock_(123456789),
        send_stats_proxy_(&simulated_clock_,
                          VideoSendStream::Config(nullptr),
                          VideoEncoderConfig::ContentType::kRealtimeVideo),
        encoder_(&send_stats_proxy_),
        encoder_rtcp_feedback_(
            &simulated_clock_,
            std::vector<uint32_t>(1, VieKeyRequestTest::kSsrc),
            &encoder_) {}

 protected:
  const uint32_t kSsrc = 1234;

  SimulatedClock simulated_clock_;
  SendStatisticsProxy send_stats_proxy_;
  MockVideoStreamEncoder encoder_;
  EncoderRtcpFeedback encoder_rtcp_feedback_;
};

TEST_F(VieKeyRequestTest, CreateAndTriggerRequests) {
  EXPECT_CALL(encoder_, OnReceivedIntraFrameRequest(0)).Times(1);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);
}

TEST_F(VieKeyRequestTest, TooManyOnReceivedIntraFrameRequest) {
  EXPECT_CALL(encoder_, OnReceivedIntraFrameRequest(0)).Times(1);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);
  simulated_clock_.AdvanceTimeMilliseconds(10);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);

  EXPECT_CALL(encoder_, OnReceivedIntraFrameRequest(0)).Times(1);
  simulated_clock_.AdvanceTimeMilliseconds(300);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);
}

}  // namespace webrtc
