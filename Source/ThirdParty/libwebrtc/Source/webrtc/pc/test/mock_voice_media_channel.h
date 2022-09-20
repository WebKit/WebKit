/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef PC_TEST_MOCK_VOICE_MEDIA_CHANNEL_H_
#define PC_TEST_MOCK_VOICE_MEDIA_CHANNEL_H_

#include <memory>
#include <string>
#include <vector>

#include "media/base/media_channel.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::InvokeWithoutArgs;
using ::testing::Mock;

namespace cricket {
class MockVoiceMediaChannel : public VoiceMediaChannel {
 public:
  explicit MockVoiceMediaChannel(webrtc::TaskQueueBase* network_thread)
      : VoiceMediaChannel(network_thread) {}

  MOCK_METHOD(void, SetInterface, (NetworkInterface * iface), (override));
  MOCK_METHOD(void,
              OnPacketReceived,
              (rtc::CopyOnWriteBuffer packet, int64_t packet_time_us),
              (override));
  MOCK_METHOD(void,
              OnPacketSent,
              (const rtc::SentPacket& sent_packet),
              (override));
  MOCK_METHOD(void, OnReadyToSend, (bool ready), (override));
  MOCK_METHOD(void,
              OnNetworkRouteChanged,
              (absl::string_view transport_name,
               const rtc::NetworkRoute& network_route),
              (override));
  MOCK_METHOD(bool, AddSendStream, (const StreamParams& sp), (override));
  MOCK_METHOD(bool, RemoveSendStream, (uint32_t ssrc), (override));
  MOCK_METHOD(bool, AddRecvStream, (const StreamParams& sp), (override));
  MOCK_METHOD(bool, RemoveRecvStream, (uint32_t ssrc), (override));
  MOCK_METHOD(void, ResetUnsignaledRecvStream, (), (override));
  MOCK_METHOD(void, OnDemuxerCriteriaUpdatePending, (), (override));
  MOCK_METHOD(void, OnDemuxerCriteriaUpdateComplete, (), (override));
  MOCK_METHOD(int, GetRtpSendTimeExtnId, (), (const, override));
  MOCK_METHOD(
      void,
      SetFrameEncryptor,
      (uint32_t ssrc,
       rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor),
      (override));
  MOCK_METHOD(
      void,
      SetFrameDecryptor,
      (uint32_t ssrc,
       rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor),
      (override));
  MOCK_METHOD(void, SetVideoCodecSwitchingEnabled, (bool enabled), (override));
  MOCK_METHOD(webrtc::RtpParameters,
              GetRtpSendParameters,
              (uint32_t ssrc),
              (const, override));
  MOCK_METHOD(webrtc::RTCError,
              SetRtpSendParameters,
              (uint32_t ssrc, const webrtc::RtpParameters& parameters),
              (override));
  MOCK_METHOD(
      void,
      SetEncoderToPacketizerFrameTransformer,
      (uint32_t ssrc,
       rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer),
      (override));
  MOCK_METHOD(
      void,
      SetDepacketizerToDecoderFrameTransformer,
      (uint32_t ssrc,
       rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer),
      (override));

  MOCK_METHOD(bool,
              SetSendParameters,
              (const AudioSendParameters& params),
              (override));
  MOCK_METHOD(bool,
              SetRecvParameters,
              (const AudioRecvParameters& params),
              (override));
  MOCK_METHOD(webrtc::RtpParameters,
              GetRtpReceiveParameters,
              (uint32_t ssrc),
              (const, override));
  MOCK_METHOD(webrtc::RtpParameters,
              GetDefaultRtpReceiveParameters,
              (),
              (const, override));
  MOCK_METHOD(void, SetPlayout, (bool playout), (override));
  MOCK_METHOD(void, SetSend, (bool send), (override));
  MOCK_METHOD(bool,
              SetAudioSend,
              (uint32_t ssrc,
               bool enable,
               const AudioOptions* options,
               AudioSource* source),
              (override));
  MOCK_METHOD(bool,
              SetOutputVolume,
              (uint32_t ssrc, double volume),
              (override));
  MOCK_METHOD(bool, SetDefaultOutputVolume, (double volume), (override));
  MOCK_METHOD(bool, CanInsertDtmf, (), (override));
  MOCK_METHOD(bool,
              InsertDtmf,
              (uint32_t ssrc, int event, int duration),
              (override));
  MOCK_METHOD(bool,
              GetStats,
              (VoiceMediaInfo * info, bool get_and_clear_legacy_stats),
              (override));
  MOCK_METHOD(void,
              SetRawAudioSink,
              (uint32_t ssrc, std::unique_ptr<webrtc::AudioSinkInterface> sink),
              (override));
  MOCK_METHOD(void,
              SetDefaultRawAudioSink,
              (std::unique_ptr<webrtc::AudioSinkInterface> sink),
              (override));
  MOCK_METHOD(std::vector<webrtc::RtpSource>,
              GetSources,
              (uint32_t ssrc),
              (const, override));

  MOCK_METHOD(bool,
              SetBaseMinimumPlayoutDelayMs,
              (uint32_t ssrc, int delay_ms),
              (override));
  MOCK_METHOD(absl::optional<int>,
              GetBaseMinimumPlayoutDelayMs,
              (uint32_t ssrc),
              (const, override));
};
}  // namespace cricket

#endif  // PC_TEST_MOCK_VOICE_MEDIA_CHANNEL_H_
