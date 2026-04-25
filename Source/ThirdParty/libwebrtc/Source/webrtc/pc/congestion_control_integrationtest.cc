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

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "api/jsep.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtp_parameters.h"
#include "api/rtp_transceiver_direction.h"
#include "api/test/rtc_error_matchers.h"
#include "pc/session_description.h"
#include "pc/test/integration_test_helpers.h"
#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {

using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::HasSubstr;
using ::testing::IsTrue;
using ::testing::Not;

class PeerConnectionCongestionControlTest
    : public PeerConnectionIntegrationBaseTest {
 public:
  PeerConnectionCongestionControlTest()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kUnifiedPlan) {}
};

TEST_F(PeerConnectionCongestionControlTest,
       OfferDoesNotContainCcfbEvenIfEnabled) {
  SetFieldTrials("WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  caller()->AddAudioVideoTracks();
  std::unique_ptr<SessionDescriptionInterface> offer =
      caller()->CreateOfferAndWait();
  std::string offer_str = absl::StrCat(*offer);
  EXPECT_THAT(offer_str, Not(HasSubstr("a=rtcp-fb:* ack ccfb\r\n")));
}

TEST_F(PeerConnectionCongestionControlTest, OfferContainsCcfbIfFieldTrial) {
  SetFieldTrials("WebRTC-RFC8888CongestionControlFeedback/Enabled,offer:true/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  caller()->AddAudioVideoTracks();
  std::unique_ptr<SessionDescriptionInterface> offer =
      caller()->CreateOfferAndWait();
  std::string offer_str = absl::StrCat(*offer);
  EXPECT_THAT(offer_str, HasSubstr("a=rtcp-fb:* ack ccfb\r\n"));
}

TEST_F(PeerConnectionCongestionControlTest, ReceiveOfferSetsCcfbFlag) {
  SetFieldTrials(kCallerName,
                 "WebRTC-RFC8888CongestionControlFeedback/Enabled,offer:true/");
  SetFieldTrials(kCalleeName,
                 "WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignalingForSdpOnly();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());
  {
    // Check that the callee parsed it.
    auto parsed_contents =
        callee()->pc()->remote_description()->description()->contents();
    EXPECT_FALSE(parsed_contents.empty());
    for (const auto& content : parsed_contents) {
      EXPECT_TRUE(content.media_description()->rtcp_fb_ack_ccfb());
    }
  }

  {
    // Check that the caller also parsed it.
    auto parsed_contents =
        caller()->pc()->remote_description()->description()->contents();
    EXPECT_FALSE(parsed_contents.empty());
    for (const auto& content : parsed_contents) {
      EXPECT_TRUE(content.media_description()->rtcp_fb_ack_ccfb());
    }
  }
  // Check that the answer does not contain transport-cc
  std::string answer_str = absl::StrCat(*caller()->pc()->remote_description());
  EXPECT_THAT(answer_str, Not(HasSubstr("transport-cc")));
}

TEST_F(PeerConnectionCongestionControlTest, SendOnlySupportDoesNotEnableCcFb) {
  // Enable CCFB for sender, do not enable it for receiver
  SetFieldTrials(kCallerName,
                 "WebRTC-RFC8888CongestionControlFeedback/Enabled,offer:true/");
  SetFieldTrials(kCalleeName,
                 "WebRTC-RFC8888CongestionControlFeedback/Disabled/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignalingForSdpOnly();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());
  {
    // Check that the callee parsed the CCFB
    auto parsed_contents =
        callee()->pc()->remote_description()->description()->contents();
    EXPECT_FALSE(parsed_contents.empty());
    for (const auto& content : parsed_contents) {
      EXPECT_TRUE(content.media_description()->rtcp_fb_ack_ccfb());
    }
  }

  {
    // Check that the caller did NOT get ccfb in response
    auto parsed_contents =
        caller()->pc()->remote_description()->description()->contents();
    EXPECT_FALSE(parsed_contents.empty());
    for (const auto& content : parsed_contents) {
      EXPECT_FALSE(content.media_description()->rtcp_fb_ack_ccfb());
    }
  }
  // Check that the answer does contain transport-cc
  std::string answer_str = absl::StrCat(*caller()->pc()->remote_description());
  EXPECT_THAT(answer_str, HasSubstr("transport-cc"));
}

TEST_F(PeerConnectionCongestionControlTest,
       ReNegotiationCalleeDoesNotSupportCcfb) {
  // Enable CCFB for sender, do not enable it for receiver
  SetFieldTrials(kCallerName,
                 "WebRTC-RFC8888CongestionControlFeedback/Enabled,offer:true/");
  SetFieldTrials(kCalleeName,
                 "WebRTC-RFC8888CongestionControlFeedback/Disabled/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignalingForSdpOnly();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());

  // Check that the callee answer does contain transport-cc
  std::string answer_str = absl::StrCat(*caller()->pc()->remote_description());
  ASSERT_THAT(answer_str, HasSubstr("transport-cc"));

  // Callee re-negotiates
  callee()->AddVideoTrack();
  callee()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());
  // Check that the caller's answer does contain CCFB.
  answer_str = absl::StrCat(*caller()->pc()->local_description());
  EXPECT_THAT(answer_str, Not(HasSubstr("ccfb")));
}

TEST_F(PeerConnectionCongestionControlTest, ReNegotiationBothSupportCcfb) {
  SetFieldTrials(kCallerName,
                 "WebRTC-RFC8888CongestionControlFeedback/Enabled,offer:true/"
                 "WebRTC-HeaderExtensionNegotiateMemory/Enabled/");
  SetFieldTrials(kCalleeName,
                 "WebRTC-RFC8888CongestionControlFeedback/Enabled,offer:true/"
                 "WebRTC-HeaderExtensionNegotiateMemory/Enabled/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignalingForSdpOnly();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());

  // Check that the callee's answer does  not contain transport-cc
  std::string answer_str = absl::StrCat(*caller()->pc()->remote_description());
  ASSERT_THAT(answer_str, Not(HasSubstr("transport-cc")));

  // Callee re-negotiates
  callee()->AddVideoTrack();
  callee()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());
  std::string offer_str = absl::StrCat(*callee()->pc()->local_description());
  EXPECT_THAT(offer_str, Not(HasSubstr("transport-cc")));
  EXPECT_THAT(
      offer_str,
      Not(HasSubstr("http://www.ietf.org/id/"
                    "draft-holmer-rmcat-transport-wide-cc-extensions-01")));

  answer_str = absl::StrCat(*caller()->pc()->local_description());
  EXPECT_THAT(answer_str, Not(HasSubstr("transport-cc")));
  EXPECT_THAT(
      answer_str,
      Not(HasSubstr("http://www.ietf.org/id/"
                    "draft-holmer-rmcat-transport-wide-cc-extensions-01")));
}

#ifdef WEBRTC_HAVE_SCTP
TEST_F(PeerConnectionCongestionControlTest,
       ReceiveOfferWithDataChannelsSetsCcfbFlag) {
  SetFieldTrials("WebRTC-RFC8888CongestionControlFeedback/Enabled,offer:true/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignalingForSdpOnly();
  caller()->AddAudioVideoTracks();
  caller()->CreateDataChannel();
  callee()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());
  {
    // Check that the callee parsed it.
    auto parsed_contents =
        callee()->pc()->remote_description()->description()->contents();
    EXPECT_FALSE(parsed_contents.empty());
    bool has_data_channel = false;
    for (const auto& content : parsed_contents) {
      if (content.type == MediaProtocolType::kSctp) {
        EXPECT_FALSE(content.media_description()->rtcp_fb_ack_ccfb());
        has_data_channel = true;
      } else {
        EXPECT_TRUE(content.media_description()->rtcp_fb_ack_ccfb());
      }
    }
    ASSERT_TRUE(has_data_channel);
  }

  {
    // Check that the caller also parsed the answer.
    auto parsed_contents =
        caller()->pc()->remote_description()->description()->contents();
    EXPECT_FALSE(parsed_contents.empty());
    bool has_data_channel = false;
    for (const auto& content : parsed_contents) {
      if (content.type == MediaProtocolType::kSctp) {
        EXPECT_FALSE(content.media_description()->rtcp_fb_ack_ccfb());
        has_data_channel = true;
      } else {
        EXPECT_TRUE(content.media_description()->rtcp_fb_ack_ccfb());
      }
    }
    ASSERT_TRUE(has_data_channel);
  }
  // Check that the answer does not contain transport-cc
  std::string answer_str = absl::StrCat(*caller()->pc()->remote_description());
  EXPECT_THAT(answer_str, Not(HasSubstr("transport-cc")));
}
#endif

TEST_F(PeerConnectionCongestionControlTest, NegotiatingCcfbRemovesTsn) {
  SetFieldTrials("WebRTC-RFC8888CongestionControlFeedback/Enabled,offer:true/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignalingForSdpOnly();
  callee()->AddVideoTrack();
  callee()->AddAudioTrack();
  // Add transceivers to caller in order to accomodate reception
  caller()->pc()->AddTransceiver(MediaType::VIDEO);
  caller()->pc()->AddTransceiver(MediaType::AUDIO);

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());

  ASSERT_THAT(caller()->pc()->GetTransceivers().size(), Eq(2));
  EXPECT_THAT(
      caller()->pc()->GetTransceivers()[0]->GetNegotiatedHeaderExtensions(),
      Not(Contains(
          AllOf(Field("uri", &RtpHeaderExtensionCapability::uri,
                      RtpExtension::kTransportSequenceNumberUri),
                Not(Field("direction", &RtpHeaderExtensionCapability::direction,
                          RtpTransceiverDirection::kStopped))))))
      << " in caller negotiated header extensions";
  EXPECT_THAT(
      caller()->pc()->GetTransceivers()[1]->GetNegotiatedHeaderExtensions(),
      Not(Contains(
          AllOf(Field("uri", &RtpHeaderExtensionCapability::uri,
                      RtpExtension::kTransportSequenceNumberUri),
                Not(Field("direction", &RtpHeaderExtensionCapability::direction,
                          RtpTransceiverDirection::kStopped))))))
      << " in caller negotiated header extensions";

  ASSERT_THAT(caller()->pc()->GetSenders().size(), Eq(2));
  EXPECT_THAT(
      caller()->pc()->GetSenders()[0]->GetParameters().header_extensions,
      Not(Contains(Field("uri", &RtpExtension::uri,
                         RtpExtension::kTransportSequenceNumberUri))))
      << " in caller sender parameters";
  EXPECT_THAT(
      caller()->pc()->GetSenders()[1]->GetParameters().header_extensions,
      Not(Contains(Field("uri", &RtpExtension::uri,
                         RtpExtension::kTransportSequenceNumberUri))))
      << " in caller sender parameters";
  EXPECT_THAT(
      caller()->pc()->GetReceivers()[0]->GetParameters().header_extensions,
      Not(Contains(Field("uri", &RtpExtension::uri,
                         RtpExtension::kTransportSequenceNumberUri))))
      << " in caller receiver parameters";
  EXPECT_THAT(caller()->pc()->GetReceivers()[1]->media_type(),
              Eq(MediaType::AUDIO));
  EXPECT_THAT(
      caller()->pc()->GetReceivers()[1]->GetParameters().header_extensions,
      Not(Contains(Field("uri", &RtpExtension::uri,
                         RtpExtension::kTransportSequenceNumberUri))))
      << " in caller receiver parameters";

  EXPECT_THAT(
      callee()->pc()->GetSenders()[0]->GetParameters().header_extensions,
      Not(Contains(Field("uri", &RtpExtension::uri,
                         RtpExtension::kTransportSequenceNumberUri))))
      << " in callee sender parameters";
  EXPECT_THAT(
      callee()->pc()->GetSenders()[1]->GetParameters().header_extensions,
      Not(Contains(Field("uri", &RtpExtension::uri,
                         RtpExtension::kTransportSequenceNumberUri))))
      << " in callee sender parameters";

  ASSERT_THAT(callee()->pc()->GetReceivers().size(), Eq(2));
  EXPECT_THAT(
      callee()->pc()->GetReceivers()[0]->GetParameters().header_extensions,
      Not(Contains(Field("uri", &RtpExtension::uri,
                         RtpExtension::kTransportSequenceNumberUri))))
      << " in callee receiver parameters";
  EXPECT_THAT(
      callee()->pc()->GetReceivers()[1]->GetParameters().header_extensions,
      Not(Contains(Field("uri", &RtpExtension::uri,
                         RtpExtension::kTransportSequenceNumberUri))))
      << " in callee receiver parameters";
}

TEST_F(PeerConnectionCongestionControlTest, CcfbGetsUsed) {
  SetFieldTrials("WebRTC-RFC8888CongestionControlFeedback/Enabled,offer:true/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  media_expectations.CalleeExpectsSomeVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  auto pc_internal = caller()->pc_internal();
  EXPECT_THAT(
      WaitUntil(
          [&] {
            return pc_internal->FeedbackAccordingToRfc8888CountForTesting();
          },
          Gt(0)),
      IsRtcOk());
  // There should be no transport-cc generated.
  EXPECT_THAT(pc_internal->FeedbackAccordingToTransportCcCountForTesting(),
              Eq(0));
}

TEST_F(PeerConnectionCongestionControlTest, CcfbGetsUsedWithPrAnswer) {
  SetFieldTrials("WebRTC-RFC8888CongestionControlFeedback/Enabled,offer:true/");
  metrics::Reset();
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  callee()->SetGeneratedSdpMunger(
      [](std::unique_ptr<SessionDescriptionInterface>& sdp) {
        SetSdpType(sdp, SdpType::kPrAnswer);
      });
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil(
                  [&] {
                    return caller()->pc()->signaling_state() ==
                           PeerConnectionInterface::kHaveRemotePrAnswer;
                  },
                  IsTrue()),
              IsRtcOk());
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  media_expectations.CalleeExpectsSomeVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  auto pc_internal = caller()->pc_internal();
  EXPECT_THAT(
      WaitUntil(
          [&] {
            return pc_internal->FeedbackAccordingToRfc8888CountForTesting();
          },
          Gt(0)),
      IsRtcOk());
  // There should be no transport-cc generated.
  EXPECT_THAT(pc_internal->FeedbackAccordingToTransportCcCountForTesting(),
              Eq(0));
  // Note that metrics are picked up from both PCs, so the number
  // of metric counts is 2.
  EXPECT_METRIC_EQ(
      metrics::NumSamples("WebRTC.PeerConnection.NegotiatedFeedbackType"), 2);
  EXPECT_METRIC_EQ(
      metrics::NumEvents("WebRTC.PeerConnection.NegotiatedFeedbackType",
                         static_cast<int>(RtcpFeedbackType::CCFB)),
      2);
  EXPECT_METRIC_EQ(
      metrics::NumEvents("WebRTC.PeerConnection.NegotiatedFeedbackType",
                         static_cast<int>(RtcpFeedbackType::TRANSPORT_CC)),
      0);
}

TEST_F(PeerConnectionCongestionControlTest, TransportCcGetsUsed) {
  SetFieldTrials("WebRTC-RFC8888CongestionControlFeedback/Disabled/");
  metrics::Reset();
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  media_expectations.CalleeExpectsSomeVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  auto pc_internal = caller()->pc_internal();
  EXPECT_THAT(
      WaitUntil(
          [&] {
            return pc_internal->FeedbackAccordingToTransportCcCountForTesting();
          },
          Gt(0)),
      IsRtcOk());
  // Test that RFC 8888 feedback is NOT generated when field trial disabled.
  EXPECT_THAT(pc_internal->FeedbackAccordingToRfc8888CountForTesting(), Eq(0));
  // Note that metrics are picked up from both PCs, so the number
  // of metric counts is 2.
  EXPECT_METRIC_EQ(
      metrics::NumSamples("WebRTC.PeerConnection.NegotiatedFeedbackType"), 2);
  EXPECT_METRIC_EQ(
      metrics::NumEvents("WebRTC.PeerConnection.NegotiatedFeedbackType",
                         static_cast<int>(RtcpFeedbackType::CCFB)),
      0);
  EXPECT_METRIC_EQ(
      metrics::NumEvents("WebRTC.PeerConnection.NegotiatedFeedbackType",
                         static_cast<int>(RtcpFeedbackType::TRANSPORT_CC)),
      2);
}

TEST_F(PeerConnectionCongestionControlTest,
       TransportCcGetsUsedIfCalleeNotEnabledRfc8888) {
  SetFieldTrials(kCallerName,
                 "WebRTC-RFC8888CongestionControlFeedback/Enabled,offer:true/");
  SetFieldTrials(kCalleeName,
                 "WebRTC-RFC8888CongestionControlFeedback/Disabled/");
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(WaitUntil([&] { return SignalingStateStable(); }, IsTrue()),
              IsRtcOk());
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  media_expectations.CalleeExpectsSomeVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  auto pc_internal = caller()->pc_internal();
  EXPECT_THAT(
      WaitUntil(
          [&] {
            return pc_internal->FeedbackAccordingToTransportCcCountForTesting();
          },
          Gt(0)),
      IsRtcOk());
  // Test that RFC 8888 feedback is NOT generated when field trial disabled.
  EXPECT_THAT(pc_internal->FeedbackAccordingToRfc8888CountForTesting(), Eq(0));
}

}  // namespace webrtc
