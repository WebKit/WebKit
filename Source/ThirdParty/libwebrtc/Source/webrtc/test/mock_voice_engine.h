/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_MOCK_VOICE_ENGINE_H_
#define AUDIO_MOCK_VOICE_ENGINE_H_

#include <memory>

#include "modules/audio_device/include/mock_audio_transport.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "test/gmock.h"
#include "test/mock_voe_channel_proxy.h"
#include "voice_engine/voice_engine_impl.h"

namespace webrtc {
namespace voe {
class TransmitMixer;
}  // namespace voe

namespace test {

// NOTE: This class inherits from VoiceEngineImpl so that its clients will be
// able to get the various interfaces as usual, via T::GetInterface().
class MockVoiceEngine : public VoiceEngineImpl {
 public:
  // TODO(nisse): Valid overrides commented out, because the gmock
  // methods don't use any override declarations, and we want to avoid
  // warnings from -Winconsistent-missing-override. See
  // http://crbug.com/428099.
  MockVoiceEngine(
      rtc::scoped_refptr<AudioDecoderFactory> decoder_factory = nullptr)
      : decoder_factory_(decoder_factory) {
    // Increase ref count so this object isn't automatically deleted whenever
    // interfaces are Release():d.
    ++_ref_count;
    // We add this default behavior to make the mock easier to use in tests. It
    // will create a NiceMock of a voe::ChannelProxy.
    // TODO(ossu): As long as AudioReceiveStream is implemented as a wrapper
    // around Channel, we need to make sure ChannelProxy returns the same
    // decoder factory as the one passed in when creating an AudioReceiveStream.
    ON_CALL(*this, ChannelProxyFactory(testing::_))
        .WillByDefault(testing::Invoke([this](int channel_id) {
          auto* proxy =
              new testing::NiceMock<webrtc::test::MockVoEChannelProxy>();
          EXPECT_CALL(*proxy, GetAudioDecoderFactory())
              .WillRepeatedly(testing::ReturnRef(decoder_factory_));
          EXPECT_CALL(*proxy, SetReceiveCodecs(testing::_))
              .WillRepeatedly(testing::Invoke(
                  [](const std::map<int, SdpAudioFormat>& codecs) {
                    EXPECT_THAT(codecs, testing::IsEmpty());
                  }));
          EXPECT_CALL(*proxy, GetRtpRtcp(testing::_, testing::_))
              .WillRepeatedly(
                  testing::SetArgPointee<0>(GetMockRtpRtcp(channel_id)));
          return proxy;
        }));

    ON_CALL(*this, audio_transport())
        .WillByDefault(testing::Return(&mock_audio_transport_));
  }
  virtual ~MockVoiceEngine() /* override */ {
    // Decrease ref count before base class d-tor is called; otherwise it will
    // trigger an assertion.
    --_ref_count;
  }

  // These need to be the same each call to channel_id and must not leak.
  MockRtpRtcp* GetMockRtpRtcp(int channel_id) {
    if (mock_rtp_rtcps_.find(channel_id) == mock_rtp_rtcps_.end()) {
      mock_rtp_rtcps_[channel_id].reset(new ::testing::NiceMock<MockRtpRtcp>);
    }
    return mock_rtp_rtcps_[channel_id].get();
  }

  // Allows injecting a ChannelProxy factory.
  MOCK_METHOD1(ChannelProxyFactory, voe::ChannelProxy*(int channel_id));

  // VoiceEngineImpl
  virtual std::unique_ptr<voe::ChannelProxy> GetChannelProxy(
      int channel_id) /* override */ {
    return std::unique_ptr<voe::ChannelProxy>(ChannelProxyFactory(channel_id));
  }

  // VoEBase
  MOCK_METHOD3(
      Init,
      int(AudioDeviceModule* external_adm,
          AudioProcessing* external_apm,
          const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory));
  MOCK_METHOD0(transmit_mixer, voe::TransmitMixer*());
  MOCK_METHOD0(Terminate, void());
  MOCK_METHOD0(CreateChannel, int());
  MOCK_METHOD1(CreateChannel, int(const ChannelConfig& config));
  MOCK_METHOD1(DeleteChannel, int(int channel));
  MOCK_METHOD1(StartPlayout, int(int channel));
  MOCK_METHOD1(StopPlayout, int(int channel));
  MOCK_METHOD1(StartSend, int(int channel));
  MOCK_METHOD1(StopSend, int(int channel));
  MOCK_METHOD0(audio_transport, AudioTransport*());

 private:
  // TODO(ossu): I'm not particularly happy about keeping the decoder factory
  // here, but due to how gmock is implemented, I cannot just keep it in the
  // functor implementing the default version of ChannelProxyFactory, above.
  // GMock creates an unfortunate copy of the functor, which would cause us to
  // return a dangling reference. Fortunately, this should go away once
  // voe::Channel does.
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;

  std::map<int, std::unique_ptr<MockRtpRtcp>> mock_rtp_rtcps_;

  MockAudioTransport mock_audio_transport_;
};
}  // namespace test
}  // namespace webrtc

#endif  // AUDIO_MOCK_VOICE_ENGINE_H_
