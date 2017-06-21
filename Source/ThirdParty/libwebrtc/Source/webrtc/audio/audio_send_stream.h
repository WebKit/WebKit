/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_AUDIO_SEND_STREAM_H_
#define WEBRTC_AUDIO_AUDIO_SEND_STREAM_H_

#include <memory>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/call/audio_send_stream.h"
#include "webrtc/call/audio_state.h"
#include "webrtc/call/bitrate_allocator.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/voice_engine/transport_feedback_packet_loss_tracker.h"

namespace webrtc {
class VoiceEngine;
class RtcEventLog;
class RtcpBandwidthObserver;
class RtcpRttStats;
class RtpTransportControllerSendInterface;

namespace voe {
class ChannelProxy;
}  // namespace voe

namespace internal {
class AudioSendStream final : public webrtc::AudioSendStream,
                              public webrtc::BitrateAllocatorObserver,
                              public webrtc::PacketFeedbackObserver {
 public:
  AudioSendStream(const webrtc::AudioSendStream::Config& config,
                  const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
                  rtc::TaskQueue* worker_queue,
                  RtpTransportControllerSendInterface* transport,
                  BitrateAllocator* bitrate_allocator,
                  RtcEventLog* event_log,
                  RtcpRttStats* rtcp_rtt_stats,
                  const rtc::Optional<RtpState>& suspended_rtp_state);
  ~AudioSendStream() override;

  // webrtc::AudioSendStream implementation.
  void Reconfigure(const webrtc::AudioSendStream::Config& config) override;

  void Start() override;
  void Stop() override;
  bool SendTelephoneEvent(int payload_type, int payload_frequency, int event,
                          int duration_ms) override;
  void SetMuted(bool muted) override;
  webrtc::AudioSendStream::Stats GetStats() const override;

  void SignalNetworkState(NetworkState state);
  bool DeliverRtcp(const uint8_t* packet, size_t length);

  // Implements BitrateAllocatorObserver.
  uint32_t OnBitrateUpdated(uint32_t bitrate_bps,
                            uint8_t fraction_loss,
                            int64_t rtt,
                            int64_t bwe_period_ms) override;

  // From PacketFeedbackObserver.
  void OnPacketAdded(uint32_t ssrc, uint16_t seq_num) override;
  void OnPacketFeedbackVector(
      const std::vector<PacketFeedback>& packet_feedback_vector) override;

  const webrtc::AudioSendStream::Config& config() const;
  void SetTransportOverhead(int transport_overhead_per_packet);

  RtpState GetRtpState() const;

 private:
  VoiceEngine* voice_engine() const;

  // These are all static to make it less likely that (the old) config_ is
  // accessed unintentionally.
  static void ConfigureStream(AudioSendStream* stream,
                              const Config& new_config,
                              bool first_time);
  static bool SetupSendCodec(AudioSendStream* stream, const Config& new_config);
  static bool ReconfigureSendCodec(AudioSendStream* stream,
                                   const Config& new_config);
  static void ReconfigureANA(AudioSendStream* stream, const Config& new_config);
  static void ReconfigureCNG(AudioSendStream* stream, const Config& new_config);
  static void ReconfigureBitrateObserver(AudioSendStream* stream,
                                         const Config& new_config);

  void ConfigureBitrateObserver(int min_bitrate_bps, int max_bitrate_bps);
  void RemoveBitrateObserver();

  void RegisterCngPayloadType(int payload_type, int clockrate_hz);

  rtc::ThreadChecker worker_thread_checker_;
  rtc::ThreadChecker pacer_thread_checker_;
  rtc::TaskQueue* worker_queue_;
  webrtc::AudioSendStream::Config config_;
  rtc::scoped_refptr<webrtc::AudioState> audio_state_;
  std::unique_ptr<voe::ChannelProxy> channel_proxy_;
  RtcEventLog* const event_log_;

  BitrateAllocator* const bitrate_allocator_;
  RtpTransportControllerSendInterface* const transport_;
  std::unique_ptr<RtcpBandwidthObserver> bandwidth_observer_;

  rtc::CriticalSection packet_loss_tracker_cs_;
  TransportFeedbackPacketLossTracker packet_loss_tracker_
      GUARDED_BY(&packet_loss_tracker_cs_);

  RtpRtcp* rtp_rtcp_module_;
  rtc::Optional<RtpState> const suspended_rtp_state_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioSendStream);
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_AUDIO_AUDIO_SEND_STREAM_H_
