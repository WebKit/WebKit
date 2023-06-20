/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "pc/media_session.h"
#include "pc/session_description.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/peer_scenario/peer_scenario.h"

namespace webrtc {
namespace test {
namespace {
RtpHeaderExtensionMap AudioExtensions(
    const SessionDescriptionInterface& session) {
  auto* audio_desc =
      cricket::GetFirstAudioContentDescription(session.description());
  return RtpHeaderExtensionMap(audio_desc->rtp_header_extensions());
}

}  // namespace

TEST(RemoteEstimateEndToEnd, OfferedCapabilityIsInAnswer) {
  PeerScenario s(*test_info_);

  auto* caller = s.CreateClient(PeerScenarioClient::Config());
  auto* callee = s.CreateClient(PeerScenarioClient::Config());

  auto send_link = {s.net()->NodeBuilder().Build().node};
  auto ret_link = {s.net()->NodeBuilder().Build().node};

  s.net()->CreateRoute(caller->endpoint(), send_link, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), ret_link, caller->endpoint());

  auto signaling = s.ConnectSignaling(caller, callee, send_link, ret_link);
  caller->CreateVideo("VIDEO", PeerScenarioClient::VideoSendTrackConfig());
  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp(
      [](SessionDescriptionInterface* offer) {
        for (auto& cont : offer->description()->contents()) {
          cont.media_description()->set_remote_estimate(true);
        }
      },
      [&](const SessionDescriptionInterface& answer) {
        for (auto& cont : answer.description()->contents()) {
          EXPECT_TRUE(cont.media_description()->remote_estimate());
        }
        offer_exchange_done = true;
      });
  RTC_CHECK(s.WaitAndProcess(&offer_exchange_done));
}

TEST(RemoteEstimateEndToEnd, AudioUsesAbsSendTimeExtension) {
  // Defined before PeerScenario so it gets destructed after, to avoid use after
  // free.
  std::atomic<bool> received_abs_send_time(false);
  PeerScenario s(*test_info_);

  auto* caller = s.CreateClient(PeerScenarioClient::Config());
  auto* callee = s.CreateClient(PeerScenarioClient::Config());

  auto send_node = s.net()->NodeBuilder().Build().node;
  auto ret_node = s.net()->NodeBuilder().Build().node;

  s.net()->CreateRoute(caller->endpoint(), {send_node}, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {ret_node}, caller->endpoint());

  auto signaling = s.ConnectSignaling(caller, callee, {send_node}, {ret_node});
  caller->CreateAudio("AUDIO", cricket::AudioOptions());
  signaling.StartIceSignaling();
  RtpHeaderExtensionMap extension_map;
  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp(
      [&extension_map](SessionDescriptionInterface* offer) {
        extension_map = AudioExtensions(*offer);
        EXPECT_TRUE(extension_map.IsRegistered(kRtpExtensionAbsoluteSendTime));
      },
      [&](const SessionDescriptionInterface& answer) {
        EXPECT_TRUE(AudioExtensions(answer).IsRegistered(
            kRtpExtensionAbsoluteSendTime));
        offer_exchange_done = true;
      });
  RTC_CHECK(s.WaitAndProcess(&offer_exchange_done));
  send_node->router()->SetWatcher(
      [extension_map, &received_abs_send_time](const EmulatedIpPacket& packet) {
        // The dummy packets used by the fake signaling are filled with 0. We
        // want to ignore those and we can do that on the basis that the first
        // byte of RTP packets are guaranteed to not be 0.
        RtpPacket rtp_packet(&extension_map);
        // TODO(bugs.webrtc.org/14525): Look why there are RTP packets with
        // payload 72 or 73 (these don't have the RTP AbsoluteSendTime
        // Extension).
        if (rtp_packet.Parse(packet.data) && rtp_packet.PayloadType() == 111) {
          EXPECT_TRUE(rtp_packet.HasExtension<AbsoluteSendTime>());
          received_abs_send_time = true;
        }
      });
  RTC_CHECK(s.WaitAndProcess(&received_abs_send_time));
  caller->pc()->Close();
  callee->pc()->Close();
}
}  // namespace test
}  // namespace webrtc
