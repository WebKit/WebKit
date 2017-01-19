/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/encoder_rtcp_feedback.h"

#include "webrtc/modules/utility/include/mock/mock_process_thread.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/video/vie_encoder.h"

using ::testing::NiceMock;

namespace webrtc {

class MockVieEncoder : public ViEEncoder {
 public:
  MockVieEncoder()
      : ViEEncoder(1,
                   nullptr,
                   VideoSendStream::Config::EncoderSettings("fake", 0, nullptr),
                   nullptr,
                   nullptr) {}
  ~MockVieEncoder() { Stop(); }

  MOCK_METHOD1(OnReceivedIntraFrameRequest, void(size_t));
  MOCK_METHOD1(OnReceivedSLI, void(uint8_t picture_id));
  MOCK_METHOD1(OnReceivedRPSI, void(uint64_t picture_id));
};

class VieKeyRequestTest : public ::testing::Test {
 public:
  VieKeyRequestTest()
      : simulated_clock_(123456789),
        encoder_rtcp_feedback_(
            &simulated_clock_,
            std::vector<uint32_t>(1, VieKeyRequestTest::kSsrc),
            &encoder_) {}

 protected:
  const uint32_t kSsrc = 1234;
  MockVieEncoder encoder_;
  SimulatedClock simulated_clock_;
  EncoderRtcpFeedback encoder_rtcp_feedback_;
};

TEST_F(VieKeyRequestTest, CreateAndTriggerRequests) {
  EXPECT_CALL(encoder_, OnReceivedIntraFrameRequest(0)).Times(1);
  encoder_rtcp_feedback_.OnReceivedIntraFrameRequest(kSsrc);

  const uint8_t sli_picture_id = 3;
  EXPECT_CALL(encoder_, OnReceivedSLI(sli_picture_id)).Times(1);
  encoder_rtcp_feedback_.OnReceivedSLI(kSsrc, sli_picture_id);

  const uint64_t rpsi_picture_id = 9;
  EXPECT_CALL(encoder_, OnReceivedRPSI(rpsi_picture_id)).Times(1);
  encoder_rtcp_feedback_.OnReceivedRPSI(kSsrc, rpsi_picture_id);
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
