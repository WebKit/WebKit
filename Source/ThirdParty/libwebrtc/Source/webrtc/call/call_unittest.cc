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
#include <memory>

#include "webrtc/call/audio_state.h"
#include "webrtc/call/call.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/modules/audio_coding/codecs/mock/mock_audio_decoder_factory.h"
#include "webrtc/modules/audio_mixer/audio_mixer_impl.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_transport.h"
#include "webrtc/test/mock_voice_engine.h"

namespace {

struct CallHelper {
  explicit CallHelper(
      rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory = nullptr)
      : voice_engine_(decoder_factory) {
    webrtc::AudioState::Config audio_state_config;
    audio_state_config.voice_engine = &voice_engine_;
    audio_state_config.audio_mixer = webrtc::AudioMixerImpl::Create();
    EXPECT_CALL(voice_engine_, audio_device_module());
    EXPECT_CALL(voice_engine_, audio_processing());
    EXPECT_CALL(voice_engine_, audio_transport());
    webrtc::Call::Config config(&event_log_);
    config.audio_state = webrtc::AudioState::Create(audio_state_config);
    call_.reset(webrtc::Call::Create(config));
  }

  webrtc::Call* operator->() { return call_.get(); }
  webrtc::test::MockVoiceEngine* voice_engine() { return &voice_engine_; }

 private:
  testing::NiceMock<webrtc::test::MockVoiceEngine> voice_engine_;
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
  config.voe_channel_id = 123;
  AudioSendStream* stream = call->CreateAudioSendStream(config);
  EXPECT_NE(stream, nullptr);
  call->DestroyAudioSendStream(stream);
}

TEST(CallTest, CreateDestroy_AudioReceiveStream) {
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory(
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>);
  CallHelper call(decoder_factory);
  AudioReceiveStream::Config config;
  config.rtp.remote_ssrc = 42;
  config.voe_channel_id = 123;
  config.decoder_factory = decoder_factory;
  AudioReceiveStream* stream = call->CreateAudioReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  call->DestroyAudioReceiveStream(stream);
}

TEST(CallTest, CreateDestroy_AudioSendStreams) {
  CallHelper call;
  AudioSendStream::Config config(nullptr);
  config.voe_channel_id = 123;
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
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory(
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>);
  CallHelper call(decoder_factory);
  AudioReceiveStream::Config config;
  config.voe_channel_id = 123;
  config.decoder_factory = decoder_factory;
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
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory(
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>);
  CallHelper call(decoder_factory);

  constexpr int kRecvChannelId = 101;

  // Set up the mock to create a channel proxy which we know of, so that we can
  // add our expectations to it.
  test::MockVoEChannelProxy* recv_channel_proxy = nullptr;
  EXPECT_CALL(*call.voice_engine(), ChannelProxyFactory(testing::_))
      .WillRepeatedly(testing::Invoke([&](int channel_id) {
        test::MockVoEChannelProxy* channel_proxy =
            new testing::NiceMock<test::MockVoEChannelProxy>();
        EXPECT_CALL(*channel_proxy, GetAudioDecoderFactory())
            .WillRepeatedly(testing::ReturnRef(decoder_factory));
        // If being called for the send channel, save a pointer to the channel
        // proxy for later.
        if (channel_id == kRecvChannelId) {
          EXPECT_FALSE(recv_channel_proxy);
          recv_channel_proxy = channel_proxy;
        }
        return channel_proxy;
      }));

  AudioReceiveStream::Config recv_config;
  recv_config.rtp.remote_ssrc = 42;
  recv_config.rtp.local_ssrc = 777;
  recv_config.voe_channel_id = kRecvChannelId;
  recv_config.decoder_factory = decoder_factory;
  AudioReceiveStream* recv_stream = call->CreateAudioReceiveStream(recv_config);
  EXPECT_NE(recv_stream, nullptr);

  EXPECT_CALL(*recv_channel_proxy, AssociateSendChannel(testing::_)).Times(1);
  AudioSendStream::Config send_config(nullptr);
  send_config.rtp.ssrc = 777;
  send_config.voe_channel_id = 123;
  AudioSendStream* send_stream = call->CreateAudioSendStream(send_config);
  EXPECT_NE(send_stream, nullptr);

  EXPECT_CALL(*recv_channel_proxy, DisassociateSendChannel()).Times(1);
  call->DestroyAudioSendStream(send_stream);

  EXPECT_CALL(*recv_channel_proxy, DisassociateSendChannel()).Times(1);
  call->DestroyAudioReceiveStream(recv_stream);
}

TEST(CallTest, CreateDestroy_AssociateAudioSendReceiveStreams_SendFirst) {
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory(
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>);
  CallHelper call(decoder_factory);

  constexpr int kRecvChannelId = 101;

  // Set up the mock to create a channel proxy which we know of, so that we can
  // add our expectations to it.
  test::MockVoEChannelProxy* recv_channel_proxy = nullptr;
  EXPECT_CALL(*call.voice_engine(), ChannelProxyFactory(testing::_))
      .WillRepeatedly(testing::Invoke([&](int channel_id) {
        test::MockVoEChannelProxy* channel_proxy =
            new testing::NiceMock<test::MockVoEChannelProxy>();
        EXPECT_CALL(*channel_proxy, GetAudioDecoderFactory())
            .WillRepeatedly(testing::ReturnRef(decoder_factory));
        // If being called for the send channel, save a pointer to the channel
        // proxy for later.
        if (channel_id == kRecvChannelId) {
          EXPECT_FALSE(recv_channel_proxy);
          recv_channel_proxy = channel_proxy;
          // We need to set this expectation here since the channel proxy is
          // created as a side effect of CreateAudioReceiveStream().
          EXPECT_CALL(*recv_channel_proxy,
                      AssociateSendChannel(testing::_)).Times(1);
        }
        return channel_proxy;
      }));

  AudioSendStream::Config send_config(nullptr);
  send_config.rtp.ssrc = 777;
  send_config.voe_channel_id = 123;
  AudioSendStream* send_stream = call->CreateAudioSendStream(send_config);
  EXPECT_NE(send_stream, nullptr);

  AudioReceiveStream::Config recv_config;
  recv_config.rtp.remote_ssrc = 42;
  recv_config.rtp.local_ssrc = 777;
  recv_config.voe_channel_id = kRecvChannelId;
  recv_config.decoder_factory = decoder_factory;
  AudioReceiveStream* recv_stream = call->CreateAudioReceiveStream(recv_config);
  EXPECT_NE(recv_stream, nullptr);

  EXPECT_CALL(*recv_channel_proxy, DisassociateSendChannel()).Times(1);
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

}  // namespace webrtc
