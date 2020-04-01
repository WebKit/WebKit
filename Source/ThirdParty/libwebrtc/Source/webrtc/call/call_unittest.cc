/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/call.h"

#include <list>
#include <map>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/test/mock_audio_mixer.h"
#include "api/transport/field_trial_based_config.h"
#include "audio/audio_receive_stream.h"
#include "audio/audio_send_stream.h"
#include "call/audio_state.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "test/fake_encoder.h"
#include "test/gtest.h"
#include "test/mock_audio_decoder_factory.h"
#include "test/mock_transport.h"

namespace {

struct CallHelper {
  CallHelper() {
    task_queue_factory_ = webrtc::CreateDefaultTaskQueueFactory();
    webrtc::AudioState::Config audio_state_config;
    audio_state_config.audio_mixer =
        new rtc::RefCountedObject<webrtc::test::MockAudioMixer>();
    audio_state_config.audio_processing =
        new rtc::RefCountedObject<webrtc::test::MockAudioProcessing>();
    audio_state_config.audio_device_module =
        new rtc::RefCountedObject<webrtc::test::MockAudioDeviceModule>();
    webrtc::Call::Config config(&event_log_);
    config.audio_state = webrtc::AudioState::Create(audio_state_config);
    config.task_queue_factory = task_queue_factory_.get();
    config.trials = &field_trials_;
    call_.reset(webrtc::Call::Create(config));
  }

  webrtc::Call* operator->() { return call_.get(); }

 private:
  webrtc::RtcEventLogNull event_log_;
  webrtc::FieldTrialBasedConfig field_trials_;
  std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
  std::unique_ptr<webrtc::Call> call_;
};
}  // namespace

namespace webrtc {

TEST(CallTest, ConstructDestruct) {
  CallHelper call;
}

TEST(CallTest, CreateDestroy_AudioSendStream) {
  CallHelper call;
  MockTransport send_transport;
  AudioSendStream::Config config(&send_transport);
  config.rtp.ssrc = 42;
  AudioSendStream* stream = call->CreateAudioSendStream(config);
  EXPECT_NE(stream, nullptr);
  call->DestroyAudioSendStream(stream);
}

TEST(CallTest, CreateDestroy_AudioReceiveStream) {
  CallHelper call;
  AudioReceiveStream::Config config;
  MockTransport rtcp_send_transport;
  config.rtp.remote_ssrc = 42;
  config.rtcp_send_transport = &rtcp_send_transport;
  config.decoder_factory =
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>();
  AudioReceiveStream* stream = call->CreateAudioReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  call->DestroyAudioReceiveStream(stream);
}

TEST(CallTest, CreateDestroy_AudioSendStreams) {
  CallHelper call;
  MockTransport send_transport;
  AudioSendStream::Config config(&send_transport);
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
  MockTransport rtcp_send_transport;
  config.rtcp_send_transport = &rtcp_send_transport;
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
  MockTransport rtcp_send_transport;
  recv_config.rtp.remote_ssrc = 42;
  recv_config.rtp.local_ssrc = 777;
  recv_config.rtcp_send_transport = &rtcp_send_transport;
  recv_config.decoder_factory =
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>();
  AudioReceiveStream* recv_stream = call->CreateAudioReceiveStream(recv_config);
  EXPECT_NE(recv_stream, nullptr);

  MockTransport send_transport;
  AudioSendStream::Config send_config(&send_transport);
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
  MockTransport send_transport;
  AudioSendStream::Config send_config(&send_transport);
  send_config.rtp.ssrc = 777;
  AudioSendStream* send_stream = call->CreateAudioSendStream(send_config);
  EXPECT_NE(send_stream, nullptr);

  AudioReceiveStream::Config recv_config;
  MockTransport rtcp_send_transport;
  recv_config.rtp.remote_ssrc = 42;
  recv_config.rtp.local_ssrc = 777;
  recv_config.rtcp_send_transport = &rtcp_send_transport;
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

TEST(CallTest, RecreatingAudioStreamWithSameSsrcReusesRtpState) {
  constexpr uint32_t kSSRC = 12345;
  CallHelper call;

  auto create_stream_and_get_rtp_state = [&](uint32_t ssrc) {
    MockTransport send_transport;
    AudioSendStream::Config config(&send_transport);
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

}  // namespace webrtc
