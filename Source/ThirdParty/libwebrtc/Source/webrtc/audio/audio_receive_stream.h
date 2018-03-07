/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_AUDIO_RECEIVE_STREAM_H_
#define AUDIO_AUDIO_RECEIVE_STREAM_H_

#include <memory>
#include <vector>

#include "api/audio/audio_mixer.h"
#include "audio/audio_state.h"
#include "call/audio_receive_stream.h"
#include "call/rtp_packet_sink_interface.h"
#include "call/syncable.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {
class PacketRouter;
class RtcEventLog;
class RtpPacketReceived;
class RtpStreamReceiverControllerInterface;
class RtpStreamReceiverInterface;

namespace voe {
class ChannelProxy;
}  // namespace voe

namespace internal {
class AudioSendStream;

class AudioReceiveStream final : public webrtc::AudioReceiveStream,
                                 public AudioMixer::Source,
                                 public Syncable {
 public:
  AudioReceiveStream(RtpStreamReceiverControllerInterface* receiver_controller,
                     PacketRouter* packet_router,
                     const webrtc::AudioReceiveStream::Config& config,
                     const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
                     webrtc::RtcEventLog* event_log);
  ~AudioReceiveStream() override;

  // webrtc::AudioReceiveStream implementation.
  void Start() override;
  void Stop() override;
  webrtc::AudioReceiveStream::Stats GetStats() const override;
  int GetOutputLevel() const override;
  void SetSink(std::unique_ptr<AudioSinkInterface> sink) override;
  void SetGain(float gain) override;
  std::vector<webrtc::RtpSource> GetSources() const override;

  // TODO(nisse): We don't formally implement RtpPacketSinkInterface, and this
  // method shouldn't be needed. But it's currently used by the
  // AudioReceiveStreamTest.ReceiveRtpPacket unittest. Figure out if that test
  // shuld be refactored or deleted, and then delete this method.
  void OnRtpPacket(const RtpPacketReceived& packet);

  // AudioMixer::Source
  AudioFrameInfo GetAudioFrameWithInfo(int sample_rate_hz,
                                       AudioFrame* audio_frame) override;
  int Ssrc() const override;
  int PreferredSampleRate() const override;

  // Syncable
  int id() const override;
  rtc::Optional<Syncable::Info> GetInfo() const override;
  uint32_t GetPlayoutTimestamp() const override;
  void SetMinimumPlayoutDelay(int delay_ms) override;

  void AssociateSendStream(AudioSendStream* send_stream);
  void SignalNetworkState(NetworkState state);
  bool DeliverRtcp(const uint8_t* packet, size_t length);
  const webrtc::AudioReceiveStream::Config& config() const;

 private:
  VoiceEngine* voice_engine() const;
  AudioState* audio_state() const;
  int SetVoiceEnginePlayout(bool playout);

  rtc::ThreadChecker worker_thread_checker_;
  rtc::ThreadChecker module_process_thread_checker_;
  const webrtc::AudioReceiveStream::Config config_;
  rtc::scoped_refptr<webrtc::AudioState> audio_state_;
  std::unique_ptr<voe::ChannelProxy> channel_proxy_;

  bool playing_ RTC_ACCESS_ON(worker_thread_checker_) = false;

  std::unique_ptr<RtpStreamReceiverInterface> rtp_stream_receiver_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioReceiveStream);
};
}  // namespace internal
}  // namespace webrtc

#endif  // AUDIO_AUDIO_RECEIVE_STREAM_H_
