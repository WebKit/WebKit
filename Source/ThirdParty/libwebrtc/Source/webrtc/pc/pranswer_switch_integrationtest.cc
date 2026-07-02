/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains tests that verify the correct working of switching
// to a different callee between PR-Answer and Answer.

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/data_channel_interface.h"
#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/test/rtc_error_matchers.h"
#include "p2p/test/test_turn_server.h"
#include "pc/peer_connection.h"
#include "pc/test/fake_rtc_certificate_generator.h"
#include "pc/test/integration_test_helpers.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {

using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::HasSubstr;
using ::testing::IsTrue;
using ::testing::Ne;
using ::testing::Not;

class PeerConnectionPrAnswerSwitchTest
    : public PeerConnectionIntegrationBaseTest {
 public:
  PeerConnectionPrAnswerSwitchTest()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kUnifiedPlan) {}
  std::unique_ptr<PeerConnectionIntegrationWrapper> SetupCallee2(
      bool addTurn,
      bool create_media_engine) {
    RTCConfiguration config;
    if (addTurn) {
      static const SocketAddress turn_server_1_internal_address{"192.0.2.1",
                                                                3478};
      static const SocketAddress turn_server_1_external_address{"192.0.3.1", 0};
      TestTurnServer* turn_server_1 = CreateTurnServer(
          turn_server_1_internal_address, turn_server_1_external_address);

      // Bypass permission check on received packets so media can be sent before
      // the candidate is signaled.
      SendTask(network_thread(), [turn_server_1] {
        turn_server_1->set_enable_permission_checks(false);
      });

      PeerConnectionInterface::IceServer ice_server_1;
      ice_server_1.urls.push_back("turn:192.0.2.1:3478");
      ice_server_1.username = "test";
      ice_server_1.password = "test";
      config.servers.push_back(ice_server_1);
      config.type = PeerConnectionInterface::kRelay;
      config.presume_writable_when_fully_relayed = true;
    }
    CreatePeerConnectionWrappersWithConfig(config, config, create_media_engine);
    PeerConnectionDependencies dependencies(nullptr);
    // Ensure that the key of callee2 is different from the key of
    // callee1, so that reconnection will trigger an ICE restart.
    std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
        new FakeRTCCertificateGenerator());
    cert_generator->use_alternate_key();
    dependencies.cert_generator = std::move(cert_generator);
    auto callee2 = CreatePeerConnectionWrapper(
        "Callee2", nullptr, &config, std::move(dependencies), nullptr,
        /*reset_encoder_factory=*/false,
        /*reset_decoder_factory=*/false, create_media_engine);
    ConnectFakeSignaling();
    callee2->set_signaling_message_receiver(caller());
    return callee2;
  }

#ifdef WEBRTC_HAVE_SCTP
  std::unique_ptr<PeerConnectionIntegrationWrapper> SetupCallee2AndDc(
      bool addTurn) {
    std::unique_ptr<PeerConnectionIntegrationWrapper> callee2 =
        SetupCallee2(addTurn, /* create_media_engine= */ false);
    DataChannelInit dc_init;
    dc_init.negotiated = true;
    dc_init.id = 77;
    caller()->CreateDataChannel("label", &dc_init);
    callee()->CreateDataChannel("label", &dc_init);
    callee2->CreateDataChannel("label", &dc_init);

    return callee2;
  }
#endif  // WEBRTC_HAVE_SCTP

  void WaitConnected(bool prAnswer,
                     PeerConnectionIntegrationWrapper* caller,
                     PeerConnectionIntegrationWrapper* callee) {
    if (prAnswer) {
      EXPECT_EQ(caller->pc()->signaling_state(),
                PeerConnectionInterface::kHaveRemotePrAnswer);
      EXPECT_EQ(callee->pc()->signaling_state(),
                PeerConnectionInterface::kHaveLocalPrAnswer);
    } else {
      EXPECT_EQ(caller->pc()->signaling_state(),
                PeerConnectionInterface::kStable);
      EXPECT_EQ(callee->pc()->signaling_state(),
                PeerConnectionInterface::kStable);
    }
    ASSERT_THAT(
        WaitUntil([&] { return caller->pc()->peer_connection_state(); },
                  Eq(PeerConnectionInterface::PeerConnectionState::kConnected)),
        IsRtcOk());
    ASSERT_THAT(
        WaitUntil([&] { return callee->pc()->peer_connection_state(); },
                  Eq(PeerConnectionInterface::PeerConnectionState::kConnected)),
        IsRtcOk());
  }

#ifdef WEBRTC_HAVE_SCTP
  void WaitConnectedAndDcOpen(bool prAnswer,
                              PeerConnectionIntegrationWrapper* caller,
                              PeerConnectionIntegrationWrapper* callee) {
    WaitConnected(prAnswer, caller, callee);
    ASSERT_THAT(WaitUntil([&] { return caller->data_channel()->state(); },
                          Eq(DataChannelInterface::kOpen)),
                IsRtcOk());
    ASSERT_THAT(WaitUntil([&] { return callee->data_channel()->state(); },
                          Eq(DataChannelInterface::kOpen)),
                IsRtcOk());
  }

  static void SendOnDatachannelWhenConnectedCallback(
      PeerConnectionIntegrationWrapper* peer,
      const std::string& data,
      std::atomic<int>& signal) {
    if (peer->pc()->peer_connection_state() ==
            PeerConnectionInterface::PeerConnectionState::kConnected &&
        peer->data_channel()->state() == DataChannelInterface::kOpen) {
      peer->data_channel()->SendAsync(DataBuffer(data), [&](RTCError err) {
        signal.store(err.ok() ? 1 : -1);
      });
    }
  }
  void VerifyReceivedDcMessages(PeerConnectionIntegrationWrapper* peer,
                                const std::string& data,
                                std::atomic<int>& signal) {
    ASSERT_THAT(WaitUntil([&] { return signal.load(); }, Ne(0)), IsRtcOk());
    EXPECT_THAT(WaitUntil([&] { return peer->data_observer()->last_message(); },
                          Eq(data)),
                IsRtcOk());
  }
#endif  // WEBRTC_HAVE_SCTP
};

#ifdef WEBRTC_HAVE_SCTP
TEST_F(PeerConnectionPrAnswerSwitchTest, DtlsRestartOneCalleeAtATime) {
  // Keep these variables here to make sure they have a wider scope than the PC
  // objects that may have unfinished async operations (see
  // `SendOnDatachannelWhenConnectedCallback`) that depend on them when they
  // (the PC objects), go out of scope.
  std::atomic<int> caller_sent_on_dc(0);
  std::atomic<int> callee2_sent_on_dc(0);

  auto callee2 = SetupCallee2AndDc(/* addTurn= */ false);
  std::unique_ptr<SessionDescriptionInterface> offer;
  callee()->SetReceivedSdpMunger(
      [&](std::unique_ptr<SessionDescriptionInterface>& sdp) {
        // Capture offer so that it can be sent to Callee2 too.
        offer = sdp->Clone();
      });
  callee()->SetGeneratedSdpMunger(
      [&](std::unique_ptr<SessionDescriptionInterface>& sdp) {
        // Modify offer to kPrAnswer
        SetSdpType(sdp, SdpType::kPrAnswer);
      });
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_FALSE(HasFailure());
  WaitConnectedAndDcOpen(/* prAnswer= */ true, caller(), callee());
  ASSERT_FALSE(HasFailure());

  caller()->set_connection_change_callback([&](auto new_state) {
    SendOnDatachannelWhenConnectedCallback(caller(), "KESO", caller_sent_on_dc);
  });
  // Install same cb on both connection_change_callback and
  // data_observer->set_state_change_callback since they can fire in any order.
  callee2->set_connection_change_callback([&](auto new_state) {
    SendOnDatachannelWhenConnectedCallback(callee2.get(), "KENT",
                                           callee2_sent_on_dc);
  });
  callee2->data_observer()->set_state_change_callback([&](auto new_state) {
    SendOnDatachannelWhenConnectedCallback(callee2.get(), "KENT",
                                           callee2_sent_on_dc);
  });

  // Now let callee2 get the offer, apply it and send the answer to caller.
  std::string offer_sdp;
  EXPECT_TRUE(offer->ToString(&offer_sdp));
  callee2->ReceiveSdpMessage(SdpType::kOffer, offer_sdp);
  WaitConnectedAndDcOpen(/* prAnswer= */ false, caller(), callee2.get());
  ASSERT_FALSE(HasFailure());

  VerifyReceivedDcMessages(caller(), "KENT", callee2_sent_on_dc);
  VerifyReceivedDcMessages(callee2.get(), "KESO", caller_sent_on_dc);
  ASSERT_FALSE(HasFailure());
}
#endif  // WEBRTC_HAVE_SCTP

TEST_F(PeerConnectionPrAnswerSwitchTest, SendMediaNoDataChannel) {
  std::unique_ptr<PeerConnectionIntegrationWrapper> second_callee =
      SetupCallee2(/* addTurn= */ false, /* create_media_engine= */ true);
  std::string saved_offer;
  caller()->AddAudioVideoTracks();
  caller()->SetGeneratedSdpMunger(
      [&](std::unique_ptr<SessionDescriptionInterface>& sdp) {
        sdp->ToString(&saved_offer);
      });
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
  WaitConnected(/* prAnswer= */ true, caller(), callee());
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  media_expectations.CalleeExpectsSomeVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  // Send original offer to second callee and wait for settlement.
  second_callee->ReceiveSdpMessage(SdpType::kOffer, saved_offer);
  EXPECT_THAT(
      WaitUntil([&] { return caller()->SignalingStateStable(); }, IsTrue()),
      IsRtcOk());
  WaitConnected(/* prAnswer= */ false, caller(), second_callee.get());
  ASSERT_FALSE(HasFailure());
}

TEST_F(PeerConnectionPrAnswerSwitchTest, MediaWithCcfbFirstThenTwcc) {
  SetFieldTrials("WebRTC-RFC8888CongestionControlFeedback/Enabled,offer:true/");
  SetFieldTrials("Callee2",
                 "WebRTC-RFC8888CongestionControlFeedback/Disabled/");
  std::unique_ptr<PeerConnectionIntegrationWrapper> second_callee =
      SetupCallee2(/* addTurn= */ false, /* create_media_engine= */ true);
  std::string saved_offer;
  caller()->AddAudioVideoTracks();
  caller()->SetGeneratedSdpMunger(
      [&](std::unique_ptr<SessionDescriptionInterface>& sdp) {
        sdp->ToString(&saved_offer);
      });
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
  WaitConnected(/* prAnswer= */ true, caller(), callee());
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  media_expectations.CalleeExpectsSomeVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  PeerConnection* caller_pc_internal = caller()->pc_internal();
  EXPECT_THAT(WaitUntil(
                  [&] {
                    return caller_pc_internal
                        ->FeedbackAccordingToRfc8888CountForTesting();
                  },
                  Gt(0)),
              IsRtcOk());
  // There should be no transport-cc generated.
  EXPECT_THAT(
      caller_pc_internal->FeedbackAccordingToTransportCcCountForTesting(),
      Eq(0));
  // The final answer does TWCC and send audio and video.
  second_callee->AddAudioVideoTracks();
  second_callee->ReceiveSdpMessage(SdpType::kOffer, saved_offer);
  EXPECT_THAT(
      WaitUntil([&] { return caller()->SignalingStateStable(); }, IsTrue()),
      IsRtcOk());
  WaitConnected(/* prAnswer= */ false, caller(), second_callee.get());
  ASSERT_FALSE(HasFailure());

  int old_ccfb_count =
      caller_pc_internal->FeedbackAccordingToRfc8888CountForTesting();
  int old_twcc_count =
      caller_pc_internal->FeedbackAccordingToTransportCcCountForTesting();
  EXPECT_THAT(WaitUntil(
                  [&] {
                    return caller_pc_internal
                        ->FeedbackAccordingToTransportCcCountForTesting();
                  },
                  Gt(old_twcc_count)),
              IsRtcOk());
  // These expects are easier to interpret than the WaitUntil log result.
  EXPECT_THAT(
      caller_pc_internal->FeedbackAccordingToTransportCcCountForTesting(),
      Gt(old_twcc_count));
  EXPECT_THAT(caller_pc_internal->FeedbackAccordingToRfc8888CountForTesting(),
              Eq(old_ccfb_count));

  PeerConnection* second_callee_pc_internal = second_callee->pc_internal();
  EXPECT_THAT(WaitUntil(
                  [&] {
                    return second_callee_pc_internal
                        ->FeedbackAccordingToTransportCcCountForTesting();
                  },
                  Gt(0)),
              IsRtcOk());
}

}  // namespace webrtc
