/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/jsep.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_transceiver_direction.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "media/base/stream_params.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/session_description.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/integration_test_helpers.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/metrics.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"

// This file contains unit tests that relate to the behavior of the
// SdpOfferAnswer module.
// Tests are written as integration tests with PeerConnection, since the
// behaviors are still linked so closely that it is hard to test them in
// isolation.

namespace webrtc {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::NotNull;
using ::testing::Pair;
using ::testing::SizeIs;

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;

namespace {

std::unique_ptr<Thread> CreateAndStartThread() {
  auto thread = Thread::Create();
  thread->Start();
  return thread;
}

}  // namespace

class SdpOfferAnswerTest : public ::testing::Test {
 public:
  SdpOfferAnswerTest()
      // Note: We use a PeerConnectionFactory with a distinct
      // signaling thread, so that thread handling can be tested.
      : signaling_thread_(CreateAndStartThread()),
        pc_factory_(CreatePeerConnectionFactory(
            nullptr,
            nullptr,
            signaling_thread_.get(),
            FakeAudioCaptureModule::Create(),
            CreateBuiltinAudioEncoderFactory(),
            CreateBuiltinAudioDecoderFactory(),
            std::make_unique<
                VideoEncoderFactoryTemplate<LibvpxVp8EncoderTemplateAdapter,
                                            LibvpxVp9EncoderTemplateAdapter,
                                            OpenH264EncoderTemplateAdapter,
                                            LibaomAv1EncoderTemplateAdapter>>(),
            std::make_unique<
                VideoDecoderFactoryTemplate<LibvpxVp8DecoderTemplateAdapter,
                                            LibvpxVp9DecoderTemplateAdapter,
                                            OpenH264DecoderTemplateAdapter,
                                            Dav1dDecoderTemplateAdapter>>(),
            nullptr /* audio_mixer */,
            nullptr /* audio_processing */,
            nullptr /* audio_frame_processor */)) {
    metrics::Reset();
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnection(
      absl::string_view field_trials = "") {
    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    return CreatePeerConnection(config, field_trials);
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnection(
      const RTCConfiguration& config,
      absl::string_view field_trials) {
    auto observer = std::make_unique<MockPeerConnectionObserver>();
    PeerConnectionDependencies pc_deps(observer.get());
    pc_deps.trials = CreateTestFieldTrialsPtr(field_trials);
    auto result =
        pc_factory_->CreatePeerConnectionOrError(config, std::move(pc_deps));
    EXPECT_TRUE(result.ok());
    observer->SetPeerConnectionInterface(result.value().get());
    return std::make_unique<PeerConnectionWrapper>(
        pc_factory_, result.MoveValue(), std::move(observer));
  }

  std::optional<RtpCodecCapability> FindFirstSendCodecWithName(
      MediaType media_type,
      const std::string& name) const {
    std::vector<RtpCodecCapability> codecs =
        pc_factory_->GetRtpSenderCapabilities(media_type).codecs;
    for (const auto& codec : codecs) {
      if (absl::EqualsIgnoreCase(codec.name, name)) {
        return codec;
      }
    }
    return std::nullopt;
  }

 protected:
  std::unique_ptr<Thread> signaling_thread_;
  scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;

 private:
  AutoThread main_thread_;
};

TEST_F(SdpOfferAnswerTest, OnTrackReturnsProxiedObject) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto audio_transceiver = caller->AddTransceiver(MediaType::AUDIO);

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  // Verify that caller->observer->OnTrack() has been called with a
  // proxied transceiver object.
  ASSERT_EQ(callee->observer()->on_track_transceivers_.size(), 1u);
  auto transceiver = callee->observer()->on_track_transceivers_[0];
  // Since the signaling thread is not the current thread,
  // this will DCHECK if the transceiver is not proxied.
  transceiver->stopped();
}

TEST_F(SdpOfferAnswerTest, BundleRejectsCodecCollisionsAudioVideo) {
  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0 1\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:0\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:1\r\n"
      "a=rtpmap:111 H264/90000\r\n"
      "a=fmtp:111 "
      "level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
      "42e01f\r\n";

  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  ASSERT_NE(desc, nullptr);
  RTCError error;
  pc->SetRemoteDescription(std::move(desc), &error);
  // There is no error yet but the metrics counter will increase.
  EXPECT_TRUE(error.ok());

  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.PeerConnection.ValidBundledPayloadTypes",
                            false));

  // Tolerate codec collisions in rejected m-lines.
  pc = CreatePeerConnection();
  auto rejected_offer = CreateSessionDescription(
      SdpType::kOffer,
      absl::StrReplaceAll(sdp, {{"m=video 9 ", "m=video 0 "}}));
  pc->SetRemoteDescription(std::move(rejected_offer), &error);
  EXPECT_TRUE(error.ok());
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.PeerConnection.ValidBundledPayloadTypes",
                            true));
}

TEST_F(SdpOfferAnswerTest, BundleRejectsCodecCollisionsVideoFmtp) {
  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0 1\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:0\r\n"
      "a=rtpmap:111 H264/90000\r\n"
      "a=fmtp:111 "
      "level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
      "42e01f\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:1\r\n"
      "a=rtpmap:111 H264/90000\r\n"
      "a=fmtp:111 "
      "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
      "42e01f\r\n";

  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  ASSERT_NE(desc, nullptr);
  RTCError error;
  pc->SetRemoteDescription(std::move(desc), &error);
  EXPECT_TRUE(error.ok());
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.PeerConnection.ValidBundledPayloadTypes",
                            false));
}

TEST_F(SdpOfferAnswerTest, BundleCodecCollisionInDifferentBundlesAllowed) {
  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0\r\n"
      "a=group:BUNDLE 1\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:0\r\n"
      "a=rtpmap:111 H264/90000\r\n"
      "a=fmtp:111 "
      "level-asymmetry-allowed=1;packetization-mode=0;profile-level-id="
      "42e01f\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:1\r\n"
      "a=rtpmap:111 H264/90000\r\n"
      "a=fmtp:111 "
      "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
      "42e01f\r\n";

  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  ASSERT_NE(desc, nullptr);
  RTCError error;
  pc->SetRemoteDescription(std::move(desc), &error);
  EXPECT_TRUE(error.ok());
  EXPECT_METRIC_EQ(
      0, metrics::NumEvents("WebRTC.PeerConnection.ValidBundledPayloadTypes",
                            false));
}

TEST_F(SdpOfferAnswerTest, BundleMeasuresHeaderExtensionIdCollision) {
  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0 1\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:0\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      "a=extmap:3 "
      "http://www.ietf.org/id/"
      "draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 112\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:1\r\n"
      "a=rtpmap:112 VP8/90000\r\n"
      "a=extmap:3 "
      "http://www.ietf.org/id/"
      "draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n";
  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  ASSERT_NE(desc, nullptr);
  RTCError error;
  pc->SetRemoteDescription(std::move(desc), &error);
  EXPECT_TRUE(error.ok());
}

// extmap:3 is used with two different URIs which is not allowed.
TEST_F(SdpOfferAnswerTest, BundleRejectsHeaderExtensionIdCollision) {
  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0 1\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:0\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      "a=extmap:3 "
      "http://www.ietf.org/id/"
      "draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 112\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:1\r\n"
      "a=rtpmap:112 VP8/90000\r\n"
      "a=extmap:3 urn:3gpp:video-orientation\r\n";
  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  ASSERT_NE(desc, nullptr);
  RTCError error;
  pc->SetRemoteDescription(std::move(desc), &error);
  EXPECT_FALSE(error.ok());
  EXPECT_EQ(error.type(), RTCErrorType::INVALID_PARAMETER);
}

// transport-wide cc is negotiated with two different ids 3 and 4.
// This is not a good idea but tolerable.
TEST_F(SdpOfferAnswerTest, BundleAcceptsDifferentIdsForSameExtension) {
  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0 1\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:0\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      "a=extmap:3 "
      "http://www.ietf.org/id/"
      "draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 112\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:1\r\n"
      "a=rtpmap:112 VP8/90000\r\n"
      "a=extmap:4 "
      "http://www.ietf.org/id/"
      "draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n";
  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  ASSERT_NE(desc, nullptr);
  RTCError error;
  pc->SetRemoteDescription(std::move(desc), &error);
  EXPECT_TRUE(error.ok());
}

TEST_F(SdpOfferAnswerTest, LargeMidsAreRejected) {
  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=rtpmap:111 VP8/90000\r\n"
      "a=mid:01234567890123456\r\n";
  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  ASSERT_NE(desc, nullptr);
  RTCError error;
  pc->SetRemoteDescription(std::move(desc), &error);
  EXPECT_FALSE(error.ok());
  EXPECT_EQ(error.type(), RTCErrorType::INVALID_PARAMETER);
}

TEST_F(SdpOfferAnswerTest, RollbackPreservesAddTrackMid) {
  std::string sdp =
      "v=0\r\n"
      "o=- 4131505339648218884 3 IN IP4 **-----**\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=ice-lite\r\n"
      "a=msid-semantic: WMS 100030878598094:4Qs1PjbLM32RK5u3\r\n"
      "a=ice-ufrag:zGWFZ+fVXDeN6UoI/136\r\n"
      "a=ice-pwd:9AUNgUqRNI5LSIrC1qFD2iTR\r\n"
      "a=fingerprint:sha-256 "
      "AD:52:52:E0:B1:37:34:21:0E:15:8E:B7:56:56:7B:B4:39:0E:6D:1C:F5:84:A7:EE:"
      "B5:27:3E:30:B1:7D:69:42\r\n"
      "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n"
      "a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid\r\n"
      "a=group:BUNDLE 0 1\r\n"
      "m=audio 40005 UDP/TLS/RTP/SAVPF 111\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      "a=fmtp:111 "
      "maxaveragebitrate=20000;maxplaybackrate=16000;minptime=10;usedtx=1;"
      "useinbandfec=1;stereo=0\r\n"
      "a=rtcp-fb:111 nack\r\n"
      "a=setup:passive\r\n"
      "a=mid:0\r\n"
      "a=msid:- 75156ebd-e705-4da1-920e-2dac39794dfd\r\n"
      "a=ptime:60\r\n"
      "a=recvonly\r\n"
      "a=rtcp-mux\r\n"
      "m=audio 40005 UDP/TLS/RTP/SAVPF 111\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      "a=fmtp:111 "
      "maxaveragebitrate=20000;maxplaybackrate=16000;minptime=10;usedtx=1;"
      "useinbandfec=1;stereo=0\r\n"
      "a=rtcp-fb:111 nack\r\n"
      "a=setup:passive\r\n"
      "a=mid:1\r\n"
      "a=msid:100030878598094:4Qs1PjbLM32RK5u3 9695447562408476674\r\n"
      "a=ptime:60\r\n"
      "a=sendonly\r\n"
      "a=ssrc:2565730539 cname:100030878598094:4Qs1PjbLM32RK5u3\r\n"
      "a=rtcp-mux\r\n";
  auto pc = CreatePeerConnection();
  auto audio_track = pc->AddAudioTrack("audio_track", {});
  auto first_transceiver = pc->pc()->GetTransceivers()[0];
  EXPECT_FALSE(first_transceiver->mid().has_value());
  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  ASSERT_NE(desc, nullptr);
  RTCError error;
  ASSERT_TRUE(pc->SetRemoteDescription(std::move(desc)));
  pc->CreateAnswerAndSetAsLocal();
  auto saved_mid = first_transceiver->mid();
  EXPECT_TRUE(saved_mid.has_value());
  auto offer_before_rollback = pc->CreateOfferAndSetAsLocal();
  EXPECT_EQ(saved_mid, first_transceiver->mid());
  auto rollback = pc->CreateRollback();
  ASSERT_NE(rollback, nullptr);
  ASSERT_TRUE(pc->SetLocalDescription(std::move(rollback)));
  EXPECT_EQ(saved_mid, first_transceiver->mid());
  auto offer2 = pc->CreateOfferAndSetAsLocal();
  ASSERT_NE(offer2, nullptr);
  EXPECT_EQ(saved_mid, first_transceiver->mid());
}

#ifdef WEBRTC_HAVE_SCTP
TEST_F(SdpOfferAnswerTest, RejectedDataChannelsDoNotGetReoffered) {
  auto pc = CreatePeerConnection();
  EXPECT_TRUE(pc->pc()->CreateDataChannelOrError("dc", nullptr).ok());
  EXPECT_TRUE(pc->CreateOfferAndSetAsLocal());
  auto mid = pc->pc()->local_description()->description()->contents()[0].mid();

  // An answer that rejects the datachannel content.
  std::string sdp =
      "v=0\r\n"
      "o=- 4131505339648218884 3 IN IP4 **-----**\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=ice-ufrag:zGWFZ+fVXDeN6UoI/136\r\n"
      "a=ice-pwd:9AUNgUqRNI5LSIrC1qFD2iTR\r\n"
      "a=fingerprint:sha-256 "
      "AD:52:52:E0:B1:37:34:21:0E:15:8E:B7:56:56:7B:B4:39:0E:6D:1C:F5:84:A7:EE:"
      "B5:27:3E:30:B1:7D:69:42\r\n"
      "a=setup:passive\r\n"
      "m=application 0 UDP/DTLS/SCTP webrtc-datachannel\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=sctp-port:5000\r\n"
      "a=max-message-size:262144\r\n"
      "a=mid:" +
      mid + "\r\n";
  std::unique_ptr<SessionDescriptionInterface> answer =
      CreateSessionDescription(SdpType::kAnswer, sdp);
  ASSERT_TRUE(pc->SetRemoteDescription(std::move(answer)));
  // The subsequent offer should not recycle the m-line since the existing data
  // channel is closed.
  std::unique_ptr<SessionDescriptionInterface> offer = pc->CreateOffer();
  const auto& offer_contents = offer->description()->contents();
  ASSERT_EQ(offer_contents.size(), 1u);
  EXPECT_EQ(offer_contents[0].mid(), mid);
  EXPECT_EQ(offer_contents[0].rejected, true);
}

TEST_F(SdpOfferAnswerTest, RejectedDataChannelsDoGetReofferedWhenActive) {
  auto pc = CreatePeerConnection();
  EXPECT_TRUE(pc->pc()->CreateDataChannelOrError("dc", nullptr).ok());
  EXPECT_TRUE(pc->CreateOfferAndSetAsLocal());
  auto mid = pc->pc()->local_description()->description()->contents()[0].mid();

  // An answer that rejects the datachannel content.
  std::string sdp =
      "v=0\r\n"
      "o=- 4131505339648218884 3 IN IP4 **-----**\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=ice-ufrag:zGWFZ+fVXDeN6UoI/136\r\n"
      "a=ice-pwd:9AUNgUqRNI5LSIrC1qFD2iTR\r\n"
      "a=fingerprint:sha-256 "
      "AD:52:52:E0:B1:37:34:21:0E:15:8E:B7:56:56:7B:B4:39:0E:6D:1C:F5:84:A7:EE:"
      "B5:27:3E:30:B1:7D:69:42\r\n"
      "a=setup:passive\r\n"
      "m=application 0 UDP/DTLS/SCTP webrtc-datachannel\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=sctp-port:5000\r\n"
      "a=max-message-size:262144\r\n"
      "a=mid:" +
      mid + "\r\n";
  std::unique_ptr<SessionDescriptionInterface> answer =
      CreateSessionDescription(SdpType::kAnswer, sdp);
  ASSERT_TRUE(pc->SetRemoteDescription(std::move(answer)));

  // The subsequent offer should recycle the m-line when there is a new data
  // channel.
  EXPECT_TRUE(pc->pc()->CreateDataChannelOrError("dc2", nullptr).ok());
  EXPECT_TRUE(pc->pc()->ShouldFireNegotiationNeededEvent(
      pc->observer()->latest_negotiation_needed_event()));

  std::unique_ptr<SessionDescriptionInterface> offer = pc->CreateOffer();
  const auto& offer_contents = offer->description()->contents();
  ASSERT_EQ(offer_contents.size(), 1u);
  EXPECT_EQ(offer_contents[0].mid(), mid);
  EXPECT_EQ(offer_contents[0].rejected, false);
}

TEST_F(SdpOfferAnswerTest, AlwaysNegotiateDataChannels) {
  RTCConfiguration config;
  config.always_negotiate_data_channels = true;
  auto caller = CreatePeerConnection(config, /*field_trials=*/"");

  // No data channels are created.
  auto video_transceiver = caller->AddTransceiver(MediaType::VIDEO);
  auto offer = caller->CreateOffer();
  ASSERT_THAT(offer, NotNull());

  auto& contents = offer->description()->contents();
  ASSERT_THAT(contents, SizeIs(2));
  // SCTP is negotiated first.
  EXPECT_EQ(MediaProtocolType::kSctp, contents[0].type);
  EXPECT_EQ(MediaProtocolType::kRtp, contents[1].type);
}

TEST_F(SdpOfferAnswerTest, AlwaysNegotiateDataChannelsNegotiationNeeded) {
  RTCConfiguration config;
  config.always_negotiate_data_channels = true;
  auto caller = CreatePeerConnection(config, /*field_trials=*/"");
  auto callee = CreatePeerConnection();

  // ONN should not fire.
  EXPECT_FALSE(caller->observer()->has_negotiation_needed_event());

  // No data channels are created.
  auto video_transceiver = caller->AddTransceiver(MediaType::VIDEO);
  EXPECT_TRUE(caller->pc()->ShouldFireNegotiationNeededEvent(
      caller->observer()->latest_negotiation_needed_event()));
  auto offer = caller->CreateOfferAndSetAsLocal();
  ASSERT_THAT(offer, NotNull());

  auto& contents = offer->description()->contents();
  ASSERT_THAT(contents, SizeIs(2));
  // SCTP is negotiated first.
  EXPECT_EQ(MediaProtocolType::kSctp, contents[0].type);
  EXPECT_EQ(MediaProtocolType::kRtp, contents[1].type);

  // Negotiate to clear ONN.
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  auto answer = callee->CreateAnswerAndSetAsLocal();
  ASSERT_THAT(answer, NotNull());
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));

  // Create a datachannel.
  EXPECT_TRUE(caller->pc()->CreateDataChannelOrError("first_dc", nullptr).ok());
  EXPECT_FALSE(caller->pc()->ShouldFireNegotiationNeededEvent(
      caller->observer()->latest_negotiation_needed_event()));
}
#endif  // WEBRTC_HAVE_SCTP

TEST_F(SdpOfferAnswerTest, SimulcastAnswerWithNoRidsIsRejected) {
  auto pc = CreatePeerConnection();

  RtpTransceiverInit init;
  RtpEncodingParameters rid1;
  rid1.rid = "1";
  init.send_encodings.push_back(rid1);
  RtpEncodingParameters rid2;
  rid2.rid = "2";
  init.send_encodings.push_back(rid2);

  auto transceiver = pc->AddTransceiver(MediaType::VIDEO, init);
  EXPECT_TRUE(pc->CreateOfferAndSetAsLocal());
  auto mid = pc->pc()->local_description()->description()->contents()[0].mid();

  // A SDP answer with simulcast but without mid/rid extensions.
  std::string sdp =
      "v=0\r\n"
      "o=- 4131505339648218884 3 IN IP4 **-----**\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=ice-ufrag:zGWFZ+fVXDeN6UoI/136\r\n"
      "a=ice-pwd:9AUNgUqRNI5LSIrC1qFD2iTR\r\n"
      "a=fingerprint:sha-256 "
      "AD:52:52:E0:B1:37:34:21:0E:15:8E:B7:56:56:7B:B4:39:0E:6D:1C:F5:84:A7:EE:"
      "B5:27:3E:30:B1:7D:69:42\r\n"
      "a=setup:passive\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp:9 IN IP4 0.0.0.0\r\n"
      "a=mid:" +
      mid +
      "\r\n"
      "a=recvonly\r\n"
      "a=rtcp-mux\r\n"
      "a=rtcp-rsize\r\n"
      "a=rtpmap:96 VP8/90000\r\n"
      "a=rid:1 recv\r\n"
      "a=rid:2 recv\r\n"
      "a=simulcast:recv 1;2\r\n";
  std::string extensions =
      "a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid\r\n"
      "a=extmap:10 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id\r\n";
  std::unique_ptr<SessionDescriptionInterface> answer =
      CreateSessionDescription(SdpType::kAnswer, sdp);
  EXPECT_FALSE(pc->SetRemoteDescription(std::move(answer)));

  auto answer_with_extensions =
      CreateSessionDescription(SdpType::kAnswer, sdp + extensions);
  EXPECT_TRUE(pc->SetRemoteDescription(std::move(answer_with_extensions)));

  // Tolerate the lack of mid/rid extensions in rejected m-lines.
  EXPECT_TRUE(pc->CreateOfferAndSetAsLocal());
  auto rejected_answer = CreateSessionDescription(
      SdpType::kAnswer,
      absl::StrReplaceAll(sdp, {{"m=video 9 ", "m=video 0 "}}));
  EXPECT_TRUE(pc->SetRemoteDescription(std::move(rejected_answer)));
}

TEST_F(SdpOfferAnswerTest, SimulcastOfferWithMixedCodec) {
  auto pc = CreatePeerConnection("WebRTC-MixedCodecSimulcast/Enabled/");

  std::optional<RtpCodecCapability> vp8_codec_capability =
      FindFirstSendCodecWithName(MediaType::VIDEO, kVp8CodecName);
  ASSERT_TRUE(vp8_codec_capability);
  std::optional<RtpCodecCapability> vp9_codec_capability =
      FindFirstSendCodecWithName(MediaType::VIDEO, kVp9CodecName);
  ASSERT_TRUE(vp9_codec_capability);

  RtpTransceiverInit init;
  RtpEncodingParameters rid1;
  rid1.rid = "1";
  rid1.codec = *vp8_codec_capability;
  init.send_encodings.push_back(rid1);
  RtpEncodingParameters rid2;
  rid2.rid = "2";
  rid2.codec = *vp9_codec_capability;
  init.send_encodings.push_back(rid2);

  auto transceiver = pc->AddTransceiver(MediaType::VIDEO, init);
  std::unique_ptr<SessionDescriptionInterface> offer = pc->CreateOffer();
  auto& offer_contents = offer->description()->contents();
  auto send_codecs = offer_contents[0].media_description()->codecs();
  // Verify that the serialized SDP includes pt=.
  std::string sdp;
  offer->ToString(&sdp);
  const Codec* vp8_send_codec = nullptr;
  const Codec* vp9_send_codec = nullptr;
  for (auto& codec : send_codecs) {
    if (codec.name == vp8_codec_capability->name && !vp8_send_codec) {
      vp8_send_codec = &codec;
    }
    if (codec.name == vp9_codec_capability->name && !vp9_send_codec) {
      vp9_send_codec = &codec;
    }
  }
  ASSERT_TRUE(vp8_send_codec);
  ASSERT_TRUE(vp9_send_codec);
  EXPECT_THAT(sdp, testing::HasSubstr("a=rid:1 send pt=" +
                                      std::to_string(vp8_send_codec->id)));
  EXPECT_THAT(sdp, testing::HasSubstr("a=rid:2 send pt=" +
                                      std::to_string(vp9_send_codec->id)));
  // Verify that SDP containing pt= can be parsed correctly.
  auto offer2 = CreateSessionDescription(SdpType::kOffer, sdp);
  auto& offer_contents2 = offer2->description()->contents();
  auto send_rids2 = offer_contents2[0].media_description()->streams()[0].rids();
  EXPECT_EQ(send_rids2[0].codecs.size(), 1u);
  EXPECT_EQ(send_rids2[0].codecs[0], *vp8_send_codec);
  EXPECT_EQ(send_rids2[1].codecs.size(), 1u);
  EXPECT_EQ(send_rids2[1].codecs[0], *vp9_send_codec);
}

TEST_F(SdpOfferAnswerTest, SimulcastAnswerWithPayloadType) {
  auto pc = CreatePeerConnection("WebRTC-MixedCodecSimulcast/Enabled/");

  // A SDP offer with recv simulcast with payload type
  std::string sdp =
      "v=0\r\n"
      "o=- 4131505339648218884 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=ice-ufrag:zGWFZ+fVXDeN6UoI/136\r\n"
      "a=ice-pwd:9AUNgUqRNI5LSIrC1qFD2iTR\r\n"
      "a=fingerprint:sha-256 "
      "AD:52:52:E0:B1:37:34:21:0E:15:8E:B7:56:56:7B:B4:39:0E:6D:1C:F5:84:A7:EE:"
      "B5:27:3E:30:B1:7D:69:42\r\n"
      "a=setup:passive\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96 97\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp:9 IN IP4 0.0.0.0\r\n"
      "a=mid:0\r\n"
      "a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid\r\n"
      "a=extmap:10 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id\r\n"
      "a=recvonly\r\n"
      "a=rtcp-mux\r\n"
      "a=rtcp-rsize\r\n"
      "a=rtpmap:96 VP8/90000\r\n"
      "a=rtpmap:97 VP9/90000\r\n"
      "a=rid:1 recv pt=96\r\n"
      "a=rid:2 recv pt=97\r\n"
      "a=simulcast:recv 1;2\r\n";

  std::unique_ptr<SessionDescriptionInterface> offer =
      CreateSessionDescription(SdpType::kOffer, sdp);
  EXPECT_TRUE(pc->SetRemoteDescription(std::move(offer)));

  auto transceiver = pc->pc()->GetTransceivers()[0];
  EXPECT_TRUE(
      transceiver->SetDirectionWithError(RtpTransceiverDirection::kSendOnly)
          .ok());

  // Check the generated SDP.
  std::unique_ptr<SessionDescriptionInterface> answer = pc->CreateAnswer();
  answer->ToString(&sdp);
  EXPECT_THAT(sdp, testing::HasSubstr("a=rid:1 send pt=96\r\n"));
  EXPECT_THAT(sdp, testing::HasSubstr("a=rid:2 send pt=97\r\n"));

  EXPECT_TRUE(pc->SetLocalDescription(std::move(answer)));
}

TEST_F(SdpOfferAnswerTest, ExpectAllSsrcsSpecifiedInSsrcGroupFid) {
  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96 97\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:0\r\n"
      "a=rtpmap:96 H264/90000\r\n"
      "a=fmtp:96 "
      "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
      "42e01f\r\n"
      "a=rtpmap:97 rtx/90000\r\n"
      "a=fmtp:97 apt=96\r\n"
      "a=ssrc-group:FID 1 2\r\n"
      "a=ssrc:1 cname:test\r\n";
  std::unique_ptr<SessionDescriptionInterface> offer =
      CreateSessionDescription(SdpType::kOffer, sdp);
  RTCError error;
  pc->SetRemoteDescription(std::move(offer), &error);
  EXPECT_FALSE(error.ok());
  EXPECT_EQ(error.type(), RTCErrorType::INVALID_PARAMETER);
}

TEST_F(SdpOfferAnswerTest, ExpectAllSsrcsSpecifiedInSsrcGroupFecFr) {
  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96 98\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:0\r\n"
      "a=rtpmap:96 H264/90000\r\n"
      "a=fmtp:96 "
      "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
      "42e01f\r\n"
      "a=rtpmap:98 flexfec-03/90000\r\n"
      "a=fmtp:98 repair-window=10000000\r\n"
      "a=ssrc-group:FEC-FR 1 2\r\n"
      "a=ssrc:1 cname:test\r\n";
  std::unique_ptr<SessionDescriptionInterface> offer =
      CreateSessionDescription(SdpType::kOffer, sdp);
  RTCError error;
  pc->SetRemoteDescription(std::move(offer), &error);
  EXPECT_FALSE(error.ok());
  EXPECT_EQ(error.type(), RTCErrorType::INVALID_PARAMETER);
}

TEST_F(SdpOfferAnswerTest, ExpectTwoSsrcsInSsrcGroupFid) {
  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96 97\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:0\r\n"
      "a=rtpmap:96 H264/90000\r\n"
      "a=fmtp:96 "
      "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
      "42e01f\r\n"
      "a=rtpmap:97 rtx/90000\r\n"
      "a=fmtp:97 apt=96\r\n"
      "a=ssrc-group:FID 1 2 3\r\n"
      "a=ssrc:1 cname:test\r\n"
      "a=ssrc:2 cname:test\r\n"
      "a=ssrc:3 cname:test\r\n";
  std::unique_ptr<SessionDescriptionInterface> offer =
      CreateSessionDescription(SdpType::kOffer, sdp);
  RTCError error;
  pc->SetRemoteDescription(std::move(offer), &error);
  EXPECT_FALSE(error.ok());
  EXPECT_EQ(error.type(), RTCErrorType::INVALID_PARAMETER);
}

TEST_F(SdpOfferAnswerTest, ExpectTwoSsrcsInSsrcGroupFecFr) {
  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96 98\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:0\r\n"
      "a=rtpmap:96 H264/90000\r\n"
      "a=fmtp:96 "
      "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
      "42e01f\r\n"
      "a=rtpmap:98 flexfec-03/90000\r\n"
      "a=fmtp:98 repair-window=10000000\r\n"
      "a=ssrc-group:FEC-FR 1 2 3\r\n"
      "a=ssrc:1 cname:test\r\n"
      "a=ssrc:2 cname:test\r\n"
      "a=ssrc:3 cname:test\r\n";
  std::unique_ptr<SessionDescriptionInterface> offer =
      CreateSessionDescription(SdpType::kOffer, sdp);
  RTCError error;
  pc->SetRemoteDescription(std::move(offer), &error);
  EXPECT_FALSE(error.ok());
  EXPECT_EQ(error.type(), RTCErrorType::INVALID_PARAMETER);
}

TEST_F(SdpOfferAnswerTest, ExpectAtMostFourSsrcsInSsrcGroupSIM) {
  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96 97\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:0\r\n"
      "a=rtpmap:96 H264/90000\r\n"
      "a=fmtp:96 "
      "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
      "42e01f\r\n"
      "a=rtpmap:97 rtx/90000\r\n"
      "a=fmtp:97 apt=96\r\n"
      "a=ssrc-group:SIM 1 2 3 4\r\n"
      "a=ssrc:1 cname:test\r\n"
      "a=ssrc:2 cname:test\r\n"
      "a=ssrc:3 cname:test\r\n"
      "a=ssrc:4 cname:test\r\n";
  std::unique_ptr<SessionDescriptionInterface> offer =
      CreateSessionDescription(SdpType::kOffer, sdp);
  RTCError error;
  pc->SetRemoteDescription(std::move(offer), &error);
  EXPECT_FALSE(error.ok());
  EXPECT_EQ(error.type(), RTCErrorType::INVALID_PARAMETER);
}

TEST_F(SdpOfferAnswerTest, DuplicateSsrcsDisallowedInLocalDescription) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});
  pc->AddVideoTrack("video_track", {});
  std::unique_ptr<SessionDescriptionInterface> offer = pc->CreateOffer();
  auto& offer_contents = offer->description()->contents();
  ASSERT_EQ(offer_contents.size(), 2u);
  uint32_t second_ssrc = offer_contents[1].media_description()->first_ssrc();

  offer->description()
      ->contents()[0]
      .media_description()
      ->mutable_streams()[0]
      .ssrcs[0] = second_ssrc;
  EXPECT_FALSE(pc->SetLocalDescription(std::move(offer)));
}

TEST_F(SdpOfferAnswerTest,
       DuplicateSsrcsAcrossMlinesDisallowedInLocalDescriptionTwoSsrc) {
  auto pc = CreatePeerConnection();

  pc->AddAudioTrack("audio_track", {});
  pc->AddVideoTrack("video_track", {});
  std::unique_ptr<SessionDescriptionInterface> offer = pc->CreateOffer();
  auto& offer_contents = offer->description()->contents();
  ASSERT_EQ(offer_contents.size(), 2u);
  uint32_t audio_ssrc = offer_contents[0].media_description()->first_ssrc();
  ASSERT_EQ(offer_contents[1].media_description()->streams().size(), 1u);
  auto& video_stream = offer->description()
                           ->contents()[1]
                           .media_description()
                           ->mutable_streams()[0];
  ASSERT_EQ(video_stream.ssrcs.size(), 2u);
  ASSERT_EQ(video_stream.ssrc_groups.size(), 1u);
  video_stream.ssrcs[1] = audio_ssrc;
  video_stream.ssrc_groups[0].ssrcs[1] = audio_ssrc;
  video_stream.ssrc_groups[0].semantics = kSimSsrcGroupSemantics;
  std::string sdp;
  offer->ToString(&sdp);

  // Trim the last two lines which contain ssrc-specific attributes
  // that we change/munge above. Guarded with expectation about what
  // should be removed in case the SDP generation changes.
  size_t end = sdp.rfind("\r\n");
  end = sdp.rfind("\r\n", end - 2);
  end = sdp.rfind("\r\n", end - 2);
  EXPECT_EQ(sdp.substr(end + 2), "a=ssrc:" + absl::StrCat(audio_ssrc) +
                                     " cname:" + video_stream.cname +
                                     "\r\n"
                                     "a=ssrc:" +
                                     absl::StrCat(audio_ssrc) +
                                     " msid:- video_track\r\n");

  auto modified_offer =
      CreateSessionDescription(SdpType::kOffer, sdp.substr(0, end + 2));
  EXPECT_FALSE(pc->SetLocalDescription(std::move(modified_offer)));
}

TEST_F(SdpOfferAnswerTest,
       DuplicateSsrcsAcrossMlinesDisallowedInLocalDescriptionThreeSsrcs) {
  auto pc = CreatePeerConnection();

  pc->AddAudioTrack("audio_track", {});
  pc->AddVideoTrack("video_track", {});
  std::unique_ptr<SessionDescriptionInterface> offer = pc->CreateOffer();
  auto& offer_contents = offer->description()->contents();
  ASSERT_EQ(offer_contents.size(), 2u);
  uint32_t audio_ssrc = offer_contents[0].media_description()->first_ssrc();
  ASSERT_EQ(offer_contents[1].media_description()->streams().size(), 1u);
  auto& video_stream = offer->description()
                           ->contents()[1]
                           .media_description()
                           ->mutable_streams()[0];
  ASSERT_EQ(video_stream.ssrcs.size(), 2u);
  ASSERT_EQ(video_stream.ssrc_groups.size(), 1u);
  video_stream.ssrcs.push_back(audio_ssrc);
  video_stream.ssrc_groups[0].ssrcs.push_back(audio_ssrc);
  video_stream.ssrc_groups[0].semantics = kSimSsrcGroupSemantics;
  std::string sdp;
  offer->ToString(&sdp);

  // Trim the last two lines which contain ssrc-specific attributes
  // that we change/munge above. Guarded with expectation about what
  // should be removed in case the SDP generation changes.
  size_t end = sdp.rfind("\r\n");
  end = sdp.rfind("\r\n", end - 2);
  end = sdp.rfind("\r\n", end - 2);
  EXPECT_EQ(sdp.substr(end + 2), "a=ssrc:" + absl::StrCat(audio_ssrc) +
                                     " cname:" + video_stream.cname +
                                     "\r\n"
                                     "a=ssrc:" +
                                     absl::StrCat(audio_ssrc) +
                                     " msid:- video_track\r\n");

  auto modified_offer =
      CreateSessionDescription(SdpType::kOffer, sdp.substr(0, end + 2));
  EXPECT_FALSE(pc->SetLocalDescription(std::move(modified_offer)));
}

TEST_F(SdpOfferAnswerTest, AllowOnlyOneSsrcGroupPerSemanticAndPrimarySsrc) {
  auto pc = CreatePeerConnection();

  pc->AddAudioTrack("audio_track", {});
  pc->AddVideoTrack("video_track", {});
  std::unique_ptr<SessionDescriptionInterface> offer = pc->CreateOffer();
  auto& offer_contents = offer->description()->contents();
  ASSERT_EQ(offer_contents.size(), 2u);
  uint32_t audio_ssrc = offer_contents[0].media_description()->first_ssrc();
  ASSERT_EQ(offer_contents[1].media_description()->streams().size(), 1u);
  auto& video_stream = offer->description()
                           ->contents()[1]
                           .media_description()
                           ->mutable_streams()[0];
  ASSERT_EQ(video_stream.ssrcs.size(), 2u);
  ASSERT_EQ(video_stream.ssrc_groups.size(), 1u);
  video_stream.ssrcs.push_back(audio_ssrc);
  video_stream.ssrc_groups.push_back(
      {kFidSsrcGroupSemantics, {video_stream.ssrcs[0], audio_ssrc}});
  std::string sdp;
  offer->ToString(&sdp);

  // Trim the last two lines which contain ssrc-specific attributes
  // that we change/munge above. Guarded with expectation about what
  // should be removed in case the SDP generation changes.
  size_t end = sdp.rfind("\r\n");
  end = sdp.rfind("\r\n", end - 2);
  end = sdp.rfind("\r\n", end - 2);
  EXPECT_EQ(sdp.substr(end + 2), "a=ssrc:" + absl::StrCat(audio_ssrc) +
                                     " cname:" + video_stream.cname +
                                     "\r\n"
                                     "a=ssrc:" +
                                     absl::StrCat(audio_ssrc) +
                                     " msid:- video_track\r\n");

  auto modified_offer =
      CreateSessionDescription(SdpType::kOffer, sdp.substr(0, end + 2));
  EXPECT_FALSE(pc->SetLocalDescription(std::move(modified_offer)));
}

TEST_F(SdpOfferAnswerTest, OfferWithRtxAndNoMsidIsNotRejected) {
  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96 97\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp-mux\r\n"
      "a=sendonly\r\n"
      "a=mid:0\r\n"
      // "a=msid:stream obsoletetrack\r\n"
      "a=rtpmap:96 VP8/90000\r\n"
      "a=rtpmap:97 rtx/90000\r\n"
      "a=fmtp:97 apt=96\r\n"
      "a=ssrc-group:FID 1 2\r\n"
      "a=ssrc:1 cname:test\r\n"
      "a=ssrc:2 cname:test\r\n";
  std::unique_ptr<SessionDescriptionInterface> offer =
      CreateSessionDescription(SdpType::kOffer, sdp);
  EXPECT_TRUE(pc->SetRemoteDescription(std::move(offer)));
}

TEST_F(SdpOfferAnswerTest, RejectsAnswerWithInvalidTransport) {
  auto pc1 = CreatePeerConnection();
  pc1->AddAudioTrack("audio_track", {});
  auto pc2 = CreatePeerConnection();
  pc2->AddAudioTrack("anotheraudio_track", {});

  auto initial_offer = pc1->CreateOfferAndSetAsLocal();
  ASSERT_EQ(initial_offer->description()->contents().size(), 1u);
  auto mid = initial_offer->description()->contents()[0].mid();

  EXPECT_TRUE(pc2->SetRemoteDescription(std::move(initial_offer)));
  auto initial_answer = pc2->CreateAnswerAndSetAsLocal();

  std::string sdp;
  initial_answer->ToString(&sdp);
  EXPECT_TRUE(pc1->SetRemoteDescription(std::move(initial_answer)));

  auto transceivers = pc1->pc()->GetTransceivers();
  ASSERT_EQ(transceivers.size(), 1u);
  // This stops the only transport.
  transceivers[0]->StopStandard();

  auto subsequent_offer = pc1->CreateOfferAndSetAsLocal();
  // But the remote answers with a non-rejected m-line which is not valid.
  auto bad_answer = CreateSessionDescription(
      SdpType::kAnswer,
      absl::StrReplaceAll(sdp, {{"a=group:BUNDLE " + mid + "\r\n", ""}}));

  RTCError error;
  pc1->SetRemoteDescription(std::move(bad_answer), &error);
  EXPECT_FALSE(error.ok());
  EXPECT_EQ(error.type(), RTCErrorType::INVALID_PARAMETER);
}

TEST_F(SdpOfferAnswerTest, SdpMungingWithInvalidPayloadTypeIsRejected) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  std::unique_ptr<SessionDescriptionInterface> offer = pc->CreateOffer();
  ASSERT_EQ(offer->description()->contents().size(), 1u);
  auto* audio = offer->description()->contents()[0].media_description();
  ASSERT_GT(audio->codecs().size(), 0u);
  EXPECT_TRUE(audio->rtcp_mux());
  auto codecs = audio->codecs();
  for (int invalid_payload_type = 64; invalid_payload_type < 96;
       invalid_payload_type++) {
    codecs[0].id =
        invalid_payload_type;  // The range [64-95] is disallowed with rtcp_mux.
    audio->set_codecs(codecs);
    // ASSERT to avoid getting into a bad state.
    ASSERT_FALSE(pc->SetLocalDescription(offer->Clone()));
    ASSERT_FALSE(pc->SetRemoteDescription(offer->Clone()));
  }
}

TEST_F(SdpOfferAnswerTest, MsidSignalingInSubsequentOfferAnswer) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {});

  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=msid-semantic: WMS\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp:9 IN IP4 0.0.0.0\r\n"
      "a=recvonly\r\n"
      "a=rtcp-mux\r\n"
      "a=rtpmap:111 opus/48000/2\r\n";

  std::unique_ptr<SessionDescriptionInterface> offer =
      CreateSessionDescription(SdpType::kOffer, sdp);
  EXPECT_TRUE(pc->SetRemoteDescription(std::move(offer)));

  // Check the generated SDP.
  std::unique_ptr<SessionDescriptionInterface> answer = pc->CreateAnswer();
  answer->ToString(&sdp);
  EXPECT_NE(std::string::npos, sdp.find("a=msid:- audio_track\r\n"));

  EXPECT_TRUE(pc->SetLocalDescription(std::move(answer)));

  // Check the local description object.
  auto local_description = pc->pc()->local_description();
  ASSERT_EQ(local_description->description()->contents().size(), 1u);
  auto streams = local_description->description()
                     ->contents()[0]
                     .media_description()
                     ->streams();
  ASSERT_EQ(streams.size(), 1u);
  EXPECT_EQ(streams[0].id, "audio_track");

  // Check the serialization of the local description.
  local_description->ToString(&sdp);
  EXPECT_NE(std::string::npos, sdp.find("a=msid:- audio_track\r\n"));
}

// Regression test for crbug.com/328522463
// where the stream parameters got recreated which changed the ssrc.
TEST_F(SdpOfferAnswerTest, MsidSignalingUnknownRespondsWithMsidAndKeepsSsrc) {
  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("audio_track", {"default"});
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0\r\n"
      // "a=msid-semantic: WMS *\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp:9 IN IP4 0.0.0.0\r\n"
      "a=recvonly\r\n"
      "a=rtcp-mux\r\n"
      "a=mid:0\r\n"
      "a=rtpmap:111 opus/48000/2\r\n";

  std::unique_ptr<SessionDescriptionInterface> offer =
      CreateSessionDescription(SdpType::kOffer, sdp);
  EXPECT_TRUE(pc->SetRemoteDescription(std::move(offer)));
  auto first_transceiver = pc->pc()->GetTransceivers()[0];
  EXPECT_TRUE(first_transceiver
                  ->SetDirectionWithError(RtpTransceiverDirection::kSendOnly)
                  .ok());
  // Check the generated *serialized* SDP.
  std::unique_ptr<SessionDescriptionInterface> answer = pc->CreateAnswer();
  const auto& answer_contents = answer->description()->contents();
  ASSERT_EQ(answer_contents.size(), 1u);
  auto answer_streams = answer_contents[0].media_description()->streams();
  ASSERT_EQ(answer_streams.size(), 1u);
  std::string first_stream_serialized = answer_streams[0].ToString();
  uint32_t first_ssrc = answer_contents[0].media_description()->first_ssrc();

  answer->ToString(&sdp);
  EXPECT_TRUE(
      pc->SetLocalDescription(CreateSessionDescription(SdpType::kAnswer, sdp)));

  auto reoffer = pc->CreateOffer();
  const auto& offer_contents = reoffer->description()->contents();
  ASSERT_EQ(offer_contents.size(), 1u);

  auto offer_streams = offer_contents[0].media_description()->streams();
  ASSERT_EQ(offer_streams.size(), 1u);
  std::string second_stream_serialized = offer_streams[0].ToString();
  uint32_t second_ssrc = offer_contents[0].media_description()->first_ssrc();

  EXPECT_EQ(first_ssrc, second_ssrc);
  EXPECT_EQ(first_stream_serialized, second_stream_serialized);
  EXPECT_TRUE(pc->SetLocalDescription(std::move(reoffer)));
}

// Runs for each payload type in the valid dynamic ranges.
class SdpOfferAnswerWithPayloadTypeTest
    : public SdpOfferAnswerTest,
      public testing::WithParamInterface<int> {
 public:
  static std::vector<int> GetAllPayloadTypesInValidDynamicRange() {
    std::vector<int> payload_types;
    // The lower range is [35, 63].
    for (int pt = 35; pt <= 63; ++pt) {
      payload_types.push_back(pt);
    }
    // The upper range is [96, 127].
    for (int pt = 96; pt <= 127; ++pt) {
      payload_types.push_back(pt);
    }
    return payload_types;
  }
};

TEST_P(SdpOfferAnswerWithPayloadTypeTest,
       FollowUpOfferDoesNotRepurposePayloadType) {
  int payload_type = GetParam();
  std::string payload_type_str = absl::StrCat(payload_type);

  auto pc = CreatePeerConnection();
  std::string sdp =
      "v=0\r\n"
      "o=- 8506393630701383055 2 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0\r\n"
      "a=extmap-allow-mixed\r\n"
      "a=msid-semantic: WMS\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF " +
      payload_type_str +
      "\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=rtcp:9 IN IP4 0.0.0.0\r\n"
      "a=ice-ufrag:7ZPs\r\n"
      "a=ice-pwd:3/ZaqZrZaVzg1Tfju5x3CGeJ\r\n"
      "a=ice-options:trickle\r\n"
      "a=fingerprint:sha-256 7D:29:C5:B8:D2:30:57:F3:0D:CA:0A:8E:4B:6A:AE:53:26"
      ":9F:14:DF:47:8E:0C:A3:EC:8D:B1:71:B5:D5:5A:9C\r\n"
      "a=setup:actpass\r\n"
      "a=mid:0\r\n"
      "a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid\r\n"
      "a=sendrecv\r\n"
      "a=msid:- e2628265-b712-40de-81c9-76d49b7079a0\r\n"
      "a=rtcp-mux\r\n"
      "a=rtcp-rsize\r\n"
      "a=rtpmap:" +
      payload_type_str +
      " VP9/90000\r\n"
      "a=rtcp-fb:" +
      payload_type_str +
      " goog-remb\r\n"
      "a=rtcp-fb:" +
      payload_type_str +
      " transport-cc\r\n"
      "a=rtcp-fb:" +
      payload_type_str +
      " ccm fir\r\n"
      "a=rtcp-fb:" +
      payload_type_str +
      " nack\r\n"
      "a=rtcp-fb:" +
      payload_type_str +
      " nack pli\r\n"
      "a=fmtp:" +
      payload_type_str +
      " profile-id=0\r\n"
      "a=ssrc:2245042191 cname:A206VC6FXsn47EwJ\r\n"
      "a=ssrc:2245042191 msid:- e2628265-b712-40de-81c9-76d49b7079a0\r\n";

  // Set remote offer with given PT for VP9.
  EXPECT_TRUE(
      pc->SetRemoteDescription(CreateSessionDescription(SdpType::kOffer, sdp)));
  // The answer should accept the PT for VP9.
  std::unique_ptr<SessionDescriptionInterface> answer = pc->CreateAnswer();
  {
    const auto* mid_0 = answer->description()->GetContentDescriptionByName("0");
    ASSERT_TRUE(mid_0);
    ASSERT_THAT(mid_0->codecs(), SizeIs(1));
    const auto& codec = mid_0->codecs()[0];
    EXPECT_EQ(codec.name, "VP9");
    EXPECT_EQ(codec.id, payload_type);
    std::string param;
    EXPECT_TRUE(codec.GetParam("profile-id", &param));
    EXPECT_EQ(param, "0");
  }

  EXPECT_TRUE(pc->SetLocalDescription(std::move(answer)));
  // The follow-up offer should continue to use the same PT for VP9.
  std::unique_ptr<SessionDescriptionInterface> offer = pc->CreateOffer();
  {
    const auto* mid_0 = offer->description()->GetContentDescriptionByName("0");
    ASSERT_TRUE(mid_0);
    // We should have more codecs to offer than the one previously negotiated.
    const auto& codecs = mid_0->codecs();
    ASSERT_GT(codecs.size(), 1u);
    // The previously negotiated PT should still map to the same VP9 codec.
    auto it = std::find_if(
        codecs.begin(), codecs.end(),
        [&](const Codec& codec) { return codec.id == payload_type; });
    ASSERT_TRUE(it != codecs.end());
    const auto& vp9_codec = *it;
    EXPECT_EQ(vp9_codec.name, "VP9");
    EXPECT_EQ(vp9_codec.id, payload_type);
    std::string param;
    EXPECT_TRUE(vp9_codec.GetParam("profile-id", &param));
    EXPECT_EQ(param, "0");
    // None of the other codecs should collide with our VP9 PT.
    for (const auto& codec : codecs) {
      if (codec == vp9_codec) {
        continue;
      }
      EXPECT_NE(codec.id, vp9_codec.id);
    }
  }
  // Last sanity check: it's always possible to set an unmunged local offer.
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer)));
}

INSTANTIATE_TEST_SUITE_P(
    SdpOfferAnswerWithPayloadTypeTest,
    SdpOfferAnswerWithPayloadTypeTest,
    ::testing::ValuesIn(SdpOfferAnswerWithPayloadTypeTest::
                            GetAllPayloadTypesInValidDynamicRange()),
    ::testing::PrintToStringParamName());

// Test variant with boolean order for audio-video and video-audio.
class SdpOfferAnswerShuffleMediaTypes
    : public SdpOfferAnswerTest,
      public testing::WithParamInterface<bool> {
 public:
  SdpOfferAnswerShuffleMediaTypes() : SdpOfferAnswerTest() {}
};

TEST_P(SdpOfferAnswerShuffleMediaTypes,
       RecyclingWithDifferentKindAndSameMidFailsAnswer) {
  bool audio_first = GetParam();
  auto pc1 = CreatePeerConnection();
  auto pc2 = CreatePeerConnection();
  if (audio_first) {
    pc1->AddAudioTrack("audio_track", {});
    pc2->AddVideoTrack("video_track", {});
  } else {
    pc2->AddAudioTrack("audio_track", {});
    pc1->AddVideoTrack("video_track", {});
  }

  auto initial_offer = pc1->CreateOfferAndSetAsLocal();
  ASSERT_EQ(initial_offer->description()->contents().size(), 1u);
  auto mid1 = initial_offer->description()->contents()[0].mid();
  std::string rejected_answer_sdp =
      "v=0\r\n"
      "o=- 8621259572628890423 2 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "m=" +
      std::string(audio_first ? "audio" : "video") +
      " 0 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n";
  auto rejected_answer =
      CreateSessionDescription(SdpType::kAnswer, rejected_answer_sdp);
  EXPECT_TRUE(pc1->SetRemoteDescription(std::move(rejected_answer)));

  std::unique_ptr<SessionDescriptionInterface> offer =
      pc2->CreateOfferAndSetAsLocal();  // This will generate a mid=0 too
  ASSERT_EQ(offer->description()->contents().size(), 1u);
  auto mid2 = offer->description()->contents()[0].mid();
  EXPECT_EQ(mid1, mid2);  // Check that the mids collided.
  EXPECT_TRUE(pc1->SetRemoteDescription(std::move(offer)));
  std::unique_ptr<SessionDescriptionInterface> answer = pc1->CreateAnswer();
  EXPECT_FALSE(pc1->SetLocalDescription(std::move(answer)));
}

// Similar to the previous test but with implicit rollback and creating
// an offer, triggering a different codepath.
TEST_P(SdpOfferAnswerShuffleMediaTypes,
       RecyclingWithDifferentKindAndSameMidFailsOffer) {
  bool audio_first = GetParam();
  auto pc1 = CreatePeerConnection();
  auto pc2 = CreatePeerConnection();
  if (audio_first) {
    pc1->AddAudioTrack("audio_track", {});
    pc2->AddVideoTrack("video_track", {});
  } else {
    pc2->AddAudioTrack("audio_track", {});
    pc1->AddVideoTrack("video_track", {});
  }

  auto initial_offer = pc1->CreateOfferAndSetAsLocal();
  ASSERT_EQ(initial_offer->description()->contents().size(), 1u);
  auto mid1 = initial_offer->description()->contents()[0].mid();
  std::string rejected_answer_sdp =
      "v=0\r\n"
      "o=- 8621259572628890423 2 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "m=" +
      std::string(audio_first ? "audio" : "video") +
      " 0 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n";
  auto rejected_answer =
      CreateSessionDescription(SdpType::kAnswer, rejected_answer_sdp);
  EXPECT_TRUE(pc1->SetRemoteDescription(std::move(rejected_answer)));

  std::unique_ptr<SessionDescriptionInterface> offer =
      pc2->CreateOfferAndSetAsLocal();  // This will generate a mid=0 too
  ASSERT_EQ(offer->description()->contents().size(), 1u);
  auto mid2 = offer->description()->contents()[0].mid();
  EXPECT_EQ(mid1, mid2);  // Check that the mids collided.
  EXPECT_TRUE(pc1->SetRemoteDescription(std::move(offer)));
  EXPECT_FALSE(pc1->CreateOffer());
}

INSTANTIATE_TEST_SUITE_P(SdpOfferAnswerShuffleMediaTypes,
                         SdpOfferAnswerShuffleMediaTypes,
                         ::testing::Values(true, false));

TEST_F(SdpOfferAnswerTest, OfferWithNoCompatibleCodecsIsRejectedInAnswer) {
  auto pc = CreatePeerConnection();
  // An offer with no common codecs. This should reject both contents
  // in the answer without throwing an error.
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=audio 9 RTP/SAVPF 97\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=sendrecv\r\n"
      "a=rtpmap:97 x-unknown/90000\r\n"
      "a=rtcp-mux\r\n"
      "m=video 9 RTP/SAVPF 98\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=sendrecv\r\n"
      "a=rtpmap:98 H263-1998/90000\r\n"
      "a=fmtp:98 CIF=1;QCIF=1\r\n"
      "a=rtcp-mux\r\n";

  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  ASSERT_NE(desc, nullptr);
  RTCError error;
  pc->SetRemoteDescription(std::move(desc), &error);
  EXPECT_TRUE(error.ok());

  std::unique_ptr<SessionDescriptionInterface> answer = pc->CreateAnswer();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(answer_contents.size(), 2u);
  EXPECT_EQ(answer_contents[0].rejected, true);
  EXPECT_EQ(answer_contents[1].rejected, true);
}

TEST_F(SdpOfferAnswerTest, OfferWithRejectedMlineWithoutFingerprintIsAccepted) {
  auto pc = CreatePeerConnection();
  // A rejected m-line without fingerprint.
  // The answer does not require one.
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "m=audio 0 RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=sendrecv\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      "a=rtcp-mux\r\n";
  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  ASSERT_NE(desc, nullptr);
  RTCError error;
  pc->SetRemoteDescription(std::move(desc), &error);
  EXPECT_TRUE(error.ok());

  std::unique_ptr<SessionDescriptionInterface> answer = pc->CreateAnswer();
  EXPECT_TRUE(pc->SetLocalDescription(std::move(answer)));
}

TEST_F(SdpOfferAnswerTest, MidBackfillAnswer) {
  auto pc = CreatePeerConnection();
  // An offer without a mid backfills the mid. This is currently
  // done with a per-peerconnection counter that starts from 0.
  // JSEP says to only include the mid in the answer if it was in the offer
  // but due to backfill it is always present.
  // TODO: https://issues.webrtc.org/issues/338529222 - don't respond with mid.
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "m=audio 9 RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=sendrecv\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      // "a=mid:0\r\n"
      "a=rtcp-mux\r\n";
  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  ASSERT_NE(desc, nullptr);
  RTCError error;
  pc->SetRemoteDescription(std::move(desc), &error);
  EXPECT_TRUE(error.ok());
  auto offer_contents =
      pc->pc()->remote_description()->description()->contents();
  ASSERT_EQ(offer_contents.size(), 1u);
  EXPECT_EQ(offer_contents[0].mid(), "0");
  std::unique_ptr<SessionDescriptionInterface> answer =
      pc->CreateAnswerAndSetAsLocal();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(answer_contents.size(), 1u);
  EXPECT_EQ(answer_contents[0].mid(), offer_contents[0].mid());
}

TEST_F(SdpOfferAnswerTest, MidBackfillDoesNotCheckAgainstBundleGroup) {
  auto pc = CreatePeerConnection();
  // An offer with a BUNDLE group specifying a mid that is not present
  // in the offer. This is not rejected due to the mid being backfilled
  // starting at 0.
  // TODO: https://issues.webrtc.org/issues/338528603 - reject this.
  std::string sdp =
      "v=0\r\n"
      "o=- 0 3 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=setup:actpass\r\n"
      "a=ice-ufrag:ETEn\r\n"
      "a=ice-pwd:OtSK0WpNtpUjkY4+86js7Z/l\r\n"
      "a=fingerprint:sha-1 "
      "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB\r\n"
      "a=setup:actpass\r\n"
      "a=group:BUNDLE 0\r\n"
      "m=audio 9 RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=sendrecv\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      // "a=mid:0\r\n"
      "a=rtcp-mux\r\n";
  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  ASSERT_NE(desc, nullptr);
  RTCError error;
  pc->SetRemoteDescription(std::move(desc), &error);
  EXPECT_TRUE(error.ok());
  EXPECT_TRUE(pc->CreateAnswerAndSetAsLocal());
}

TEST_F(SdpOfferAnswerTest, ReducedSizeNegotiated) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto audio_transceiver = caller->AddTransceiver(MediaType::AUDIO);
  auto video_transceiver = caller->AddTransceiver(MediaType::VIDEO);

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  auto receivers = callee->pc()->GetReceivers();
  ASSERT_EQ(receivers.size(), 2u);
  auto audio_recv_param = receivers[0]->GetParameters();
  EXPECT_TRUE(audio_recv_param.rtcp.reduced_size);
  auto video_recv_param = receivers[1]->GetParameters();
  EXPECT_TRUE(video_recv_param.rtcp.reduced_size);

  auto senders = caller->pc()->GetSenders();
  ASSERT_EQ(senders.size(), 2u);
  auto audio_send_param = senders[0]->GetParameters();
  EXPECT_TRUE(audio_send_param.rtcp.reduced_size);
  auto video_send_param = senders[1]->GetParameters();
  EXPECT_TRUE(video_send_param.rtcp.reduced_size);
}

TEST_F(SdpOfferAnswerTest, ReducedSizeNotNegotiated) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto audio_transceiver = caller->AddTransceiver(MediaType::AUDIO);
  auto video_transceiver = caller->AddTransceiver(MediaType::VIDEO);

  std::unique_ptr<SessionDescriptionInterface> offer =
      caller->CreateOfferAndSetAsLocal();
  ASSERT_THAT(offer, NotNull());
  std::string sdp;
  offer->ToString(&sdp);
  // Remove rtcp-rsize attribute.
  auto modified_offer = CreateSessionDescription(
      SdpType::kOffer, absl::StrReplaceAll(sdp, {{"a=rtcp-rsize\r\n", ""}}));
  EXPECT_TRUE(callee->SetRemoteDescription(std::move(modified_offer)));
  std::unique_ptr<SessionDescriptionInterface> answer =
      callee->CreateAnswerAndSetAsLocal();
  EXPECT_TRUE(caller->SetRemoteDescription(std::move(answer)));

  auto receivers = callee->pc()->GetReceivers();
  ASSERT_EQ(receivers.size(), 2u);
  auto audio_recv_param = receivers[0]->GetParameters();
  EXPECT_FALSE(audio_recv_param.rtcp.reduced_size);
  auto video_recv_param = receivers[1]->GetParameters();
  EXPECT_FALSE(video_recv_param.rtcp.reduced_size);

  auto senders = caller->pc()->GetSenders();
  ASSERT_EQ(senders.size(), 2u);
  auto audio_send_param = senders[0]->GetParameters();
  EXPECT_FALSE(audio_send_param.rtcp.reduced_size);
  auto video_send_param = senders[1]->GetParameters();
  EXPECT_FALSE(video_send_param.rtcp.reduced_size);
}

TEST_F(SdpOfferAnswerTest, PayloadTypeMatchingWithSubsequentOfferAnswer) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  // 1. Restrict codecs and set a local description and remote description.
  //    with a different payload type.
  auto video_transceiver = caller->AddTransceiver(MediaType::VIDEO);
  std::vector<RtpCodecCapability> codec_caps =
      pc_factory_->GetRtpReceiverCapabilities(MediaType::VIDEO).codecs;
  std::erase_if(codec_caps, [](const RtpCodecCapability& codec) {
    return !absl::EqualsIgnoreCase(codec.name, "VP8");
  });
  EXPECT_TRUE(video_transceiver->SetCodecPreferences(codec_caps).ok());

  auto offer1 = caller->CreateOfferAndSetAsLocal();

  // 2. Add additional supported but not offered codec before SRD
  auto& contents = offer1->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  std::vector<Codec> codecs = media_description->codecs();
  ASSERT_EQ(codecs.size(), 1u);
  ASSERT_NE(codecs[0].id, 127);
  auto av1 = CreateVideoCodec(SdpVideoFormat("AV1", {}));
  av1.id = 127;
  codecs.insert(codecs.begin(), av1);
  media_description->set_codecs(codecs);
  EXPECT_TRUE(callee->SetRemoteDescription(std::move(offer1)));

  auto answer1 = callee->CreateAnswerAndSetAsLocal();
  EXPECT_TRUE(caller->SetRemoteDescription(std::move(answer1)));

  // 3. sCP to reenable that codec. Payload type is not matched at this point.
  codec_caps = pc_factory_->GetRtpReceiverCapabilities(MediaType::VIDEO).codecs;
  std::erase_if(codec_caps, [](const RtpCodecCapability& codec) {
    return !(absl::EqualsIgnoreCase(codec.name, "VP8") ||
             absl::EqualsIgnoreCase(codec.name, "AV1"));
  });
  EXPECT_TRUE(video_transceiver->SetCodecPreferences(codec_caps).ok());
  auto offer2 = caller->CreateOffer();
  auto& contents2 = offer2->description()->contents();
  ASSERT_EQ(contents2.size(), 1u);
  auto* media_description2 = contents2[0].media_description();
  codecs = media_description2->codecs();
  ASSERT_EQ(codecs.size(), 2u);
  EXPECT_EQ(codecs[1].name, av1.name);
  // At this point, the value 127 may or may not have been chosen.

  // 4. O/A triggered by remote. This "locks in" the payload type.
  auto offer3 = callee->CreateOfferAndSetAsLocal();
  EXPECT_TRUE(caller->SetRemoteDescription(std::move(offer3)));
  EXPECT_TRUE(caller->CreateAnswerAndSetAsLocal());

  // 5. Subsequent offer has the payload type.
  auto offer4 = caller->CreateOfferAndSetAsLocal();
  auto& contents4 = offer4->description()->contents();
  ASSERT_EQ(contents4.size(), 1u);
  auto* media_description4 = contents4[0].media_description();
  ASSERT_TRUE(media_description4);
  codecs = media_description4->codecs();
  ASSERT_EQ(codecs.size(), 2u);
  EXPECT_EQ(codecs[1].name, av1.name);
  EXPECT_EQ(codecs[1].id, av1.id);
}

#ifdef WEBRTC_HAVE_SCTP
TEST_F(SdpOfferAnswerTest, SctpInitDisabled) {
  auto pc1 = CreatePeerConnection("WebRTC-Sctp-Snap/Disabled/");
  auto pc2 = CreatePeerConnection("WebRTC-Sctp-Snap/Disabled/");
  EXPECT_TRUE(pc1->pc()->CreateDataChannelOrError("dc", nullptr).ok());
  auto offer = pc1->CreateOfferAndSetAsLocal();
  ASSERT_NE(offer, nullptr);

  {
    auto& contents = offer->description()->contents();
    ASSERT_EQ(contents.size(), 1u);
    auto* media_description = contents[0].media_description();
    ASSERT_TRUE(media_description);
    auto* sctp_description = media_description->as_sctp();
    ASSERT_TRUE(sctp_description);
    EXPECT_FALSE(sctp_description->sctp_init());
  }

  RTCError error;
  EXPECT_TRUE(pc2->SetRemoteDescription(std::move(offer)));
  auto answer = pc2->CreateAnswerAndSetAsLocal();
  ASSERT_NE(answer, nullptr);

  {
    auto& contents = answer->description()->contents();
    ASSERT_EQ(contents.size(), 1u);
    auto* media_description = contents[0].media_description();
    ASSERT_TRUE(media_description);
    auto* sctp_description = media_description->as_sctp();
    ASSERT_TRUE(sctp_description);
    EXPECT_FALSE(sctp_description->sctp_init());
  }

  EXPECT_TRUE(pc1->SetRemoteDescription(std::move(answer)));
}

TEST_F(SdpOfferAnswerTest, SctpInitWithTrial) {
  auto pc1 = CreatePeerConnection("WebRTC-Sctp-Snap/Enabled/");
  auto pc2 = CreatePeerConnection("WebRTC-Sctp-Snap/Enabled/");
  EXPECT_TRUE(pc1->pc()->CreateDataChannelOrError("dc", nullptr).ok());
  auto offer = pc1->CreateOfferAndSetAsLocal();
  ASSERT_NE(offer, nullptr);

  {
    auto& contents = offer->description()->contents();
    ASSERT_EQ(contents.size(), 1u);
    auto* media_description = contents[0].media_description();
    ASSERT_TRUE(media_description);
    auto* sctp_description = media_description->as_sctp();
    ASSERT_TRUE(sctp_description);
    EXPECT_TRUE(sctp_description->sctp_init());
  }

  RTCError error;
  EXPECT_TRUE(pc2->SetRemoteDescription(std::move(offer)));
  auto answer = pc2->CreateAnswerAndSetAsLocal();
  ASSERT_NE(answer, nullptr);

  {
    auto& contents = answer->description()->contents();
    ASSERT_EQ(contents.size(), 1u);
    auto* media_description = contents[0].media_description();
    ASSERT_TRUE(media_description);
    auto* sctp_description = media_description->as_sctp();
    ASSERT_TRUE(sctp_description);
    EXPECT_TRUE(sctp_description->sctp_init());
  }

  EXPECT_TRUE(pc1->SetRemoteDescription(std::move(answer)));
}

TEST_F(SdpOfferAnswerTest, AnswerNoSctpInitInOffer) {
  auto pc = CreatePeerConnection("WebRTC-Sctp-Snap/Enabled/");

  std::string sdp =
      "v=0\r\n"
      "o=- 4131505339648218884 3 IN IP4 **-----**\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=ice-ufrag:zGWFZ+fVXDeN6UoI/136\r\n"
      "a=ice-pwd:9AUNgUqRNI5LSIrC1qFD2iTR\r\n"
      "a=fingerprint:sha-256 "
      "AD:52:52:E0:B1:37:34:21:0E:15:8E:B7:56:56:7B:B4:39:0E:6D:1C:F5:84:A7:EE:"
      "B5:27:3E:30:B1:7D:69:42\r\n"
      "a=setup:passive\r\n"
      "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=sctp-port:5000\r\n"
      "a=max-message-size:262144\r\n"
      // a=sctp-init:cookiemonster\r\n"  // no sctp-init present.
      "a=mid:0\r\n";
  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  ASSERT_NE(desc, nullptr);

  EXPECT_TRUE(pc->SetRemoteDescription(std::move(desc)));
  auto answer = pc->CreateAnswerAndSetAsLocal();
  ASSERT_NE(answer, nullptr);
  EXPECT_TRUE(answer->ToString(&sdp));

  auto& contents = answer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  auto* media_description = contents[0].media_description();
  ASSERT_TRUE(media_description);
  auto* sctp_description = media_description->as_sctp();
  ASSERT_TRUE(sctp_description);
  EXPECT_FALSE(sctp_description->sctp_init());
}

TEST_F(SdpOfferAnswerTest, AnswerNonBase64SctpInit) {
  std::string sdp =
      "v=0\r\n"
      "o=- 4131505339648218884 3 IN IP4 **-----**\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=ice-ufrag:zGWFZ+fVXDeN6UoI/136\r\n"
      "a=ice-pwd:9AUNgUqRNI5LSIrC1qFD2iTR\r\n"
      "a=fingerprint:sha-256 "
      "AD:52:52:E0:B1:37:34:21:0E:15:8E:B7:56:56:7B:B4:39:0E:6D:1C:F5:84:A7:EE:"
      "B5:27:3E:30:B1:7D:69:42\r\n"
      "a=setup:passive\r\n"
      "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=sctp-port:5000\r\n"
      "a=max-message-size:262144\r\n"
      "a=sctp-init:not valid base64\r\n"
      "a=mid:0\r\n";
  auto desc = CreateSessionDescription(SdpType::kOffer, sdp);
  EXPECT_EQ(desc, nullptr);
}
#endif  // WEBRTC_HAVE_SCTP

class SdpOfferAnswerDirectionTest
    : public SdpOfferAnswerTest,
      public testing::WithParamInterface<
          std::tuple<RtpTransceiverDirection, RtpTransceiverDirection, bool>> {
 public:
  SdpOfferAnswerDirectionTest() : SdpOfferAnswerTest() {}
};

TEST_P(SdpOfferAnswerDirectionTest, IncompatibleDirection) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(MediaType::VIDEO);
  EXPECT_TRUE(transceiver->SetDirectionWithError(std::get<0>(GetParam())).ok());

  auto offer = caller->CreateOfferAndSetAsLocal();
  EXPECT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  ASSERT_THAT(callee->pc()->GetTransceivers(), SizeIs(1));
  auto callee_transceiver = callee->pc()->GetTransceivers()[0];
  EXPECT_TRUE(callee_transceiver
                  ->SetDirectionWithError(RtpTransceiverDirection::kInactive)
                  .ok());
  auto answer = callee->CreateAnswerAndSetAsLocal();
  // Modify the answer.
  ASSERT_THAT(answer->description()->contents(), SizeIs(1));
  ContentInfo& content = answer->description()->contents()[0];
  EXPECT_EQ(content.media_description()->direction(),
            RtpTransceiverDirection::kInactive);
  content.media_description()->set_direction(std::get<1>(GetParam()));

  EXPECT_EQ(caller->SetRemoteDescription(std::move(answer)),
            std::get<2>(GetParam()));
}

TEST_P(SdpOfferAnswerDirectionTest, IncompatibleDirectionRejected) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(MediaType::VIDEO);
  EXPECT_TRUE(transceiver->SetDirectionWithError(std::get<0>(GetParam())).ok());

  auto offer = caller->CreateOfferAndSetAsLocal();
  EXPECT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  ASSERT_THAT(callee->pc()->GetTransceivers(), SizeIs(1));
  auto callee_transceiver = callee->pc()->GetTransceivers()[0];
  EXPECT_TRUE(callee_transceiver
                  ->SetDirectionWithError(RtpTransceiverDirection::kInactive)
                  .ok());
  auto answer = callee->CreateAnswerAndSetAsLocal();
  // Modify the answer and reject it. This can happen e.g. with
  // a rejected m-line that lacks a direction which defaults to "sendrecv".
  ASSERT_THAT(answer->description()->contents(), SizeIs(1));
  ContentInfo& content = answer->description()->contents()[0];
  EXPECT_EQ(content.media_description()->direction(),
            RtpTransceiverDirection::kInactive);
  content.media_description()->set_direction(std::get<1>(GetParam()));
  content.rejected = true;

  EXPECT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}

INSTANTIATE_TEST_SUITE_P(SdpOfferAnswerDirectionTest,
                         SdpOfferAnswerDirectionTest,
                         ::testing::Values(
                             // sendrecv.
                             std::make_tuple(RtpTransceiverDirection::kSendRecv,
                                             RtpTransceiverDirection::kSendRecv,
                                             true),
                             std::make_tuple(RtpTransceiverDirection::kSendRecv,
                                             RtpTransceiverDirection::kSendOnly,
                                             true),
                             std::make_tuple(RtpTransceiverDirection::kSendRecv,
                                             RtpTransceiverDirection::kRecvOnly,
                                             true),
                             std::make_tuple(RtpTransceiverDirection::kSendRecv,
                                             RtpTransceiverDirection::kInactive,
                                             true),
                             // sendonly.
                             std::make_tuple(RtpTransceiverDirection::kSendOnly,
                                             RtpTransceiverDirection::kSendRecv,
                                             false),
                             std::make_tuple(RtpTransceiverDirection::kSendOnly,
                                             RtpTransceiverDirection::kSendOnly,
                                             false),
                             std::make_tuple(RtpTransceiverDirection::kSendOnly,
                                             RtpTransceiverDirection::kRecvOnly,
                                             true),
                             std::make_tuple(RtpTransceiverDirection::kSendOnly,
                                             RtpTransceiverDirection::kInactive,
                                             true),
                             // recvonly.
                             std::make_tuple(RtpTransceiverDirection::kRecvOnly,
                                             RtpTransceiverDirection::kSendRecv,
                                             false),
                             std::make_tuple(RtpTransceiverDirection::kRecvOnly,
                                             RtpTransceiverDirection::kSendOnly,
                                             true),
                             std::make_tuple(RtpTransceiverDirection::kRecvOnly,
                                             RtpTransceiverDirection::kRecvOnly,
                                             false),
                             std::make_tuple(RtpTransceiverDirection::kRecvOnly,
                                             RtpTransceiverDirection::kInactive,
                                             true),
                             // inactive.
                             std::make_tuple(RtpTransceiverDirection::kInactive,
                                             RtpTransceiverDirection::kSendRecv,
                                             false),
                             std::make_tuple(RtpTransceiverDirection::kInactive,
                                             RtpTransceiverDirection::kSendOnly,
                                             false),
                             std::make_tuple(RtpTransceiverDirection::kInactive,
                                             RtpTransceiverDirection::kRecvOnly,
                                             false),
                             std::make_tuple(RtpTransceiverDirection::kInactive,
                                             RtpTransceiverDirection::kInactive,
                                             true)));

}  // namespace webrtc
