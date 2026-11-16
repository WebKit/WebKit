/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <string>
#include <utility>
#include <vector>

#include "api/audio_options.h"
#include "api/jsep.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats_report.h"
#include "api/test/network_emulation_manager.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "pc/session_description.h"
#include "test/create_frame_generator_capturer.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/network/network_emulation.h"
#include "test/peer_scenario/bwe_integration_tests/stats_utilities.h"
#include "test/peer_scenario/peer_scenario.h"
#include "test/peer_scenario/peer_scenario_client.h"
#include "test/peer_scenario/signaling_route.h"

// This test that WebRTC has basic support for receive side bandwidth
// estimation. Ie, if neither RFC 8888 congestion control feedback, nor
// transport sequence number header extensions is negotiated, bandwidth
// estimation falls back to rely on RTCP REMB.

// RTC event logs can be gathered from these tests.
// Add --peer_logs=true --peer_logs_root=/tmp/receive_side/ to write logs to
// /tmp/receive_side
namespace webrtc {
namespace {

using test::GetAvailableSendBitrate;
using test::PeerScenario;
using test::PeerScenarioClient;

MATCHER_P2(AvailableSendBitrateIsBetween, low, high, "") {
  DataRate available_bwe = GetAvailableSendBitrate(arg);
  return available_bwe > low && available_bwe < high;
}

struct SendMediaTestParams {
  std::vector<EmulatedNetworkNode*> caller_to_callee_path;
  std::vector<EmulatedNetworkNode*> callee_to_caller_path;
};

struct SendMediaTestResult {
  // Stats gathered every second during the call.
  std::vector<scoped_refptr<const RTCStatsReport>> caller_stats;
  std::vector<scoped_refptr<const RTCStatsReport>> callee_stats;
};

// Sends audio and video from a caller to a callee.
SendMediaTestResult SendMediaInOneDirection(SendMediaTestParams params,
                                            PeerScenario& s) {
  PeerScenarioClient::Config config;
  PeerScenarioClient* caller = s.CreateClient(config);
  PeerScenarioClient* callee = s.CreateClient(config);

  PeerScenarioClient::VideoSendTrackConfig video_conf;
  video_conf.generator.squares_video->framerate = 30;
  video_conf.generator.squares_video->width = 640;
  video_conf.generator.squares_video->height = 480;
  caller->CreateAudio("AUDIO_1", {});
  caller->CreateVideo("VIDEO_1", video_conf);

  s.net()->CreateRoute(caller->endpoint(), params.caller_to_callee_path,
                       callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), params.callee_to_caller_path,
                       caller->endpoint());
  auto signaling =
      s.ConnectSignaling(caller, callee, params.caller_to_callee_path,
                         params.callee_to_caller_path);

  signaling.StartIceSignaling();
  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp(
      [&](SessionDescriptionInterface* offer) {
        // Modify offer before sending to callee.
        for (ContentInfo& content_info : offer->description()->contents()) {
          // No RFC 8888 type of feedback.
          content_info.media_description()->set_rtcp_fb_ack_ccfb(false);
          std::vector<Codec> codecs =
              content_info.media_description()->codecs();
          // Dont offer ack type "transport-cc".
          for (Codec& codec : codecs) {
            codec.feedback_params.Remove(
                FeedbackParam(kRtcpFbParamTransportCc));
          }
          content_info.media_description()->set_codecs(codecs);
          // Do not offer TWCC header extension.
          RtpHeaderExtensions offered_rtp_header_extensions =
              content_info.media_description()->rtp_header_extensions();
          RtpHeaderExtensions filtered_rtp_header_extensions;
          for (const RtpExtension& extension : offered_rtp_header_extensions) {
            if (extension.uri != RtpExtension::kTransportSequenceNumberUri) {
              filtered_rtp_header_extensions.push_back(extension);
            }
          }
          content_info.media_description()->set_rtp_header_extensions(
              filtered_rtp_header_extensions);
        }
      },
      [&](const SessionDescriptionInterface& answer) {
        offer_exchange_done = true;
      });
  // Wait for SDP negotiation.
  s.WaitAndProcess(&offer_exchange_done);
  SendMediaTestResult result;

  Timestamp end_time = s.net()->Now() + TimeDelta::Seconds(20);
  while (s.net()->Now() < end_time) {
    s.ProcessMessages(TimeDelta::Seconds(1));
    result.caller_stats.push_back(GetStatsAndProcess(s, caller));
    result.callee_stats.push_back(GetStatsAndProcess(s, callee));
  }
  return result;
}

TEST(ReceiveSideBweTest, CallerWithRembAdaptsToLinkCapacity600KbpsRtt100ms) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  SendMediaTestParams params;
  params.caller_to_callee_path = {s.net()
                                      ->NodeBuilder()
                                      .capacity(DataRate::KilobitsPerSec(600))
                                      .delay_ms(50)
                                      .Build()
                                      .node};
  params.callee_to_caller_path = {s.net()
                                      ->NodeBuilder()
                                      .capacity(DataRate::KilobitsPerSec(600))
                                      .delay_ms(50)
                                      .Build()
                                      .node};
  SendMediaTestResult result = SendMediaInOneDirection(std::move(params), s);

  EXPECT_THAT(result.caller_stats.back(),
              AvailableSendBitrateIsBetween(DataRate::KilobitsPerSec(300),
                                            DataRate::KilobitsPerSec(800)));
}

// TODO: webrtc:464929459 - also add test for when REMB is not negotiated.

}  // namespace
}  // namespace webrtc
