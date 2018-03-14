/*
 *  Copyright 2008 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>

#include "logging/rtc_event_log/rtc_event_log.h"
#include "media/base/fakemediaengine.h"
#include "media/base/fakevideocapturer.h"
#include "media/base/testutils.h"
#include "media/engine/fakewebrtccall.h"
#include "pc/channelmanager.h"
#include "pc/test/faketransportcontroller.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"

namespace {
const bool kDefaultSrtpRequired = true;
}

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
      : network_(rtc::Thread::CreateWithSocketServer()),
        worker_(rtc::Thread::Create()),
        fme_(new cricket::FakeMediaEngine()),
        fdme_(new cricket::FakeDataEngine()),
        cm_(new cricket::ChannelManager(
            std::unique_ptr<MediaEngineInterface>(fme_),
            std::unique_ptr<DataEngineInterface>(fdme_),
            rtc::Thread::Current(),
            rtc::Thread::Current())),
        fake_call_(webrtc::Call::Config(&event_log_)),
        transport_controller_(
            new cricket::FakeTransportController(ICEROLE_CONTROLLING)) {
    fme_->SetAudioCodecs(MAKE_VECTOR(kAudioCodecs));
    fme_->SetVideoCodecs(MAKE_VECTOR(kVideoCodecs));
  }

  webrtc::RtcEventLogNullImpl event_log_;
  std::unique_ptr<rtc::Thread> network_;
  std::unique_ptr<rtc::Thread> worker_;
  // |fme_| and |fdme_| are actually owned by |cm_|.
  cricket::FakeMediaEngine* fme_;
  cricket::FakeDataEngine* fdme_;
  std::unique_ptr<cricket::ChannelManager> cm_;
  cricket::FakeCall fake_call_;
  std::unique_ptr<cricket::FakeTransportController> transport_controller_;
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
  network_->Start();
  worker_->Start();
  EXPECT_FALSE(cm_->initialized());
  EXPECT_EQ(rtc::Thread::Current(), cm_->worker_thread());
  EXPECT_TRUE(cm_->set_network_thread(network_.get()));
  EXPECT_EQ(network_.get(), cm_->network_thread());
  EXPECT_TRUE(cm_->set_worker_thread(worker_.get()));
  EXPECT_EQ(worker_.get(), cm_->worker_thread());
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
  cricket::DtlsTransportInternal* rtp_transport =
      transport_controller_->CreateDtlsTransport(
          cricket::CN_AUDIO, cricket::ICE_CANDIDATE_COMPONENT_RTP);
  cricket::VoiceChannel* voice_channel = cm_->CreateVoiceChannel(
      &fake_call_, cricket::MediaConfig(),
      rtp_transport, nullptr /*rtcp_transport*/,
      rtc::Thread::Current(), cricket::CN_AUDIO, kDefaultSrtpRequired,
      AudioOptions());
  EXPECT_TRUE(voice_channel != nullptr);
  cricket::VideoChannel* video_channel = cm_->CreateVideoChannel(
      &fake_call_, cricket::MediaConfig(),
      rtp_transport, nullptr /*rtcp_transport*/,
      rtc::Thread::Current(), cricket::CN_VIDEO, kDefaultSrtpRequired,
      VideoOptions());
  EXPECT_TRUE(video_channel != nullptr);
  cricket::RtpDataChannel* rtp_data_channel = cm_->CreateRtpDataChannel(
      cricket::MediaConfig(), rtp_transport, nullptr /*rtcp_transport*/,
      rtc::Thread::Current(), cricket::CN_DATA, kDefaultSrtpRequired);
  EXPECT_TRUE(rtp_data_channel != nullptr);
  cm_->DestroyVideoChannel(video_channel);
  cm_->DestroyVoiceChannel(voice_channel);
  cm_->DestroyRtpDataChannel(rtp_data_channel);
  cm_->Terminate();
}

// Test that we can create and destroy a voice and video channel with a worker.
TEST_F(ChannelManagerTest, CreateDestroyChannelsOnThread) {
  network_->Start();
  worker_->Start();
  EXPECT_TRUE(cm_->set_worker_thread(worker_.get()));
  EXPECT_TRUE(cm_->set_network_thread(network_.get()));
  EXPECT_TRUE(cm_->Init());
  transport_controller_.reset(new cricket::FakeTransportController(
      network_.get(), ICEROLE_CONTROLLING));
  cricket::DtlsTransportInternal* rtp_transport =
      transport_controller_->CreateDtlsTransport(
          cricket::CN_AUDIO, cricket::ICE_CANDIDATE_COMPONENT_RTP);
  cricket::VoiceChannel* voice_channel = cm_->CreateVoiceChannel(
      &fake_call_, cricket::MediaConfig(),
      rtp_transport, nullptr /*rtcp_transport*/,
      rtc::Thread::Current(), cricket::CN_AUDIO, kDefaultSrtpRequired,
      AudioOptions());
  EXPECT_TRUE(voice_channel != nullptr);
  cricket::VideoChannel* video_channel = cm_->CreateVideoChannel(
      &fake_call_, cricket::MediaConfig(),
      rtp_transport, nullptr /*rtcp_transport*/,
      rtc::Thread::Current(), cricket::CN_VIDEO, kDefaultSrtpRequired,
      VideoOptions());
  EXPECT_TRUE(video_channel != nullptr);
  cricket::RtpDataChannel* rtp_data_channel = cm_->CreateRtpDataChannel(
      cricket::MediaConfig(), rtp_transport, nullptr /*rtcp_transport*/,
      rtc::Thread::Current(), cricket::CN_DATA, kDefaultSrtpRequired);
  EXPECT_TRUE(rtp_data_channel != nullptr);
  cm_->DestroyVideoChannel(video_channel);
  cm_->DestroyVoiceChannel(voice_channel);
  cm_->DestroyRtpDataChannel(rtp_data_channel);
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

enum class RTPTransportType { kRtp, kSrtp, kDtlsSrtp };

class ChannelManagerTestWithRtpTransport
    : public ChannelManagerTest,
      public ::testing::WithParamInterface<RTPTransportType> {
 public:
  std::unique_ptr<webrtc::RtpTransportInternal> CreateRtpTransport() {
    RTPTransportType type = GetParam();
    switch (type) {
      case RTPTransportType::kRtp:
        return CreatePlainRtpTransport();
      case RTPTransportType::kSrtp:
        return CreateSrtpTransport();
      case RTPTransportType::kDtlsSrtp:
        return CreateDtlsSrtpTransport();
    }
    return nullptr;
  }

  void TestCreateDestroyChannels(webrtc::RtpTransportInternal* rtp_transport) {
    cricket::VoiceChannel* voice_channel = cm_->CreateVoiceChannel(
        &fake_call_, cricket::MediaConfig(), rtp_transport,
        rtc::Thread::Current(), cricket::CN_AUDIO, kDefaultSrtpRequired,
        AudioOptions());
    EXPECT_TRUE(voice_channel != nullptr);
    cricket::VideoChannel* video_channel = cm_->CreateVideoChannel(
        &fake_call_, cricket::MediaConfig(), rtp_transport,
        rtc::Thread::Current(), cricket::CN_VIDEO, kDefaultSrtpRequired,
        VideoOptions());
    EXPECT_TRUE(video_channel != nullptr);
    cricket::RtpDataChannel* rtp_data_channel = cm_->CreateRtpDataChannel(
        cricket::MediaConfig(), rtp_transport, rtc::Thread::Current(),
        cricket::CN_DATA, kDefaultSrtpRequired);
    EXPECT_TRUE(rtp_data_channel != nullptr);
    cm_->DestroyVideoChannel(video_channel);
    cm_->DestroyVoiceChannel(voice_channel);
    cm_->DestroyRtpDataChannel(rtp_data_channel);
    cm_->Terminate();
  }

 private:
  std::unique_ptr<webrtc::RtpTransportInternal> CreatePlainRtpTransport() {
    return rtc::MakeUnique<webrtc::RtpTransport>(/*rtcp_mux_required=*/true);
  }

  std::unique_ptr<webrtc::RtpTransportInternal> CreateSrtpTransport() {
    auto rtp_transport =
        rtc::MakeUnique<webrtc::RtpTransport>(/*rtcp_mux_required=*/true);
    auto srtp_transport =
        rtc::MakeUnique<webrtc::SrtpTransport>(std::move(rtp_transport));
    return srtp_transport;
  }

  std::unique_ptr<webrtc::RtpTransportInternal> CreateDtlsSrtpTransport() {
    auto rtp_transport =
        rtc::MakeUnique<webrtc::RtpTransport>(/*rtcp_mux_required=*/true);
    auto srtp_transport =
        rtc::MakeUnique<webrtc::SrtpTransport>(std::move(rtp_transport));
    auto dtls_srtp_transport_ =
        rtc::MakeUnique<webrtc::DtlsSrtpTransport>(std::move(srtp_transport));
    return dtls_srtp_transport_;
  }
};

TEST_P(ChannelManagerTestWithRtpTransport, CreateDestroyChannels) {
  EXPECT_TRUE(cm_->Init());
  auto rtp_transport = CreateRtpTransport();
  TestCreateDestroyChannels(rtp_transport.get());
}

TEST_P(ChannelManagerTestWithRtpTransport, CreateDestroyChannelsOnThread) {
  network_->Start();
  worker_->Start();
  EXPECT_TRUE(cm_->set_worker_thread(worker_.get()));
  EXPECT_TRUE(cm_->set_network_thread(network_.get()));
  EXPECT_TRUE(cm_->Init());
  auto rtp_transport = CreateRtpTransport();
  TestCreateDestroyChannels(rtp_transport.get());
}

INSTANTIATE_TEST_CASE_P(ChannelManagerTest,
                        ChannelManagerTestWithRtpTransport,
                        ::testing::Values(RTPTransportType::kRtp,
                                          RTPTransportType::kSrtp,
                                          RTPTransportType::kDtlsSrtp));
}  // namespace cricket
