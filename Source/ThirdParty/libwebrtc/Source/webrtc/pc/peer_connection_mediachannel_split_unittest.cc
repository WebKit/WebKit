/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Integration tests for PeerConnection to exercise the options of
// either splitting or not splitting the MediaChannel object.
// These tests exercise a full stack over a simulated network.

// TODO(bugs.webrtc.org/13931): Delete these tests when split is landed.

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats.h"
#include "api/stats/rtc_stats_report.h"
#include "api/stats/rtcstats_objects.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "pc/session_description.h"
#include "pc/test/integration_test_helpers.h"
#include "rtc_base/gunit.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

class PeerConnectionMediaChannelSplitTest
    : public PeerConnectionIntegrationBaseTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  PeerConnectionMediaChannelSplitTest()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kUnifiedPlan,
                                          /* field_trials = */ GetParam()) {}
};

int NacksReceivedCount(PeerConnectionIntegrationWrapper& pc) {
  rtc::scoped_refptr<const webrtc::RTCStatsReport> report = pc.NewGetStats();
  auto sender_stats = report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  if (sender_stats.size() != 1) {
    ADD_FAILURE();
    return 0;
  }
  if (!sender_stats[0]->nack_count.is_defined()) {
    return 0;
  }
  return *sender_stats[0]->nack_count;
}

int NacksSentCount(PeerConnectionIntegrationWrapper& pc) {
  rtc::scoped_refptr<const webrtc::RTCStatsReport> report = pc.NewGetStats();
  auto receiver_stats = report->GetStatsOfType<RTCInboundRtpStreamStats>();
  if (receiver_stats.size() != 1) {
    ADD_FAILURE();
    return 0;
  }
  if (!receiver_stats[0]->nack_count.is_defined()) {
    return 0;
  }
  return *receiver_stats[0]->nack_count;
}

// Test disabled because it is flaky.
TEST_P(PeerConnectionMediaChannelSplitTest,
       DISABLED_AudioPacketLossCausesNack) {
  RTCConfiguration config;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  auto audio_transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalAudioTrack());
  ASSERT_TRUE(audio_transceiver_or_error.ok());
  auto send_transceiver = audio_transceiver_or_error.MoveValue();
  // Munge the SDP to include NACK and RRTR on Opus, and remove all other
  // codecs.
  caller()->SetGeneratedSdpMunger([](cricket::SessionDescription* desc) {
    for (ContentInfo& content : desc->contents()) {
      cricket::AudioContentDescription* media =
          content.media_description()->as_audio();
      std::vector<cricket::AudioCodec> codecs = media->codecs();
      std::vector<cricket::AudioCodec> codecs_out;
      for (cricket::AudioCodec codec : codecs) {
        if (codec.name == "opus") {
          codec.AddFeedbackParam(cricket::FeedbackParam(
              cricket::kRtcpFbParamNack, cricket::kParamValueEmpty));
          codec.AddFeedbackParam(cricket::FeedbackParam(
              cricket::kRtcpFbParamRrtr, cricket::kParamValueEmpty));
          codecs_out.push_back(codec);
        }
      }
      EXPECT_FALSE(codecs_out.empty());
      media->set_codecs(codecs_out);
    }
  });

  caller()->CreateAndSetAndSignalOffer();
  // Check for failure in helpers
  ASSERT_FALSE(HasFailure());
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio(1);
  ExpectNewFrames(media_expectations);
  ASSERT_FALSE(HasFailure());

  virtual_socket_server()->set_drop_probability(0.2);

  // Wait until callee has sent at least one NACK.
  // Note that due to stats caching, this might only be visible 50 ms
  // after the nack was in fact sent.
  EXPECT_TRUE_WAIT(NacksSentCount(*callee()) > 0, kDefaultTimeout);
  ASSERT_FALSE(HasFailure());

  virtual_socket_server()->set_drop_probability(0.0);
  // Wait until caller has received at least one NACK
  EXPECT_TRUE_WAIT(NacksReceivedCount(*caller()) > 0, kDefaultTimeout);
}

TEST_P(PeerConnectionMediaChannelSplitTest, VideoPacketLossCausesNack) {
  RTCConfiguration config;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  auto video_transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalVideoTrack());
  ASSERT_TRUE(video_transceiver_or_error.ok());
  auto send_transceiver = video_transceiver_or_error.MoveValue();
  // Munge the SDP to include NACK and RRTR on VP8, and remove all other
  // codecs.
  caller()->SetGeneratedSdpMunger([](cricket::SessionDescription* desc) {
    for (ContentInfo& content : desc->contents()) {
      cricket::VideoContentDescription* media =
          content.media_description()->as_video();
      std::vector<cricket::VideoCodec> codecs = media->codecs();
      std::vector<cricket::VideoCodec> codecs_out;
      for (cricket::VideoCodec codec : codecs) {
        if (codec.name == "VP8") {
          ASSERT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
              cricket::kRtcpFbParamNack, cricket::kParamValueEmpty)));
          codecs_out.push_back(codec);
        }
      }
      EXPECT_FALSE(codecs_out.empty());
      media->set_codecs(codecs_out);
    }
  });

  caller()->CreateAndSetAndSignalOffer();
  // Check for failure in helpers
  ASSERT_FALSE(HasFailure());
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeVideo(1);
  ExpectNewFrames(media_expectations);
  ASSERT_FALSE(HasFailure());

  virtual_socket_server()->set_drop_probability(0.2);

  // Wait until callee has sent at least one NACK.
  // Note that due to stats caching, this might only be visible 50 ms
  // after the nack was in fact sent.
  EXPECT_TRUE_WAIT(NacksSentCount(*callee()) > 0, kDefaultTimeout);
  ASSERT_FALSE(HasFailure());

  // Wait until caller has received at least one NACK
  EXPECT_TRUE_WAIT(NacksReceivedCount(*caller()) > 0, kDefaultTimeout);
}

INSTANTIATE_TEST_SUITE_P(PeerConnectionMediaChannelSplitTest,
                         PeerConnectionMediaChannelSplitTest,
                         Values("WebRTC-SplitMediaChannel/Disabled/",
                                "WebRTC-SplitMediaChannel/Enabled/"));

}  // namespace

}  // namespace webrtc
