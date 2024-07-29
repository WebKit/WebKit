/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <utility>

#include "api/stats/rtcstats_objects.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "pc/media_session.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "test/create_frame_generator_capturer.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/peer_scenario/peer_scenario.h"
#include "test/peer_scenario/peer_scenario_client.h"

#if WEBRTC_ENABLE_PROTOBUF
#include "api/test/network_emulation/schedulable_network_node_builder.h"
#endif

namespace webrtc {
namespace test {

using ::testing::SizeIs;
using ::testing::Test;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

rtc::scoped_refptr<const RTCStatsReport> GetStatsAndProcess(
    PeerScenario& s,
    PeerScenarioClient* client) {
  auto stats_collector =
      rtc::make_ref_counted<webrtc::MockRTCStatsCollectorCallback>();
  client->pc()->GetStats(stats_collector.get());
  s.ProcessMessages(TimeDelta::Millis(0));
  RTC_CHECK(stats_collector->called());
  return stats_collector->report();
}

DataRate GetAvailableSendBitrate(
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCIceCandidatePairStats>();
  if (stats.empty()) {
    return DataRate::Zero();
  }
  return DataRate::BitsPerSec(*stats[0]->available_outgoing_bitrate);
}

#if WEBRTC_ENABLE_PROTOBUF
TEST(BweRampupTest, BweRampUpWhenCapacityIncrease) {
  PeerScenario s(*test_info_);

  PeerScenarioClient* caller = s.CreateClient({});
  PeerScenarioClient* callee = s.CreateClient({});

  network_behaviour::NetworkConfigSchedule schedule;
  auto initial_config = schedule.add_item();
  initial_config->set_link_capacity_kbps(500);
  auto updated_capacity = schedule.add_item();
  updated_capacity->set_time_since_first_sent_packet_ms(3000);
  updated_capacity->set_link_capacity_kbps(3000);
  SchedulableNetworkNodeBuilder schedulable_builder(*s.net(),
                                                    std::move(schedule));

  auto caller_node = schedulable_builder.Build(/*random_seed=*/1);
  auto callee_node = s.net()->NodeBuilder().capacity_kbps(5000).Build().node;
  s.net()->CreateRoute(caller->endpoint(), {caller_node}, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {callee_node}, caller->endpoint());

  FrameGeneratorCapturerConfig::SquaresVideo video_resolution = {
      .framerate = 30, .width = 1280, .height = 720};
  PeerScenarioClient::VideoSendTrack track = caller->CreateVideo(
      "VIDEO", {.generator = {.squares_video = video_resolution}});

  auto signaling =
      s.ConnectSignaling(caller, callee, {caller_node}, {callee_node});

  signaling.StartIceSignaling();

  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp([&](const SessionDescriptionInterface& answer) {
    offer_exchange_done = true;
  });
  // Wait for SDP negotiation.
  s.WaitAndProcess(&offer_exchange_done);

  s.ProcessMessages(TimeDelta::Seconds(5));
  DataRate bwe_before_capacity_increase =
      GetAvailableSendBitrate(GetStatsAndProcess(s, caller));
  EXPECT_GT(bwe_before_capacity_increase.kbps(), 300);
  EXPECT_LT(bwe_before_capacity_increase.kbps(), 650);
  s.ProcessMessages(TimeDelta::Seconds(15));
  EXPECT_GT(GetAvailableSendBitrate(GetStatsAndProcess(s, caller)).kbps(),
            1000);
}
#endif  // WEBRTC_ENABLE_PROTOBUF

// Test that caller BWE can rampup even if callee can not demux incoming RTP
// packets.
TEST(BweRampupTest, RampUpWithUndemuxableRtpPackets) {
  PeerScenario s(*test_info_);

  PeerScenarioClient::Config config = PeerScenarioClient::Config();
  config.disable_encryption = true;
  PeerScenarioClient* caller = s.CreateClient(config);
  PeerScenarioClient* callee = s.CreateClient(config);

  auto send_node = s.net()->NodeBuilder().Build().node;
  auto ret_node = s.net()->NodeBuilder().Build().node;

  s.net()->CreateRoute(caller->endpoint(), {send_node}, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {ret_node}, caller->endpoint());

  auto signaling = s.ConnectSignaling(caller, callee, {send_node}, {ret_node});
  PeerScenarioClient::VideoSendTrackConfig video_conf;
  video_conf.generator.squares_video->framerate = 15;

  PeerScenarioClient::VideoSendTrack track =
      caller->CreateVideo("VIDEO", video_conf);

  signaling.StartIceSignaling();

  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp(
      [&](SessionDescriptionInterface* offer) {
        RtpHeaderExtensionMap extension_map(
            cricket::GetFirstVideoContentDescription(offer->description())
                ->rtp_header_extensions());
        ASSERT_TRUE(extension_map.IsRegistered(kRtpExtensionMid));
        const std::string video_mid =
            cricket::GetFirstVideoContent(offer->description())->mid();
        send_node->router()->SetFilter([extension_map, video_mid, &send_node](
                                           const EmulatedIpPacket& packet) {
          if (IsRtpPacket(packet.data)) {
            // Replace Mid with another. This should lead to that packets
            // can not be demuxed by the callee, but BWE should still
            // function.
            RtpPacket parsed_packet;
            parsed_packet.IdentifyExtensions(extension_map);
            EXPECT_TRUE(parsed_packet.Parse(packet.data));
            std::string mid;
            if (parsed_packet.GetExtension<RtpMid>(&mid)) {
              if (mid == video_mid) {
                parsed_packet.SetExtension<RtpMid>("x");
                EmulatedIpPacket updated_packet(packet.from, packet.to,
                                                parsed_packet.Buffer(),
                                                packet.arrival_time);
                send_node->OnPacketReceived(std::move(updated_packet));
                return false;
              }
            }
          }
          return true;
        });
      },
      [&](const SessionDescriptionInterface& answer) {
        offer_exchange_done = true;
      });
  // Wait for SDP negotiation and the packet filter to be setup.
  s.WaitAndProcess(&offer_exchange_done);

  DataRate initial_bwe = GetAvailableSendBitrate(GetStatsAndProcess(s, caller));
  s.ProcessMessages(TimeDelta::Seconds(2));

  auto callee_inbound_stats =
      GetStatsAndProcess(s, callee)->GetStatsOfType<RTCInboundRtpStreamStats>();
  ASSERT_THAT(callee_inbound_stats, SizeIs(1));
  ASSERT_EQ(*callee_inbound_stats[0]->frames_received, 0u);

  DataRate final_bwe = GetAvailableSendBitrate(GetStatsAndProcess(s, caller));
  // Ensure BWE has increased from the initial BWE. BWE will not increase unless
  // RTCP feedback is recevied. The increase is just an arbitrary value to
  // ensure BWE has increased beyond noise levels.
  EXPECT_GT(final_bwe, initial_bwe + DataRate::KilobitsPerSec(345));
}

struct InitialProbeTestParams {
  DataRate network_capacity;
  DataRate expected_bwe_min;
};
class BweRampupWithInitialProbeTest
    : public Test,
      public WithParamInterface<InitialProbeTestParams> {};

INSTANTIATE_TEST_SUITE_P(
    BweRampupWithInitialProbeTest,
    BweRampupWithInitialProbeTest,
    ValuesIn<InitialProbeTestParams>(
        {{
             .network_capacity = DataRate::KilobitsPerSec(3000),
             .expected_bwe_min = DataRate::KilobitsPerSec(2500),
         },
         {
             .network_capacity = webrtc::DataRate::KilobitsPerSec(500),
             .expected_bwe_min = webrtc::DataRate::KilobitsPerSec(400),
         }}));

// Test that caller and callee BWE rampup even if no media packets are sent.
// - BandWidthEstimationSettings.allow_probe_without_media must be set.
// - A Video RtpTransceiver with RTX support needs to be negotiated.
TEST_P(BweRampupWithInitialProbeTest, BweRampUpBothDirectionsWithoutMedia) {
  PeerScenario s(*::testing::UnitTest::GetInstance()->current_test_info());
  InitialProbeTestParams test_params = GetParam();

  PeerScenarioClient* caller = s.CreateClient({});
  PeerScenarioClient* callee = s.CreateClient({});

  auto video_result = caller->pc()->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  ASSERT_EQ(video_result.error().type(), RTCErrorType::NONE);

  caller->pc()->ReconfigureBandwidthEstimation(
      {.allow_probe_without_media = true});
  callee->pc()->ReconfigureBandwidthEstimation(
      {.allow_probe_without_media = true});

  auto node_builder =
      s.net()->NodeBuilder().capacity_kbps(test_params.network_capacity.kbps());
  auto caller_node = node_builder.Build().node;
  auto callee_node = node_builder.Build().node;
  s.net()->CreateRoute(caller->endpoint(), {caller_node}, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {callee_node}, caller->endpoint());

  auto signaling =
      s.ConnectSignaling(caller, callee, {caller_node}, {callee_node});
  signaling.StartIceSignaling();

  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp(
      [&]() {
        // When remote description has been set, a transceiver is created.
        // Set the diretion to sendrecv so that it can be used for BWE probing
        // from callee -> caller.
        ASSERT_THAT(callee->pc()->GetTransceivers(), SizeIs(1));
        ASSERT_TRUE(
            callee->pc()
                ->GetTransceivers()[0]
                ->SetDirectionWithError(RtpTransceiverDirection::kSendRecv)
                .ok());
      },
      [&](const SessionDescriptionInterface& answer) {
        offer_exchange_done = true;
      });
  // Wait for SDP negotiation.
  s.WaitAndProcess(&offer_exchange_done);

  // Test that 1s after offer/answer exchange finish, we have a BWE estimate,
  // even though no video frames have been sent.
  s.ProcessMessages(TimeDelta::Seconds(2));

  auto callee_inbound_stats =
      GetStatsAndProcess(s, callee)->GetStatsOfType<RTCInboundRtpStreamStats>();
  ASSERT_THAT(callee_inbound_stats, SizeIs(1));
  ASSERT_EQ(*callee_inbound_stats[0]->frames_received, 0u);
  auto caller_inbound_stats =
      GetStatsAndProcess(s, caller)->GetStatsOfType<RTCInboundRtpStreamStats>();
  ASSERT_THAT(caller_inbound_stats, SizeIs(1));
  ASSERT_EQ(*caller_inbound_stats[0]->frames_received, 0u);

  DataRate caller_bwe = GetAvailableSendBitrate(GetStatsAndProcess(s, caller));
  EXPECT_GT(caller_bwe.kbps(), test_params.expected_bwe_min.kbps());
  EXPECT_LE(caller_bwe.kbps(), test_params.network_capacity.kbps());
  DataRate callee_bwe = GetAvailableSendBitrate(GetStatsAndProcess(s, callee));
  EXPECT_GT(callee_bwe.kbps(), test_params.expected_bwe_min.kbps());
  EXPECT_LE(callee_bwe.kbps(), test_params.network_capacity.kbps());
}

// Test that we can reconfigure bandwidth estimation and send new BWE probes.
// In this test, camera is stopped, and some times later, the app want to get a
// new BWE estimate.
TEST(BweRampupTest, CanReconfigureBweAfterStopingVideo) {
  PeerScenario s(*::testing::UnitTest::GetInstance()->current_test_info());
  PeerScenarioClient* caller = s.CreateClient({});
  PeerScenarioClient* callee = s.CreateClient({});

  auto node_builder = s.net()->NodeBuilder().capacity_kbps(1000);
  auto caller_node = node_builder.Build().node;
  auto callee_node = node_builder.Build().node;
  s.net()->CreateRoute(caller->endpoint(), {caller_node}, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {callee_node}, caller->endpoint());

  PeerScenarioClient::VideoSendTrack track = caller->CreateVideo("VIDEO", {});

  auto signaling =
      s.ConnectSignaling(caller, callee, {caller_node}, {callee_node});

  signaling.StartIceSignaling();

  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp([&](const SessionDescriptionInterface& answer) {
    offer_exchange_done = true;
  });
  // Wait for SDP negotiation.
  s.WaitAndProcess(&offer_exchange_done);

  // Send a TCP messages to the receiver using the same downlink node.
  // This is done just to force a lower BWE than the link capacity.
  webrtc::TcpMessageRoute* tcp_route = s.net()->CreateTcpRoute(
      s.net()->CreateRoute({caller_node}), s.net()->CreateRoute({callee_node}));
  DataRate bwe_before_restart = DataRate::Zero();

  std::atomic<bool> message_delivered(false);
  tcp_route->SendMessage(
      /*size=*/5'00'000,
      /*on_received=*/[&]() { message_delivered = true; });
  s.WaitAndProcess(&message_delivered);
  bwe_before_restart = GetAvailableSendBitrate(GetStatsAndProcess(s, caller));

  // Camera is stopped.
  track.capturer->Stop();
  s.ProcessMessages(TimeDelta::Seconds(2));

  // Some time later, the app is interested in restarting BWE since we may want
  // to resume video eventually.
  caller->pc()->ReconfigureBandwidthEstimation(
      {.allow_probe_without_media = true});
  s.ProcessMessages(TimeDelta::Seconds(1));
  DataRate bwe_after_restart =
      GetAvailableSendBitrate(GetStatsAndProcess(s, caller));
  EXPECT_GT(bwe_after_restart.kbps(), bwe_before_restart.kbps() + 300);
  EXPECT_LT(bwe_after_restart.kbps(), 1000);
}

}  // namespace test
}  // namespace webrtc
