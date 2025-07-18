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

#include "api/stats/rtcstats_objects.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rtpfb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "test/create_frame_generator_capturer.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/peer_scenario/peer_scenario.h"
#include "test/peer_scenario/peer_scenario_client.h"

namespace webrtc {
namespace {

using test::PeerScenario;
using test::PeerScenarioClient;
using ::testing::HasSubstr;

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

DataRate GetAvailableSendBitrate(
    const scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCIceCandidatePairStats>();
  if (stats.empty()) {
    return DataRate::Zero();
  }
  return DataRate::BitsPerSec(*stats[0]->available_outgoing_bitrate);
}

TEST(L4STest, NegotiateAndUseCcfbIfEnabled) {
  test::ScopedFieldTrials trials(
      "WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  PeerScenario s(*test_info_);

  PeerScenarioClient::Config config = PeerScenarioClient::Config();
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

  caller->CreateVideo("VIDEO_1", video_conf);
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
  // TODO: bugs.webrtc.org/42225697 - Fix bug. Caller sends both transport
  // sequence number feedback and congestion control feedback. So
  // callee still send packets with transport sequence number header extensions
  // even though it has been removed from the answer.
  // EXPECT_EQ(send_node_feedback_counter.FeedbackAccordingToTransportCc(), 0);

  EXPECT_GT(ret_node_feedback_counter.FeedbackAccordingToRfc8888(), 0);
  EXPECT_EQ(ret_node_feedback_counter.FeedbackAccordingToTransportCc(), 0);
}

TEST(L4STest, CallerAdaptToLinkCapacityWithoutEcn) {
  test::ScopedFieldTrials trials(
      "WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  PeerScenario s(*test_info_);

  PeerScenarioClient::Config config = PeerScenarioClient::Config();
  PeerScenarioClient* caller = s.CreateClient(config);
  PeerScenarioClient* callee = s.CreateClient(config);

  auto caller_to_callee = s.net()
                              ->NodeBuilder()
                              .capacity(DataRate::KilobitsPerSec(600))
                              .Build()
                              .node;
  auto callee_to_caller = s.net()->NodeBuilder().Build().node;
  s.net()->CreateRoute(caller->endpoint(), {caller_to_callee},
                       callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {callee_to_caller},
                       caller->endpoint());

  auto signaling = s.ConnectSignaling(caller, callee, {caller_to_callee},
                                      {callee_to_caller});
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
  DataRate available_bwe =
      GetAvailableSendBitrate(GetStatsAndProcess(s, caller));
  EXPECT_GT(available_bwe.kbps(), 500);
  EXPECT_LT(available_bwe.kbps(), 610);
}

TEST(L4STest, SendsEct1UntilFirstFeedback) {
  test::ScopedFieldTrials trials(
      "WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  PeerScenario s(*test_info_);

  PeerScenarioClient::Config config = PeerScenarioClient::Config();
  config.disable_encryption = true;
  PeerScenarioClient* caller = s.CreateClient(config);
  PeerScenarioClient* callee = s.CreateClient(config);

  // Create network path from caller to callee.
  auto caller_to_callee = s.net()->NodeBuilder().Build().node;
  auto callee_to_caller = s.net()->NodeBuilder().Build().node;
  s.net()->CreateRoute(caller->endpoint(), {caller_to_callee},
                       callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {callee_to_caller},
                       caller->endpoint());

  RtcpFeedbackCounter feedback_counter;
  std::atomic<bool> seen_ect1_feedback = false;
  std::atomic<bool> seen_not_ect_feedback = false;
  callee_to_caller->router()->SetWatcher([&](const EmulatedIpPacket& packet) {
    feedback_counter.Count(packet);
    if (feedback_counter.ect1() > 0) {
      seen_ect1_feedback = true;
      RTC_LOG(LS_INFO) << " ect 1" << feedback_counter.ect1();
    }
    if (feedback_counter.not_ect() > 0) {
      seen_not_ect_feedback = true;
      RTC_LOG(LS_INFO) << " not ect" << feedback_counter.not_ect();
    }
  });

  auto signaling = s.ConnectSignaling(caller, callee, {caller_to_callee},
                                      {callee_to_caller});
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
  EXPECT_TRUE(s.WaitAndProcess(&seen_ect1_feedback, TimeDelta::Seconds(1)));
  EXPECT_FALSE(seen_not_ect_feedback);
  EXPECT_TRUE(s.WaitAndProcess(&seen_not_ect_feedback, TimeDelta::Seconds(1)));
}

TEST(L4STest, SendsEct1AfterRouteChange) {
  test::ScopedFieldTrials trials(
      "WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  PeerScenario s(*test_info_);

  PeerScenarioClient::Config config;
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
          RTC_LOG(LS_INFO) << " ect 1 feedback on wifi: "
                           << wifi_feedback_counter.ect1();
        }
        if (wifi_feedback_counter.not_ect() > 0) {
          seen_not_ect_on_wifi_feedback = true;
          RTC_LOG(LS_INFO) << " not ect feedback on wifi: "
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
          RTC_LOG(LS_INFO) << " ect 1 feedback on cellular: "
                           << cellular_feedback_counter.ect1();
        }
      });
  // Disable callees wifi and expect that the connection switch to cellular and
  // sends packets with ECT(1) again.
  s.net()->DisableEndpoint(callee->endpoint(0));
  EXPECT_TRUE(
      s.WaitAndProcess(&seen_ect1_on_cellular_feedback, TimeDelta::Seconds(5)));
}

}  // namespace
}  // namespace webrtc
