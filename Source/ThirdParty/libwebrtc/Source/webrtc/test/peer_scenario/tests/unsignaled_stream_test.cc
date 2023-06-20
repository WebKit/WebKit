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
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "pc/media_session.h"
#include "pc/session_description.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/peer_scenario/peer_scenario.h"

namespace webrtc {
namespace test {
namespace {

enum class MidTestConfiguration {
  // Legacy endpoint setup where PT demuxing is used.
  kMidNotNegotiated,
  // MID is negotiated but missing from packets. PT demuxing is disabled, so
  // SSRCs have to be added to the SDP for WebRTC to forward packets correctly.
  // Happens when client is spec compliant but the SFU isn't. Popular legacy.
  kMidNegotiatedButMissingFromPackets,
  // Fully spec-compliant: MID is present so we can safely drop packets with
  // unknown MIDs.
  kMidNegotiatedAndPresentInPackets,
};

// Gives the parameterized test a readable suffix.
std::string TestParametersMidTestConfigurationToString(
    testing::TestParamInfo<MidTestConfiguration> info) {
  switch (info.param) {
    case MidTestConfiguration::kMidNotNegotiated:
      return "MidNotNegotiated";
    case MidTestConfiguration::kMidNegotiatedButMissingFromPackets:
      return "MidNegotiatedButMissingFromPackets";
    case MidTestConfiguration::kMidNegotiatedAndPresentInPackets:
      return "MidNegotiatedAndPresentInPackets";
  }
}

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

class UnsignaledStreamTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<MidTestConfiguration> {};

TEST_P(UnsignaledStreamTest, ReplacesUnsignaledStreamOnCompletedSignaling) {
  // This test covers a scenario that might occur if a remote client starts
  // sending media packets before negotiation has completed. Depending on setup,
  // these packets either get dropped or trigger an unsignalled default stream
  // to be created, and connects that to a default video sink.
  // In some edge cases using Unified Plan and PT demuxing, the default stream
  // is create in a different transceiver to where the media SSRC will actually
  // be used. This test verifies that the default stream is removed properly,
  // and that packets are demuxed and video frames reach the desired sink.
  const MidTestConfiguration kMidTestConfiguration = GetParam();

  // Defined before PeerScenario so it gets destructed after, to avoid use after
  // free.
  PeerScenario s(*::testing::UnitTest::GetInstance()->current_test_info());

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
  // inject a new media packet using a different ssrc. What happens depends on
  // the test configuration.
  //
  // MidTestConfiguration::kMidNotNegotiated:
  // - MID is not negotiated which means PT-based demuxing is enabled. Because
  //   the packets have no MID, the second ssrc packet gets forwarded to the
  //   first m= section. This will create a "default stream" for the second ssrc
  //   and connect it to the default video sink (not set in this test). The test
  //   verifies we can recover from this when we later get packets for the first
  //   ssrc.
  //
  // MidTestConfiguration::kMidNegotiatedButMissingFromPackets:
  // - MID is negotiated wich means PT-based demuxing is disabled. Because we
  //   modify the packets not to contain the MID anyway (simulating a legacy SFU
  //   that does not negotiate properly) unknown SSRCs are dropped but do not
  //   otherwise cause any issues.
  //
  // MidTestConfiguration::kMidNegotiatedAndPresentInPackets:
  // - MID is negotiated which means PT-based demuxing is enabled. In this case
  //   the packets have the MID so they either get forwarded or dropped
  //   depending on if the MID is known. The spec-compliant way is also the most
  //   straight-forward one.

  uint32_t first_ssrc = 0;
  uint32_t second_ssrc = 0;
  absl::optional<int> mid_header_extension_id = absl::nullopt;

  signaling.NegotiateSdp(
      /* munge_sdp = */
      [&](SessionDescriptionInterface* offer) {
        // Obtain the MID header extension ID and if we want the
        // MidTestConfiguration::kMidNotNegotiated setup then we remove the MID
        // header extension through SDP munging (otherwise SDP is not modified).
        for (cricket::ContentInfo& content_info :
             offer->description()->contents()) {
          std::vector<RtpExtension> header_extensions =
              content_info.media_description()->rtp_header_extensions();
          for (auto it = header_extensions.begin();
               it != header_extensions.end(); ++it) {
            if (it->uri == RtpExtension::kMidUri) {
              // MID header extension found!
              mid_header_extension_id = it->id;
              if (kMidTestConfiguration ==
                  MidTestConfiguration::kMidNotNegotiated) {
                // Munge away the extension.
                header_extensions.erase(it);
              }
              break;
            }
          }
          content_info.media_description()->set_rtp_header_extensions(
              std::move(header_extensions));
        }
        ASSERT_TRUE(mid_header_extension_id.has_value());
      },
      /* modify_sdp = */
      [&](SessionDescriptionInterface* offer) {
        first_ssrc = get_ssrc(offer, 0);
        second_ssrc = first_ssrc + 1;

        send_node->router()->SetWatcher([&](const EmulatedIpPacket& packet) {
          if (IsRtpPacket(packet.data) &&
              ByteReader<uint32_t>::ReadBigEndian(&(packet.cdata()[8])) ==
                  first_ssrc &&
              !got_unsignaled_packet) {
            // Parse packet and modify the SSRC to simulate a second m=
            // section that has not been negotiated yet.
            std::vector<RtpExtension> extensions;
            extensions.emplace_back(RtpExtension::kMidUri,
                                    mid_header_extension_id.value());
            RtpHeaderExtensionMap extensions_map(extensions);
            RtpPacket parsed_packet;
            parsed_packet.IdentifyExtensions(extensions_map);
            ASSERT_TRUE(parsed_packet.Parse(packet.data));
            parsed_packet.SetSsrc(second_ssrc);
            // The MID extension is present if and only if it was negotiated.
            // If present, we either want to remove it or modify it depending
            // on setup.
            switch (kMidTestConfiguration) {
              case MidTestConfiguration::kMidNotNegotiated:
                EXPECT_FALSE(parsed_packet.HasExtension<RtpMid>());
                break;
              case MidTestConfiguration::kMidNegotiatedButMissingFromPackets:
                EXPECT_TRUE(parsed_packet.HasExtension<RtpMid>());
                ASSERT_TRUE(parsed_packet.RemoveExtension(RtpMid::kId));
                break;
              case MidTestConfiguration::kMidNegotiatedAndPresentInPackets:
                EXPECT_TRUE(parsed_packet.HasExtension<RtpMid>());
                // The simulated second m= section would have a different MID.
                // If we don't modify it here then `second_ssrc` would end up
                // being mapped to the first m= section which would cause SSRC
                // conflicts if we later add the same SSRC to a second m=
                // section. Hidden assumption: first m= section does not use
                // MID:1.
                ASSERT_TRUE(parsed_packet.SetExtension<RtpMid>("1"));
                break;
            }
            // Inject the modified packet.
            rtc::CopyOnWriteBuffer updated_buffer = parsed_packet.Buffer();
            EmulatedIpPacket updated_packet(
                packet.from, packet.to, updated_buffer, packet.arrival_time);
            send_node->OnPacketReceived(std::move(updated_packet));
            got_unsignaled_packet = true;
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

INSTANTIATE_TEST_SUITE_P(
    All,
    UnsignaledStreamTest,
    ::testing::Values(MidTestConfiguration::kMidNotNegotiated,
                      MidTestConfiguration::kMidNegotiatedButMissingFromPackets,
                      MidTestConfiguration::kMidNegotiatedAndPresentInPackets),
    TestParametersMidTestConfigurationToString);

}  // namespace test
}  // namespace webrtc
