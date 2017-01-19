/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_AUDIO_RECEIVE_STREAM_H_
#define WEBRTC_AUDIO_AUDIO_RECEIVE_STREAM_H_

#include <memory>

#include "webrtc/api/audio/audio_mixer.h"
#include "webrtc/api/call/audio_receive_stream.h"
#include "webrtc/api/call/audio_state.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"

namespace webrtc {
class CongestionController;
class RemoteBitrateEstimator;
class RtcEventLog;

namespace voe {
class ChannelProxy;
}  // namespace voe

namespace internal {
class AudioSendStream;

class AudioReceiveStream final : public webrtc::AudioReceiveStream,
                                 public AudioMixer::Source {
 public:
  AudioReceiveStream(CongestionController* congestion_controller,
                     const webrtc::AudioReceiveStream::Config& config,
                     const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
                     webrtc::RtcEventLog* event_log);
  ~AudioReceiveStream() override;

  // webrtc::AudioReceiveStream implementation.
  void Start() override;
  void Stop() override;
  webrtc::AudioReceiveStream::Stats GetStats() const override;
  void SetSink(std::unique_ptr<AudioSinkInterface> sink) override;
  void SetGain(float gain) override;

  void AssociateSendStream(AudioSendStream* send_stream);
  void SignalNetworkState(NetworkState state);
  bool DeliverRtcp(const uint8_t* packet, size_t length);
  bool DeliverRtp(const uint8_t* packet,
                  size_t length,
                  const PacketTime& packet_time);
  const webrtc::AudioReceiveStream::Config& config() const;

  // AudioMixer::Source
  AudioFrameInfo GetAudioFrameWithInfo(int sample_rate_hz,
                                       AudioFrame* audio_frame) override;
  int PreferredSampleRate() const override;
  int Ssrc() const override;

 private:
  VoiceEngine* voice_engine() const;

  rtc::ThreadChecker thread_checker_;
  RemoteBitrateEstimator* remote_bitrate_estimator_ = nullptr;
  const webrtc::AudioReceiveStream::Config config_;
  rtc::scoped_refptr<webrtc::AudioState> audio_state_;
  std::unique_ptr<RtpHeaderParser> rtp_header_parser_;
  std::unique_ptr<voe::ChannelProxy> channel_proxy_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioReceiveStream);
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_AUDIO_AUDIO_RECEIVE_STREAM_H_
