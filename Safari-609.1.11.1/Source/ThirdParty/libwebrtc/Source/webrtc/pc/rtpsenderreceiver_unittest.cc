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

#include "api/rtpparameters.h"
#include "api/test/fake_frame_decryptor.h"
#include "api/test/fake_frame_encryptor.h"
#include "media/base/fakemediaengine.h"
#include "media/base/rtpdataengine.h"
#include "media/base/testutils.h"
#include "media/engine/fakewebrtccall.h"
#include "p2p/base/fakedtlstransport.h"
#include "pc/audiotrack.h"
#include "pc/channelmanager.h"
#include "pc/localaudiosource.h"
#include "pc/mediastream.h"
#include "pc/remoteaudiosource.h"
#include "pc/rtpreceiver.h"
#include "pc/rtpsender.h"
#include "pc/streamcollection.h"
#include "pc/test/fakevideotracksource.h"
#include "pc/videotrack.h"
#include "pc/videotracksource.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace {

static const char kStreamId1[] = "local_stream_1";
static const char kVideoTrackId[] = "video_1";
static const char kAudioTrackId[] = "audio_1";
static const uint32_t kVideoSsrc = 98;
static const uint32_t kVideoSsrc2 = 100;
static const uint32_t kAudioSsrc = 99;
static const uint32_t kAudioSsrc2 = 101;
static const uint32_t kVideoSsrcSimulcast = 102;
static const uint32_t kVideoSimulcastLayerCount = 2;
static const int kDefaultTimeout = 10000;  // 10 seconds.
}  // namespace

namespace webrtc {

class RtpSenderReceiverTest : public testing::Test,
                              public sigslot::has_slots<> {
 public:
  RtpSenderReceiverTest()
      : network_thread_(rtc::Thread::Current()),
        worker_thread_(rtc::Thread::Current()),
        // Create fake media engine/etc. so we can create channels to use to
        // test RtpSenders/RtpReceivers.
        media_engine_(new cricket::FakeMediaEngine()),
        channel_manager_(absl::WrapUnique(media_engine_),
                         absl::make_unique<cricket::RtpDataEngine>(),
                         worker_thread_,
                         network_thread_),
        fake_call_(),
        local_stream_(MediaStream::Create(kStreamId1)) {
    // Create channels to be used by the RtpSenders and RtpReceivers.
    channel_manager_.Init();
    bool srtp_required = true;
    rtp_dtls_transport_ = absl::make_unique<cricket::FakeDtlsTransport>(
        "fake_dtls_transport", cricket::ICE_CANDIDATE_COMPONENT_RTP);
    rtp_transport_ = CreateDtlsSrtpTransport();

    voice_channel_ = channel_manager_.CreateVoiceChannel(
        &fake_call_, cricket::MediaConfig(), rtp_transport_.get(),
        /*media_transport=*/nullptr, rtc::Thread::Current(), cricket::CN_AUDIO,
        srtp_required, webrtc::CryptoOptions(), cricket::AudioOptions());
    video_channel_ = channel_manager_.CreateVideoChannel(
        &fake_call_, cricket::MediaConfig(), rtp_transport_.get(),
        rtc::Thread::Current(), cricket::CN_VIDEO, srtp_required,
        webrtc::CryptoOptions(), cricket::VideoOptions());
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

  std::unique_ptr<webrtc::RtpTransportInternal> CreateDtlsSrtpTransport() {
    auto dtls_srtp_transport = absl::make_unique<webrtc::DtlsSrtpTransport>(
        /*rtcp_mux_required=*/true);
    dtls_srtp_transport->SetDtlsTransports(rtp_dtls_transport_.get(),
                                           /*rtcp_dtls_transport=*/nullptr);
    return dtls_srtp_transport;
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
    video_track_ =
        VideoTrack::Create(kVideoTrackId, source, rtc::Thread::Current());
    EXPECT_TRUE(local_stream_->AddTrack(video_track_));
  }

  void CreateAudioRtpSender() { CreateAudioRtpSender(nullptr); }

  void CreateAudioRtpSender(
      const rtc::scoped_refptr<LocalAudioSource>& source) {
    audio_track_ = AudioTrack::Create(kAudioTrackId, source);
    EXPECT_TRUE(local_stream_->AddTrack(audio_track_));
    audio_rtp_sender_ =
        new AudioRtpSender(worker_thread_, audio_track_->id(), nullptr);
    ASSERT_TRUE(audio_rtp_sender_->SetTrack(audio_track_));
    audio_rtp_sender_->set_stream_ids({local_stream_->id()});
    audio_rtp_sender_->SetMediaChannel(voice_media_channel_);
    audio_rtp_sender_->SetSsrc(kAudioSsrc);
    audio_rtp_sender_->GetOnDestroyedSignal()->connect(
        this, &RtpSenderReceiverTest::OnAudioSenderDestroyed);
    VerifyVoiceChannelInput();
  }

  void CreateAudioRtpSenderWithNoTrack() {
    audio_rtp_sender_ = new AudioRtpSender(worker_thread_, /*id=*/"", nullptr);
    audio_rtp_sender_->SetMediaChannel(voice_media_channel_);
  }

  void OnAudioSenderDestroyed() { audio_sender_destroyed_signal_fired_ = true; }

  void CreateVideoRtpSender(uint32_t ssrc) {
    CreateVideoRtpSender(false, ssrc);
  }

  void CreateVideoRtpSender() { CreateVideoRtpSender(false); }

  void CreateVideoRtpSenderWithSimulcast(
      int num_layers = kVideoSimulcastLayerCount) {
    std::vector<uint32_t> ssrcs;
    for (int i = 0; i < num_layers; ++i)
      ssrcs.push_back(kVideoSsrcSimulcast + i);
    cricket::StreamParams stream_params =
        cricket::CreateSimStreamParams("cname", ssrcs);
    video_media_channel_->AddSendStream(stream_params);
    uint32_t primary_ssrc = stream_params.first_ssrc();
    CreateVideoRtpSender(primary_ssrc);
  }

  void CreateVideoRtpSender(bool is_screencast, uint32_t ssrc = kVideoSsrc) {
    AddVideoTrack(is_screencast);
    video_rtp_sender_ = new VideoRtpSender(worker_thread_, video_track_->id());
    ASSERT_TRUE(video_rtp_sender_->SetTrack(video_track_));
    video_rtp_sender_->set_stream_ids({local_stream_->id()});
    video_rtp_sender_->SetMediaChannel(video_media_channel_);
    video_rtp_sender_->SetSsrc(ssrc);
    VerifyVideoChannelInput(ssrc);
  }
  void CreateVideoRtpSenderWithNoTrack() {
    video_rtp_sender_ = new VideoRtpSender(worker_thread_, /*id=*/"");
    video_rtp_sender_->SetMediaChannel(video_media_channel_);
  }

  void DestroyAudioRtpSender() {
    audio_rtp_sender_ = nullptr;
    VerifyVoiceChannelNoInput();
  }

  void DestroyVideoRtpSender() {
    video_rtp_sender_ = nullptr;
    VerifyVideoChannelNoInput();
  }

  void CreateAudioRtpReceiver(
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams = {}) {
    audio_rtp_receiver_ = new AudioRtpReceiver(
        rtc::Thread::Current(), kAudioTrackId, std::move(streams));
    audio_rtp_receiver_->SetMediaChannel(voice_media_channel_);
    audio_rtp_receiver_->SetupMediaChannel(kAudioSsrc);
    audio_track_ = audio_rtp_receiver_->audio_track();
    VerifyVoiceChannelOutput();
  }

  void CreateVideoRtpReceiver(
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams = {}) {
    video_rtp_receiver_ = new VideoRtpReceiver(
        rtc::Thread::Current(), kVideoTrackId, std::move(streams));
    video_rtp_receiver_->SetMediaChannel(video_media_channel_);
    video_rtp_receiver_->SetupMediaChannel(kVideoSsrc);
    video_track_ = video_rtp_receiver_->video_track();
    VerifyVideoChannelOutput();
  }

  void CreateVideoRtpReceiverWithSimulcast(
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams = {},
      int num_layers = kVideoSimulcastLayerCount) {
    std::vector<uint32_t> ssrcs;
    for (int i = 0; i < num_layers; ++i)
      ssrcs.push_back(kVideoSsrcSimulcast + i);
    cricket::StreamParams stream_params =
        cricket::CreateSimStreamParams("cname", ssrcs);
    video_media_channel_->AddRecvStream(stream_params);
    uint32_t primary_ssrc = stream_params.first_ssrc();

    video_rtp_receiver_ = new VideoRtpReceiver(
        rtc::Thread::Current(), kVideoTrackId, std::move(streams));
    video_rtp_receiver_->SetMediaChannel(video_media_channel_);
    video_rtp_receiver_->SetupMediaChannel(primary_ssrc);
    video_track_ = video_rtp_receiver_->video_track();
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
  rtc::Thread* const network_thread_;
  rtc::Thread* const worker_thread_;
  webrtc::RtcEventLogNullImpl event_log_;
  // The |rtp_dtls_transport_| and |rtp_transport_| should be destroyed after
  // the |channel_manager|.
  std::unique_ptr<cricket::DtlsTransportInternal> rtp_dtls_transport_;
  std::unique_ptr<webrtc::RtpTransportInternal> rtp_transport_;
  // |media_engine_| is actually owned by |channel_manager_|.
  cricket::FakeMediaEngine* media_engine_;
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

TEST_F(RtpSenderReceiverTest, AddAndDestroyAudioRtpReceiverWithStreams) {
  CreateAudioRtpReceiver({local_stream_});
  DestroyAudioRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, AddAndDestroyVideoRtpReceiverWithStreams) {
  CreateVideoRtpReceiver({local_stream_});
  DestroyVideoRtpReceiver();
}

// Test that the AudioRtpSender applies options from the local audio source.
TEST_F(RtpSenderReceiverTest, LocalAudioSourceOptionsApplied) {
  cricket::AudioOptions options;
  options.echo_cancellation = true;
  auto source = LocalAudioSource::Create(&options);
  CreateAudioRtpSender(source.get());

  EXPECT_EQ(true, voice_media_channel_->options().echo_cancellation);

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
  CreateAudioRtpSenderWithNoTrack();
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
  CreateVideoRtpSenderWithNoTrack();

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
  CreateAudioRtpSenderWithNoTrack();
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
  CreateAudioRtpSenderWithNoTrack();
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
  CreateVideoRtpSenderWithNoTrack();
  video_rtp_sender_->SetSsrc(kVideoSsrc);
  video_rtp_sender_->SetTrack(video_track_);
  VerifyVideoChannelInput();

  DestroyVideoRtpSender();
}

// Test that the media channel is enabled for sending when the video sender
// has a track and SSRC, when the SSRC is set last.
TEST_F(RtpSenderReceiverTest, VideoSenderEarlyWarmupTrackThenSsrc) {
  AddVideoTrack();
  CreateVideoRtpSenderWithNoTrack();
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
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params).ok());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderCanSetParametersBeforeNegotiation) {
  audio_rtp_sender_ = new AudioRtpSender(worker_thread_, /*id=*/"", nullptr);

  RtpParameters params = audio_rtp_sender_->GetParameters();
  ASSERT_EQ(1u, params.encodings.size());
  params.encodings[0].max_bitrate_bps = 90000;
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params).ok());

  params = audio_rtp_sender_->GetParameters();
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params).ok());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 90000);

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderInitParametersMovedAfterNegotiation) {
  audio_track_ = AudioTrack::Create(kAudioTrackId, nullptr);
  EXPECT_TRUE(local_stream_->AddTrack(audio_track_));

  audio_rtp_sender_ =
      new AudioRtpSender(worker_thread_, audio_track_->id(), nullptr);
  ASSERT_TRUE(audio_rtp_sender_->SetTrack(audio_track_));
  audio_rtp_sender_->set_stream_ids({local_stream_->id()});

  std::vector<RtpEncodingParameters> init_encodings(1);
  init_encodings[0].max_bitrate_bps = 60000;
  audio_rtp_sender_->set_init_send_encodings(init_encodings);

  RtpParameters params = audio_rtp_sender_->GetParameters();
  ASSERT_EQ(1u, params.encodings.size());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 60000);

  // Simulate the setLocalDescription call
  std::vector<uint32_t> ssrcs(1, 1);
  cricket::StreamParams stream_params =
      cricket::CreateSimStreamParams("cname", ssrcs);
  voice_media_channel_->AddSendStream(stream_params);
  audio_rtp_sender_->SetMediaChannel(voice_media_channel_);
  audio_rtp_sender_->SetSsrc(1);

  params = audio_rtp_sender_->GetParameters();
  ASSERT_EQ(1u, params.encodings.size());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 60000);

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       AudioSenderMustCallGetParametersBeforeSetParametersBeforeNegotiation) {
  audio_rtp_sender_ = new AudioRtpSender(worker_thread_, /*id=*/"", nullptr);

  RtpParameters params;
  RTCError result = audio_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());
  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       AudioSenderMustCallGetParametersBeforeSetParameters) {
  CreateAudioRtpSender();

  RtpParameters params;
  RTCError result = audio_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       AudioSenderSetParametersInvalidatesTransactionId) {
  CreateAudioRtpSender();

  RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params).ok());
  RTCError result = audio_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderDetectTransactionIdModification) {
  CreateAudioRtpSender();

  RtpParameters params = audio_rtp_sender_->GetParameters();
  params.transaction_id = "";
  RTCError result = audio_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_MODIFICATION, result.type());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderCheckTransactionIdRefresh) {
  CreateAudioRtpSender();

  RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_NE(params.transaction_id.size(), 0U);
  auto saved_transaction_id = params.transaction_id;
  params = audio_rtp_sender_->GetParameters();
  EXPECT_NE(saved_transaction_id, params.transaction_id);

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderSetParametersOldValueFail) {
  CreateAudioRtpSender();

  RtpParameters params = audio_rtp_sender_->GetParameters();
  RtpParameters second_params = audio_rtp_sender_->GetParameters();

  RTCError result = audio_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_MODIFICATION, result.type());
  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderCantSetUnimplementedRtpParameters) {
  CreateAudioRtpSender();
  RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());

  // Unimplemented RtpParameters: mid
  params.mid = "dummy_mid";
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            audio_rtp_sender_->SetParameters(params).type());
  params = audio_rtp_sender_->GetParameters();

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       AudioSenderCantSetUnimplementedRtpEncodingParameters) {
  CreateAudioRtpSender();
  RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());

  // Unimplemented RtpParameters: codec_payload_type, fec, rtx, dtx, ptime,
  // scale_resolution_down_by, scale_framerate_down_by, rid, dependency_rids.
  params.encodings[0].codec_payload_type = 1;
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            audio_rtp_sender_->SetParameters(params).type());
  params = audio_rtp_sender_->GetParameters();

  params.encodings[0].fec = RtpFecParameters();
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            audio_rtp_sender_->SetParameters(params).type());
  params = audio_rtp_sender_->GetParameters();

  params.encodings[0].rtx = RtpRtxParameters();
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            audio_rtp_sender_->SetParameters(params).type());
  params = audio_rtp_sender_->GetParameters();

  params.encodings[0].dtx = DtxStatus::ENABLED;
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            audio_rtp_sender_->SetParameters(params).type());
  params = audio_rtp_sender_->GetParameters();

  params.encodings[0].ptime = 1;
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            audio_rtp_sender_->SetParameters(params).type());
  params = audio_rtp_sender_->GetParameters();

  params.encodings[0].scale_resolution_down_by = 2.0;
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            audio_rtp_sender_->SetParameters(params).type());
  params = audio_rtp_sender_->GetParameters();

  params.encodings[0].rid = "dummy_rid";
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            audio_rtp_sender_->SetParameters(params).type());
  params = audio_rtp_sender_->GetParameters();

  params.encodings[0].dependency_rids.push_back("dummy_rid");
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            audio_rtp_sender_->SetParameters(params).type());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, SetAudioMaxSendBitrate) {
  CreateAudioRtpSender();

  EXPECT_EQ(-1, voice_media_channel_->max_bps());
  webrtc::RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_FALSE(params.encodings[0].max_bitrate_bps);
  params.encodings[0].max_bitrate_bps = 1000;
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params).ok());

  // Read back the parameters and verify they have been changed.
  params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(1000, params.encodings[0].max_bitrate_bps);

  // Verify that the audio channel received the new parameters.
  params = voice_media_channel_->GetRtpSendParameters(kAudioSsrc);
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(1000, params.encodings[0].max_bitrate_bps);

  // Verify that the global bitrate limit has not been changed.
  EXPECT_EQ(-1, voice_media_channel_->max_bps());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, SetAudioBitratePriority) {
  CreateAudioRtpSender();

  webrtc::RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(webrtc::kDefaultBitratePriority,
            params.encodings[0].bitrate_priority);
  double new_bitrate_priority = 2.0;
  params.encodings[0].bitrate_priority = new_bitrate_priority;
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params).ok());

  params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(new_bitrate_priority, params.encodings[0].bitrate_priority);

  params = voice_media_channel_->GetRtpSendParameters(kAudioSsrc);
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(new_bitrate_priority, params.encodings[0].bitrate_priority);

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCanSetParameters) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCanSetParametersBeforeNegotiation) {
  video_rtp_sender_ = new VideoRtpSender(worker_thread_, /*id=*/"");

  RtpParameters params = video_rtp_sender_->GetParameters();
  ASSERT_EQ(1u, params.encodings.size());
  params.encodings[0].max_bitrate_bps = 90000;
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());

  params = video_rtp_sender_->GetParameters();
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 90000);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderInitParametersMovedAfterNegotiation) {
  AddVideoTrack(false);

  video_rtp_sender_ = new VideoRtpSender(worker_thread_, video_track_->id());
  ASSERT_TRUE(video_rtp_sender_->SetTrack(video_track_));
  video_rtp_sender_->set_stream_ids({local_stream_->id()});

  std::vector<RtpEncodingParameters> init_encodings(2);
  init_encodings[0].max_bitrate_bps = 60000;
  init_encodings[1].max_bitrate_bps = 900000;
  video_rtp_sender_->set_init_send_encodings(init_encodings);

  RtpParameters params = video_rtp_sender_->GetParameters();
  ASSERT_EQ(2u, params.encodings.size());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 60000);
  EXPECT_EQ(params.encodings[1].max_bitrate_bps, 900000);

  // Simulate the setLocalDescription call
  std::vector<uint32_t> ssrcs;
  for (int i = 0; i < 2; ++i)
    ssrcs.push_back(kVideoSsrcSimulcast + i);
  cricket::StreamParams stream_params =
      cricket::CreateSimStreamParams("cname", ssrcs);
  video_media_channel_->AddSendStream(stream_params);
  video_rtp_sender_->SetMediaChannel(video_media_channel_);
  video_rtp_sender_->SetSsrc(kVideoSsrcSimulcast);

  params = video_rtp_sender_->GetParameters();
  ASSERT_EQ(2u, params.encodings.size());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 60000);
  EXPECT_EQ(params.encodings[1].max_bitrate_bps, 900000);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       VideoSenderInitParametersMovedAfterManualSimulcastAndNegotiation) {
  AddVideoTrack(false);

  video_rtp_sender_ = new VideoRtpSender(worker_thread_, video_track_->id());
  ASSERT_TRUE(video_rtp_sender_->SetTrack(video_track_));
  video_rtp_sender_->set_stream_ids({local_stream_->id()});

  std::vector<RtpEncodingParameters> init_encodings(1);
  init_encodings[0].max_bitrate_bps = 60000;
  video_rtp_sender_->set_init_send_encodings(init_encodings);

  RtpParameters params = video_rtp_sender_->GetParameters();
  ASSERT_EQ(1u, params.encodings.size());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 60000);

  // Simulate the setLocalDescription call as if the user used SDP munging
  // to enable simulcast
  std::vector<uint32_t> ssrcs;
  for (int i = 0; i < 2; ++i)
    ssrcs.push_back(kVideoSsrcSimulcast + i);
  cricket::StreamParams stream_params =
      cricket::CreateSimStreamParams("cname", ssrcs);
  video_media_channel_->AddSendStream(stream_params);
  video_rtp_sender_->SetMediaChannel(video_media_channel_);
  video_rtp_sender_->SetSsrc(kVideoSsrcSimulcast);

  params = video_rtp_sender_->GetParameters();
  ASSERT_EQ(2u, params.encodings.size());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 60000);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       VideoSenderMustCallGetParametersBeforeSetParametersBeforeNegotiation) {
  video_rtp_sender_ = new VideoRtpSender(worker_thread_, /*id=*/"");

  RtpParameters params;
  RTCError result = video_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());
  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       VideoSenderMustCallGetParametersBeforeSetParameters) {
  CreateVideoRtpSender();

  RtpParameters params;
  RTCError result = video_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       VideoSenderSetParametersInvalidatesTransactionId) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());
  RTCError result = video_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderDetectTransactionIdModification) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  params.transaction_id = "";
  RTCError result = video_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_MODIFICATION, result.type());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCheckTransactionIdRefresh) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_NE(params.transaction_id.size(), 0U);
  auto saved_transaction_id = params.transaction_id;
  params = video_rtp_sender_->GetParameters();
  EXPECT_NE(saved_transaction_id, params.transaction_id);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderSetParametersOldValueFail) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  RtpParameters second_params = video_rtp_sender_->GetParameters();

  RTCError result = video_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_MODIFICATION, result.type());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCantSetUnimplementedRtpParameters) {
  CreateVideoRtpSender();
  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());

  // Unimplemented RtpParameters: mid
  params.mid = "dummy_mid";
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            video_rtp_sender_->SetParameters(params).type());
  params = video_rtp_sender_->GetParameters();

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       VideoSenderCantSetUnimplementedEncodingParameters) {
  CreateVideoRtpSender();
  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());

  // Unimplemented RtpParameters: codec_payload_type, fec, rtx, dtx, ptime,
  // scale_resolution_down_by, scale_framerate_down_by, rid, dependency_rids.
  params.encodings[0].codec_payload_type = 1;
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            video_rtp_sender_->SetParameters(params).type());
  params = video_rtp_sender_->GetParameters();

  params.encodings[0].fec = RtpFecParameters();
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            video_rtp_sender_->SetParameters(params).type());
  params = video_rtp_sender_->GetParameters();

  params.encodings[0].rtx = RtpRtxParameters();
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            video_rtp_sender_->SetParameters(params).type());
  params = video_rtp_sender_->GetParameters();

  params.encodings[0].dtx = DtxStatus::ENABLED;
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            video_rtp_sender_->SetParameters(params).type());
  params = video_rtp_sender_->GetParameters();

  params.encodings[0].ptime = 1;
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            video_rtp_sender_->SetParameters(params).type());
  params = video_rtp_sender_->GetParameters();

  params.encodings[0].scale_resolution_down_by = 2.0;
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            video_rtp_sender_->SetParameters(params).type());
  params = video_rtp_sender_->GetParameters();

  params.encodings[0].rid = "dummy_rid";
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            video_rtp_sender_->SetParameters(params).type());
  params = video_rtp_sender_->GetParameters();

  params.encodings[0].dependency_rids.push_back("dummy_rid");
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            video_rtp_sender_->SetParameters(params).type());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       VideoSenderCantSetUnimplementedEncodingParametersWithSimulcast) {
  CreateVideoRtpSenderWithSimulcast();
  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(kVideoSimulcastLayerCount, params.encodings.size());

  // Unimplemented RtpParameters: codec_payload_type, fec, rtx, dtx, ptime,
  // scale_resolution_down_by, scale_framerate_down_by, rid, dependency_rids.
  for (size_t i = 0; i < params.encodings.size(); i++) {
    params.encodings[i].codec_payload_type = 1;
    EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
              video_rtp_sender_->SetParameters(params).type());
    params = video_rtp_sender_->GetParameters();

    params.encodings[i].fec = RtpFecParameters();
    EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
              video_rtp_sender_->SetParameters(params).type());
    params = video_rtp_sender_->GetParameters();

    params.encodings[i].rtx = RtpRtxParameters();
    EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
              video_rtp_sender_->SetParameters(params).type());
    params = video_rtp_sender_->GetParameters();

    params.encodings[i].dtx = DtxStatus::ENABLED;
    EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
              video_rtp_sender_->SetParameters(params).type());
    params = video_rtp_sender_->GetParameters();

    params.encodings[i].ptime = 1;
    EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
              video_rtp_sender_->SetParameters(params).type());
    params = video_rtp_sender_->GetParameters();

    params.encodings[i].scale_resolution_down_by = 2.0;
    EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
              video_rtp_sender_->SetParameters(params).type());
    params = video_rtp_sender_->GetParameters();

    params.encodings[i].rid = "dummy_rid";
    EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
              video_rtp_sender_->SetParameters(params).type());
    params = video_rtp_sender_->GetParameters();

    params.encodings[i].dependency_rids.push_back("dummy_rid");
    EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
              video_rtp_sender_->SetParameters(params).type());
  }

  DestroyVideoRtpSender();
}

// A video sender can have multiple simulcast layers, in which case it will
// contain multiple RtpEncodingParameters. This tests that if this is the case
// (simulcast), then we can't set the bitrate_priority, or max_bitrate_bps
// for any encodings besides at index 0, because these are both implemented
// "per-sender."
TEST_F(RtpSenderReceiverTest, VideoSenderCantSetPerSenderEncodingParameters) {
  // Add a simulcast specific send stream that contains 2 encoding parameters.
  CreateVideoRtpSenderWithSimulcast();
  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(kVideoSimulcastLayerCount, params.encodings.size());

  params.encodings[1].bitrate_priority = 2.0;
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            video_rtp_sender_->SetParameters(params).type());
  params = video_rtp_sender_->GetParameters();

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCantSetReadOnlyEncodingParameters) {
  // Add a simulcast specific send stream that contains 2 encoding parameters.
  CreateVideoRtpSenderWithSimulcast();
  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(kVideoSimulcastLayerCount, params.encodings.size());

  for (size_t i = 0; i < params.encodings.size(); i++) {
    params.encodings[i].ssrc = 1337;
    EXPECT_EQ(RTCErrorType::INVALID_MODIFICATION,
              video_rtp_sender_->SetParameters(params).type());
    params = video_rtp_sender_->GetParameters();
  }

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, SetVideoMinMaxSendBitrate) {
  CreateVideoRtpSender();

  EXPECT_EQ(-1, video_media_channel_->max_bps());
  webrtc::RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_FALSE(params.encodings[0].min_bitrate_bps);
  EXPECT_FALSE(params.encodings[0].max_bitrate_bps);
  params.encodings[0].min_bitrate_bps = 100;
  params.encodings[0].max_bitrate_bps = 1000;
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());

  // Read back the parameters and verify they have been changed.
  params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(100, params.encodings[0].min_bitrate_bps);
  EXPECT_EQ(1000, params.encodings[0].max_bitrate_bps);

  // Verify that the video channel received the new parameters.
  params = video_media_channel_->GetRtpSendParameters(kVideoSsrc);
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(100, params.encodings[0].min_bitrate_bps);
  EXPECT_EQ(1000, params.encodings[0].max_bitrate_bps);

  // Verify that the global bitrate limit has not been changed.
  EXPECT_EQ(-1, video_media_channel_->max_bps());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, SetVideoMinMaxSendBitrateSimulcast) {
  // Add a simulcast specific send stream that contains 2 encoding parameters.
  CreateVideoRtpSenderWithSimulcast();

  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(kVideoSimulcastLayerCount, params.encodings.size());
  params.encodings[0].min_bitrate_bps = 100;
  params.encodings[0].max_bitrate_bps = 1000;
  params.encodings[1].min_bitrate_bps = 200;
  params.encodings[1].max_bitrate_bps = 2000;
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());

  // Verify that the video channel received the new parameters.
  params = video_media_channel_->GetRtpSendParameters(kVideoSsrcSimulcast);
  EXPECT_EQ(kVideoSimulcastLayerCount, params.encodings.size());
  EXPECT_EQ(100, params.encodings[0].min_bitrate_bps);
  EXPECT_EQ(1000, params.encodings[0].max_bitrate_bps);
  EXPECT_EQ(200, params.encodings[1].min_bitrate_bps);
  EXPECT_EQ(2000, params.encodings[1].max_bitrate_bps);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, SetVideoBitratePriority) {
  CreateVideoRtpSender();

  webrtc::RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(webrtc::kDefaultBitratePriority,
            params.encodings[0].bitrate_priority);
  double new_bitrate_priority = 2.0;
  params.encodings[0].bitrate_priority = new_bitrate_priority;
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());

  params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(new_bitrate_priority, params.encodings[0].bitrate_priority);

  params = video_media_channel_->GetRtpSendParameters(kVideoSsrc);
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(new_bitrate_priority, params.encodings[0].bitrate_priority);

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

TEST_F(RtpSenderReceiverTest, VideoReceiverCanGetParametersWithSimulcast) {
  CreateVideoRtpReceiverWithSimulcast({}, 2);

  RtpParameters params = video_rtp_receiver_->GetParameters();
  EXPECT_EQ(2u, params.encodings.size());

  DestroyVideoRtpReceiver();
}

// Test that makes sure that a video track content hint translates to the proper
// value for sources that are not screencast.
TEST_F(RtpSenderReceiverTest, PropagatesVideoTrackContentHint) {
  CreateVideoRtpSender();

  video_track_->set_enabled(true);

  // |video_track_| is not screencast by default.
  EXPECT_EQ(false, video_media_channel_->options().is_screencast);
  // No content hint should be set by default.
  EXPECT_EQ(VideoTrackInterface::ContentHint::kNone,
            video_track_->content_hint());
  // Setting detailed should turn a non-screencast source into screencast mode.
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kDetailed);
  EXPECT_EQ(true, video_media_channel_->options().is_screencast);
  // Removing the content hint should turn the track back into non-screencast
  // mode.
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kNone);
  EXPECT_EQ(false, video_media_channel_->options().is_screencast);
  // Setting fluid should remain in non-screencast mode (its default).
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kFluid);
  EXPECT_EQ(false, video_media_channel_->options().is_screencast);
  // Setting text should have the same effect as Detailed
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kText);
  EXPECT_EQ(true, video_media_channel_->options().is_screencast);

  DestroyVideoRtpSender();
}

// Test that makes sure that a video track content hint translates to the proper
// value for screencast sources.
TEST_F(RtpSenderReceiverTest,
       PropagatesVideoTrackContentHintForScreencastSource) {
  CreateVideoRtpSender(true);

  video_track_->set_enabled(true);

  // |video_track_| with a screencast source should be screencast by default.
  EXPECT_EQ(true, video_media_channel_->options().is_screencast);
  // No content hint should be set by default.
  EXPECT_EQ(VideoTrackInterface::ContentHint::kNone,
            video_track_->content_hint());
  // Setting fluid should turn a screencast source into non-screencast mode.
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kFluid);
  EXPECT_EQ(false, video_media_channel_->options().is_screencast);
  // Removing the content hint should turn the track back into screencast mode.
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kNone);
  EXPECT_EQ(true, video_media_channel_->options().is_screencast);
  // Setting detailed should still remain in screencast mode (its default).
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kDetailed);
  EXPECT_EQ(true, video_media_channel_->options().is_screencast);
  // Setting text should have the same effect as Detailed
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kText);
  EXPECT_EQ(true, video_media_channel_->options().is_screencast);

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
  video_rtp_sender_ = new VideoRtpSender(worker_thread_, video_track_->id());
  ASSERT_TRUE(video_rtp_sender_->SetTrack(video_track_));
  video_rtp_sender_->set_stream_ids({local_stream_->id()});
  video_rtp_sender_->SetMediaChannel(video_media_channel_);
  video_track_->set_enabled(true);

  // Sender is not ready to send (no SSRC) so no option should have been set.
  EXPECT_EQ(absl::nullopt, video_media_channel_->options().is_screencast);

  // Verify that the content hint is accounted for when video_rtp_sender_ does
  // get enabled.
  video_rtp_sender_->SetSsrc(kVideoSsrc);
  EXPECT_EQ(true, video_media_channel_->options().is_screencast);

  // And removing the hint should go back to false (to verify that false was
  // default correctly).
  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kNone);
  EXPECT_EQ(false, video_media_channel_->options().is_screencast);

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

// Validate that the default FrameEncryptor setting is nullptr.
TEST_F(RtpSenderReceiverTest, AudioSenderCanSetFrameEncryptor) {
  CreateAudioRtpSender();
  rtc::scoped_refptr<FrameEncryptorInterface> fake_frame_encryptor(
      new FakeFrameEncryptor());
  EXPECT_EQ(nullptr, audio_rtp_sender_->GetFrameEncryptor());
  audio_rtp_sender_->SetFrameEncryptor(fake_frame_encryptor);
  EXPECT_EQ(fake_frame_encryptor.get(),
            audio_rtp_sender_->GetFrameEncryptor().get());
}

// Validate that setting a FrameEncryptor after the send stream is stopped does
// nothing.
TEST_F(RtpSenderReceiverTest, AudioSenderCannotSetFrameEncryptorAfterStop) {
  CreateAudioRtpSender();
  rtc::scoped_refptr<FrameEncryptorInterface> fake_frame_encryptor(
      new FakeFrameEncryptor());
  EXPECT_EQ(nullptr, audio_rtp_sender_->GetFrameEncryptor());
  audio_rtp_sender_->Stop();
  audio_rtp_sender_->SetFrameEncryptor(fake_frame_encryptor);
  // TODO(webrtc:9926) - Validate media channel not set once fakes updated.
}

// Validate that the default FrameEncryptor setting is nullptr.
TEST_F(RtpSenderReceiverTest, AudioReceiverCanSetFrameDecryptor) {
  CreateAudioRtpReceiver();
  rtc::scoped_refptr<FrameDecryptorInterface> fake_frame_decryptor(
      new FakeFrameDecryptor());
  EXPECT_EQ(nullptr, audio_rtp_receiver_->GetFrameDecryptor());
  audio_rtp_receiver_->SetFrameDecryptor(fake_frame_decryptor);
  EXPECT_EQ(fake_frame_decryptor.get(),
            audio_rtp_receiver_->GetFrameDecryptor().get());
}

// Validate that the default FrameEncryptor setting is nullptr.
TEST_F(RtpSenderReceiverTest, AudioReceiverCannotSetFrameDecryptorAfterStop) {
  CreateAudioRtpReceiver();
  rtc::scoped_refptr<FrameDecryptorInterface> fake_frame_decryptor(
      new FakeFrameDecryptor());
  EXPECT_EQ(nullptr, audio_rtp_receiver_->GetFrameDecryptor());
  audio_rtp_receiver_->Stop();
  audio_rtp_receiver_->SetFrameDecryptor(fake_frame_decryptor);
  // TODO(webrtc:9926) - Validate media channel not set once fakes updated.
}

// Validate that the default FrameEncryptor setting is nullptr.
TEST_F(RtpSenderReceiverTest, VideoSenderCanSetFrameEncryptor) {
  CreateVideoRtpSender();
  rtc::scoped_refptr<FrameEncryptorInterface> fake_frame_encryptor(
      new FakeFrameEncryptor());
  EXPECT_EQ(nullptr, video_rtp_sender_->GetFrameEncryptor());
  video_rtp_sender_->SetFrameEncryptor(fake_frame_encryptor);
  EXPECT_EQ(fake_frame_encryptor.get(),
            video_rtp_sender_->GetFrameEncryptor().get());
}

// Validate that setting a FrameEncryptor after the send stream is stopped does
// nothing.
TEST_F(RtpSenderReceiverTest, VideoSenderCannotSetFrameEncryptorAfterStop) {
  CreateVideoRtpSender();
  rtc::scoped_refptr<FrameEncryptorInterface> fake_frame_encryptor(
      new FakeFrameEncryptor());
  EXPECT_EQ(nullptr, video_rtp_sender_->GetFrameEncryptor());
  video_rtp_sender_->Stop();
  video_rtp_sender_->SetFrameEncryptor(fake_frame_encryptor);
  // TODO(webrtc:9926) - Validate media channel not set once fakes updated.
}

// Validate that the default FrameEncryptor setting is nullptr.
TEST_F(RtpSenderReceiverTest, VideoReceiverCanSetFrameDecryptor) {
  CreateVideoRtpReceiver();
  rtc::scoped_refptr<FrameDecryptorInterface> fake_frame_decryptor(
      new FakeFrameDecryptor());
  EXPECT_EQ(nullptr, video_rtp_receiver_->GetFrameDecryptor());
  video_rtp_receiver_->SetFrameDecryptor(fake_frame_decryptor);
  EXPECT_EQ(fake_frame_decryptor.get(),
            video_rtp_receiver_->GetFrameDecryptor().get());
}

// Validate that the default FrameEncryptor setting is nullptr.
TEST_F(RtpSenderReceiverTest, VideoReceiverCannotSetFrameDecryptorAfterStop) {
  CreateVideoRtpReceiver();
  rtc::scoped_refptr<FrameDecryptorInterface> fake_frame_decryptor(
      new FakeFrameDecryptor());
  EXPECT_EQ(nullptr, video_rtp_receiver_->GetFrameDecryptor());
  video_rtp_receiver_->Stop();
  video_rtp_receiver_->SetFrameDecryptor(fake_frame_decryptor);
  // TODO(webrtc:9926) - Validate media channel not set once fakes updated.
}

}  // namespace webrtc
