/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/base/gunit.h"
#include "webrtc/media/base/fakemediaengine.h"
#include "webrtc/ortc/ortcfactory.h"
#include "webrtc/ortc/testrtpparameters.h"
#include "webrtc/p2p/base/fakepackettransport.h"
#include "webrtc/pc/test/fakevideotracksource.h"

namespace webrtc {

// This test uses an individual RtpSender using only the public interface, and
// verifies that its behaves as designed at an API level. Also tests that
// parameters are applied to the audio/video engines as expected. Network and
// media interfaces are faked to isolate what's being tested.
//
// This test shouldn't result any any actual media being sent. That sort of
// test should go in ortcfactory_integrationtest.cc.
class OrtcRtpSenderTest : public testing::Test {
 public:
  OrtcRtpSenderTest() : fake_packet_transport_("fake") {
    // Need to set the fake packet transport to writable, in order to test that
    // the "send" flag is applied to the media engine based on the encoding
    // |active| flag.
    fake_packet_transport_.SetWritable(true);
    fake_media_engine_ = new cricket::FakeMediaEngine();
    // Note: This doesn't need to use fake network classes, since we already
    // use FakePacketTransport.
    auto ortc_factory_result = OrtcFactory::Create(
        nullptr, nullptr, nullptr, nullptr, nullptr,
        std::unique_ptr<cricket::MediaEngineInterface>(fake_media_engine_));
    ortc_factory_ = ortc_factory_result.MoveValue();
    RtcpParameters rtcp_parameters;
    rtcp_parameters.mux = true;
    auto rtp_transport_result = ortc_factory_->CreateRtpTransport(
        rtcp_parameters, &fake_packet_transport_, nullptr, nullptr);
    rtp_transport_ = rtp_transport_result.MoveValue();
  }

 protected:
  rtc::scoped_refptr<AudioTrackInterface> CreateAudioTrack(
      const std::string& id) {
    return ortc_factory_->CreateAudioTrack(id, nullptr);
  }

  rtc::scoped_refptr<VideoTrackInterface> CreateVideoTrack(
      const std::string& id) {
    return rtc::scoped_refptr<webrtc::VideoTrackInterface>(
        ortc_factory_->CreateVideoTrack(id, FakeVideoTrackSource::Create()));
  }

  // Owned by |ortc_factory_|.
  cricket::FakeMediaEngine* fake_media_engine_;
  rtc::FakePacketTransport fake_packet_transport_;
  std::unique_ptr<OrtcFactoryInterface> ortc_factory_;
  std::unique_ptr<RtpTransportInterface> rtp_transport_;
};

TEST_F(OrtcRtpSenderTest, GetAndSetTrack) {
  // Test GetTrack with a sender constructed with a track.
  auto audio_track = CreateAudioTrack("audio");
  auto audio_sender_result =
      ortc_factory_->CreateRtpSender(audio_track, rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();
  EXPECT_EQ(audio_track, audio_sender->GetTrack());

  // Test GetTrack after SetTrack.
  auto video_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport_.get());
  auto video_sender = video_sender_result.MoveValue();
  auto video_track = CreateVideoTrack("video1");
  EXPECT_TRUE(video_sender->SetTrack(video_track).ok());
  EXPECT_EQ(video_track, video_sender->GetTrack());
  video_track = CreateVideoTrack("video2");
  EXPECT_TRUE(video_sender->SetTrack(video_track).ok());
  EXPECT_EQ(video_track, video_sender->GetTrack());
}

// Test that track can be set when previously unset, even after Send has been
// called.
TEST_F(OrtcRtpSenderTest, SetTrackWhileSending) {
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();
  EXPECT_TRUE(audio_sender->Send(MakeMinimalOpusParameters()).ok());
  EXPECT_TRUE(audio_sender->SetTrack(CreateAudioTrack("audio")).ok());

  auto video_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport_.get());
  auto video_sender = video_sender_result.MoveValue();
  EXPECT_TRUE(video_sender->Send(MakeMinimalVp8Parameters()).ok());
  EXPECT_TRUE(video_sender->SetTrack(CreateVideoTrack("video")).ok());
}

// Test that track can be changed mid-sending. Differs from the above test in
// that the track is set and being changed, rather than unset and being set for
// the first time.
TEST_F(OrtcRtpSenderTest, ChangeTrackWhileSending) {
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      CreateAudioTrack("audio1"), rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();
  EXPECT_TRUE(audio_sender->Send(MakeMinimalOpusParameters()).ok());
  EXPECT_TRUE(audio_sender->SetTrack(CreateAudioTrack("audio2")).ok());

  auto video_sender_result = ortc_factory_->CreateRtpSender(
      CreateVideoTrack("video1"), rtp_transport_.get());
  auto video_sender = video_sender_result.MoveValue();
  EXPECT_TRUE(video_sender->Send(MakeMinimalVp8Parameters()).ok());
  EXPECT_TRUE(video_sender->SetTrack(CreateVideoTrack("video2")).ok());
}

// Test that track can be set to null while sending.
TEST_F(OrtcRtpSenderTest, UnsetTrackWhileSending) {
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      CreateAudioTrack("audio"), rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();
  EXPECT_TRUE(audio_sender->Send(MakeMinimalOpusParameters()).ok());
  EXPECT_TRUE(audio_sender->SetTrack(nullptr).ok());

  auto video_sender_result = ortc_factory_->CreateRtpSender(
      CreateVideoTrack("video"), rtp_transport_.get());
  auto video_sender = video_sender_result.MoveValue();
  EXPECT_TRUE(video_sender->Send(MakeMinimalVp8Parameters()).ok());
  EXPECT_TRUE(video_sender->SetTrack(nullptr).ok());
}

// Shouldn't be able to set an audio track on a video sender or vice versa.
TEST_F(OrtcRtpSenderTest, SetTrackOfWrongKindFails) {
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER,
            audio_sender->SetTrack(CreateVideoTrack("video")).type());

  auto video_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport_.get());
  auto video_sender = video_sender_result.MoveValue();
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER,
            video_sender->SetTrack(CreateAudioTrack("audio")).type());
}

// Currently SetTransport isn't supported. When it is, replace this test with a
// test/tests for it.
TEST_F(OrtcRtpSenderTest, SetTransportFails) {
  rtc::FakePacketTransport fake_packet_transport("another_transport");
  RtcpParameters rtcp_parameters;
  rtcp_parameters.mux = true;
  auto rtp_transport_result = ortc_factory_->CreateRtpTransport(
      rtcp_parameters, &fake_packet_transport, nullptr, nullptr);
  auto rtp_transport = rtp_transport_result.MoveValue();

  auto sender_result = ortc_factory_->CreateRtpSender(cricket::MEDIA_TYPE_AUDIO,
                                                      rtp_transport_.get());
  auto sender = sender_result.MoveValue();
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_OPERATION,
            sender->SetTransport(rtp_transport.get()).type());
}

TEST_F(OrtcRtpSenderTest, GetTransport) {
  auto result = ortc_factory_->CreateRtpSender(cricket::MEDIA_TYPE_AUDIO,
                                               rtp_transport_.get());
  EXPECT_EQ(rtp_transport_.get(), result.value()->GetTransport());
}

// Test that "Send" causes the expected parameters to be applied to the media
// engine level, for an audio sender.
TEST_F(OrtcRtpSenderTest, SendAppliesAudioParametersToMediaEngine) {
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();

  // First, create parameters with all the bells and whistles.
  RtpParameters parameters;

  RtpCodecParameters opus_codec;
  opus_codec.name = "opus";
  opus_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  opus_codec.payload_type = 120;
  opus_codec.clock_rate.emplace(48000);
  opus_codec.num_channels.emplace(2);
  opus_codec.parameters["minptime"] = "10";
  opus_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::TRANSPORT_CC);
  parameters.codecs.push_back(std::move(opus_codec));

  // Add two codecs, expecting the first to be used.
  // TODO(deadbeef): Once "codec_payload_type" is supported, use it to select a
  // codec that's not at the top of the list.
  RtpCodecParameters isac_codec;
  isac_codec.name = "ISAC";
  isac_codec.kind = cricket::MEDIA_TYPE_AUDIO;
  isac_codec.payload_type = 110;
  isac_codec.clock_rate.emplace(16000);
  parameters.codecs.push_back(std::move(isac_codec));

  RtpEncodingParameters encoding;
  encoding.ssrc.emplace(0xdeadbeef);
  encoding.max_bitrate_bps.emplace(20000);
  parameters.encodings.push_back(std::move(encoding));

  parameters.header_extensions.emplace_back(
      "urn:ietf:params:rtp-hdrext:ssrc-audio-level", 3);

  EXPECT_TRUE(audio_sender->Send(parameters).ok());

  // Now verify that the parameters were applied to the fake media engine layer
  // that exists below BaseChannel.
  cricket::FakeVoiceMediaChannel* fake_voice_channel =
      fake_media_engine_->GetVoiceChannel(0);
  ASSERT_NE(nullptr, fake_voice_channel);
  EXPECT_TRUE(fake_voice_channel->sending());

  // Verify codec parameters.
  ASSERT_GT(fake_voice_channel->send_codecs().size(), 0u);
  const cricket::AudioCodec& top_codec = fake_voice_channel->send_codecs()[0];
  EXPECT_EQ("opus", top_codec.name);
  EXPECT_EQ(120, top_codec.id);
  EXPECT_EQ(48000, top_codec.clockrate);
  EXPECT_EQ(2u, top_codec.channels);
  ASSERT_NE(top_codec.params.end(), top_codec.params.find("minptime"));
  EXPECT_EQ("10", top_codec.params.at("minptime"));

  // Verify encoding parameters.
  EXPECT_EQ(20000, fake_voice_channel->max_bps());
  ASSERT_EQ(1u, fake_voice_channel->send_streams().size());
  const cricket::StreamParams& send_stream =
      fake_voice_channel->send_streams()[0];
  EXPECT_EQ(1u, send_stream.ssrcs.size());
  EXPECT_EQ(0xdeadbeef, send_stream.first_ssrc());

  // Verify header extensions.
  ASSERT_EQ(1u, fake_voice_channel->send_extensions().size());
  const RtpExtension& extension = fake_voice_channel->send_extensions()[0];
  EXPECT_EQ("urn:ietf:params:rtp-hdrext:ssrc-audio-level", extension.uri);
  EXPECT_EQ(3, extension.id);
}

// Test that "Send" causes the expected parameters to be applied to the media
// engine level, for a video sender.
TEST_F(OrtcRtpSenderTest, SendAppliesVideoParametersToMediaEngine) {
  auto video_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport_.get());
  auto video_sender = video_sender_result.MoveValue();

  // First, create parameters with all the bells and whistles.
  RtpParameters parameters;

  RtpCodecParameters vp8_codec;
  vp8_codec.name = "VP8";
  vp8_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  vp8_codec.payload_type = 99;
  // Try a couple types of feedback params. "Generic NACK" is a bit of a
  // special case, so test it here.
  vp8_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::CCM,
                                       RtcpFeedbackMessageType::FIR);
  vp8_codec.rtcp_feedback.emplace_back(RtcpFeedbackType::NACK,
                                       RtcpFeedbackMessageType::GENERIC_NACK);
  parameters.codecs.push_back(std::move(vp8_codec));

  RtpCodecParameters vp8_rtx_codec;
  vp8_rtx_codec.name = "rtx";
  vp8_rtx_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  vp8_rtx_codec.payload_type = 100;
  vp8_rtx_codec.parameters["apt"] = "99";
  parameters.codecs.push_back(std::move(vp8_rtx_codec));

  // Add two codecs, expecting the first to be used.
  // TODO(deadbeef): Once "codec_payload_type" is supported, use it to select a
  // codec that's not at the top of the list.
  RtpCodecParameters vp9_codec;
  vp9_codec.name = "VP9";
  vp9_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  vp9_codec.payload_type = 102;
  parameters.codecs.push_back(std::move(vp9_codec));

  RtpCodecParameters vp9_rtx_codec;
  vp9_rtx_codec.name = "rtx";
  vp9_rtx_codec.kind = cricket::MEDIA_TYPE_VIDEO;
  vp9_rtx_codec.payload_type = 103;
  vp9_rtx_codec.parameters["apt"] = "102";
  parameters.codecs.push_back(std::move(vp9_rtx_codec));

  RtpEncodingParameters encoding;
  encoding.ssrc.emplace(0xdeadbeef);
  encoding.rtx.emplace(0xbaadfeed);
  encoding.max_bitrate_bps.emplace(99999);
  parameters.encodings.push_back(std::move(encoding));

  parameters.header_extensions.emplace_back("urn:3gpp:video-orientation", 4);
  parameters.header_extensions.emplace_back(
      "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay", 6);

  EXPECT_TRUE(video_sender->Send(parameters).ok());

  // Now verify that the parameters were applied to the fake media engine layer
  // that exists below BaseChannel.
  cricket::FakeVideoMediaChannel* fake_video_channel =
      fake_media_engine_->GetVideoChannel(0);
  ASSERT_NE(nullptr, fake_video_channel);
  EXPECT_TRUE(fake_video_channel->sending());

  // Verify codec parameters.
  ASSERT_GE(fake_video_channel->send_codecs().size(), 2u);
  const cricket::VideoCodec& top_codec = fake_video_channel->send_codecs()[0];
  EXPECT_EQ("VP8", top_codec.name);
  EXPECT_EQ(99, top_codec.id);
  EXPECT_TRUE(top_codec.feedback_params.Has({"ccm", "fir"}));
  EXPECT_TRUE(top_codec.feedback_params.Has(cricket::FeedbackParam("nack")));

  const cricket::VideoCodec& rtx_codec = fake_video_channel->send_codecs()[1];
  EXPECT_EQ("rtx", rtx_codec.name);
  EXPECT_EQ(100, rtx_codec.id);
  ASSERT_NE(rtx_codec.params.end(), rtx_codec.params.find("apt"));
  EXPECT_EQ("99", rtx_codec.params.at("apt"));

  // Verify encoding parameters.
  EXPECT_EQ(99999, fake_video_channel->max_bps());
  ASSERT_EQ(1u, fake_video_channel->send_streams().size());
  const cricket::StreamParams& send_stream =
      fake_video_channel->send_streams()[0];
  EXPECT_EQ(2u, send_stream.ssrcs.size());
  EXPECT_EQ(0xdeadbeef, send_stream.first_ssrc());
  uint32_t rtx_ssrc = 0u;
  EXPECT_TRUE(send_stream.GetFidSsrc(send_stream.first_ssrc(), &rtx_ssrc));
  EXPECT_EQ(0xbaadfeed, rtx_ssrc);

  // Verify header extensions.
  ASSERT_EQ(2u, fake_video_channel->send_extensions().size());
  const RtpExtension& extension1 = fake_video_channel->send_extensions()[0];
  EXPECT_EQ("urn:3gpp:video-orientation", extension1.uri);
  EXPECT_EQ(4, extension1.id);
  const RtpExtension& extension2 = fake_video_channel->send_extensions()[1];
  EXPECT_EQ("http://www.webrtc.org/experiments/rtp-hdrext/playout-delay",
            extension2.uri);
  EXPECT_EQ(6, extension2.id);
}

// Ensure that when primary or RTX SSRCs are left unset, they're generated
// automatically.
TEST_F(OrtcRtpSenderTest, SendGeneratesSsrcsWhenEmpty) {
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();
  RtpParameters parameters = MakeMinimalOpusParametersWithNoSsrc();
  // Default RTX parameters, with no SSRC.
  parameters.encodings[0].rtx.emplace();
  EXPECT_TRUE(audio_sender->Send(parameters).ok());

  cricket::FakeVoiceMediaChannel* fake_voice_channel =
      fake_media_engine_->GetVoiceChannel(0);
  ASSERT_NE(nullptr, fake_voice_channel);
  ASSERT_EQ(1u, fake_voice_channel->send_streams().size());
  const cricket::StreamParams& audio_send_stream =
      fake_voice_channel->send_streams()[0];
  EXPECT_NE(0u, audio_send_stream.first_ssrc());
  uint32_t rtx_ssrc = 0u;
  EXPECT_TRUE(
      audio_send_stream.GetFidSsrc(audio_send_stream.first_ssrc(), &rtx_ssrc));
  EXPECT_NE(0u, rtx_ssrc);
  EXPECT_NE(audio_send_stream.first_ssrc(), rtx_ssrc);

  auto video_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport_.get());
  auto video_sender = video_sender_result.MoveValue();
  parameters = MakeMinimalVp8ParametersWithNoSsrc();
  // Default RTX parameters, with no SSRC.
  parameters.encodings[0].rtx.emplace();
  EXPECT_TRUE(video_sender->Send(parameters).ok());

  cricket::FakeVideoMediaChannel* fake_video_channel =
      fake_media_engine_->GetVideoChannel(0);
  ASSERT_NE(nullptr, fake_video_channel);
  ASSERT_EQ(1u, fake_video_channel->send_streams().size());
  const cricket::StreamParams& video_send_stream =
      fake_video_channel->send_streams()[0];
  EXPECT_NE(0u, video_send_stream.first_ssrc());
  rtx_ssrc = 0u;
  EXPECT_TRUE(
      video_send_stream.GetFidSsrc(video_send_stream.first_ssrc(), &rtx_ssrc));
  EXPECT_NE(0u, rtx_ssrc);
  EXPECT_NE(video_send_stream.first_ssrc(), rtx_ssrc);
  EXPECT_NE(video_send_stream.first_ssrc(), audio_send_stream.first_ssrc());
}

// Test changing both the send codec and SSRC at the same time, and verify that
// the new parameters are applied to the media engine level.
TEST_F(OrtcRtpSenderTest, CallingSendTwiceChangesParameters) {
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();
  EXPECT_TRUE(
      audio_sender->Send(MakeMinimalOpusParametersWithSsrc(0x11111111)).ok());
  EXPECT_TRUE(
      audio_sender->Send(MakeMinimalIsacParametersWithSsrc(0x22222222)).ok());

  cricket::FakeVoiceMediaChannel* fake_voice_channel =
      fake_media_engine_->GetVoiceChannel(0);
  ASSERT_NE(nullptr, fake_voice_channel);
  ASSERT_GT(fake_voice_channel->send_codecs().size(), 0u);
  EXPECT_EQ("ISAC", fake_voice_channel->send_codecs()[0].name);
  ASSERT_EQ(1u, fake_voice_channel->send_streams().size());
  EXPECT_EQ(0x22222222u, fake_voice_channel->send_streams()[0].first_ssrc());

  auto video_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport_.get());
  auto video_sender = video_sender_result.MoveValue();
  EXPECT_TRUE(
      video_sender->Send(MakeMinimalVp8ParametersWithSsrc(0x33333333)).ok());
  EXPECT_TRUE(
      video_sender->Send(MakeMinimalVp9ParametersWithSsrc(0x44444444)).ok());

  cricket::FakeVideoMediaChannel* fake_video_channel =
      fake_media_engine_->GetVideoChannel(0);
  ASSERT_NE(nullptr, fake_video_channel);
  ASSERT_GT(fake_video_channel->send_codecs().size(), 0u);
  EXPECT_EQ("VP9", fake_video_channel->send_codecs()[0].name);
  ASSERT_EQ(1u, fake_video_channel->send_streams().size());
  EXPECT_EQ(0x44444444u, fake_video_channel->send_streams()[0].first_ssrc());
}

// Ensure that if the |active| flag of RtpEncodingParameters is set to false,
// sending stops at the media engine level.
TEST_F(OrtcRtpSenderTest, DeactivatingEncodingStopsSending) {
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();
  RtpParameters parameters = MakeMinimalOpusParameters();
  EXPECT_TRUE(audio_sender->Send(parameters).ok());

  // Expect "sending" flag to initially be true.
  cricket::FakeVoiceMediaChannel* fake_voice_channel =
      fake_media_engine_->GetVoiceChannel(0);
  ASSERT_NE(nullptr, fake_voice_channel);
  EXPECT_TRUE(fake_voice_channel->sending());

  // Deactivate encoding and expect it to change to false.
  parameters.encodings[0].active = false;
  EXPECT_TRUE(audio_sender->Send(parameters).ok());
  EXPECT_FALSE(fake_voice_channel->sending());

  // Try the same thing for video now.
  auto video_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport_.get());
  auto video_sender = video_sender_result.MoveValue();
  parameters = MakeMinimalVp8Parameters();
  EXPECT_TRUE(video_sender->Send(parameters).ok());

  cricket::FakeVideoMediaChannel* fake_video_channel =
      fake_media_engine_->GetVideoChannel(0);
  ASSERT_NE(nullptr, fake_video_channel);
  EXPECT_TRUE(fake_video_channel->sending());

  parameters.encodings[0].active = false;
  EXPECT_TRUE(video_sender->Send(parameters).ok());
  EXPECT_FALSE(fake_video_channel->sending());
}

// Ensure that calling Send with an empty list of encodings causes send streams
// at the media engine level to be cleared.
TEST_F(OrtcRtpSenderTest, CallingSendWithEmptyEncodingsClearsSendStreams) {
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();
  RtpParameters parameters = MakeMinimalOpusParameters();
  EXPECT_TRUE(audio_sender->Send(parameters).ok());
  parameters.encodings.clear();
  EXPECT_TRUE(audio_sender->Send(parameters).ok());

  cricket::FakeVoiceMediaChannel* fake_voice_channel =
      fake_media_engine_->GetVoiceChannel(0);
  ASSERT_NE(nullptr, fake_voice_channel);
  EXPECT_TRUE(fake_voice_channel->send_streams().empty());

  auto video_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport_.get());
  auto video_sender = video_sender_result.MoveValue();
  parameters = MakeMinimalVp8Parameters();
  EXPECT_TRUE(video_sender->Send(parameters).ok());
  parameters.encodings.clear();
  EXPECT_TRUE(video_sender->Send(parameters).ok());

  cricket::FakeVideoMediaChannel* fake_video_channel =
      fake_media_engine_->GetVideoChannel(0);
  ASSERT_NE(nullptr, fake_video_channel);
  EXPECT_TRUE(fake_video_channel->send_streams().empty());
}

// These errors should be covered by rtpparametersconversion_unittest.cc, but
// we should at least test that those errors are propogated from calls to Send,
// with a few examples.
TEST_F(OrtcRtpSenderTest, SendReturnsErrorOnInvalidParameters) {
  auto result = ortc_factory_->CreateRtpSender(cricket::MEDIA_TYPE_VIDEO,
                                               rtp_transport_.get());
  auto sender = result.MoveValue();
  // NACK feedback missing message type.
  RtpParameters invalid_feedback = MakeMinimalVp8Parameters();
  invalid_feedback.codecs[0].rtcp_feedback.emplace_back(RtcpFeedbackType::NACK);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER,
            sender->Send(invalid_feedback).type());
  // Negative payload type.
  RtpParameters invalid_pt = MakeMinimalVp8Parameters();
  invalid_pt.codecs[0].payload_type = -1;
  EXPECT_EQ(RTCErrorType::INVALID_RANGE, sender->Send(invalid_pt).type());
  // Duplicate codec payload types.
  RtpParameters duplicate_payload_types = MakeMinimalVp8Parameters();
  duplicate_payload_types.codecs.push_back(duplicate_payload_types.codecs[0]);
  duplicate_payload_types.codecs.back().name = "VP9";
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER,
            sender->Send(duplicate_payload_types).type());
}

// Two senders using the same transport shouldn't be able to use the same
// payload type to refer to different codecs, same header extension IDs to
// refer to different extensions, or same SSRC.
TEST_F(OrtcRtpSenderTest, SendReturnsErrorOnIdConflicts) {
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport_.get());
  auto video_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();
  auto video_sender = video_sender_result.MoveValue();

  // First test payload type conflict.
  RtpParameters audio_parameters = MakeMinimalOpusParameters();
  RtpParameters video_parameters = MakeMinimalVp8Parameters();
  audio_parameters.codecs[0].payload_type = 100;
  video_parameters.codecs[0].payload_type = 100;
  EXPECT_TRUE(audio_sender->Send(audio_parameters).ok());
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER,
            video_sender->Send(video_parameters).type());

  // Test header extension ID conflict.
  video_parameters.codecs[0].payload_type = 110;
  audio_parameters.header_extensions.emplace_back("foo", 4);
  video_parameters.header_extensions.emplace_back("bar", 4);
  EXPECT_TRUE(audio_sender->Send(audio_parameters).ok());
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER,
            video_sender->Send(video_parameters).type());

  // Test SSRC conflict. Have an RTX SSRC that conflicts with a primary SSRC
  // for extra challenge.
  video_parameters.header_extensions[0].uri = "foo";
  audio_parameters.encodings[0].ssrc.emplace(0xdeadbeef);
  video_parameters.encodings[0].ssrc.emplace(0xabbaabba);
  video_parameters.encodings[0].rtx.emplace(0xdeadbeef);
  EXPECT_TRUE(audio_sender->Send(audio_parameters).ok());
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER,
            video_sender->Send(video_parameters).type());

  // Sanity check that parameters can be set if the conflicts are all resolved.
  video_parameters.encodings[0].rtx->ssrc.emplace(0xbaadf00d);
  EXPECT_TRUE(video_sender->Send(video_parameters).ok());
}

// Ensure that deleting a sender causes send streams at the media engine level
// to be cleared.
TEST_F(OrtcRtpSenderTest, DeletingSenderClearsSendStreams) {
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();
  EXPECT_TRUE(audio_sender->Send(MakeMinimalOpusParameters()).ok());

  // Also create an audio receiver, to prevent the voice channel from being
  // completely deleted.
  auto audio_receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport_.get());
  auto audio_receiver = audio_receiver_result.MoveValue();
  EXPECT_TRUE(audio_receiver->Receive(MakeMinimalOpusParameters()).ok());

  audio_sender.reset(nullptr);
  cricket::FakeVoiceMediaChannel* fake_voice_channel =
      fake_media_engine_->GetVoiceChannel(0);
  ASSERT_NE(nullptr, fake_voice_channel);
  EXPECT_TRUE(fake_voice_channel->send_streams().empty());

  auto video_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport_.get());
  auto video_sender = video_sender_result.MoveValue();
  EXPECT_TRUE(video_sender->Send(MakeMinimalVp8Parameters()).ok());

  // Also create an video receiver, to prevent the video channel from being
  // completely deleted.
  auto video_receiver_result = ortc_factory_->CreateRtpReceiver(
      cricket::MEDIA_TYPE_VIDEO, rtp_transport_.get());
  auto video_receiver = video_receiver_result.MoveValue();
  EXPECT_TRUE(video_receiver->Receive(MakeMinimalVp8Parameters()).ok());

  video_sender.reset(nullptr);
  cricket::FakeVideoMediaChannel* fake_video_channel =
      fake_media_engine_->GetVideoChannel(0);
  ASSERT_NE(nullptr, fake_video_channel);
  EXPECT_TRUE(fake_video_channel->send_streams().empty());
}

// If Send hasn't been called, GetParameters should return empty parameters.
TEST_F(OrtcRtpSenderTest, GetDefaultParameters) {
  auto result = ortc_factory_->CreateRtpSender(cricket::MEDIA_TYPE_AUDIO,
                                               rtp_transport_.get());
  EXPECT_EQ(RtpParameters(), result.value()->GetParameters());
  result = ortc_factory_->CreateRtpSender(cricket::MEDIA_TYPE_VIDEO,
                                          rtp_transport_.get());
  EXPECT_EQ(RtpParameters(), result.value()->GetParameters());
}

// Test that GetParameters returns the last parameters passed into Send, along
// with the implementation-default values filled in where they were left unset.
TEST_F(OrtcRtpSenderTest,
       GetParametersReturnsLastSetParametersWithDefaultsFilled) {
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      CreateAudioTrack("audio"), rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();

  RtpParameters opus_parameters = MakeMinimalOpusParameters();
  EXPECT_TRUE(audio_sender->Send(opus_parameters).ok());
  EXPECT_EQ(opus_parameters, audio_sender->GetParameters());

  RtpParameters isac_parameters = MakeMinimalIsacParameters();
  // Sanity check that num_channels actually is left unset.
  ASSERT_FALSE(isac_parameters.codecs[0].num_channels);
  EXPECT_TRUE(audio_sender->Send(isac_parameters).ok());
  // Should be filled with a default "num channels" of 1.
  // TODO(deadbeef): This should actually default to 2 for some codecs. Update
  // this test once that's implemented.
  isac_parameters.codecs[0].num_channels.emplace(1);
  EXPECT_EQ(isac_parameters, audio_sender->GetParameters());

  auto video_sender_result = ortc_factory_->CreateRtpSender(
      CreateVideoTrack("video"), rtp_transport_.get());
  auto video_sender = video_sender_result.MoveValue();

  RtpParameters vp8_parameters = MakeMinimalVp8Parameters();
  // Sanity check that clock_rate actually is left unset.
  EXPECT_TRUE(video_sender->Send(vp8_parameters).ok());
  // Should be filled with a default clock rate of 90000.
  vp8_parameters.codecs[0].clock_rate.emplace(90000);
  EXPECT_EQ(vp8_parameters, video_sender->GetParameters());

  RtpParameters vp9_parameters = MakeMinimalVp9Parameters();
  // Sanity check that clock_rate actually is left unset.
  EXPECT_TRUE(video_sender->Send(vp9_parameters).ok());
  // Should be filled with a default clock rate of 90000.
  vp9_parameters.codecs[0].clock_rate.emplace(90000);
  EXPECT_EQ(vp9_parameters, video_sender->GetParameters());
}

TEST_F(OrtcRtpSenderTest, GetKind) {
  // Construct one sender from the "kind" enum and another from a track.
  auto audio_sender_result = ortc_factory_->CreateRtpSender(
      cricket::MEDIA_TYPE_AUDIO, rtp_transport_.get());
  auto video_sender_result = ortc_factory_->CreateRtpSender(
      CreateVideoTrack("video"), rtp_transport_.get());
  auto audio_sender = audio_sender_result.MoveValue();
  auto video_sender = video_sender_result.MoveValue();
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, audio_sender->GetKind());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, video_sender->GetKind());
}

}  // namespace webrtc
