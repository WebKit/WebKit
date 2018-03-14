/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <list>
#include <map>
#include <memory>
#include <utility>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/test/mock_audio_mixer.h"
#include "audio/audio_send_stream.h"
#include "audio/audio_receive_stream.h"
#include "call/audio_state.h"
#include "call/call.h"
#include "call/fake_rtp_transport_controller_send.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "modules/congestion_controller/include/mock/mock_send_side_congestion_controller.h"
#include "modules/pacing/mock/mock_paced_sender.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "rtc_base/ptr_util.h"
#include "test/fake_encoder.h"
#include "test/gtest.h"
#include "test/mock_audio_decoder_factory.h"
#include "test/mock_transport.h"

namespace {

struct CallHelper {
  CallHelper() {
    webrtc::AudioState::Config audio_state_config;
    audio_state_config.audio_mixer =
        new rtc::RefCountedObject<webrtc::test::MockAudioMixer>();
    audio_state_config.audio_processing =
        new rtc::RefCountedObject<webrtc::test::MockAudioProcessing>();
    audio_state_config.audio_device_module =
        new rtc::RefCountedObject<webrtc::test::MockAudioDeviceModule>();
    webrtc::Call::Config config(&event_log_);
    config.audio_state = webrtc::AudioState::Create(audio_state_config);
    call_.reset(webrtc::Call::Create(config));
  }

  webrtc::Call* operator->() { return call_.get(); }

 private:
  webrtc::RtcEventLogNullImpl event_log_;
  std::unique_ptr<webrtc::Call> call_;
};
}  // namespace

namespace webrtc {

TEST(CallTest, ConstructDestruct) {
  CallHelper call;
}

TEST(CallTest, CreateDestroy_AudioSendStream) {
  CallHelper call;
  AudioSendStream::Config config(nullptr);
  config.rtp.ssrc = 42;
  AudioSendStream* stream = call->CreateAudioSendStream(config);
  EXPECT_NE(stream, nullptr);
  call->DestroyAudioSendStream(stream);
}

TEST(CallTest, CreateDestroy_AudioReceiveStream) {
  CallHelper call;
  AudioReceiveStream::Config config;
  config.rtp.remote_ssrc = 42;
  config.decoder_factory =
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>();
  AudioReceiveStream* stream = call->CreateAudioReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  call->DestroyAudioReceiveStream(stream);
}

TEST(CallTest, CreateDestroy_AudioSendStreams) {
  CallHelper call;
  AudioSendStream::Config config(nullptr);
  std::list<AudioSendStream*> streams;
  for (int i = 0; i < 2; ++i) {
    for (uint32_t ssrc = 0; ssrc < 1234567; ssrc += 34567) {
      config.rtp.ssrc = ssrc;
      AudioSendStream* stream = call->CreateAudioSendStream(config);
      EXPECT_NE(stream, nullptr);
      if (ssrc & 1) {
        streams.push_back(stream);
      } else {
        streams.push_front(stream);
      }
    }
    for (auto s : streams) {
      call->DestroyAudioSendStream(s);
    }
    streams.clear();
  }
}

TEST(CallTest, CreateDestroy_AudioReceiveStreams) {
  CallHelper call;
  AudioReceiveStream::Config config;
  config.decoder_factory =
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>();
  std::list<AudioReceiveStream*> streams;
  for (int i = 0; i < 2; ++i) {
    for (uint32_t ssrc = 0; ssrc < 1234567; ssrc += 34567) {
      config.rtp.remote_ssrc = ssrc;
      AudioReceiveStream* stream = call->CreateAudioReceiveStream(config);
      EXPECT_NE(stream, nullptr);
      if (ssrc & 1) {
        streams.push_back(stream);
      } else {
        streams.push_front(stream);
      }
    }
    for (auto s : streams) {
      call->DestroyAudioReceiveStream(s);
    }
    streams.clear();
  }
}

TEST(CallTest, CreateDestroy_AssociateAudioSendReceiveStreams_RecvFirst) {
  CallHelper call;
  AudioReceiveStream::Config recv_config;
  recv_config.rtp.remote_ssrc = 42;
  recv_config.rtp.local_ssrc = 777;
  recv_config.decoder_factory =
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>();
  AudioReceiveStream* recv_stream = call->CreateAudioReceiveStream(recv_config);
  EXPECT_NE(recv_stream, nullptr);

  AudioSendStream::Config send_config(nullptr);
  send_config.rtp.ssrc = 777;
  AudioSendStream* send_stream = call->CreateAudioSendStream(send_config);
  EXPECT_NE(send_stream, nullptr);

  internal::AudioReceiveStream* internal_recv_stream =
      static_cast<internal::AudioReceiveStream*>(recv_stream);
  EXPECT_EQ(send_stream,
            internal_recv_stream->GetAssociatedSendStreamForTesting());

  call->DestroyAudioSendStream(send_stream);
  EXPECT_EQ(nullptr, internal_recv_stream->GetAssociatedSendStreamForTesting());

  call->DestroyAudioReceiveStream(recv_stream);
}

TEST(CallTest, CreateDestroy_AssociateAudioSendReceiveStreams_SendFirst) {
  CallHelper call;
  AudioSendStream::Config send_config(nullptr);
  send_config.rtp.ssrc = 777;
  AudioSendStream* send_stream = call->CreateAudioSendStream(send_config);
  EXPECT_NE(send_stream, nullptr);

  AudioReceiveStream::Config recv_config;
  recv_config.rtp.remote_ssrc = 42;
  recv_config.rtp.local_ssrc = 777;
  recv_config.decoder_factory =
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>();
  AudioReceiveStream* recv_stream = call->CreateAudioReceiveStream(recv_config);
  EXPECT_NE(recv_stream, nullptr);

  internal::AudioReceiveStream* internal_recv_stream =
      static_cast<internal::AudioReceiveStream*>(recv_stream);
  EXPECT_EQ(send_stream,
            internal_recv_stream->GetAssociatedSendStreamForTesting());

  call->DestroyAudioReceiveStream(recv_stream);

  call->DestroyAudioSendStream(send_stream);
}

TEST(CallTest, CreateDestroy_FlexfecReceiveStream) {
  CallHelper call;
  MockTransport rtcp_send_transport;
  FlexfecReceiveStream::Config config(&rtcp_send_transport);
  config.payload_type = 118;
  config.remote_ssrc = 38837212;
  config.protected_media_ssrcs = {27273};

  FlexfecReceiveStream* stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  call->DestroyFlexfecReceiveStream(stream);
}

TEST(CallTest, CreateDestroy_FlexfecReceiveStreams) {
  CallHelper call;
  MockTransport rtcp_send_transport;
  FlexfecReceiveStream::Config config(&rtcp_send_transport);
  config.payload_type = 118;
  std::list<FlexfecReceiveStream*> streams;

  for (int i = 0; i < 2; ++i) {
    for (uint32_t ssrc = 0; ssrc < 1234567; ssrc += 34567) {
      config.remote_ssrc = ssrc;
      config.protected_media_ssrcs = {ssrc + 1};
      FlexfecReceiveStream* stream = call->CreateFlexfecReceiveStream(config);
      EXPECT_NE(stream, nullptr);
      if (ssrc & 1) {
        streams.push_back(stream);
      } else {
        streams.push_front(stream);
      }
    }
    for (auto s : streams) {
      call->DestroyFlexfecReceiveStream(s);
    }
    streams.clear();
  }
}

TEST(CallTest, MultipleFlexfecReceiveStreamsProtectingSingleVideoStream) {
  CallHelper call;
  MockTransport rtcp_send_transport;
  FlexfecReceiveStream::Config config(&rtcp_send_transport);
  config.payload_type = 118;
  config.protected_media_ssrcs = {1324234};
  FlexfecReceiveStream* stream;
  std::list<FlexfecReceiveStream*> streams;

  config.remote_ssrc = 838383;
  stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  streams.push_back(stream);

  config.remote_ssrc = 424993;
  stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  streams.push_back(stream);

  config.remote_ssrc = 99383;
  stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  streams.push_back(stream);

  config.remote_ssrc = 5548;
  stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  streams.push_back(stream);

  for (auto s : streams) {
    call->DestroyFlexfecReceiveStream(s);
  }
}

namespace {
struct CallBitrateHelper {
  CallBitrateHelper() : CallBitrateHelper(Call::Config::BitrateConfig()) {}

  explicit CallBitrateHelper(const Call::Config::BitrateConfig& bitrate_config)
      : mock_cc_(Clock::GetRealTimeClock(), &event_log_, &pacer_) {
    Call::Config config(&event_log_);
    config.bitrate_config = bitrate_config;
    call_.reset(
        Call::Create(config, rtc::MakeUnique<FakeRtpTransportControllerSend>(
                                 &packet_router_, &pacer_, &mock_cc_)));
  }

  webrtc::Call* operator->() { return call_.get(); }
  testing::NiceMock<test::MockSendSideCongestionController>& mock_cc() {
    return mock_cc_;
  }

 private:
  webrtc::RtcEventLogNullImpl event_log_;
  PacketRouter packet_router_;
  testing::NiceMock<MockPacedSender> pacer_;
  testing::NiceMock<test::MockSendSideCongestionController> mock_cc_;
  std::unique_ptr<Call> call_;
};
}  // namespace

TEST(CallBitrateTest, SetBitrateConfigWithValidConfigCallsSetBweBitrates) {
  CallBitrateHelper call;

  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 1;
  bitrate_config.start_bitrate_bps = 2;
  bitrate_config.max_bitrate_bps = 3;

  EXPECT_CALL(call.mock_cc(), SetBweBitrates(1, 2, 3));
  call->SetBitrateConfig(bitrate_config);
}

TEST(CallBitrateTest, SetBitrateConfigWithDifferentMinCallsSetBweBitrates) {
  CallBitrateHelper call;

  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 10;
  bitrate_config.start_bitrate_bps = 20;
  bitrate_config.max_bitrate_bps = 30;
  call->SetBitrateConfig(bitrate_config);

  bitrate_config.min_bitrate_bps = 11;
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(11, -1, 30));
  call->SetBitrateConfig(bitrate_config);
}

TEST(CallBitrateTest, SetBitrateConfigWithDifferentStartCallsSetBweBitrates) {
  CallBitrateHelper call;

  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 10;
  bitrate_config.start_bitrate_bps = 20;
  bitrate_config.max_bitrate_bps = 30;
  call->SetBitrateConfig(bitrate_config);

  bitrate_config.start_bitrate_bps = 21;
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(10, 21, 30));
  call->SetBitrateConfig(bitrate_config);
}

TEST(CallBitrateTest, SetBitrateConfigWithDifferentMaxCallsSetBweBitrates) {
  CallBitrateHelper call;

  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 10;
  bitrate_config.start_bitrate_bps = 20;
  bitrate_config.max_bitrate_bps = 30;
  call->SetBitrateConfig(bitrate_config);

  bitrate_config.max_bitrate_bps = 31;
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(10, -1, 31));
  call->SetBitrateConfig(bitrate_config);
}

TEST(CallBitrateTest, SetBitrateConfigWithSameConfigElidesSecondCall) {
  CallBitrateHelper call;
  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 1;
  bitrate_config.start_bitrate_bps = 2;
  bitrate_config.max_bitrate_bps = 3;

  EXPECT_CALL(call.mock_cc(), SetBweBitrates(1, 2, 3)).Times(1);
  call->SetBitrateConfig(bitrate_config);
  call->SetBitrateConfig(bitrate_config);
}

TEST(CallBitrateTest,
     SetBitrateConfigWithSameMinMaxAndNegativeStartElidesSecondCall) {
  CallBitrateHelper call;

  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 1;
  bitrate_config.start_bitrate_bps = 2;
  bitrate_config.max_bitrate_bps = 3;

  EXPECT_CALL(call.mock_cc(), SetBweBitrates(1, 2, 3)).Times(1);
  call->SetBitrateConfig(bitrate_config);

  bitrate_config.start_bitrate_bps = -1;
  call->SetBitrateConfig(bitrate_config);
}

TEST(CallTest, RecreatingAudioStreamWithSameSsrcReusesRtpState) {
  constexpr uint32_t kSSRC = 12345;
  CallHelper call;

  auto create_stream_and_get_rtp_state = [&](uint32_t ssrc) {
    AudioSendStream::Config config(nullptr);
    config.rtp.ssrc = ssrc;
    AudioSendStream* stream = call->CreateAudioSendStream(config);
    const RtpState rtp_state =
        static_cast<internal::AudioSendStream*>(stream)->GetRtpState();
    call->DestroyAudioSendStream(stream);
    return rtp_state;
  };

  const RtpState rtp_state1 = create_stream_and_get_rtp_state(kSSRC);
  const RtpState rtp_state2 = create_stream_and_get_rtp_state(kSSRC);

  EXPECT_EQ(rtp_state1.sequence_number, rtp_state2.sequence_number);
  EXPECT_EQ(rtp_state1.start_timestamp, rtp_state2.start_timestamp);
  EXPECT_EQ(rtp_state1.timestamp, rtp_state2.timestamp);
  EXPECT_EQ(rtp_state1.capture_time_ms, rtp_state2.capture_time_ms);
  EXPECT_EQ(rtp_state1.last_timestamp_time_ms,
            rtp_state2.last_timestamp_time_ms);
  EXPECT_EQ(rtp_state1.media_has_been_sent, rtp_state2.media_has_been_sent);
}

TEST(CallBitrateTest, BiggerMaskMinUsed) {
  CallBitrateHelper call;
  Call::Config::BitrateConfigMask mask;
  mask.min_bitrate_bps = 1234;

  EXPECT_CALL(call.mock_cc(),
              SetBweBitrates(*mask.min_bitrate_bps, testing::_, testing::_));
  call->SetBitrateConfigMask(mask);
}

TEST(CallBitrateTest, BiggerConfigMinUsed) {
  CallBitrateHelper call;
  Call::Config::BitrateConfigMask mask;
  mask.min_bitrate_bps = 1000;
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(1000, testing::_, testing::_));
  call->SetBitrateConfigMask(mask);

  Call::Config::BitrateConfig config;
  config.min_bitrate_bps = 1234;

  EXPECT_CALL(call.mock_cc(), SetBweBitrates(1234, testing::_, testing::_));
  call->SetBitrateConfig(config);
}

// The last call to set start should be used.
TEST(CallBitrateTest, LatestStartMaskPreferred) {
  CallBitrateHelper call;
  Call::Config::BitrateConfigMask mask;
  mask.start_bitrate_bps = 1300;

  EXPECT_CALL(call.mock_cc(),
              SetBweBitrates(testing::_, *mask.start_bitrate_bps, testing::_));
  call->SetBitrateConfigMask(mask);

  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.start_bitrate_bps = 1200;

  EXPECT_CALL(
      call.mock_cc(),
      SetBweBitrates(testing::_, bitrate_config.start_bitrate_bps, testing::_));
  call->SetBitrateConfig(bitrate_config);
}

TEST(CallBitrateTest, SmallerMaskMaxUsed) {
  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.max_bitrate_bps = bitrate_config.start_bitrate_bps + 2000;
  CallBitrateHelper call(bitrate_config);

  Call::Config::BitrateConfigMask mask;
  mask.max_bitrate_bps = bitrate_config.start_bitrate_bps + 1000;

  EXPECT_CALL(call.mock_cc(),
              SetBweBitrates(testing::_, testing::_, *mask.max_bitrate_bps));
  call->SetBitrateConfigMask(mask);
}

TEST(CallBitrateTest, SmallerConfigMaxUsed) {
  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.max_bitrate_bps = bitrate_config.start_bitrate_bps + 1000;
  CallBitrateHelper call(bitrate_config);

  Call::Config::BitrateConfigMask mask;
  mask.max_bitrate_bps = bitrate_config.start_bitrate_bps + 2000;

  // Expect no calls because nothing changes
  EXPECT_CALL(call.mock_cc(),
              SetBweBitrates(testing::_, testing::_, testing::_))
      .Times(0);
  call->SetBitrateConfigMask(mask);
}

TEST(CallBitrateTest, MaskStartLessThanConfigMinClamped) {
  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 2000;
  CallBitrateHelper call(bitrate_config);

  Call::Config::BitrateConfigMask mask;
  mask.start_bitrate_bps = 1000;

  EXPECT_CALL(call.mock_cc(), SetBweBitrates(2000, 2000, testing::_));
  call->SetBitrateConfigMask(mask);
}

TEST(CallBitrateTest, MaskStartGreaterThanConfigMaxClamped) {
  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.start_bitrate_bps = 2000;
  CallBitrateHelper call(bitrate_config);

  Call::Config::BitrateConfigMask mask;
  mask.max_bitrate_bps = 1000;

  EXPECT_CALL(call.mock_cc(), SetBweBitrates(testing::_, -1, 1000));
  call->SetBitrateConfigMask(mask);
}

TEST(CallBitrateTest, MaskMinGreaterThanConfigMaxClamped) {
  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 2000;
  CallBitrateHelper call(bitrate_config);

  Call::Config::BitrateConfigMask mask;
  mask.max_bitrate_bps = 1000;

  EXPECT_CALL(call.mock_cc(), SetBweBitrates(1000, testing::_, 1000));
  call->SetBitrateConfigMask(mask);
}

TEST(CallBitrateTest, SettingMaskStartForcesUpdate) {
  CallBitrateHelper call;

  Call::Config::BitrateConfigMask mask;
  mask.start_bitrate_bps = 1000;

  // SetBweBitrates should be called twice with the same params since
  // start_bitrate_bps is set.
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(testing::_, 1000, testing::_))
      .Times(2);
  call->SetBitrateConfigMask(mask);
  call->SetBitrateConfigMask(mask);
}

TEST(CallBitrateTest, SetBitrateConfigWithNoChangesDoesNotCallSetBweBitrates) {
  CallBitrateHelper call;

  Call::Config::BitrateConfig config1;
  config1.min_bitrate_bps = 0;
  config1.start_bitrate_bps = 1000;
  config1.max_bitrate_bps = -1;

  Call::Config::BitrateConfig config2;
  config2.min_bitrate_bps = 0;
  config2.start_bitrate_bps = -1;
  config2.max_bitrate_bps = -1;

  // The second call should not call SetBweBitrates because it doesn't
  // change any values.
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(0, 1000, -1));
  call->SetBitrateConfig(config1);
  call->SetBitrateConfig(config2);
}

// If SetBitrateConfig changes the max, but not the effective max,
// SetBweBitrates shouldn't be called, to avoid unnecessary encoder
// reconfigurations.
TEST(CallBitrateTest, SetBweBitratesNotCalledWhenEffectiveMaxUnchanged) {
  CallBitrateHelper call;

  Call::Config::BitrateConfig config;
  config.min_bitrate_bps = 0;
  config.start_bitrate_bps = -1;
  config.max_bitrate_bps = 2000;
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(testing::_, testing::_, 2000));
  call->SetBitrateConfig(config);

  // Reduce effective max to 1000 with the mask.
  Call::Config::BitrateConfigMask mask;
  mask.max_bitrate_bps = 1000;
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(testing::_, testing::_, 1000));
  call->SetBitrateConfigMask(mask);

  // This leaves the effective max unchanged, so SetBweBitrates shouldn't be
  // called again.
  config.max_bitrate_bps = 1000;
  call->SetBitrateConfig(config);
}

// When the "start bitrate" mask is removed, SetBweBitrates shouldn't be called
// again, since nothing's changing.
TEST(CallBitrateTest, SetBweBitratesNotCalledWhenStartMaskRemoved) {
  CallBitrateHelper call;

  Call::Config::BitrateConfigMask mask;
  mask.start_bitrate_bps = 1000;
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(0, 1000, -1));
  call->SetBitrateConfigMask(mask);

  mask.start_bitrate_bps.reset();
  call->SetBitrateConfigMask(mask);
}

// Test that if SetBitrateConfig is called after SetBitrateConfigMask applies a
// "start" value, the SetBitrateConfig call won't apply that start value a
// second time.
TEST(CallBitrateTest, SetBitrateConfigAfterSetBitrateConfigMaskWithStart) {
  CallBitrateHelper call;

  Call::Config::BitrateConfigMask mask;
  mask.start_bitrate_bps = 1000;
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(0, 1000, -1));
  call->SetBitrateConfigMask(mask);

  Call::Config::BitrateConfig config;
  config.min_bitrate_bps = 0;
  config.start_bitrate_bps = -1;
  config.max_bitrate_bps = 5000;
  // The start value isn't changing, so SetBweBitrates should be called with
  // -1.
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(0, -1, 5000));
  call->SetBitrateConfig(config);
}

TEST(CallBitrateTest, SetBweBitratesNotCalledWhenClampedMinUnchanged) {
  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.start_bitrate_bps = 500;
  bitrate_config.max_bitrate_bps = 1000;
  CallBitrateHelper call(bitrate_config);

  // Set min to 2000; it is clamped to the max (1000).
  Call::Config::BitrateConfigMask mask;
  mask.min_bitrate_bps = 2000;
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(1000, -1, 1000));
  call->SetBitrateConfigMask(mask);

  // Set min to 3000; the clamped value stays the same so nothing happens.
  mask.min_bitrate_bps = 3000;
  call->SetBitrateConfigMask(mask);
}

}  // namespace webrtc
