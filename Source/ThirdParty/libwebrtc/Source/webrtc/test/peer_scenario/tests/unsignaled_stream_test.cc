/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/stream_params.h"
#include "modules/rtp_rtcp/source/byte_io.h"

#include "pc/media_session.h"
#include "pc/session_description.h"
#include "test/field_trial.h"
#include "test/peer_scenario/peer_scenario.h"
#include "test/rtp_header_parser.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {

class FrameObserver : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  FrameObserver() : frame_observed_(false) {}
  void OnFrame(const VideoFrame&) override { frame_observed_ = true; }

  std::atomic<bool> frame_observed_;
};

uint32_t get_ssrc(SessionDescriptionInterface* offer, size_t track_index) {
  EXPECT_LT(track_index, offer->description()->contents().size());
  return offer->description()
      ->contents()[track_index]
      .media_description()
      ->streams()[0]
      .ssrcs[0];
}

void set_ssrc(SessionDescriptionInterface* offer, size_t index, uint32_t ssrc) {
  EXPECT_LT(index, offer->description()->contents().size());
  cricket::StreamParams& new_stream_params = offer->description()
                                                 ->contents()[index]
                                                 .media_description()
                                                 ->mutable_streams()[0];
  new_stream_params.ssrcs[0] = ssrc;
  new_stream_params.ssrc_groups[0].ssrcs[0] = ssrc;
}

}  // namespace

TEST(UnsignaledStreamTest, ReplacesUnsignaledStreamOnCompletedSignaling) {
  // This test covers a scenario that might occur if a remote client starts
  // sending media packets before negotiation has completed. These packets will
  // trigger an unsignalled default stream to be created, and connects that to
  // a default video sink.
  // In some edge cases using unified plan, the default stream is create in a
  // different transceiver to where the media SSRC will actually be used.
  // This test verifies that the default stream is removed properly, and that
  // packets are demuxed and video frames reach the desired sink.

  // Defined before PeerScenario so it gets destructed after, to avoid use after
  // free.
  PeerScenario s(*test_info_);

  PeerScenarioClient::Config config = PeerScenarioClient::Config();
  // Disable encryption so that we can inject a fake early media packet without
  // triggering srtp failures.
  config.disable_encryption = true;
  auto* caller = s.CreateClient(config);
  auto* callee = s.CreateClient(config);

  auto send_node = s.net()->NodeBuilder().Build().node;
  auto ret_node = s.net()->NodeBuilder().Build().node;

  s.net()->CreateRoute(caller->endpoint(), {send_node}, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {ret_node}, caller->endpoint());

  auto signaling = s.ConnectSignaling(caller, callee, {send_node}, {ret_node});
  PeerScenarioClient::VideoSendTrackConfig video_conf;
  video_conf.generator.squares_video->framerate = 15;

  auto first_track = caller->CreateVideo("VIDEO", video_conf);
  FrameObserver first_sink;
  callee->AddVideoReceiveSink(first_track.track->id(), &first_sink);

  signaling.StartIceSignaling();
  std::atomic<bool> offer_exchange_done(false);
  std::atomic<bool> got_unsignaled_packet(false);

  // We will capture the media ssrc of the first added stream, and preemptively
  // inject a new media packet using a different ssrc.
  // This will create "default stream" for the second ssrc and connected it to
  // the default video sink (not set in this test).
  uint32_t first_ssrc = 0;
  uint32_t second_ssrc = 0;

  signaling.NegotiateSdp(
      /* munge_sdp = */ {},
      /* modify_sdp = */
      [&](SessionDescriptionInterface* offer) {
        first_ssrc = get_ssrc(offer, 0);
        second_ssrc = first_ssrc + 1;

        send_node->router()->SetWatcher([&](const EmulatedIpPacket& packet) {
          if (packet.size() > 1 && packet.cdata()[0] >> 6 == 2 &&
              !RtpHeaderParser::IsRtcp(packet.data.cdata(),
                                       packet.data.size())) {
            if (ByteReader<uint32_t>::ReadBigEndian(&(packet.cdata()[8])) ==
                    first_ssrc &&
                !got_unsignaled_packet) {
              rtc::CopyOnWriteBuffer updated_buffer = packet.data;
              ByteWriter<uint32_t>::WriteBigEndian(
                  updated_buffer.MutableData() + 8, second_ssrc);
              EmulatedIpPacket updated_packet(
                  packet.from, packet.to, updated_buffer, packet.arrival_time);
              send_node->OnPacketReceived(std::move(updated_packet));
              got_unsignaled_packet = true;
            }
          }
        });
      },
      [&](const SessionDescriptionInterface& answer) {
        EXPECT_EQ(answer.description()->contents().size(), 1u);
        offer_exchange_done = true;
      });
  EXPECT_TRUE(s.WaitAndProcess(&offer_exchange_done));
  EXPECT_TRUE(s.WaitAndProcess(&got_unsignaled_packet));
  EXPECT_TRUE(s.WaitAndProcess(&first_sink.frame_observed_));

  auto second_track = caller->CreateVideo("VIDEO2", video_conf);
  FrameObserver second_sink;
  callee->AddVideoReceiveSink(second_track.track->id(), &second_sink);

  // Create a second video stream, munge the sdp to force it to use our fake
  // early media ssrc.
  offer_exchange_done = false;
  signaling.NegotiateSdp(
      /* munge_sdp = */
      [&](SessionDescriptionInterface* offer) {
        set_ssrc(offer, 1, second_ssrc);
      },
      /* modify_sdp = */ {},
      [&](const SessionDescriptionInterface& answer) {
        EXPECT_EQ(answer.description()->contents().size(), 2u);
        offer_exchange_done = true;
      });
  EXPECT_TRUE(s.WaitAndProcess(&offer_exchange_done));
  EXPECT_TRUE(s.WaitAndProcess(&second_sink.frame_observed_));
  caller->pc()->Close();
  callee->pc()->Close();
}

}  // namespace test
}  // namespace webrtc
