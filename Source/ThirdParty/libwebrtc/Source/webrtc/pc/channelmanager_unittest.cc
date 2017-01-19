/*
 *  Copyright 2008 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/fakemediacontroller.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/media/base/fakemediaengine.h"
#include "webrtc/media/base/fakevideocapturer.h"
#include "webrtc/media/base/testutils.h"
#include "webrtc/media/engine/fakewebrtccall.h"
#include "webrtc/p2p/base/faketransportcontroller.h"
#include "webrtc/pc/channelmanager.h"

namespace cricket {

static const AudioCodec kAudioCodecs[] = {
    AudioCodec(97, "voice", 1, 2, 3), AudioCodec(111, "OPUS", 48000, 32000, 2),
};

static const VideoCodec kVideoCodecs[] = {
    VideoCodec(99, "H264"), VideoCodec(100, "VP8"), VideoCodec(96, "rtx"),
};

class ChannelManagerTest : public testing::Test {
 protected:
  ChannelManagerTest()
      : fme_(new cricket::FakeMediaEngine()),
        fdme_(new cricket::FakeDataEngine()),
        cm_(new cricket::ChannelManager(fme_, fdme_, rtc::Thread::Current())),
        fake_call_(webrtc::Call::Config(&event_log_)),
        fake_mc_(cm_, &fake_call_),
        transport_controller_(
            new cricket::FakeTransportController(ICEROLE_CONTROLLING)) {}

  virtual void SetUp() {
    fme_->SetAudioCodecs(MAKE_VECTOR(kAudioCodecs));
    fme_->SetVideoCodecs(MAKE_VECTOR(kVideoCodecs));
  }

  virtual void TearDown() {
    delete transport_controller_;
    delete cm_;
    cm_ = NULL;
    fdme_ = NULL;
    fme_ = NULL;
  }

  webrtc::RtcEventLogNullImpl event_log_;
  rtc::Thread network_;
  rtc::Thread worker_;
  cricket::FakeMediaEngine* fme_;
  cricket::FakeDataEngine* fdme_;
  cricket::ChannelManager* cm_;
  cricket::FakeCall fake_call_;
  cricket::FakeMediaController fake_mc_;
  cricket::FakeTransportController* transport_controller_;
};

// Test that we startup/shutdown properly.
TEST_F(ChannelManagerTest, StartupShutdown) {
  EXPECT_FALSE(cm_->initialized());
  EXPECT_EQ(rtc::Thread::Current(), cm_->worker_thread());
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->initialized());
  cm_->Terminate();
  EXPECT_FALSE(cm_->initialized());
}

// Test that we startup/shutdown properly with a worker thread.
TEST_F(ChannelManagerTest, StartupShutdownOnThread) {
  network_.Start();
  worker_.Start();
  EXPECT_FALSE(cm_->initialized());
  EXPECT_EQ(rtc::Thread::Current(), cm_->worker_thread());
  EXPECT_TRUE(cm_->set_network_thread(&network_));
  EXPECT_EQ(&network_, cm_->network_thread());
  EXPECT_TRUE(cm_->set_worker_thread(&worker_));
  EXPECT_EQ(&worker_, cm_->worker_thread());
  EXPECT_TRUE(cm_->Init());
  EXPECT_TRUE(cm_->initialized());
  // Setting the network or worker thread while initialized should fail.
  EXPECT_FALSE(cm_->set_network_thread(rtc::Thread::Current()));
  EXPECT_FALSE(cm_->set_worker_thread(rtc::Thread::Current()));
  cm_->Terminate();
  EXPECT_FALSE(cm_->initialized());
}

// Test that we can create and destroy a voice and video channel.
TEST_F(ChannelManagerTest, CreateDestroyChannels) {
  EXPECT_TRUE(cm_->Init());
  cricket::VoiceChannel* voice_channel = cm_->CreateVoiceChannel(
      &fake_mc_, transport_controller_, cricket::CN_AUDIO, nullptr, false,
      AudioOptions());
  EXPECT_TRUE(voice_channel != nullptr);
  cricket::VideoChannel* video_channel = cm_->CreateVideoChannel(
      &fake_mc_, transport_controller_, cricket::CN_VIDEO, nullptr, false,
      VideoOptions());
  EXPECT_TRUE(video_channel != nullptr);
  cricket::DataChannel* data_channel =
      cm_->CreateDataChannel(transport_controller_, cricket::CN_DATA, nullptr,
                             false, cricket::DCT_RTP);
  EXPECT_TRUE(data_channel != nullptr);
  cm_->DestroyVideoChannel(video_channel);
  cm_->DestroyVoiceChannel(voice_channel);
  cm_->DestroyDataChannel(data_channel);
  cm_->Terminate();
}

// Test that we can create and destroy a voice and video channel with a worker.
TEST_F(ChannelManagerTest, CreateDestroyChannelsOnThread) {
  network_.Start();
  worker_.Start();
  EXPECT_TRUE(cm_->set_worker_thread(&worker_));
  EXPECT_TRUE(cm_->set_network_thread(&network_));
  EXPECT_TRUE(cm_->Init());
  delete transport_controller_;
  transport_controller_ =
      new cricket::FakeTransportController(&network_, ICEROLE_CONTROLLING);
  cricket::VoiceChannel* voice_channel = cm_->CreateVoiceChannel(
      &fake_mc_, transport_controller_, cricket::CN_AUDIO, nullptr, false,
      AudioOptions());
  EXPECT_TRUE(voice_channel != nullptr);
  cricket::VideoChannel* video_channel = cm_->CreateVideoChannel(
      &fake_mc_, transport_controller_, cricket::CN_VIDEO, nullptr, false,
      VideoOptions());
  EXPECT_TRUE(video_channel != nullptr);
  cricket::DataChannel* data_channel =
      cm_->CreateDataChannel(transport_controller_, cricket::CN_DATA, nullptr,
                             false, cricket::DCT_RTP);
  EXPECT_TRUE(data_channel != nullptr);
  cm_->DestroyVideoChannel(video_channel);
  cm_->DestroyVoiceChannel(voice_channel);
  cm_->DestroyDataChannel(data_channel);
  cm_->Terminate();
}

// Test that we fail to create a voice/video channel if the session is unable
// to create a cricket::TransportChannel
TEST_F(ChannelManagerTest, NoTransportChannelTest) {
  EXPECT_TRUE(cm_->Init());
  transport_controller_->set_fail_channel_creation(true);
  // The test is useless unless the session does not fail creating
  // cricket::TransportChannel.
  ASSERT_TRUE(transport_controller_->CreateTransportChannel_n(
                  "audio", cricket::ICE_CANDIDATE_COMPONENT_RTP) == nullptr);

  cricket::VoiceChannel* voice_channel = cm_->CreateVoiceChannel(
      &fake_mc_, transport_controller_, cricket::CN_AUDIO, nullptr, false,
      AudioOptions());
  EXPECT_TRUE(voice_channel == nullptr);
  cricket::VideoChannel* video_channel = cm_->CreateVideoChannel(
      &fake_mc_, transport_controller_, cricket::CN_VIDEO, nullptr, false,
      VideoOptions());
  EXPECT_TRUE(video_channel == nullptr);
  cricket::DataChannel* data_channel =
      cm_->CreateDataChannel(transport_controller_, cricket::CN_DATA, nullptr,
                             false, cricket::DCT_RTP);
  EXPECT_TRUE(data_channel == nullptr);
  cm_->Terminate();
}

TEST_F(ChannelManagerTest, SetVideoRtxEnabled) {
  std::vector<VideoCodec> codecs;
  const VideoCodec rtx_codec(96, "rtx");

  // By default RTX is disabled.
  cm_->GetSupportedVideoCodecs(&codecs);
  EXPECT_FALSE(ContainsMatchingCodec(codecs, rtx_codec));

  // Enable and check.
  EXPECT_TRUE(cm_->SetVideoRtxEnabled(true));
  cm_->GetSupportedVideoCodecs(&codecs);
  EXPECT_TRUE(ContainsMatchingCodec(codecs, rtx_codec));

  // Disable and check.
  EXPECT_TRUE(cm_->SetVideoRtxEnabled(false));
  cm_->GetSupportedVideoCodecs(&codecs);
  EXPECT_FALSE(ContainsMatchingCodec(codecs, rtx_codec));

  // Cannot toggle rtx after initialization.
  EXPECT_TRUE(cm_->Init());
  EXPECT_FALSE(cm_->SetVideoRtxEnabled(true));
  EXPECT_FALSE(cm_->SetVideoRtxEnabled(false));

  // Can set again after terminate.
  cm_->Terminate();
  EXPECT_TRUE(cm_->SetVideoRtxEnabled(true));
  cm_->GetSupportedVideoCodecs(&codecs);
  EXPECT_TRUE(ContainsMatchingCodec(codecs, rtx_codec));
}

}  // namespace cricket
