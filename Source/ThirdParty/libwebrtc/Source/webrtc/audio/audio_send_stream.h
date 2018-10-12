/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_AUDIO_SEND_STREAM_H_
#define AUDIO_AUDIO_SEND_STREAM_H_

#include <memory>
#include <vector>

#include "audio/time_interval.h"
#include "audio/transport_feedback_packet_loss_tracker.h"
#include "call/audio_send_stream.h"
#include "call/audio_state.h"
#include "call/bitrate_allocator.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {
class RtcEventLog;
class RtcpBandwidthObserver;
class RtcpRttStats;
class RtpTransportControllerSendInterface;

namespace voe {
class ChannelSendProxy;
}  // namespace voe

namespace internal {
class AudioState;

class AudioSendStream final : public webrtc::AudioSendStream,
                              public webrtc::BitrateAllocatorObserver,
                              public webrtc::PacketFeedbackObserver {
 public:
  AudioSendStream(const webrtc::AudioSendStream::Config& config,
                  const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
                  rtc::TaskQueue* worker_queue,
                  ProcessThread* module_process_thread,
                  RtpTransportControllerSendInterface* transport,
                  BitrateAllocator* bitrate_allocator,
                  RtcEventLog* event_log,
                  RtcpRttStats* rtcp_rtt_stats,
                  const absl::optional<RtpState>& suspended_rtp_state,
                  TimeInterval* overall_call_lifetime);
  // For unit tests, which need to supply a mock channel proxy.
  AudioSendStream(const webrtc::AudioSendStream::Config& config,
                  const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
                  rtc::TaskQueue* worker_queue,
                  RtpTransportControllerSendInterface* transport,
                  BitrateAllocator* bitrate_allocator,
                  RtcEventLog* event_log,
                  RtcpRttStats* rtcp_rtt_stats,
                  const absl::optional<RtpState>& suspended_rtp_state,
                  TimeInterval* overall_call_lifetime,
                  std::unique_ptr<voe::ChannelSendProxy> channel_proxy);
  ~AudioSendStream() override;

  // webrtc::AudioSendStream implementation.
  const webrtc::AudioSendStream::Config& GetConfig() const override;
  void Reconfigure(const webrtc::AudioSendStream::Config& config) override;
  void Start() override;
  void Stop() override;
  void SendAudioData(std::unique_ptr<AudioFrame> audio_frame) override;
  bool SendTelephoneEvent(int payload_type,
                          int payload_frequency,
                          int event,
                          int duration_ms) override;
  void SetMuted(bool muted) override;
  webrtc::AudioSendStream::Stats GetStats() const override;
  webrtc::AudioSendStream::Stats GetStats(
      bool has_remote_tracks) const override;

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

  void SetTransportOverhead(int transport_overhead_per_packet);

  RtpState GetRtpState() const;
  const voe::ChannelSendProxy& GetChannelProxy() const;

 private:
  class TimedTransport;

  internal::AudioState* audio_state();
  const internal::AudioState* audio_state() const;

  void StoreEncoderProperties(int sample_rate_hz, size_t num_channels);

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

  void ConfigureBitrateObserver(int min_bitrate_bps,
                                int max_bitrate_bps,
                                double bitrate_priority,
                                bool has_packet_feedback);
  void RemoveBitrateObserver();

  void RegisterCngPayloadType(int payload_type, int clockrate_hz);

  rtc::ThreadChecker worker_thread_checker_;
  rtc::ThreadChecker pacer_thread_checker_;
  rtc::RaceChecker audio_capture_race_checker_;
  rtc::TaskQueue* worker_queue_;
  webrtc::AudioSendStream::Config config_;
  rtc::scoped_refptr<webrtc::AudioState> audio_state_;
  std::unique_ptr<voe::ChannelSendProxy> channel_proxy_;
  RtcEventLog* const event_log_;

  int encoder_sample_rate_hz_ = 0;
  size_t encoder_num_channels_ = 0;
  bool sending_ = false;

  BitrateAllocator* const bitrate_allocator_;
  RtpTransportControllerSendInterface* const transport_;

  rtc::CriticalSection packet_loss_tracker_cs_;
  TransportFeedbackPacketLossTracker packet_loss_tracker_
      RTC_GUARDED_BY(&packet_loss_tracker_cs_);

  RtpRtcp* rtp_rtcp_module_;
  absl::optional<RtpState> const suspended_rtp_state_;

  std::unique_ptr<TimedTransport> timed_send_transport_adapter_;
  TimeInterval active_lifetime_;
  TimeInterval* overall_call_lifetime_ = nullptr;

  // RFC 5285: Each distinct extension MUST have a unique ID. The value 0 is
  // reserved for padding and MUST NOT be used as a local identifier.
  // So it should be safe to use 0 here to indicate "not configured".
  struct ExtensionIds {
    int audio_level = 0;
    int transport_sequence_number = 0;
    int mid = 0;
  };
  static ExtensionIds FindExtensionIds(
      const std::vector<RtpExtension>& extensions);

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioSendStream);
};
}  // namespace internal
}  // namespace webrtc

#endif  // AUDIO_AUDIO_SEND_STREAM_H_
