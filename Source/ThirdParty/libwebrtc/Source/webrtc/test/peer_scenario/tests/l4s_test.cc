/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "absl/strings/str_cat.h"
#include "api/audio_options.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats_report.h"
#include "api/stats/rtcstats_objects.h"
#include "api/test/network_emulation/dual_pi2_network_queue.h"
#include "api/test/network_emulation/network_emulation_interfaces.h"
#include "api/test/network_emulation/network_queue.h"
#include "api/test/network_emulation_manager.h"
#include "api/transport/ecn_marking.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rtpfb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/network_constants.h"
#include "test/create_frame_generator_capturer.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/network/network_emulation.h"
#include "test/peer_scenario/peer_scenario.h"
#include "test/peer_scenario/peer_scenario_client.h"
#include "test/peer_scenario/signaling_route.h"

namespace webrtc {
namespace {

using test::PeerScenario;
using test::PeerScenarioClient;
using ::testing::HasSubstr;
using ::testing::TestWithParam;

// RTC event logs can be gathered from these tests.
// Add --peer_logs=true --peer_logs_root=/tmp/l4s/ to write logs to /tmp/l4s

// Helper class used for counting RTCP feedback messages.
class RtcpFeedbackCounter {
 public:
  void Count(const EmulatedIpPacket& packet) {
    if (!IsRtcpPacket(packet.data)) {
      return;
    }
    rtcp::CommonHeader header;
    ASSERT_TRUE(header.Parse(packet.data.cdata(), packet.data.size()));
    if (header.type() != rtcp::Rtpfb::kPacketType) {
      return;
    }
    if (header.fmt() == rtcp::CongestionControlFeedback::kFeedbackMessageType) {
      ++congestion_control_feedback_;
      rtcp::CongestionControlFeedback fb;
      ASSERT_TRUE(fb.Parse(header));
      for (const rtcp::CongestionControlFeedback::PacketInfo& info :
           fb.packets()) {
        switch (info.ecn) {
          case EcnMarking::kNotEct:
            ++not_ect_;
            break;
          case EcnMarking::kEct0:
            // Not used.
            RTC_CHECK_NOTREACHED();
            break;
          case EcnMarking::kEct1:
            // ECN-Capable Transport
            ++ect1_;
            break;
          case EcnMarking::kCe:
            ++ce_;
        }
      }
    }
    if (header.fmt() == rtcp::TransportFeedback::kFeedbackMessageType) {
      ++transport_sequence_number_feedback_;
    }
  }

  int FeedbackAccordingToRfc8888() const {
    return congestion_control_feedback_;
  }
  int FeedbackAccordingToTransportCc() const {
    return transport_sequence_number_feedback_;
  }
  int not_ect() const { return not_ect_; }
  int ect1() const { return ect1_; }
  int ce() const { return ce_; }

 private:
  int congestion_control_feedback_ = 0;
  int transport_sequence_number_feedback_ = 0;
  int not_ect_ = 0;
  int ect1_ = 0;
  int ce_ = 0;
};

scoped_refptr<const RTCStatsReport> GetStatsAndProcess(
    PeerScenario& s,
    PeerScenarioClient* client) {
  auto stats_collector = make_ref_counted<MockRTCStatsCollectorCallback>();
  client->pc()->GetStats(stats_collector.get());
  s.ProcessMessages(TimeDelta::Millis(0));
  RTC_CHECK(stats_collector->called());
  return stats_collector->report();
}

std::optional<int64_t> GetPacketsSentWithEct1(
    const scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  if (stats.empty()) {
    return std::nullopt;
  }
  return stats[0]->packets_sent_with_ect1;
}

std::optional<int64_t> GetPacketsReceivedWithEct1(
    const scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCInboundRtpStreamStats>();
  if (stats.empty()) {
    return std::nullopt;
  }
  return stats[0]->packets_received_with_ect1;
}

std::optional<int64_t> GetPacketsReceivedWithCe(
    const scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCInboundRtpStreamStats>();
  if (stats.empty()) {
    return std::nullopt;
  }
  return stats[0]->packets_received_with_ce;
}

TEST(L4STest, NegotiateAndUseCcfbIfEnabled) {
  PeerScenario s(*test_info_);

  PeerScenarioClient::Config config;
  config.field_trials.Set("WebRTC-RFC8888CongestionControlFeedback",
                          "Enabled,offer:true");
  config.disable_encryption = true;
  PeerScenarioClient* caller = s.CreateClient(config);
  PeerScenarioClient* callee = s.CreateClient(config);

  // Create network path from caller to callee.
  auto send_node = s.net()->NodeBuilder().Build().node;
  auto ret_node = s.net()->NodeBuilder().Build().node;
  s.net()->CreateRoute(caller->endpoint(), {send_node}, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {ret_node}, caller->endpoint());

  RtcpFeedbackCounter send_node_feedback_counter;
  send_node->router()->SetWatcher([&](const EmulatedIpPacket& packet) {
    send_node_feedback_counter.Count(packet);
  });
  RtcpFeedbackCounter ret_node_feedback_counter;
  ret_node->router()->SetWatcher([&](const EmulatedIpPacket& packet) {
    ret_node_feedback_counter.Count(packet);
  });

  auto signaling = s.ConnectSignaling(caller, callee, {send_node}, {ret_node});
  PeerScenarioClient::VideoSendTrackConfig video_conf;
  video_conf.generator.squares_video->framerate = 15;

  caller->CreateAudio("AUDIO_1", AudioOptions());
  caller->CreateVideo("VIDEO_1", video_conf);
  callee->CreateAudio("AUDIO_2", AudioOptions());
  callee->CreateVideo("VIDEO_2", video_conf);

  signaling.StartIceSignaling();

  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp(
      [&](SessionDescriptionInterface* offer) {
        std::string offer_str = absl::StrCat(*offer);
        // Check that the offer contain both congestion control feedback
        // accoring to RFC 8888, and transport-cc and the header extension
        // http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
        EXPECT_THAT(offer_str, HasSubstr("a=rtcp-fb:* ack ccfb\r\n"));
        EXPECT_THAT(offer_str, HasSubstr("transport-cc"));
        EXPECT_THAT(
            offer_str,
            HasSubstr("http://www.ietf.org/id/"
                      "draft-holmer-rmcat-transport-wide-cc-extensions"));
      },
      [&](const SessionDescriptionInterface& answer) {
        std::string answer_str = absl::StrCat(answer);
        EXPECT_THAT(answer_str, HasSubstr("a=rtcp-fb:* ack ccfb\r\n"));
        // Check that the answer does not contain transport-cc nor the
        // header extension
        // http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
        EXPECT_THAT(answer_str, Not(HasSubstr("transport-cc")));
        EXPECT_THAT(
            answer_str,
            Not(HasSubstr(" http://www.ietf.org/id/"
                          "draft-holmer-rmcat-transport-wide-cc-extensions-")));
        offer_exchange_done = true;
      });
  // Wait for SDP negotiation and the packet filter to be setup.
  s.WaitAndProcess(&offer_exchange_done);

  s.ProcessMessages(TimeDelta::Seconds(2));
  EXPECT_GT(send_node_feedback_counter.FeedbackAccordingToRfc8888(), 0);
  EXPECT_EQ(send_node_feedback_counter.FeedbackAccordingToTransportCc(), 0);

  EXPECT_GT(ret_node_feedback_counter.FeedbackAccordingToRfc8888(), 0);
  EXPECT_EQ(ret_node_feedback_counter.FeedbackAccordingToTransportCc(), 0);
}

TEST(L4STest, NoCcfbSentAfterRenegotiationAndCallerCachLocalDescription) {
  // The caller supports CCFB, but the callee does not.
  // This test that the caller does not start sending CCFB after renegotiation
  // even if the local description is cached. The callers local description
  // will contain CCFB since it was used in the initial offer.
  PeerScenario s(*test_info_);
  PeerScenarioClient::Config caller_config;
  caller_config.disable_encryption = true;
  caller_config.field_trials.Set("WebRTC-RFC8888CongestionControlFeedback",
                                 "Enabled,offer:true");
  PeerScenarioClient* caller = s.CreateClient(caller_config);

  PeerScenarioClient::Config callee_config;
  callee_config.disable_encryption = true;
  callee_config.field_trials.Set("WebRTC-RFC8888CongestionControlFeedback",
                                 "Disabled");
  PeerScenarioClient* callee = s.CreateClient(callee_config);

  auto caller_to_callee = s.net()
                              ->NodeBuilder()
                              .capacity(DataRate::KilobitsPerSec(600))
                              .Build()
                              .node;
  auto callee_to_caller = s.net()
                              ->NodeBuilder()
                              .capacity(DataRate::KilobitsPerSec(600))
                              .Build()
                              .node;
  RtcpFeedbackCounter callee_feedback_counter;
  caller_to_callee->router()->SetWatcher([&](const EmulatedIpPacket& packet) {
    callee_feedback_counter.Count(packet);
  });
  RtcpFeedbackCounter caller_feedback_counter;
  callee_to_caller->router()->SetWatcher([&](const EmulatedIpPacket& packet) {
    caller_feedback_counter.Count(packet);
  });

  s.net()->CreateRoute(caller->endpoint(), {caller_to_callee},
                       callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {callee_to_caller},
                       caller->endpoint());

  auto signaling = s.ConnectSignaling(caller, callee, {caller_to_callee},
                                      {callee_to_caller});
  PeerScenarioClient::VideoSendTrackConfig video_conf;
  video_conf.generator.squares_video->framerate = 30;
  video_conf.generator.squares_video->width = 640;
  video_conf.generator.squares_video->height = 360;
  caller->CreateVideo("FROM_CALLER", video_conf);
  callee->CreateVideo("FROM_CALLEE", video_conf);

  signaling.StartIceSignaling();
  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp([&](const SessionDescriptionInterface& answer) {
    offer_exchange_done = true;
  });
  ASSERT_TRUE(s.WaitAndProcess(&offer_exchange_done));
  s.ProcessMessages(TimeDelta::Seconds(2));

  EXPECT_EQ(caller_feedback_counter.FeedbackAccordingToRfc8888(), 0);
  EXPECT_EQ(callee_feedback_counter.FeedbackAccordingToRfc8888(), 0);
  int transport_cc_caller =
      caller_feedback_counter.FeedbackAccordingToTransportCc();
  int transport_cc_callee =
      callee_feedback_counter.FeedbackAccordingToTransportCc();
  EXPECT_GT(transport_cc_caller, 0);
  EXPECT_GT(transport_cc_callee, 0);

  offer_exchange_done = false;
  // Save the caller's local description and use it as answer to the next offer
  // from callee.
  std::string answer_str;
  caller->pc()->local_description()->ToString(&answer_str);
  ASSERT_FALSE(answer_str.empty());
  ASSERT_THAT(answer_str, HasSubstr("a=rtcp-fb:* ack ccfb\r\n"));

  callee->CreateAndSetSdp(
      [&](SessionDescriptionInterface* /*munge_offer*/) {
        // Do not munge the offer.
      },
      [&](std::string offer) {
        // Callee does not support ccfb and does not have it in the offer.
        ASSERT_THAT(offer, Not(HasSubstr("a=rtcp-fb:* ack ccfb\r\n")));
        caller->SetRemoteDescription(
            offer, SdpType::kOffer, [&](RTCError error) {
              ASSERT_TRUE(error.ok());
              caller->SetLocalDescription(
                  answer_str, SdpType::kAnswer, [&](RTCError error) {
                    ASSERT_TRUE(error.ok());
                    callee->SetRemoteDescription(answer_str, SdpType::kAnswer,
                                                 [&](RTCError error) {
                                                   ASSERT_TRUE(error.ok());
                                                   offer_exchange_done = true;
                                                 });
                  });
            });
      });
  ASSERT_TRUE(s.WaitAndProcess(&offer_exchange_done));
  s.ProcessMessages(TimeDelta::Seconds(4));

  EXPECT_EQ(caller_feedback_counter.FeedbackAccordingToRfc8888(), 0);
  EXPECT_EQ(callee_feedback_counter.FeedbackAccordingToRfc8888(), 0);
  EXPECT_GT(caller_feedback_counter.FeedbackAccordingToTransportCc(),
            transport_cc_caller);
  EXPECT_GT(callee_feedback_counter.FeedbackAccordingToTransportCc(),
            transport_cc_callee);
}

#if !defined(WEBRTC_ANDROID)
// TODO: bugs.webrtc.org/447037083 - for some reason a "fake" hardware
// encoder/decoder is used on
// https://ci.chromium.org/ui/p/webrtc/builders/try/android_arm64_rel
// generic_decoder.cc: (line 306): Decoder implementation: DecoderInfo {
// prefers_late_decoding = implementation_name = 'fake_decoder',
// is_hardware_accelerated = true }
// Figure out how to run libvpx instead.

DataRate GetAvailableSendBitrate(
    const scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCIceCandidatePairStats>();
  if (stats.empty()) {
    return DataRate::Zero();
  }
  return DataRate::BitsPerSec(*stats[0]->available_outgoing_bitrate);
}

TimeDelta GetAverageRoundTripTime(
    const scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCIceCandidatePairStats>();
  if (stats.empty() || (stats[0]->responses_received.value_or(0) == 0)) {
    return TimeDelta::Zero();
  }

  return TimeDelta::Seconds(*stats[0]->total_round_trip_time /
                            *stats[0]->responses_received);
}

struct SupportRfc8888Params {
  bool caller_supports_rfc8888 = false;
  bool callee_supports_rfc8888 = false;
  std::string test_suffix;
};

class FeedbackFormatTest : public TestWithParam<SupportRfc8888Params> {};

TEST_P(FeedbackFormatTest, DISABLED_AdaptToLinkCapacityWithoutEcn) {
  const SupportRfc8888Params& params = GetParam();
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());

  PeerScenarioClient::Config caller_config;
  caller_config.disable_encryption = true;
  caller_config.field_trials.Set(
      "WebRTC-RFC8888CongestionControlFeedback",
      params.caller_supports_rfc8888 ? "Enabled,offer:true" : "Disabled");
  PeerScenarioClient* caller = s.CreateClient(caller_config);

  PeerScenarioClient::Config callee_config;
  callee_config.disable_encryption = true;
  callee_config.field_trials.Set(
      "WebRTC-RFC8888CongestionControlFeedback",
      params.callee_supports_rfc8888 ? "Enabled" : "Disabled");
  PeerScenarioClient* callee = s.CreateClient(callee_config);

  auto caller_to_callee = s.net()
                              ->NodeBuilder()
                              .capacity(DataRate::KilobitsPerSec(250))
                              .Build()
                              .node;
  auto callee_to_caller = s.net()
                              ->NodeBuilder()
                              .capacity(DataRate::KilobitsPerSec(250))
                              .Build()
                              .node;
  RtcpFeedbackCounter callee_feedback_counter;
  caller_to_callee->router()->SetWatcher([&](const EmulatedIpPacket& packet) {
    callee_feedback_counter.Count(packet);
  });
  RtcpFeedbackCounter caller_feedback_counter;
  callee_to_caller->router()->SetWatcher([&](const EmulatedIpPacket& packet) {
    caller_feedback_counter.Count(packet);
  });

  s.net()->CreateRoute(caller->endpoint(), {caller_to_callee},
                       callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {callee_to_caller},
                       caller->endpoint());

  auto signaling = s.ConnectSignaling(caller, callee, {caller_to_callee},
                                      {callee_to_caller});
  PeerScenarioClient::VideoSendTrackConfig video_conf;
  video_conf.generator.squares_video->framerate = 30;
  video_conf.generator.squares_video->width = 320;
  video_conf.generator.squares_video->height = 240;
  caller->CreateVideo("FROM_CALLER", video_conf);
  callee->CreateVideo("FROM_CALLEE", video_conf);
  caller->CreateAudio("FROM_CALLER", AudioOptions());
  callee->CreateAudio("FROM_CALLEE", AudioOptions());

  signaling.StartIceSignaling();
  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp([&](const SessionDescriptionInterface& answer) {
    offer_exchange_done = true;
  });
  s.WaitAndProcess(&offer_exchange_done);
  s.ProcessMessages(TimeDelta::Seconds(5));

  DataRate caller_available_bwe =
      GetAvailableSendBitrate(GetStatsAndProcess(s, caller));
  EXPECT_GT(caller_available_bwe.kbps(), 150);
  EXPECT_LT(caller_available_bwe.kbps(), 260);

  DataRate callee_available_bwe =
      GetAvailableSendBitrate(GetStatsAndProcess(s, callee));
  EXPECT_GT(callee_available_bwe.kbps(), 150);
  EXPECT_LT(callee_available_bwe.kbps(), 260);

  EXPECT_LT(GetAverageRoundTripTime(GetStatsAndProcess(s, caller)),
            TimeDelta::Millis(200));

  if (params.caller_supports_rfc8888 && params.callee_supports_rfc8888) {
    EXPECT_GT(caller_feedback_counter.FeedbackAccordingToRfc8888(), 0);
    EXPECT_GT(callee_feedback_counter.FeedbackAccordingToRfc8888(), 0);
    EXPECT_EQ(caller_feedback_counter.FeedbackAccordingToTransportCc(), 0);
    EXPECT_EQ(callee_feedback_counter.FeedbackAccordingToTransportCc(), 0);
  } else {
    EXPECT_EQ(caller_feedback_counter.FeedbackAccordingToRfc8888(), 0);
    EXPECT_EQ(callee_feedback_counter.FeedbackAccordingToRfc8888(), 0);
    EXPECT_GT(caller_feedback_counter.FeedbackAccordingToTransportCc(), 0);
    EXPECT_GT(callee_feedback_counter.FeedbackAccordingToTransportCc(), 0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    L4STest,
    FeedbackFormatTest,
    testing::Values(
        SupportRfc8888Params{.caller_supports_rfc8888 = true,
                             .test_suffix = "OnlyCallerSupportsRfc8888"},
        SupportRfc8888Params{.callee_supports_rfc8888 = true,
                             .test_suffix = "OnlyCalleeSupportsRfc8888"},
        SupportRfc8888Params{.caller_supports_rfc8888 = true,
                             .callee_supports_rfc8888 = true,
                             .test_suffix = "SupportsRfc8888"}),
    [](const testing::TestParamInfo<SupportRfc8888Params>& info) {
      return info.param.test_suffix;
    });

struct SendMediaTestResult {
  // Stats gathered at the end of the call.
  scoped_refptr<const RTCStatsReport> caller_stats;
};

struct SendMediaTestParams {
  bool use_dual_pi = false;
  DataRate link_capacity;
  TimeDelta one_way_delay;
  std::map</*trial*/ std::string, /*group*/ std::string> field_trials;
};

// Sends audio and video from a caller to a callee with symmetric
// uplink/downlink network.
SendMediaTestResult SendMediaInOneDirection(const SendMediaTestParams params) {
  PeerScenario s(*testing::UnitTest::GetInstance()->current_test_info());
  PeerScenarioClient::Config config;
  for (auto [trial, group] : params.field_trials) {
    config.field_trials.Set(trial, group);
  }
  PeerScenarioClient* caller = s.CreateClient(config);
  PeerScenarioClient* callee = s.CreateClient(config);

  NetworkEmulationManager::SimulatedNetworkNode::Builder network_builder =
      s.net()
          ->NodeBuilder()
          .capacity(params.link_capacity)
          .delay_ms(params.one_way_delay.ms());
  std::unique_ptr<NetworkQueueFactory> queue_factory;
  if (params.use_dual_pi) {
    queue_factory = std::make_unique<DualPi2NetworkQueueFactory>(
        DualPi2NetworkQueue::Config({.target_delay = TimeDelta::Millis(10)}));
    network_builder.queue_factory(*queue_factory);
  }

  EmulatedNetworkNode* caller_to_callee = network_builder.Build().node;
  EmulatedNetworkNode* callee_to_caller = network_builder.Build().node;
  s.net()->CreateRoute(caller->endpoint(), {caller_to_callee},
                       callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {callee_to_caller},
                       caller->endpoint());

  auto signaling = s.ConnectSignaling(caller, callee, {caller_to_callee},
                                      {callee_to_caller});
  PeerScenarioClient::VideoSendTrackConfig video_conf;
  video_conf.generator.squares_video->framerate = 30;
  video_conf.generator.squares_video->width = 1280;
  video_conf.generator.squares_video->height = 720;
  caller->CreateAudio("AUDIO_1", {});
  caller->CreateVideo("VIDEO_1", video_conf);

  signaling.StartIceSignaling();
  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp([&](const SessionDescriptionInterface& answer) {
    offer_exchange_done = true;
  });
  s.WaitAndProcess(&offer_exchange_done);
  s.ProcessMessages(TimeDelta::Seconds(10));

  SendMediaTestResult result;
  result.caller_stats = GetStatsAndProcess(s, caller);
  return result;
}

TEST(L4STest, CallerAdaptsToLinkCapacity600KbpsRtt100msNoEcnWithGoogCC) {
  SendMediaTestParams params;
  params.use_dual_pi = false;  // Simulated network will not support ECN.
  params.link_capacity = DataRate::KilobitsPerSec(600);
  params.one_way_delay = TimeDelta::Millis(50);
  params.field_trials = {
      {"WebRTC-RFC8888CongestionControlFeedback", "Enabled,offer:true"}};

  SendMediaTestResult result = SendMediaInOneDirection(params);
  DataRate available_bwe = GetAvailableSendBitrate(result.caller_stats);
  EXPECT_GT(available_bwe, DataRate::KilobitsPerSec(500));
  EXPECT_LT(available_bwe, DataRate::KilobitsPerSec(660));
}

TEST(L4STest, CallerAdaptsToLinkCapacity600KbpsRtt100msNoEcnWithScream) {
  SendMediaTestParams params;
  params.use_dual_pi = false;  // Simulated network will not support ECN.
  params.link_capacity = DataRate::KilobitsPerSec(600);
  params.one_way_delay = TimeDelta::Millis(50);
  params.field_trials = {
      {"WebRTC-RFC8888CongestionControlFeedback", "Enabled,offer:true"},
      {"WebRTC-Bwe-ScreamV2", "Enabled"}};

  SendMediaTestResult result = SendMediaInOneDirection(params);
  DataRate available_bwe = GetAvailableSendBitrate(result.caller_stats);
  // TODO: bugs.webrtc.org/447037083 - Investigate behaviour.
  // Encoder rate increase slower than target rate. Once the encoder rate start
  // increasing, target rate drops too much.
  EXPECT_GT(available_bwe, DataRate::KilobitsPerSec(200));
  EXPECT_LT(available_bwe, DataRate::KilobitsPerSec(800));
}

TEST(L4STest, CallerAdaptsToLinkCapacity600KbpsRtt100msEcnWithScream) {
  SendMediaTestParams params;
  params.use_dual_pi = true;  // Simulated network will support ECN.
  params.link_capacity = DataRate::KilobitsPerSec(600);
  params.one_way_delay = TimeDelta::Millis(50);
  params.field_trials = {
      {"WebRTC-RFC8888CongestionControlFeedback", "Enabled,offer:true"},
      {"WebRTC-Bwe-ScreamV2", "Enabled"}};

  SendMediaTestResult result = SendMediaInOneDirection(params);
  DataRate available_bwe = GetAvailableSendBitrate(result.caller_stats);
  EXPECT_GT(available_bwe, DataRate::KilobitsPerSec(350));
  EXPECT_LT(available_bwe, DataRate::KilobitsPerSec(660));
}

TEST(L4STest, CallerAdaptsToLinkCapacity1000KbpsRtt100msEcnWithScream) {
  SendMediaTestParams params;
  params.use_dual_pi = true;  // Simulated network will support ECN.
  params.link_capacity = DataRate::KilobitsPerSec(1000);
  params.one_way_delay = TimeDelta::Millis(50);
  params.field_trials = {
      {"WebRTC-RFC8888CongestionControlFeedback", "Enabled,offer:true"},
      {"WebRTC-Bwe-ScreamV2", "Enabled"}};

  SendMediaTestResult result = SendMediaInOneDirection(params);
  DataRate available_bwe = GetAvailableSendBitrate(result.caller_stats);
  EXPECT_GT(available_bwe, DataRate::KilobitsPerSec(600));
  EXPECT_LT(available_bwe, DataRate::KilobitsPerSec(1000));
}

TEST(L4STest, DISABLED_CallerAdaptsToLinkCapacity2MbpsRtt50msNoEcnWithScream) {
  SendMediaTestParams params;
  params.use_dual_pi = false;  // Simulated network will not support ECN.
  params.link_capacity = DataRate::KilobitsPerSec(2000);
  params.one_way_delay = TimeDelta::Millis(25);
  params.field_trials = {
      {"WebRTC-RFC8888CongestionControlFeedback", "Enabled,offer:true"},
      {"WebRTC-Bwe-ScreamV2", "Enabled"}};

  SendMediaTestResult result = SendMediaInOneDirection(params);
  DataRate available_bwe = GetAvailableSendBitrate(result.caller_stats);
  EXPECT_GT(available_bwe, DataRate::KilobitsPerSec(1600));
  // TODO: bugs.webrtc.org/447037083 - Even if reference window is limited by
  // seen data in flight, target rate can still increase due to that RTT
  // decrease.
  EXPECT_LE(available_bwe, DataRate::KilobitsPerSec(2600));
}

TEST(L4STest, CallerAdaptsToLinkCapacity2MbpsRtt50msEcnWithScream) {
  SendMediaTestParams params;
  params.use_dual_pi = true;  // Simulated network will support ECN.
  params.link_capacity = DataRate::KilobitsPerSec(2000);
  params.one_way_delay = TimeDelta::Millis(25);
  params.field_trials = {
      {"WebRTC-RFC8888CongestionControlFeedback", "Enabled,offer:true"},
      {"WebRTC-Bwe-ScreamV2", "Enabled"}};

  SendMediaTestResult result = SendMediaInOneDirection(params);
  DataRate available_bwe = GetAvailableSendBitrate(result.caller_stats);
  EXPECT_GT(available_bwe, DataRate::KilobitsPerSec(1500));
  EXPECT_LT(available_bwe, DataRate::KilobitsPerSec(2100));
}

TEST(L4STest, CallerAdaptsToLinkCapacity2MbpsRtt50msNoEcnWithGoogCC) {
  SendMediaTestParams params;
  params.use_dual_pi = false;  // Simulated network will support ECN.
  params.link_capacity = DataRate::KilobitsPerSec(2000);
  params.one_way_delay = TimeDelta::Millis(25);
  params.field_trials = {
      {"WebRTC-RFC8888CongestionControlFeedback", "Enabled,offer:true"},
  };

  SendMediaTestResult result = SendMediaInOneDirection(params);
  DataRate available_bwe = GetAvailableSendBitrate(result.caller_stats);
  EXPECT_GT(available_bwe, DataRate::KilobitsPerSec(1000));
  EXPECT_LT(available_bwe, DataRate::KilobitsPerSec(2600));
}
#endif

TEST(L4STest, SendsEct1WithScream) {
  PeerScenario s(*test_info_);
  PeerScenarioClient::Config config;
  config.field_trials.Set("WebRTC-RFC8888CongestionControlFeedback",
                          "Enabled,offer:true");
  config.field_trials.Set("WebRTC-Bwe-ScreamV2", "Enabled");
  config.disable_encryption = true;
  PeerScenarioClient* caller = s.CreateClient(config);
  PeerScenarioClient* callee = s.CreateClient(config);
  EmulatedNetworkNode* caller_to_callee = s.net()->NodeBuilder().Build().node;
  EmulatedNetworkNode* callee_to_caller = s.net()->NodeBuilder().Build().node;
  s.net()->CreateRoute(caller->endpoint(), {caller_to_callee},
                       callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {callee_to_caller},
                       caller->endpoint());
  RtcpFeedbackCounter feedback_counter;
  callee_to_caller->router()->SetWatcher(
      [&](const EmulatedIpPacket& packet) { feedback_counter.Count(packet); });

  test::SignalingRoute signaling = s.ConnectSignaling(
      caller, callee, {caller_to_callee}, {callee_to_caller});
  PeerScenarioClient::VideoSendTrackConfig video_conf;
  video_conf.generator.squares_video->framerate = 15;
  caller->CreateVideo("VIDEO_1", video_conf);
  signaling.StartIceSignaling();
  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp([&](const SessionDescriptionInterface& answer) {
    offer_exchange_done = true;
  });

  s.WaitAndProcess(&offer_exchange_done);
  s.ProcessMessages(TimeDelta::Seconds(3));
  EXPECT_EQ(GetPacketsSentWithEct1(GetStatsAndProcess(s, caller)),
            feedback_counter.ect1());
  EXPECT_GT(feedback_counter.ect1(), 0);
  EXPECT_EQ(feedback_counter.not_ect(), 0);
}

TEST(L4STest, SendsEct1AfterRouteChange) {
  PeerScenario s(*test_info_);

  PeerScenarioClient::Config config;
  config.field_trials.Set("WebRTC-RFC8888CongestionControlFeedback",
                          "Enabled,offer:true");
  config.disable_encryption = true;
  config.endpoints = {{0, {.type = AdapterType::ADAPTER_TYPE_WIFI}}};
  PeerScenarioClient* caller = s.CreateClient(config);
  // Callee has booth wifi and cellular adapters.
  config.endpoints = {{0, {.type = AdapterType::ADAPTER_TYPE_WIFI}},
                      {1, {.type = AdapterType::ADAPTER_TYPE_CELLULAR}}};
  PeerScenarioClient* callee = s.CreateClient(config);

  // Create network path from caller to callee.
  auto caller_to_callee = s.net()->NodeBuilder().Build().node;
  auto callee_to_caller_wifi = s.net()->NodeBuilder().Build().node;
  auto callee_to_caller_cellular = s.net()->NodeBuilder().Build().node;
  s.net()->CreateRoute(caller->endpoint(0), {caller_to_callee},
                       callee->endpoint(0));
  s.net()->CreateRoute(caller->endpoint(0), {caller_to_callee},
                       callee->endpoint(1));
  s.net()->CreateRoute(callee->endpoint(0), {callee_to_caller_wifi},
                       caller->endpoint(0));
  s.net()->CreateRoute(callee->endpoint(1), {callee_to_caller_cellular},
                       caller->endpoint(0));

  RtcpFeedbackCounter wifi_feedback_counter;
  std::atomic<bool> seen_ect1_on_wifi_feedback = false;
  std::atomic<bool> seen_not_ect_on_wifi_feedback = false;
  callee_to_caller_wifi->router()->SetWatcher(
      [&](const EmulatedIpPacket& packet) {
        wifi_feedback_counter.Count(packet);
        if (wifi_feedback_counter.ect1() > 0) {
          seen_ect1_on_wifi_feedback = true;
          RTC_LOG(LS_INFO) << "ect 1 feedback on wifi: "
                           << wifi_feedback_counter.ect1();
        }
        if (wifi_feedback_counter.not_ect() > 0) {
          seen_not_ect_on_wifi_feedback = true;
          RTC_LOG(LS_INFO) << "not ect feedback on wifi: "
                           << wifi_feedback_counter.not_ect();
        }
      });

  auto signaling = s.ConnectSignaling(caller, callee, {caller_to_callee},
                                      {callee_to_caller_wifi});
  PeerScenarioClient::VideoSendTrackConfig video_conf;
  video_conf.generator.squares_video->framerate = 15;

  caller->CreateVideo("VIDEO_1", video_conf);
  signaling.StartIceSignaling();

  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp([&](const SessionDescriptionInterface& answer) {
    offer_exchange_done = true;
  });
  s.WaitAndProcess(&offer_exchange_done);

  // Wait for first feedback where packets have been sent with ECT(1). Then
  // feedback for packets sent as not ECT since currently webrtc does not
  // implement adaptation to ECN.
  EXPECT_TRUE(
      s.WaitAndProcess(&seen_ect1_on_wifi_feedback, TimeDelta::Seconds(1)));
  EXPECT_FALSE(seen_not_ect_on_wifi_feedback);
  EXPECT_TRUE(
      s.WaitAndProcess(&seen_not_ect_on_wifi_feedback, TimeDelta::Seconds(1)));

  RtcpFeedbackCounter cellular_feedback_counter;
  std::atomic<bool> seen_ect1_on_cellular_feedback = false;
  callee_to_caller_cellular->router()->SetWatcher(
      [&](const EmulatedIpPacket& packet) {
        cellular_feedback_counter.Count(packet);
        if (cellular_feedback_counter.ect1() > 0) {
          seen_ect1_on_cellular_feedback = true;
          RTC_LOG(LS_INFO) << "ect 1 feedback on cellular: "
                           << cellular_feedback_counter.ect1();
        }
      });
  // Disable callees wifi and expect that the connection switch to cellular and
  // sends packets with ECT(1) again.
  s.net()->DisableEndpoint(callee->endpoint(0));
  EXPECT_TRUE(
      s.WaitAndProcess(&seen_ect1_on_cellular_feedback, TimeDelta::Seconds(5)));

  // Check statistics.
  auto packets_sent_with_ect1_stats =
      GetPacketsSentWithEct1(GetStatsAndProcess(s, caller));
  EXPECT_EQ(packets_sent_with_ect1_stats,
            wifi_feedback_counter.ect1() + cellular_feedback_counter.ect1());

  auto callee_stats = GetStatsAndProcess(s, callee);
  auto packets_received_with_ect1_stats =
      GetPacketsReceivedWithEct1(callee_stats);
  auto packets_received_with_ce_stats = GetPacketsReceivedWithCe(callee_stats);
  EXPECT_EQ(packets_received_with_ect1_stats, wifi_feedback_counter.ect1());
  // TODO: bugs.webrtc.org/42225697 - testing CE would be useful.
  EXPECT_EQ(packets_received_with_ce_stats, 0);
}

}  // namespace
}  // namespace webrtc
