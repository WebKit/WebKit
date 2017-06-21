/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_MOCK_VOICE_ENGINE_H_
#define WEBRTC_AUDIO_MOCK_VOICE_ENGINE_H_

#include <memory>

#include "webrtc/modules/audio_device/include/mock_audio_device.h"
#include "webrtc/modules/audio_device/include/mock_audio_transport.h"
#include "webrtc/modules/audio_processing/include/mock_audio_processing.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/mock_voe_channel_proxy.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

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

    ON_CALL(mock_audio_device_, TimeUntilNextProcess())
        .WillByDefault(testing::Return(1000));
    ON_CALL(*this, audio_device_module())
        .WillByDefault(testing::Return(&mock_audio_device_));
    ON_CALL(*this, audio_processing())
        .WillByDefault(testing::Return(&mock_audio_processing_));
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
  MOCK_METHOD1(RegisterVoiceEngineObserver, int(VoiceEngineObserver& observer));
  MOCK_METHOD0(DeRegisterVoiceEngineObserver, int());
  MOCK_METHOD3(
      Init,
      int(AudioDeviceModule* external_adm,
          AudioProcessing* audioproc,
          const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory));
  MOCK_METHOD0(audio_processing, AudioProcessing*());
  MOCK_METHOD0(audio_device_module, AudioDeviceModule*());
  MOCK_METHOD0(transmit_mixer, voe::TransmitMixer*());
  MOCK_METHOD0(Terminate, int());
  MOCK_METHOD0(CreateChannel, int());
  MOCK_METHOD1(CreateChannel, int(const ChannelConfig& config));
  MOCK_METHOD1(DeleteChannel, int(int channel));
  MOCK_METHOD1(StartReceive, int(int channel));
  MOCK_METHOD1(StopReceive, int(int channel));
  MOCK_METHOD1(StartPlayout, int(int channel));
  MOCK_METHOD1(StopPlayout, int(int channel));
  MOCK_METHOD1(StartSend, int(int channel));
  MOCK_METHOD1(StopSend, int(int channel));
  MOCK_METHOD1(GetVersion, int(char version[1024]));
  MOCK_METHOD0(LastError, int());
  MOCK_METHOD0(audio_transport, AudioTransport*());
  MOCK_METHOD2(AssociateSendChannel,
               int(int channel, int accociate_send_channel));

  // VoECodec
  MOCK_METHOD0(NumOfCodecs, int());
  MOCK_METHOD2(GetCodec, int(int index, CodecInst& codec));
  MOCK_METHOD2(SetSendCodec, int(int channel, const CodecInst& codec));
  MOCK_METHOD2(GetSendCodec, int(int channel, CodecInst& codec));
  MOCK_METHOD2(SetBitRate, int(int channel, int bitrate_bps));
  MOCK_METHOD2(GetRecCodec, int(int channel, CodecInst& codec));
  MOCK_METHOD2(SetRecPayloadType, int(int channel, const CodecInst& codec));
  MOCK_METHOD2(GetRecPayloadType, int(int channel, CodecInst& codec));
  MOCK_METHOD3(SetSendCNPayloadType,
               int(int channel, int type, PayloadFrequencies frequency));
  MOCK_METHOD2(SetFECStatus, int(int channel, bool enable));
  MOCK_METHOD2(GetFECStatus, int(int channel, bool& enabled));
  MOCK_METHOD4(SetVADStatus,
               int(int channel, bool enable, VadModes mode, bool disableDTX));
  MOCK_METHOD4(
      GetVADStatus,
      int(int channel, bool& enabled, VadModes& mode, bool& disabledDTX));
  MOCK_METHOD2(SetOpusMaxPlaybackRate, int(int channel, int frequency_hz));
  MOCK_METHOD2(SetOpusDtx, int(int channel, bool enable_dtx));

  // VoEFile
  MOCK_METHOD7(StartPlayingFileLocally,
               int(int channel,
                   const char fileNameUTF8[1024],
                   bool loop,
                   FileFormats format,
                   float volumeScaling,
                   int startPointMs,
                   int stopPointMs));
  MOCK_METHOD6(StartPlayingFileLocally,
               int(int channel,
                   InStream* stream,
                   FileFormats format,
                   float volumeScaling,
                   int startPointMs,
                   int stopPointMs));
  MOCK_METHOD1(StopPlayingFileLocally, int(int channel));
  MOCK_METHOD1(IsPlayingFileLocally, int(int channel));
  MOCK_METHOD6(StartPlayingFileAsMicrophone,
               int(int channel,
                   const char fileNameUTF8[1024],
                   bool loop,
                   bool mixWithMicrophone,
                   FileFormats format,
                   float volumeScaling));
  MOCK_METHOD5(StartPlayingFileAsMicrophone,
               int(int channel,
                   InStream* stream,
                   bool mixWithMicrophone,
                   FileFormats format,
                   float volumeScaling));
  MOCK_METHOD1(StopPlayingFileAsMicrophone, int(int channel));
  MOCK_METHOD1(IsPlayingFileAsMicrophone, int(int channel));
  MOCK_METHOD4(StartRecordingPlayout,
               int(int channel,
                   const char* fileNameUTF8,
                   CodecInst* compression,
                   int maxSizeBytes));
  MOCK_METHOD1(StopRecordingPlayout, int(int channel));
  MOCK_METHOD3(StartRecordingPlayout,
               int(int channel, OutStream* stream, CodecInst* compression));
  MOCK_METHOD3(StartRecordingMicrophone,
               int(const char* fileNameUTF8,
                   CodecInst* compression,
                   int maxSizeBytes));
  MOCK_METHOD2(StartRecordingMicrophone,
               int(OutStream* stream, CodecInst* compression));
  MOCK_METHOD0(StopRecordingMicrophone, int());

  // VoENetwork
  MOCK_METHOD2(RegisterExternalTransport,
               int(int channel, Transport& transport));
  MOCK_METHOD1(DeRegisterExternalTransport, int(int channel));
  MOCK_METHOD3(ReceivedRTPPacket,
               int(int channel, const void* data, size_t length));
  MOCK_METHOD4(ReceivedRTPPacket,
               int(int channel,
                   const void* data,
                   size_t length,
                   const PacketTime& packet_time));
  MOCK_METHOD3(ReceivedRTCPPacket,
               int(int channel, const void* data, size_t length));

  // VoERTP_RTCP
  MOCK_METHOD2(SetLocalSSRC, int(int channel, unsigned int ssrc));
  MOCK_METHOD2(GetLocalSSRC, int(int channel, unsigned int& ssrc));
  MOCK_METHOD2(GetRemoteSSRC, int(int channel, unsigned int& ssrc));
  MOCK_METHOD3(SetSendAudioLevelIndicationStatus,
               int(int channel, bool enable, unsigned char id));
  MOCK_METHOD3(SetReceiveAudioLevelIndicationStatus,
               int(int channel, bool enable, unsigned char id));
  MOCK_METHOD3(SetSendAbsoluteSenderTimeStatus,
               int(int channel, bool enable, unsigned char id));
  MOCK_METHOD3(SetReceiveAbsoluteSenderTimeStatus,
               int(int channel, bool enable, unsigned char id));
  MOCK_METHOD2(SetRTCPStatus, int(int channel, bool enable));
  MOCK_METHOD2(GetRTCPStatus, int(int channel, bool& enabled));
  MOCK_METHOD2(SetRTCP_CNAME, int(int channel, const char cName[256]));
  MOCK_METHOD2(GetRTCP_CNAME, int(int channel, char cName[256]));
  MOCK_METHOD2(GetRemoteRTCP_CNAME, int(int channel, char cName[256]));
  MOCK_METHOD7(GetRemoteRTCPData,
               int(int channel,
                   unsigned int& NTPHigh,
                   unsigned int& NTPLow,
                   unsigned int& timestamp,
                   unsigned int& playoutTimestamp,
                   unsigned int* jitter,
                   unsigned short* fractionLost));
  MOCK_METHOD4(GetRTPStatistics,
               int(int channel,
                   unsigned int& averageJitterMs,
                   unsigned int& maxJitterMs,
                   unsigned int& discardedPackets));
  MOCK_METHOD2(GetRTCPStatistics, int(int channel, CallStatistics& stats));
  MOCK_METHOD2(GetRemoteRTCPReportBlocks,
               int(int channel, std::vector<ReportBlock>* receive_blocks));
  MOCK_METHOD3(SetREDStatus, int(int channel, bool enable, int redPayloadtype));
  MOCK_METHOD3(GetREDStatus,
               int(int channel, bool& enable, int& redPayloadtype));
  MOCK_METHOD3(SetNACKStatus, int(int channel, bool enable, int maxNoPackets));

 private:
  // TODO(ossu): I'm not particularly happy about keeping the decoder factory
  // here, but due to how gmock is implemented, I cannot just keep it in the
  // functor implementing the default version of ChannelProxyFactory, above.
  // GMock creates an unfortunate copy of the functor, which would cause us to
  // return a dangling reference. Fortunately, this should go away once
  // voe::Channel does.
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;

  std::map<int, std::unique_ptr<MockRtpRtcp>> mock_rtp_rtcps_;

  MockAudioDeviceModule mock_audio_device_;
  MockAudioProcessing mock_audio_processing_;
  MockAudioTransport mock_audio_transport_;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_AUDIO_MOCK_VOICE_ENGINE_H_
