/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>
#include <utility>

#include "webrtc/base/gunit.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/media/base/fakemediaengine.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/engine/fakewebrtccall.h"
#include "webrtc/p2p/base/faketransportcontroller.h"
#include "webrtc/pc/audiotrack.h"
#include "webrtc/pc/channelmanager.h"
#include "webrtc/pc/localaudiosource.h"
#include "webrtc/pc/mediastream.h"
#include "webrtc/pc/remoteaudiosource.h"
#include "webrtc/pc/rtpreceiver.h"
#include "webrtc/pc/rtpsender.h"
#include "webrtc/pc/streamcollection.h"
#include "webrtc/pc/test/fakevideotracksource.h"
#include "webrtc/pc/videotrack.h"
#include "webrtc/pc/videotracksource.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using ::testing::_;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace {

static const char kStreamLabel1[] = "local_stream_1";
static const char kVideoTrackId[] = "video_1";
static const char kAudioTrackId[] = "audio_1";
static const uint32_t kVideoSsrc = 98;
static const uint32_t kVideoSsrc2 = 100;
static const uint32_t kAudioSsrc = 99;
static const uint32_t kAudioSsrc2 = 101;
static const int kDefaultTimeout = 10000;  // 10 seconds.
}  // namespace

namespace webrtc {

class RtpSenderReceiverTest : public testing::Test,
                              public sigslot::has_slots<> {
 public:
  RtpSenderReceiverTest()
      :  // Create fake media engine/etc. so we can create channels to use to
         // test RtpSenders/RtpReceivers.
        media_engine_(new cricket::FakeMediaEngine()),
        channel_manager_(
            std::unique_ptr<cricket::MediaEngineInterface>(media_engine_),
            rtc::Thread::Current(),
            rtc::Thread::Current()),
        fake_call_(Call::Config(&event_log_)),
        local_stream_(MediaStream::Create(kStreamLabel1)) {
    // Create channels to be used by the RtpSenders and RtpReceivers.
    channel_manager_.Init();
    bool srtp_required = true;
    cricket::DtlsTransportInternal* rtp_transport =
        fake_transport_controller_.CreateDtlsTransport(
            cricket::CN_AUDIO, cricket::ICE_CANDIDATE_COMPONENT_RTP);
    voice_channel_ = channel_manager_.CreateVoiceChannel(
        &fake_call_, cricket::MediaConfig(),
        rtp_transport, nullptr, rtc::Thread::Current(),
        cricket::CN_AUDIO, srtp_required, cricket::AudioOptions());
    video_channel_ = channel_manager_.CreateVideoChannel(
        &fake_call_, cricket::MediaConfig(),
        rtp_transport, nullptr, rtc::Thread::Current(),
        cricket::CN_VIDEO, srtp_required, cricket::VideoOptions());
    voice_channel_->Enable(true);
    video_channel_->Enable(true);
    voice_media_channel_ = media_engine_->GetVoiceChannel(0);
    video_media_channel_ = media_engine_->GetVideoChannel(0);
    RTC_CHECK(voice_channel_);
    RTC_CHECK(video_channel_);
    RTC_CHECK(voice_media_channel_);
    RTC_CHECK(video_media_channel_);

    // Create streams for predefined SSRCs. Streams need to exist in order
    // for the senders and receievers to apply parameters to them.
    // Normally these would be created by SetLocalDescription and
    // SetRemoteDescription.
    voice_media_channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kAudioSsrc));
    voice_media_channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kAudioSsrc));
    voice_media_channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kAudioSsrc2));
    voice_media_channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kAudioSsrc2));
    video_media_channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kVideoSsrc));
    video_media_channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kVideoSsrc));
    video_media_channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kVideoSsrc2));
    video_media_channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kVideoSsrc2));
  }

  // Needed to use DTMF sender.
  void AddDtmfCodec() {
    cricket::AudioSendParameters params;
    const cricket::AudioCodec kTelephoneEventCodec(106, "telephone-event", 8000,
                                                   0, 1);
    params.codecs.push_back(kTelephoneEventCodec);
    voice_media_channel_->SetSendParameters(params);
  }

  void AddVideoTrack() { AddVideoTrack(false); }

  void AddVideoTrack(bool is_screencast) {
    rtc::scoped_refptr<VideoTrackSourceInterface> source(
        FakeVideoTrackSource::Create(is_screencast));
    video_track_ = VideoTrack::Create(kVideoTrackId, source);
    EXPECT_TRUE(local_stream_->AddTrack(video_track_));
  }

  void CreateAudioRtpSender() { CreateAudioRtpSender(nullptr); }

  void CreateAudioRtpSender(rtc::scoped_refptr<LocalAudioSource> source) {
    audio_track_ = AudioTrack::Create(kAudioTrackId, source);
    EXPECT_TRUE(local_stream_->AddTrack(audio_track_));
    audio_rtp_sender_ =
        new AudioRtpSender(local_stream_->GetAudioTracks()[0],
                           local_stream_->label(), voice_channel_, nullptr);
    audio_rtp_sender_->SetSsrc(kAudioSsrc);
    audio_rtp_sender_->GetOnDestroyedSignal()->connect(
        this, &RtpSenderReceiverTest::OnAudioSenderDestroyed);
    VerifyVoiceChannelInput();
  }

  void OnAudioSenderDestroyed() { audio_sender_destroyed_signal_fired_ = true; }

  void CreateVideoRtpSender() { CreateVideoRtpSender(false); }

  void CreateVideoRtpSender(bool is_screencast) {
    AddVideoTrack(is_screencast);
    video_rtp_sender_ =
        new VideoRtpSender(local_stream_->GetVideoTracks()[0],
                           local_stream_->label(), video_channel_);
    video_rtp_sender_->SetSsrc(kVideoSsrc);
    VerifyVideoChannelInput();
  }

  void DestroyAudioRtpSender() {
    audio_rtp_sender_ = nullptr;
    VerifyVoiceChannelNoInput();
  }

  void DestroyVideoRtpSender() {
    video_rtp_sender_ = nullptr;
    VerifyVideoChannelNoInput();
  }

  void CreateAudioRtpReceiver() {
    audio_rtp_receiver_ =
        new AudioRtpReceiver(kAudioTrackId, kAudioSsrc, voice_channel_);
    audio_track_ = audio_rtp_receiver_->audio_track();
    VerifyVoiceChannelOutput();
  }

  void CreateVideoRtpReceiver() {
    video_rtp_receiver_ = new VideoRtpReceiver(
        kVideoTrackId, rtc::Thread::Current(), kVideoSsrc, video_channel_);
    video_track_ = video_rtp_receiver_->video_track();
    VerifyVideoChannelOutput();
  }

  void DestroyAudioRtpReceiver() {
    audio_rtp_receiver_ = nullptr;
    VerifyVoiceChannelNoOutput();
  }

  void DestroyVideoRtpReceiver() {
    video_rtp_receiver_ = nullptr;
    VerifyVideoChannelNoOutput();
  }

  void VerifyVoiceChannelInput() { VerifyVoiceChannelInput(kAudioSsrc); }

  void VerifyVoiceChannelInput(uint32_t ssrc) {
    // Verify that the media channel has an audio source, and the stream isn't
    // muted.
    EXPECT_TRUE(voice_media_channel_->HasSource(ssrc));
    EXPECT_FALSE(voice_media_channel_->IsStreamMuted(ssrc));
  }

  void VerifyVideoChannelInput() { VerifyVideoChannelInput(kVideoSsrc); }

  void VerifyVideoChannelInput(uint32_t ssrc) {
    // Verify that the media channel has a video source,
    EXPECT_TRUE(video_media_channel_->HasSource(ssrc));
  }

  void VerifyVoiceChannelNoInput() { VerifyVoiceChannelNoInput(kAudioSsrc); }

  void VerifyVoiceChannelNoInput(uint32_t ssrc) {
    // Verify that the media channel's source is reset.
    EXPECT_FALSE(voice_media_channel_->HasSource(ssrc));
  }

  void VerifyVideoChannelNoInput() { VerifyVideoChannelNoInput(kVideoSsrc); }

  void VerifyVideoChannelNoInput(uint32_t ssrc) {
    // Verify that the media channel's source is reset.
    EXPECT_FALSE(video_media_channel_->HasSource(ssrc));
  }

  void VerifyVoiceChannelOutput() {
    // Verify that the volume is initialized to 1.
    double volume;
    EXPECT_TRUE(voice_media_channel_->GetOutputVolume(kAudioSsrc, &volume));
    EXPECT_EQ(1, volume);
  }

  void VerifyVideoChannelOutput() {
    // Verify that the media channel has a sink.
    EXPECT_TRUE(video_media_channel_->HasSink(kVideoSsrc));
  }

  void VerifyVoiceChannelNoOutput() {
    // Verify that the volume is reset to 0.
    double volume;
    EXPECT_TRUE(voice_media_channel_->GetOutputVolume(kAudioSsrc, &volume));
    EXPECT_EQ(0, volume);
  }

  void VerifyVideoChannelNoOutput() {
    // Verify that the media channel's sink is reset.
    EXPECT_FALSE(video_media_channel_->HasSink(kVideoSsrc));
  }

 protected:
  webrtc::RtcEventLogNullImpl event_log_;
  // |media_engine_| is actually owned by |channel_manager_|.
  cricket::FakeMediaEngine* media_engine_;
  cricket::FakeTransportController fake_transport_controller_;
  cricket::ChannelManager channel_manager_;
  cricket::FakeCall fake_call_;
  cricket::VoiceChannel* voice_channel_;
  cricket::VideoChannel* video_channel_;
  cricket::FakeVoiceMediaChannel* voice_media_channel_;
  cricket::FakeVideoMediaChannel* video_media_channel_;
  rtc::scoped_refptr<AudioRtpSender> audio_rtp_sender_;
  rtc::scoped_refptr<VideoRtpSender> video_rtp_sender_;
  rtc::scoped_refptr<AudioRtpReceiver> audio_rtp_receiver_;
  rtc::scoped_refptr<VideoRtpReceiver> video_rtp_receiver_;
  rtc::scoped_refptr<MediaStreamInterface> local_stream_;
  rtc::scoped_refptr<VideoTrackInterface> video_track_;
  rtc::scoped_refptr<AudioTrackInterface> audio_track_;
  bool audio_sender_destroyed_signal_fired_ = false;
};

// Test that |voice_channel_| is updated when an audio track is associated
// and disassociated with an AudioRtpSender.
TEST_F(RtpSenderReceiverTest, AddAndDestroyAudioRtpSender) {
  CreateAudioRtpSender();
  DestroyAudioRtpSender();
}

// Test that |video_channel_| is updated when a video track is associated and
// disassociated with a VideoRtpSender.
TEST_F(RtpSenderReceiverTest, AddAndDestroyVideoRtpSender) {
  CreateVideoRtpSender();
  DestroyVideoRtpSender();
}

// Test that |voice_channel_| is updated when a remote audio track is
// associated and disassociated with an AudioRtpReceiver.
TEST_F(RtpSenderReceiverTest, AddAndDestroyAudioRtpReceiver) {
  CreateAudioRtpReceiver();
  DestroyAudioRtpReceiver();
}

// Test that |video_channel_| is updated when a remote video track is
// associated and disassociated with a VideoRtpReceiver.
TEST_F(RtpSenderReceiverTest, AddAndDestroyVideoRtpReceiver) {
  CreateVideoRtpReceiver();
  DestroyVideoRtpReceiver();
}

// Test that the AudioRtpSender applies options from the local audio source.
TEST_F(RtpSenderReceiverTest, LocalAudioSourceOptionsApplied) {
  cricket::AudioOptions options;
  options.echo_cancellation = rtc::Optional<bool>(true);
  auto source = LocalAudioSource::Create(&options);
  CreateAudioRtpSender(source.get());

  EXPECT_EQ(rtc::Optional<bool>(true),
            voice_media_channel_->options().echo_cancellation);

  DestroyAudioRtpSender();
}

// Test that the stream is muted when the track is disabled, and unmuted when
// the track is enabled.
TEST_F(RtpSenderReceiverTest, LocalAudioTrackDisable) {
  CreateAudioRtpSender();

  audio_track_->set_enabled(false);
  EXPECT_TRUE(voice_media_channel_->IsStreamMuted(kAudioSsrc));

  audio_track_->set_enabled(true);
  EXPECT_FALSE(voice_media_channel_->IsStreamMuted(kAudioSsrc));

  DestroyAudioRtpSender();
}

// Test that the volume is set to 0 when the track is disabled, and back to
// 1 when the track is enabled.
TEST_F(RtpSenderReceiverTest, RemoteAudioTrackDisable) {
  CreateAudioRtpReceiver();

  double volume;
  EXPECT_TRUE(voice_media_channel_->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(1, volume);

  audio_track_->set_enabled(false);
  EXPECT_TRUE(voice_media_channel_->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(0, volume);

  audio_track_->set_enabled(true);
  EXPECT_TRUE(voice_media_channel_->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(1, volume);

  DestroyAudioRtpReceiver();
}

// Currently no action is taken when a remote video track is disabled or
// enabled, so there's nothing to test here, other than what is normally
// verified in DestroyVideoRtpSender.
TEST_F(RtpSenderReceiverTest, LocalVideoTrackDisable) {
  CreateVideoRtpSender();

  video_track_->set_enabled(false);
  video_track_->set_enabled(true);

  DestroyVideoRtpSender();
}

// Test that the state of the video track created by the VideoRtpReceiver is
// updated when the receiver is destroyed.
TEST_F(RtpSenderReceiverTest, RemoteVideoTrackState) {
  CreateVideoRtpReceiver();

  EXPECT_EQ(webrtc::MediaStreamTrackInterface::kLive, video_track_->state());
  EXPECT_EQ(webrtc::MediaSourceInterface::kLive,
            video_track_->GetSource()->state());

  DestroyVideoRtpReceiver();

  EXPECT_EQ(webrtc::MediaStreamTrackInterface::kEnded, video_track_->state());
  EXPECT_EQ(webrtc::MediaSourceInterface::kEnded,
            video_track_->GetSource()->state());
}

// Currently no action is taken when a remote video track is disabled or
// enabled, so there's nothing to test here, other than what is normally
// verified in DestroyVideoRtpReceiver.
TEST_F(RtpSenderReceiverTest, RemoteVideoTrackDisable) {
  CreateVideoRtpReceiver();

  video_track_->set_enabled(false);
  video_track_->set_enabled(true);

  DestroyVideoRtpReceiver();
}

// Test that the AudioRtpReceiver applies volume changes from the track source
// to the media channel.
TEST_F(RtpSenderReceiverTest, RemoteAudioTrackSetVolume) {
  CreateAudioRtpReceiver();

  double volume;
  audio_track_->GetSource()->SetVolume(0.5);
  EXPECT_TRUE(voice_media_channel_->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(0.5, volume);

  // Disable the audio track, this should prevent setting the volume.
  audio_track_->set_enabled(false);
  audio_track_->GetSource()->SetVolume(0.8);
  EXPECT_TRUE(voice_media_channel_->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(0, volume);

  // When the track is enabled, the previously set volume should take effect.
  audio_track_->set_enabled(true);
  EXPECT_TRUE(voice_media_channel_->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(0.8, volume);

  // Try changing volume one more time.
  audio_track_->GetSource()->SetVolume(0.9);
  EXPECT_TRUE(voice_media_channel_->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(0.9, volume);

  DestroyAudioRtpReceiver();
}

// Test that the media channel isn't enabled for sending if the audio sender
// doesn't have both a track and SSRC.
TEST_F(RtpSenderReceiverTest, AudioSenderWithoutTrackAndSsrc) {
  audio_rtp_sender_ = new AudioRtpSender(voice_channel_, nullptr);
  rtc::scoped_refptr<AudioTrackInterface> track =
      AudioTrack::Create(kAudioTrackId, nullptr);

  // Track but no SSRC.
  EXPECT_TRUE(audio_rtp_sender_->SetTrack(track));
  VerifyVoiceChannelNoInput();

  // SSRC but no track.
  EXPECT_TRUE(audio_rtp_sender_->SetTrack(nullptr));
  audio_rtp_sender_->SetSsrc(kAudioSsrc);
  VerifyVoiceChannelNoInput();
}

// Test that the media channel isn't enabled for sending if the video sender
// doesn't have both a track and SSRC.
TEST_F(RtpSenderReceiverTest, VideoSenderWithoutTrackAndSsrc) {
  video_rtp_sender_ = new VideoRtpSender(video_channel_);

  // Track but no SSRC.
  EXPECT_TRUE(video_rtp_sender_->SetTrack(video_track_));
  VerifyVideoChannelNoInput();

  // SSRC but no track.
  EXPECT_TRUE(video_rtp_sender_->SetTrack(nullptr));
  video_rtp_sender_->SetSsrc(kVideoSsrc);
  VerifyVideoChannelNoInput();
}

// Test that the media channel is enabled for sending when the audio sender
// has a track and SSRC, when the SSRC is set first.
TEST_F(RtpSenderReceiverTest, AudioSenderEarlyWarmupSsrcThenTrack) {
  audio_rtp_sender_ = new AudioRtpSender(voice_channel_, nullptr);
  rtc::scoped_refptr<AudioTrackInterface> track =
      AudioTrack::Create(kAudioTrackId, nullptr);
  audio_rtp_sender_->SetSsrc(kAudioSsrc);
  audio_rtp_sender_->SetTrack(track);
  VerifyVoiceChannelInput();

  DestroyAudioRtpSender();
}

// Test that the media channel is enabled for sending when the audio sender
// has a track and SSRC, when the SSRC is set last.
TEST_F(RtpSenderReceiverTest, AudioSenderEarlyWarmupTrackThenSsrc) {
  audio_rtp_sender_ = new AudioRtpSender(voice_channel_, nullptr);
  rtc::scoped_refptr<AudioTrackInterface> track =
      AudioTrack::Create(kAudioTrackId, nullptr);
  audio_rtp_sender_->SetTrack(track);
  audio_rtp_sender_->SetSsrc(kAudioSsrc);
  VerifyVoiceChannelInput();

  DestroyAudioRtpSender();
}

// Test that the media channel is enabled for sending when the video sender
// has a track and SSRC, when the SSRC is set first.
TEST_F(RtpSenderReceiverTest, VideoSenderEarlyWarmupSsrcThenTrack) {
  AddVideoTrack();
  video_rtp_sender_ = new VideoRtpSender(video_channel_);
  video_rtp_sender_->SetSsrc(kVideoSsrc);
  video_rtp_sender_->SetTrack(video_track_);
  VerifyVideoChannelInput();

  DestroyVideoRtpSender();
}

// Test that the media channel is enabled for sending when the video sender
// has a track and SSRC, when the SSRC is set last.
TEST_F(RtpSenderReceiverTest, VideoSenderEarlyWarmupTrackThenSsrc) {
  AddVideoTrack();
  video_rtp_sender_ = new VideoRtpSender(video_channel_);
  video_rtp_sender_->SetTrack(video_track_);
  video_rtp_sender_->SetSsrc(kVideoSsrc);
  VerifyVideoChannelInput();

  DestroyVideoRtpSender();
}

// Test that the media channel stops sending when the audio sender's SSRC is set
// to 0.
TEST_F(RtpSenderReceiverTest, AudioSenderSsrcSetToZero) {
  CreateAudioRtpSender();

  audio_rtp_sender_->SetSsrc(0);
  VerifyVoiceChannelNoInput();
}

// Test that the media channel stops sending when the video sender's SSRC is set
// to 0.
TEST_F(RtpSenderReceiverTest, VideoSenderSsrcSetToZero) {
  CreateAudioRtpSender();

  audio_rtp_sender_->SetSsrc(0);
  VerifyVideoChannelNoInput();
}

// Test that the media channel stops sending when the audio sender's track is
// set to null.
TEST_F(RtpSenderReceiverTest, AudioSenderTrackSetToNull) {
  CreateAudioRtpSender();

  EXPECT_TRUE(audio_rtp_sender_->SetTrack(nullptr));
  VerifyVoiceChannelNoInput();
}

// Test that the media channel stops sending when the video sender's track is
// set to null.
TEST_F(RtpSenderReceiverTest, VideoSenderTrackSetToNull) {
  CreateVideoRtpSender();

  video_rtp_sender_->SetSsrc(0);
  VerifyVideoChannelNoInput();
}

// Test that when the audio sender's SSRC is changed, the media channel stops
// sending with the old SSRC and starts sending with the new one.
TEST_F(RtpSenderReceiverTest, AudioSenderSsrcChanged) {
  CreateAudioRtpSender();

  audio_rtp_sender_->SetSsrc(kAudioSsrc2);
  VerifyVoiceChannelNoInput(kAudioSsrc);
  VerifyVoiceChannelInput(kAudioSsrc2);

  audio_rtp_sender_ = nullptr;
  VerifyVoiceChannelNoInput(kAudioSsrc2);
}

// Test that when the audio sender's SSRC is changed, the media channel stops
// sending with the old SSRC and starts sending with the new one.
TEST_F(RtpSenderReceiverTest, VideoSenderSsrcChanged) {
  CreateVideoRtpSender();

  video_rtp_sender_->SetSsrc(kVideoSsrc2);
  VerifyVideoChannelNoInput(kVideoSsrc);
  VerifyVideoChannelInput(kVideoSsrc2);

  video_rtp_sender_ = nullptr;
  VerifyVideoChannelNoInput(kVideoSsrc2);
}

TEST_F(RtpSenderReceiverTest, AudioSenderCanSetParameters) {
  CreateAudioRtpSender();

  RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params));

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, SetAudioMaxSendBitrate) {
  CreateAudioRtpSender();

  EXPECT_EQ(-1, voice_media_channel_->max_bps());
  webrtc::RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1, params.encodings.size());
  EXPECT_FALSE(params.encodings[0].max_bitrate_bps);
  params.encodings[0].max_bitrate_bps = rtc::Optional<int>(1000);
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params));

  // Read back the parameters and verify they have been changed.
  params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1, params.encodings.size());
  EXPECT_EQ(rtc::Optional<int>(1000), params.encodings[0].max_bitrate_bps);

  // Verify that the audio channel received the new parameters.
  params = voice_media_channel_->GetRtpSendParameters(kAudioSsrc);
  EXPECT_EQ(1, params.encodings.size());
  EXPECT_EQ(rtc::Optional<int>(1000), params.encodings[0].max_bitrate_bps);

  // Verify that the global bitrate limit has not been changed.
  EXPECT_EQ(-1, voice_media_channel_->max_bps());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCanSetParameters) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params));

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, SetVideoMaxSendBitrate) {
  CreateVideoRtpSender();

  EXPECT_EQ(-1, video_media_channel_->max_bps());
  webrtc::RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1, params.encodings.size());
  EXPECT_FALSE(params.encodings[0].max_bitrate_bps);
  params.encodings[0].max_bitrate_bps = rtc::Optional<int>(1000);
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params));

  // Read back the parameters and verify they have been changed.
  params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1, params.encodings.size());
  EXPECT_EQ(rtc::Optional<int>(1000), params.encodings[0].max_bitrate_bps);

  // Verify that the video channel received the new parameters.
  params = video_media_channel_->GetRtpSendParameters(kVideoSsrc);
  EXPECT_EQ(1, params.encodings.size());
  EXPECT_EQ(rtc::Optional<int>(1000), params.encodings[0].max_bitrate_bps);

  // Verify that the global bitrate limit has not been changed.
  EXPECT_EQ(-1, video_media_channel_->max_bps());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioReceiverCanSetParameters) {
  CreateAudioRtpReceiver();

  RtpParameters params = audio_rtp_receiver_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  EXPECT_TRUE(audio_rtp_receiver_->SetParameters(params));

  DestroyAudioRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, VideoReceiverCanSetParameters) {
  CreateVideoRtpReceiver();

  RtpParameters params = video_rtp_receiver_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  EXPECT_TRUE(video_rtp_receiver_->SetParameters(params));

  DestroyVideoRtpReceiver();
}

// Test that makes sure that a video track content hint translates to the proper
// value for sources that are not screencast.
TEST_F(RtpSenderReceiverTest, PropagatesVideoTrackContentHint) {
  CreateVideoRtpSender();

  video_track_->set_enabled(true);

  // |video_track_| is not screencast by default.
  EXPECT_EQ(rtc::Optional<bool>(false),
            video_media_channel_->options().is_screencast);
  // No content hint should be set by default.
  EXPECT_EQ(VideoTrackInterface::ContentHint::kNone,
            video_track_->content_hint());
  // Setting detailed should turn a non-screencast source into screencast mode.
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kDetailed);
  EXPECT_EQ(rtc::Optional<bool>(true),
            video_media_channel_->options().is_screencast);
  // Removing the content hint should turn the track back into non-screencast
  // mode.
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kNone);
  EXPECT_EQ(rtc::Optional<bool>(false),
            video_media_channel_->options().is_screencast);
  // Setting fluid should remain in non-screencast mode (its default).
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kFluid);
  EXPECT_EQ(rtc::Optional<bool>(false),
            video_media_channel_->options().is_screencast);

  DestroyVideoRtpSender();
}

// Test that makes sure that a video track content hint translates to the proper
// value for screencast sources.
TEST_F(RtpSenderReceiverTest,
       PropagatesVideoTrackContentHintForScreencastSource) {
  CreateVideoRtpSender(true);

  video_track_->set_enabled(true);

  // |video_track_| with a screencast source should be screencast by default.
  EXPECT_EQ(rtc::Optional<bool>(true),
            video_media_channel_->options().is_screencast);
  // No content hint should be set by default.
  EXPECT_EQ(VideoTrackInterface::ContentHint::kNone,
            video_track_->content_hint());
  // Setting fluid should turn a screencast source into non-screencast mode.
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kFluid);
  EXPECT_EQ(rtc::Optional<bool>(false),
            video_media_channel_->options().is_screencast);
  // Removing the content hint should turn the track back into screencast mode.
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kNone);
  EXPECT_EQ(rtc::Optional<bool>(true),
            video_media_channel_->options().is_screencast);
  // Setting detailed should still remain in screencast mode (its default).
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kDetailed);
  EXPECT_EQ(rtc::Optional<bool>(true),
            video_media_channel_->options().is_screencast);

  DestroyVideoRtpSender();
}

// Test that makes sure any content hints that are set on a track before
// VideoRtpSender is ready to send are still applied when it gets ready to send.
TEST_F(RtpSenderReceiverTest,
       PropagatesVideoTrackContentHintSetBeforeEnabling) {
  AddVideoTrack();
  // Setting detailed overrides the default non-screencast mode. This should be
  // applied even if the track is set on construction.
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kDetailed);
  video_rtp_sender_ =
      new VideoRtpSender(local_stream_->GetVideoTracks()[0],
                         local_stream_->label(), video_channel_);
  video_track_->set_enabled(true);

  // Sender is not ready to send (no SSRC) so no option should have been set.
  EXPECT_EQ(rtc::Optional<bool>(),
            video_media_channel_->options().is_screencast);

  // Verify that the content hint is accounted for when video_rtp_sender_ does
  // get enabled.
  video_rtp_sender_->SetSsrc(kVideoSsrc);
  EXPECT_EQ(rtc::Optional<bool>(true),
            video_media_channel_->options().is_screencast);

  // And removing the hint should go back to false (to verify that false was
  // default correctly).
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kNone);
  EXPECT_EQ(rtc::Optional<bool>(false),
            video_media_channel_->options().is_screencast);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderHasDtmfSender) {
  CreateAudioRtpSender();
  EXPECT_NE(nullptr, audio_rtp_sender_->GetDtmfSender());
}

TEST_F(RtpSenderReceiverTest, VideoSenderDoesNotHaveDtmfSender) {
  CreateVideoRtpSender();
  EXPECT_EQ(nullptr, video_rtp_sender_->GetDtmfSender());
}

// Test that the DTMF sender is really using |voice_channel_|, and thus returns
// true/false from CanSendDtmf based on what |voice_channel_| returns.
TEST_F(RtpSenderReceiverTest, CanInsertDtmf) {
  AddDtmfCodec();
  CreateAudioRtpSender();
  auto dtmf_sender = audio_rtp_sender_->GetDtmfSender();
  ASSERT_NE(nullptr, dtmf_sender);
  EXPECT_TRUE(dtmf_sender->CanInsertDtmf());
}

TEST_F(RtpSenderReceiverTest, CanNotInsertDtmf) {
  CreateAudioRtpSender();
  auto dtmf_sender = audio_rtp_sender_->GetDtmfSender();
  ASSERT_NE(nullptr, dtmf_sender);
  // DTMF codec has not been added, as it was in the above test.
  EXPECT_FALSE(dtmf_sender->CanInsertDtmf());
}

TEST_F(RtpSenderReceiverTest, InsertDtmf) {
  AddDtmfCodec();
  CreateAudioRtpSender();
  auto dtmf_sender = audio_rtp_sender_->GetDtmfSender();
  ASSERT_NE(nullptr, dtmf_sender);

  EXPECT_EQ(0U, voice_media_channel_->dtmf_info_queue().size());

  // Insert DTMF
  const int expected_duration = 90;
  dtmf_sender->InsertDtmf("012", expected_duration, 100);

  // Verify
  ASSERT_EQ_WAIT(3U, voice_media_channel_->dtmf_info_queue().size(),
                 kDefaultTimeout);
  const uint32_t send_ssrc =
      voice_media_channel_->send_streams()[0].first_ssrc();
  EXPECT_TRUE(CompareDtmfInfo(voice_media_channel_->dtmf_info_queue()[0],
                              send_ssrc, 0, expected_duration));
  EXPECT_TRUE(CompareDtmfInfo(voice_media_channel_->dtmf_info_queue()[1],
                              send_ssrc, 1, expected_duration));
  EXPECT_TRUE(CompareDtmfInfo(voice_media_channel_->dtmf_info_queue()[2],
                              send_ssrc, 2, expected_duration));
}

// Make sure the signal from "GetOnDestroyedSignal()" fires when the sender is
// destroyed, which is needed for the DTMF sender.
TEST_F(RtpSenderReceiverTest, TestOnDestroyedSignal) {
  CreateAudioRtpSender();
  EXPECT_FALSE(audio_sender_destroyed_signal_fired_);
  audio_rtp_sender_ = nullptr;
  EXPECT_TRUE(audio_sender_destroyed_signal_fired_);
}

}  // namespace webrtc
