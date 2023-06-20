/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/stats/rtcstats_objects.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "pc/media_session.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/peer_scenario/peer_scenario.h"
#include "test/peer_scenario/peer_scenario_client.h"

namespace webrtc {
namespace test {

using ::testing::SizeIs;

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
}  // namespace test
}  // namespace webrtc
