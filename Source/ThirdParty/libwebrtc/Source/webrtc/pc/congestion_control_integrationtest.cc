/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains tests that verify that congestion control options
// are correctly negotiated in the SDP offer/answer.

#include <string>

#include "absl/strings/str_cat.h"
#include "api/peer_connection_interface.h"
#include "pc/test/integration_test_helpers.h"
#include "rtc_base/gunit.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using testing::HasSubstr;

class PeerConnectionCongestionControlTest
    : public PeerConnectionIntegrationBaseTest {
 public:
  PeerConnectionCongestionControlTest()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kUnifiedPlan) {}
};

TEST_F(PeerConnectionCongestionControlTest, OfferContainsCcfbIfEnabled) {
  test::ScopedFieldTrials trials(
      "WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  caller()->AddAudioVideoTracks();
  auto offer = caller()->CreateOfferAndWait();
  std::string offer_str = absl::StrCat(*offer);
  EXPECT_THAT(offer_str, HasSubstr("a=rtcp-fb:* ack ccfb\r\n"));
}

TEST_F(PeerConnectionCongestionControlTest, ReceiveOfferSetsCcfbFlag) {
  test::ScopedFieldTrials trials(
      "WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignalingForSdpOnly();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Check that the callee parsed it.
  auto parsed_contents =
      callee()->pc()->remote_description()->description()->contents();
  EXPECT_FALSE(parsed_contents.empty());
  for (const auto& content : parsed_contents) {
    EXPECT_TRUE(content.media_description()->rtcp_fb_ack_ccfb());
  }
  // Check that the caller also parsed it.
  parsed_contents =
      caller()->pc()->remote_description()->description()->contents();
  EXPECT_FALSE(parsed_contents.empty());
  for (const auto& content : parsed_contents) {
    EXPECT_TRUE(content.media_description()->rtcp_fb_ack_ccfb());
  }
}

}  // namespace webrtc
