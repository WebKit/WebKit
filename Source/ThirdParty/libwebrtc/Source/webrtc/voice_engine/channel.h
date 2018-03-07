/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VOICE_ENGINE_CHANNEL_H_
#define VOICE_ENGINE_CHANNEL_H_

#include <memory>

#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/call/audio_sink.h"
#include "api/call/transport.h"
#include "api/optional.h"
#include "common_audio/resampler/include/push_resampler.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_processing/rms_level.h"
#include "modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/include/rtp_receiver.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/thread_checker.h"
#include "voice_engine/audio_level.h"
#include "voice_engine/include/voe_base.h"
#include "voice_engine/shared_data.h"

namespace rtc {
class TimestampWrapAroundHandler;
}

namespace webrtc {

class AudioDeviceModule;
class PacketRouter;
class ProcessThread;
class RateLimiter;
class ReceiveStatistics;
class RemoteNtpTimeEstimator;
class RtcEventLog;
class RTPPayloadRegistry;
class RTPReceiverAudio;
class RtpPacketReceived;
class RtpRtcp;
class RtpTransportControllerSendInterface;
class TelephoneEventHandler;

struct SenderInfo;

struct CallStatistics {
  unsigned short fractionLost;
  unsigned int cumulativeLost;
  unsigned int extendedMax;
  unsigned int jitterSamples;
  int64_t rttMs;
  size_t bytesSent;
  int packetsSent;
  size_t bytesReceived;
  int packetsReceived;
  // The capture ntp time (in local timebase) of the first played out audio
  // frame.
  int64_t capture_start_ntp_time_ms_;
};

// See section 6.4.2 in http://www.ietf.org/rfc/rfc3550.txt for details.
struct ReportBlock {
  uint32_t sender_SSRC;  // SSRC of sender
  uint32_t source_SSRC;
  uint8_t fraction_lost;
  uint32_t cumulative_num_packets_lost;
  uint32_t extended_highest_sequence_number;
  uint32_t interarrival_jitter;
  uint32_t last_SR_timestamp;
  uint32_t delay_since_last_SR;
};

namespace voe {

class RtcEventLogProxy;
class RtcpRttStatsProxy;
class RtpPacketSenderProxy;
class TransportFeedbackProxy;
class TransportSequenceNumberProxy;
class VoERtcpObserver;

// Helper class to simplify locking scheme for members that are accessed from
// multiple threads.
// Example: a member can be set on thread T1 and read by an internal audio
// thread T2. Accessing the member via this class ensures that we are
// safe and also avoid TSan v2 warnings.
class ChannelState {
 public:
  struct State {
    bool playing = false;
    bool sending = false;
  };

  ChannelState() {}
  virtual ~ChannelState() {}

  void Reset() {
    rtc::CritScope lock(&lock_);
    state_ = State();
  }

  State Get() const {
    rtc::CritScope lock(&lock_);
    return state_;
  }

  void SetPlaying(bool enable) {
    rtc::CritScope lock(&lock_);
    state_.playing = enable;
  }

  void SetSending(bool enable) {
    rtc::CritScope lock(&lock_);
    state_.sending = enable;
  }

 private:
  rtc::CriticalSection lock_;
  State state_;
};

class Channel
    : public RtpData,
      public RtpFeedback,
      public Transport,
      public AudioPacketizationCallback,  // receive encoded packets from the
                                          // ACM
      public OverheadObserver {
 public:
  friend class VoERtcpObserver;

  enum { KNumSocketThreads = 1 };
  enum { KNumberOfSocketBuffers = 8 };
  virtual ~Channel();
  static int32_t CreateChannel(Channel*& channel,
                               int32_t channelId,
                               uint32_t instanceId,
                               const VoEBase::ChannelConfig& config);
  Channel(int32_t channelId,
          uint32_t instanceId,
          const VoEBase::ChannelConfig& config);
  int32_t Init();
  void Terminate();
  int32_t SetEngineInformation(ProcessThread& moduleProcessThread,
                               AudioDeviceModule& audioDeviceModule,
                               rtc::TaskQueue* encoder_queue);

  void SetSink(std::unique_ptr<AudioSinkInterface> sink);

  // TODO(ossu): Don't use! It's only here to confirm that the decoder factory
  // passed into AudioReceiveStream is the same as the one set when creating the
  // ADM. Once Channel creation is moved into Audio{Send,Receive}Stream this can
  // go.
  const rtc::scoped_refptr<AudioDecoderFactory>& GetAudioDecoderFactory() const;

  void SetReceiveCodecs(const std::map<int, SdpAudioFormat>& codecs);

  // Send using this encoder, with this payload type.
  bool SetEncoder(int payload_type, std::unique_ptr<AudioEncoder> encoder);
  void ModifyEncoder(
      rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier);

  // API methods

  // VoEBase
  int32_t StartPlayout();
  int32_t StopPlayout();
  int32_t StartSend();
  void StopSend();

  // Codecs
  struct EncoderProps {
    int sample_rate_hz;
    size_t num_channels;
  };
  rtc::Optional<EncoderProps> GetEncoderProps() const;
  int32_t GetRecCodec(CodecInst& codec);
  void SetBitRate(int bitrate_bps, int64_t probing_interval_ms);
  bool EnableAudioNetworkAdaptor(const std::string& config_string);
  void DisableAudioNetworkAdaptor();
  void SetReceiverFrameLengthRange(int min_frame_length_ms,
                                   int max_frame_length_ms);

  // Network
  void RegisterTransport(Transport* transport);
  // TODO(nisse, solenberg): Delete when VoENetwork is deleted.
  int32_t ReceivedRTCPPacket(const uint8_t* data, size_t length);
  void OnRtpPacket(const RtpPacketReceived& packet);

  // Muting, Volume and Level.
  void SetInputMute(bool enable);
  void SetChannelOutputVolumeScaling(float scaling);
  int GetSpeechOutputLevel() const;
  int GetSpeechOutputLevelFullRange() const;
  // See description of "totalAudioEnergy" in the WebRTC stats spec:
  // https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats-totalaudioenergy
  double GetTotalOutputEnergy() const;
  double GetTotalOutputDuration() const;

  // Stats.
  int GetNetworkStatistics(NetworkStatistics& stats);
  void GetDecodingCallStatistics(AudioDecodingCallStats* stats) const;
  ANAStats GetANAStatistics() const;

  // Audio+Video Sync.
  uint32_t GetDelayEstimate() const;
  int SetMinimumPlayoutDelay(int delayMs);
  int GetPlayoutTimestamp(unsigned int& timestamp);
  int GetRtpRtcp(RtpRtcp** rtpRtcpModule, RtpReceiver** rtp_receiver) const;

  // DTMF.
  int SendTelephoneEventOutband(int event, int duration_ms);
  int SetSendTelephoneEventPayloadType(int payload_type, int payload_frequency);

  // RTP+RTCP
  int SetLocalSSRC(unsigned int ssrc);
  int SetSendAudioLevelIndicationStatus(bool enable, unsigned char id);
  int SetReceiveAudioLevelIndicationStatus(bool enable, unsigned char id);
  void EnableSendTransportSequenceNumber(int id);
  void EnableReceiveTransportSequenceNumber(int id);

  void RegisterSenderCongestionControlObjects(
      RtpTransportControllerSendInterface* transport,
      RtcpBandwidthObserver* bandwidth_observer);
  void RegisterReceiverCongestionControlObjects(PacketRouter* packet_router);
  void ResetSenderCongestionControlObjects();
  void ResetReceiverCongestionControlObjects();
  void SetRTCPStatus(bool enable);
  int SetRTCP_CNAME(const char cName[256]);
  int GetRemoteRTCPReportBlocks(std::vector<ReportBlock>* report_blocks);
  int GetRTPStatistics(CallStatistics& stats);
  void SetNACKStatus(bool enable, int maxNumberOfPackets);

  // From AudioPacketizationCallback in the ACM
  int32_t SendData(FrameType frameType,
                   uint8_t payloadType,
                   uint32_t timeStamp,
                   const uint8_t* payloadData,
                   size_t payloadSize,
                   const RTPFragmentationHeader* fragmentation) override;

  // From RtpData in the RTP/RTCP module
  int32_t OnReceivedPayloadData(const uint8_t* payloadData,
                                size_t payloadSize,
                                const WebRtcRTPHeader* rtpHeader) override;

  // From RtpFeedback in the RTP/RTCP module
  int32_t OnInitializeDecoder(int payload_type,
                              const SdpAudioFormat& audio_format,
                              uint32_t rate) override;
  void OnIncomingSSRCChanged(uint32_t ssrc) override;
  void OnIncomingCSRCChanged(uint32_t CSRC, bool added) override;

  // From Transport (called by the RTP/RTCP module)
  bool SendRtp(const uint8_t* data,
               size_t len,
               const PacketOptions& packet_options) override;
  bool SendRtcp(const uint8_t* data, size_t len) override;

  // From AudioMixer::Source.
  AudioMixer::Source::AudioFrameInfo GetAudioFrameWithInfo(
      int sample_rate_hz,
      AudioFrame* audio_frame);

  int PreferredSampleRate() const;

  uint32_t InstanceId() const { return _instanceId; }
  int32_t ChannelId() const { return _channelId; }
  bool Playing() const { return channel_state_.Get().playing; }
  bool Sending() const { return channel_state_.Get().sending; }
  RtpRtcp* RtpRtcpModulePtr() const { return _rtpRtcpModule.get(); }
  int8_t OutputEnergyLevel() const { return _outputAudioLevel.Level(); }

  // ProcessAndEncodeAudio() creates an audio frame copy and posts a task
  // on the shared encoder task queue, wich in turn calls (on the queue)
  // ProcessAndEncodeAudioOnTaskQueue() where the actual processing of the
  // audio takes place. The processing mainly consists of encoding and preparing
  // the result for sending by adding it to a send queue.
  // The main reason for using a task queue here is to release the native,
  // OS-specific, audio capture thread as soon as possible to ensure that it
  // can go back to sleep and be prepared to deliver an new captured audio
  // packet.
  void ProcessAndEncodeAudio(const AudioFrame& audio_input);

  // This version of ProcessAndEncodeAudio() is used by PushCaptureData() in
  // VoEBase and the audio in |audio_data| has not been subject to any APM
  // processing. Some extra steps are therfore needed when building up the
  // audio frame copy before using the same task as in the default call to
  // ProcessAndEncodeAudio(const AudioFrame& audio_input).
  void ProcessAndEncodeAudio(const int16_t* audio_data,
                             int sample_rate,
                             size_t number_of_frames,
                             size_t number_of_channels);

  // Associate to a send channel.
  // Used for obtaining RTT for a receive-only channel.
  void set_associate_send_channel(const ChannelOwner& channel);
  // Disassociate a send channel if it was associated.
  void DisassociateSendChannel(int channel_id);

  // Set a RtcEventLog logging object.
  void SetRtcEventLog(RtcEventLog* event_log);

  void SetRtcpRttStats(RtcpRttStats* rtcp_rtt_stats);
  void SetTransportOverhead(size_t transport_overhead_per_packet);

  // From OverheadObserver in the RTP/RTCP module
  void OnOverheadChanged(size_t overhead_bytes_per_packet) override;

  // The existence of this function alongside OnUplinkPacketLossRate is
  // a compromise. We want the encoder to be agnostic of the PLR source, but
  // we also don't want it to receive conflicting information from TWCC and
  // from RTCP-XR.
  void OnTwccBasedUplinkPacketLossRate(float packet_loss_rate);

  void OnRecoverableUplinkPacketLossRate(float recoverable_packet_loss_rate);

  std::vector<RtpSource> GetSources() const {
    return rtp_receiver_->GetSources();
  }

 private:
  class ProcessAndEncodeAudioTask;

  int GetRemoteSSRC(unsigned int& ssrc);
  void OnUplinkPacketLossRate(float packet_loss_rate);
  bool InputMute() const;
  bool OnRecoveredPacket(const uint8_t* packet, size_t packet_length);

  bool ReceivePacket(const uint8_t* packet,
                     size_t packet_length,
                     const RTPHeader& header);
  bool IsPacketInOrder(const RTPHeader& header) const;
  bool IsPacketRetransmitted(const RTPHeader& header, bool in_order) const;
  int ResendPackets(const uint16_t* sequence_numbers, int length);
  void UpdatePlayoutTimestamp(bool rtcp);
  void RegisterReceiveCodecsToRTPModule();

  int SetSendRtpHeaderExtension(bool enable,
                                RTPExtensionType type,
                                unsigned char id);

  void UpdateOverheadForEncoder()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(overhead_per_packet_lock_);

  int GetRtpTimestampRateHz() const;
  int64_t GetRTT(bool allow_associate_channel) const;

  // Called on the encoder task queue when a new input audio frame is ready
  // for encoding.
  void ProcessAndEncodeAudioOnTaskQueue(AudioFrame* audio_input);

  uint32_t _instanceId;
  int32_t _channelId;

  rtc::CriticalSection _callbackCritSect;
  rtc::CriticalSection volume_settings_critsect_;

  ChannelState channel_state_;

  std::unique_ptr<voe::RtcEventLogProxy> event_log_proxy_;
  std::unique_ptr<voe::RtcpRttStatsProxy> rtcp_rtt_stats_proxy_;

  std::unique_ptr<RtpHeaderParser> rtp_header_parser_;
  std::unique_ptr<RTPPayloadRegistry> rtp_payload_registry_;
  std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;
  std::unique_ptr<RtpReceiver> rtp_receiver_;
  TelephoneEventHandler* telephone_event_handler_;
  std::unique_ptr<RtpRtcp> _rtpRtcpModule;
  std::unique_ptr<AudioCodingModule> audio_coding_;
  std::unique_ptr<AudioSinkInterface> audio_sink_;
  AudioLevel _outputAudioLevel;
  // Downsamples to the codec rate if necessary.
  PushResampler<int16_t> input_resampler_;
  uint32_t _timeStamp RTC_ACCESS_ON(encoder_queue_);

  RemoteNtpTimeEstimator ntp_estimator_ RTC_GUARDED_BY(ts_stats_lock_);

  // Timestamp of the audio pulled from NetEq.
  rtc::Optional<uint32_t> jitter_buffer_playout_timestamp_;

  rtc::CriticalSection video_sync_lock_;
  uint32_t playout_timestamp_rtp_ RTC_GUARDED_BY(video_sync_lock_);
  uint32_t playout_delay_ms_ RTC_GUARDED_BY(video_sync_lock_);
  uint16_t send_sequence_number_;

  rtc::CriticalSection ts_stats_lock_;

  std::unique_ptr<rtc::TimestampWrapAroundHandler> rtp_ts_wraparound_handler_;
  // The rtp timestamp of the first played out audio frame.
  int64_t capture_start_rtp_time_stamp_;
  // The capture ntp time (in local timebase) of the first played out audio
  // frame.
  int64_t capture_start_ntp_time_ms_ RTC_GUARDED_BY(ts_stats_lock_);

  // uses
  ProcessThread* _moduleProcessThreadPtr;
  AudioDeviceModule* _audioDeviceModulePtr;
  Transport* _transportPtr;  // WebRtc socket or external transport
  RmsLevel rms_level_ RTC_ACCESS_ON(encoder_queue_);
  bool input_mute_ RTC_GUARDED_BY(volume_settings_critsect_);
  bool previous_frame_muted_ RTC_ACCESS_ON(encoder_queue_);
  float _outputGain RTC_GUARDED_BY(volume_settings_critsect_);
  // VoeRTP_RTCP
  // TODO(henrika): can today be accessed on the main thread and on the
  // task queue; hence potential race.
  bool _includeAudioLevelIndication;
  size_t transport_overhead_per_packet_
      RTC_GUARDED_BY(overhead_per_packet_lock_);
  size_t rtp_overhead_per_packet_ RTC_GUARDED_BY(overhead_per_packet_lock_);
  rtc::CriticalSection overhead_per_packet_lock_;
  // VoENetwork
  AudioFrame::SpeechType _outputSpeechType;
  // RtcpBandwidthObserver
  std::unique_ptr<VoERtcpObserver> rtcp_observer_;
  // An associated send channel.
  rtc::CriticalSection assoc_send_channel_lock_;
  ChannelOwner associate_send_channel_ RTC_GUARDED_BY(assoc_send_channel_lock_);

  bool pacing_enabled_;
  PacketRouter* packet_router_ = nullptr;
  std::unique_ptr<TransportFeedbackProxy> feedback_observer_proxy_;
  std::unique_ptr<TransportSequenceNumberProxy> seq_num_allocator_proxy_;
  std::unique_ptr<RtpPacketSenderProxy> rtp_packet_sender_proxy_;
  std::unique_ptr<RateLimiter> retransmission_rate_limiter_;

  // TODO(ossu): Remove once GetAudioDecoderFactory() is no longer needed.
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;

  rtc::Optional<EncoderProps> cached_encoder_props_;

  rtc::ThreadChecker construction_thread_;

  const bool use_twcc_plr_for_ana_;

  rtc::CriticalSection encoder_queue_lock_;

  bool encoder_queue_is_active_ RTC_GUARDED_BY(encoder_queue_lock_) = false;

  rtc::TaskQueue* encoder_queue_ = nullptr;
};

}  // namespace voe
}  // namespace webrtc

#endif  // VOICE_ENGINE_CHANNEL_H_
