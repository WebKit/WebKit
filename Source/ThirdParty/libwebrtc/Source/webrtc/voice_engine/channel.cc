/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/channel.h"

#include <algorithm>
#include <utility>

#include "webrtc/audio/utility/audio_frame_operations.h"
#include "webrtc/base/array_view.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/base/location.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/rate_limiter.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/config.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/modules/audio_coding/codecs/audio_format_conversion.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/rtp_rtcp/include/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/output_mixer.h"
#include "webrtc/voice_engine/statistics.h"
#include "webrtc/voice_engine/utility.h"

namespace webrtc {
namespace voe {

namespace {

constexpr int64_t kMaxRetransmissionWindowMs = 1000;
constexpr int64_t kMinRetransmissionWindowMs = 30;

}  // namespace

const int kTelephoneEventAttenuationdB = 10;

class RtcEventLogProxy final : public webrtc::RtcEventLog {
 public:
  RtcEventLogProxy() : event_log_(nullptr) {}

  bool StartLogging(const std::string& file_name,
                    int64_t max_size_bytes) override {
    RTC_NOTREACHED();
    return false;
  }

  bool StartLogging(rtc::PlatformFile log_file,
                    int64_t max_size_bytes) override {
    RTC_NOTREACHED();
    return false;
  }

  void StopLogging() override { RTC_NOTREACHED(); }

  void LogVideoReceiveStreamConfig(
      const webrtc::VideoReceiveStream::Config& config) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->LogVideoReceiveStreamConfig(config);
    }
  }

  void LogVideoSendStreamConfig(
      const webrtc::VideoSendStream::Config& config) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->LogVideoSendStreamConfig(config);
    }
  }

  void LogAudioReceiveStreamConfig(
      const webrtc::AudioReceiveStream::Config& config) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->LogAudioReceiveStreamConfig(config);
    }
  }

  void LogAudioSendStreamConfig(
      const webrtc::AudioSendStream::Config& config) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->LogAudioSendStreamConfig(config);
    }
  }

  void LogRtpHeader(webrtc::PacketDirection direction,
                    webrtc::MediaType media_type,
                    const uint8_t* header,
                    size_t packet_length) override {
    LogRtpHeader(direction, media_type, header, packet_length,
                 PacedPacketInfo::kNotAProbe);
  }

  void LogRtpHeader(webrtc::PacketDirection direction,
                    webrtc::MediaType media_type,
                    const uint8_t* header,
                    size_t packet_length,
                    int probe_cluster_id) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->LogRtpHeader(direction, media_type, header, packet_length,
                               probe_cluster_id);
    }
  }

  void LogRtcpPacket(webrtc::PacketDirection direction,
                     webrtc::MediaType media_type,
                     const uint8_t* packet,
                     size_t length) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->LogRtcpPacket(direction, media_type, packet, length);
    }
  }

  void LogAudioPlayout(uint32_t ssrc) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->LogAudioPlayout(ssrc);
    }
  }

  void LogLossBasedBweUpdate(int32_t bitrate_bps,
                             uint8_t fraction_loss,
                             int32_t total_packets) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->LogLossBasedBweUpdate(bitrate_bps, fraction_loss,
                                        total_packets);
    }
  }

  void LogDelayBasedBweUpdate(int32_t bitrate_bps,
                              BandwidthUsage detector_state) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->LogDelayBasedBweUpdate(bitrate_bps, detector_state);
    }
  }

  void LogAudioNetworkAdaptation(
      const AudioNetworkAdaptor::EncoderRuntimeConfig& config) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->LogAudioNetworkAdaptation(config);
    }
  }

  void LogProbeClusterCreated(int id,
                              int bitrate_bps,
                              int min_probes,
                              int min_bytes) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->LogProbeClusterCreated(id, bitrate_bps, min_probes,
                                         min_bytes);
    }
  };

  void LogProbeResultSuccess(int id, int bitrate_bps) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->LogProbeResultSuccess(id, bitrate_bps);
    }
  };

  void LogProbeResultFailure(int id,
                             ProbeFailureReason failure_reason) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->LogProbeResultFailure(id, failure_reason);
    }
  };

  void SetEventLog(RtcEventLog* event_log) {
    rtc::CritScope lock(&crit_);
    event_log_ = event_log;
  }

 private:
  rtc::CriticalSection crit_;
  RtcEventLog* event_log_ GUARDED_BY(crit_);
  RTC_DISALLOW_COPY_AND_ASSIGN(RtcEventLogProxy);
};

class RtcpRttStatsProxy final : public RtcpRttStats {
 public:
  RtcpRttStatsProxy() : rtcp_rtt_stats_(nullptr) {}

  void OnRttUpdate(int64_t rtt) override {
    rtc::CritScope lock(&crit_);
    if (rtcp_rtt_stats_)
      rtcp_rtt_stats_->OnRttUpdate(rtt);
  }

  int64_t LastProcessedRtt() const override {
    rtc::CritScope lock(&crit_);
    if (!rtcp_rtt_stats_)
      return 0;
    return rtcp_rtt_stats_->LastProcessedRtt();
  }

  void SetRtcpRttStats(RtcpRttStats* rtcp_rtt_stats) {
    rtc::CritScope lock(&crit_);
    rtcp_rtt_stats_ = rtcp_rtt_stats;
  }

 private:
  rtc::CriticalSection crit_;
  RtcpRttStats* rtcp_rtt_stats_ GUARDED_BY(crit_);
  RTC_DISALLOW_COPY_AND_ASSIGN(RtcpRttStatsProxy);
};

class TransportFeedbackProxy : public TransportFeedbackObserver {
 public:
  TransportFeedbackProxy() : feedback_observer_(nullptr) {
    pacer_thread_.DetachFromThread();
    network_thread_.DetachFromThread();
  }

  void SetTransportFeedbackObserver(
      TransportFeedbackObserver* feedback_observer) {
    RTC_DCHECK(thread_checker_.CalledOnValidThread());
    rtc::CritScope lock(&crit_);
    feedback_observer_ = feedback_observer;
  }

  // Implements TransportFeedbackObserver.
  void AddPacket(uint16_t sequence_number,
                 size_t length,
                 const PacedPacketInfo& pacing_info) override {
    RTC_DCHECK(pacer_thread_.CalledOnValidThread());
    rtc::CritScope lock(&crit_);
    if (feedback_observer_)
      feedback_observer_->AddPacket(sequence_number, length, pacing_info);
  }

  void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override {
    RTC_DCHECK(network_thread_.CalledOnValidThread());
    rtc::CritScope lock(&crit_);
    if (feedback_observer_)
      feedback_observer_->OnTransportFeedback(feedback);
  }
  std::vector<PacketInfo> GetTransportFeedbackVector() const override {
    RTC_NOTREACHED();
    return std::vector<PacketInfo>();
  }

 private:
  rtc::CriticalSection crit_;
  rtc::ThreadChecker thread_checker_;
  rtc::ThreadChecker pacer_thread_;
  rtc::ThreadChecker network_thread_;
  TransportFeedbackObserver* feedback_observer_ GUARDED_BY(&crit_);
};

class TransportSequenceNumberProxy : public TransportSequenceNumberAllocator {
 public:
  TransportSequenceNumberProxy() : seq_num_allocator_(nullptr) {
    pacer_thread_.DetachFromThread();
  }

  void SetSequenceNumberAllocator(
      TransportSequenceNumberAllocator* seq_num_allocator) {
    RTC_DCHECK(thread_checker_.CalledOnValidThread());
    rtc::CritScope lock(&crit_);
    seq_num_allocator_ = seq_num_allocator;
  }

  // Implements TransportSequenceNumberAllocator.
  uint16_t AllocateSequenceNumber() override {
    RTC_DCHECK(pacer_thread_.CalledOnValidThread());
    rtc::CritScope lock(&crit_);
    if (!seq_num_allocator_)
      return 0;
    return seq_num_allocator_->AllocateSequenceNumber();
  }

 private:
  rtc::CriticalSection crit_;
  rtc::ThreadChecker thread_checker_;
  rtc::ThreadChecker pacer_thread_;
  TransportSequenceNumberAllocator* seq_num_allocator_ GUARDED_BY(&crit_);
};

class RtpPacketSenderProxy : public RtpPacketSender {
 public:
  RtpPacketSenderProxy() : rtp_packet_sender_(nullptr) {}

  void SetPacketSender(RtpPacketSender* rtp_packet_sender) {
    RTC_DCHECK(thread_checker_.CalledOnValidThread());
    rtc::CritScope lock(&crit_);
    rtp_packet_sender_ = rtp_packet_sender;
  }

  // Implements RtpPacketSender.
  void InsertPacket(Priority priority,
                    uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_time_ms,
                    size_t bytes,
                    bool retransmission) override {
    rtc::CritScope lock(&crit_);
    if (rtp_packet_sender_) {
      rtp_packet_sender_->InsertPacket(priority, ssrc, sequence_number,
                                       capture_time_ms, bytes, retransmission);
    }
  }

 private:
  rtc::ThreadChecker thread_checker_;
  rtc::CriticalSection crit_;
  RtpPacketSender* rtp_packet_sender_ GUARDED_BY(&crit_);
};

class VoERtcpObserver : public RtcpBandwidthObserver {
 public:
  explicit VoERtcpObserver(Channel* owner)
      : owner_(owner), bandwidth_observer_(nullptr) {}
  virtual ~VoERtcpObserver() {}

  void SetBandwidthObserver(RtcpBandwidthObserver* bandwidth_observer) {
    rtc::CritScope lock(&crit_);
    bandwidth_observer_ = bandwidth_observer;
  }

  void OnReceivedEstimatedBitrate(uint32_t bitrate) override {
    rtc::CritScope lock(&crit_);
    if (bandwidth_observer_) {
      bandwidth_observer_->OnReceivedEstimatedBitrate(bitrate);
    }
  }

  void OnReceivedRtcpReceiverReport(const ReportBlockList& report_blocks,
                                    int64_t rtt,
                                    int64_t now_ms) override {
    {
      rtc::CritScope lock(&crit_);
      if (bandwidth_observer_) {
        bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, rtt,
                                                          now_ms);
      }
    }
    // TODO(mflodman): Do we need to aggregate reports here or can we jut send
    // what we get? I.e. do we ever get multiple reports bundled into one RTCP
    // report for VoiceEngine?
    if (report_blocks.empty())
      return;

    int fraction_lost_aggregate = 0;
    int total_number_of_packets = 0;

    // If receiving multiple report blocks, calculate the weighted average based
    // on the number of packets a report refers to.
    for (ReportBlockList::const_iterator block_it = report_blocks.begin();
         block_it != report_blocks.end(); ++block_it) {
      // Find the previous extended high sequence number for this remote SSRC,
      // to calculate the number of RTP packets this report refers to. Ignore if
      // we haven't seen this SSRC before.
      std::map<uint32_t, uint32_t>::iterator seq_num_it =
          extended_max_sequence_number_.find(block_it->sourceSSRC);
      int number_of_packets = 0;
      if (seq_num_it != extended_max_sequence_number_.end()) {
        number_of_packets = block_it->extendedHighSeqNum - seq_num_it->second;
      }
      fraction_lost_aggregate += number_of_packets * block_it->fractionLost;
      total_number_of_packets += number_of_packets;

      extended_max_sequence_number_[block_it->sourceSSRC] =
          block_it->extendedHighSeqNum;
    }
    int weighted_fraction_lost = 0;
    if (total_number_of_packets > 0) {
      weighted_fraction_lost =
          (fraction_lost_aggregate + total_number_of_packets / 2) /
          total_number_of_packets;
    }
    owner_->OnIncomingFractionLoss(weighted_fraction_lost);
  }

 private:
  Channel* owner_;
  // Maps remote side ssrc to extended highest sequence number received.
  std::map<uint32_t, uint32_t> extended_max_sequence_number_;
  rtc::CriticalSection crit_;
  RtcpBandwidthObserver* bandwidth_observer_ GUARDED_BY(crit_);
};

int32_t Channel::SendData(FrameType frameType,
                          uint8_t payloadType,
                          uint32_t timeStamp,
                          const uint8_t* payloadData,
                          size_t payloadSize,
                          const RTPFragmentationHeader* fragmentation) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SendData(frameType=%u, payloadType=%u, timeStamp=%u,"
               " payloadSize=%" PRIuS ", fragmentation=0x%x)",
               frameType, payloadType, timeStamp, payloadSize, fragmentation);

  if (_includeAudioLevelIndication) {
    // Store current audio level in the RTP/RTCP module.
    // The level will be used in combination with voice-activity state
    // (frameType) to add an RTP header extension
    _rtpRtcpModule->SetAudioLevel(rms_level_.Average());
  }

  // Push data from ACM to RTP/RTCP-module to deliver audio frame for
  // packetization.
  // This call will trigger Transport::SendPacket() from the RTP/RTCP module.
  if (!_rtpRtcpModule->SendOutgoingData(
          (FrameType&)frameType, payloadType, timeStamp,
          // Leaving the time when this frame was
          // received from the capture device as
          // undefined for voice for now.
          -1, payloadData, payloadSize, fragmentation, nullptr, nullptr)) {
    _engineStatisticsPtr->SetLastError(
        VE_RTP_RTCP_MODULE_ERROR, kTraceWarning,
        "Channel::SendData() failed to send data to RTP/RTCP module");
    return -1;
  }

  _lastLocalTimeStamp = timeStamp;
  _lastPayloadType = payloadType;

  return 0;
}

int32_t Channel::InFrameType(FrameType frame_type) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::InFrameType(frame_type=%d)", frame_type);

  rtc::CritScope cs(&_callbackCritSect);
  _sendFrameType = (frame_type == kAudioFrameSpeech);
  return 0;
}

bool Channel::SendRtp(const uint8_t* data,
                      size_t len,
                      const PacketOptions& options) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SendPacket(channel=%d, len=%" PRIuS ")", len);

  rtc::CritScope cs(&_callbackCritSect);

  if (_transportPtr == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::SendPacket() failed to send RTP packet due to"
                 " invalid transport object");
    return false;
  }

  uint8_t* bufferToSendPtr = (uint8_t*)data;
  size_t bufferLength = len;

  if (!_transportPtr->SendRtp(bufferToSendPtr, bufferLength, options)) {
    std::string transport_name =
        _externalTransport ? "external transport" : "WebRtc sockets";
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::SendPacket() RTP transmission using %s failed",
                 transport_name.c_str());
    return false;
  }
  return true;
}

bool Channel::SendRtcp(const uint8_t* data, size_t len) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SendRtcp(len=%" PRIuS ")", len);

  rtc::CritScope cs(&_callbackCritSect);
  if (_transportPtr == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::SendRtcp() failed to send RTCP packet"
                 " due to invalid transport object");
    return false;
  }

  uint8_t* bufferToSendPtr = (uint8_t*)data;
  size_t bufferLength = len;

  int n = _transportPtr->SendRtcp(bufferToSendPtr, bufferLength);
  if (n < 0) {
    std::string transport_name =
        _externalTransport ? "external transport" : "WebRtc sockets";
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::SendRtcp() transmission using %s failed",
                 transport_name.c_str());
    return false;
  }
  return true;
}

void Channel::OnIncomingSSRCChanged(uint32_t ssrc) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::OnIncomingSSRCChanged(SSRC=%d)", ssrc);

  // Update ssrc so that NTP for AV sync can be updated.
  _rtpRtcpModule->SetRemoteSSRC(ssrc);
}

void Channel::OnIncomingCSRCChanged(uint32_t CSRC, bool added) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::OnIncomingCSRCChanged(CSRC=%d, added=%d)", CSRC,
               added);
}

int32_t Channel::OnInitializeDecoder(
    int8_t payloadType,
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    int frequency,
    size_t channels,
    uint32_t rate) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::OnInitializeDecoder(payloadType=%d, "
               "payloadName=%s, frequency=%u, channels=%" PRIuS ", rate=%u)",
               payloadType, payloadName, frequency, channels, rate);

  CodecInst receiveCodec = {0};
  CodecInst dummyCodec = {0};

  receiveCodec.pltype = payloadType;
  receiveCodec.plfreq = frequency;
  receiveCodec.channels = channels;
  receiveCodec.rate = rate;
  strncpy(receiveCodec.plname, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);

  audio_coding_->Codec(payloadName, &dummyCodec, frequency, channels);
  receiveCodec.pacsize = dummyCodec.pacsize;

  // Register the new codec to the ACM
  if (!audio_coding_->RegisterReceiveCodec(receiveCodec.pltype,
                                           CodecInstToSdp(receiveCodec))) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::OnInitializeDecoder() invalid codec ("
                 "pt=%d, name=%s) received - 1",
                 payloadType, payloadName);
    _engineStatisticsPtr->SetLastError(VE_AUDIO_CODING_MODULE_ERROR);
    return -1;
  }

  return 0;
}

int32_t Channel::OnReceivedPayloadData(const uint8_t* payloadData,
                                       size_t payloadSize,
                                       const WebRtcRTPHeader* rtpHeader) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::OnReceivedPayloadData(payloadSize=%" PRIuS
               ","
               " payloadType=%u, audioChannel=%" PRIuS ")",
               payloadSize, rtpHeader->header.payloadType,
               rtpHeader->type.Audio.channel);

  if (!channel_state_.Get().playing) {
    // Avoid inserting into NetEQ when we are not playing. Count the
    // packet as discarded.
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
                 "received packet is discarded since playing is not"
                 " activated");
    return 0;
  }

  // Push the incoming payload (parsed and ready for decoding) into the ACM
  if (audio_coding_->IncomingPacket(payloadData, payloadSize, *rtpHeader) !=
      0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceWarning,
        "Channel::OnReceivedPayloadData() unable to push data to the ACM");
    return -1;
  }

  int64_t round_trip_time = 0;
  _rtpRtcpModule->RTT(rtp_receiver_->SSRC(), &round_trip_time, NULL, NULL,
                      NULL);

  std::vector<uint16_t> nack_list = audio_coding_->GetNackList(round_trip_time);
  if (!nack_list.empty()) {
    // Can't use nack_list.data() since it's not supported by all
    // compilers.
    ResendPackets(&(nack_list[0]), static_cast<int>(nack_list.size()));
  }
  return 0;
}

bool Channel::OnRecoveredPacket(const uint8_t* rtp_packet,
                                size_t rtp_packet_length) {
  RTPHeader header;
  if (!rtp_header_parser_->Parse(rtp_packet, rtp_packet_length, &header)) {
    WEBRTC_TRACE(kTraceDebug, webrtc::kTraceVoice, _channelId,
                 "IncomingPacket invalid RTP header");
    return false;
  }
  header.payload_type_frequency =
      rtp_payload_registry_->GetPayloadTypeFrequency(header.payloadType);
  if (header.payload_type_frequency < 0)
    return false;
  return ReceivePacket(rtp_packet, rtp_packet_length, header, false);
}

MixerParticipant::AudioFrameInfo Channel::GetAudioFrameWithMuted(
    int32_t id,
    AudioFrame* audioFrame) {
  unsigned int ssrc;
  RTC_CHECK_EQ(GetRemoteSSRC(ssrc), 0);
  event_log_proxy_->LogAudioPlayout(ssrc);
  // Get 10ms raw PCM data from the ACM (mixer limits output frequency)
  bool muted;
  if (audio_coding_->PlayoutData10Ms(audioFrame->sample_rate_hz_, audioFrame,
                                     &muted) == -1) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::GetAudioFrame() PlayoutData10Ms() failed!");
    // In all likelihood, the audio in this frame is garbage. We return an
    // error so that the audio mixer module doesn't add it to the mix. As
    // a result, it won't be played out and the actions skipped here are
    // irrelevant.
    return MixerParticipant::AudioFrameInfo::kError;
  }

  if (muted) {
    // TODO(henrik.lundin): We should be able to do better than this. But we
    // will have to go through all the cases below where the audio samples may
    // be used, and handle the muted case in some way.
    AudioFrameOperations::Mute(audioFrame);
  }

  // Convert module ID to internal VoE channel ID
  audioFrame->id_ = VoEChannelId(audioFrame->id_);
  // Store speech type for dead-or-alive detection
  _outputSpeechType = audioFrame->speech_type_;

  ChannelState::State state = channel_state_.Get();

  {
    // Pass the audio buffers to an optional sink callback, before applying
    // scaling/panning, as that applies to the mix operation.
    // External recipients of the audio (e.g. via AudioTrack), will do their
    // own mixing/dynamic processing.
    rtc::CritScope cs(&_callbackCritSect);
    if (audio_sink_) {
      AudioSinkInterface::Data data(
          &audioFrame->data_[0], audioFrame->samples_per_channel_,
          audioFrame->sample_rate_hz_, audioFrame->num_channels_,
          audioFrame->timestamp_);
      audio_sink_->OnData(data);
    }
  }

  float output_gain = 1.0f;
  float left_pan = 1.0f;
  float right_pan = 1.0f;
  {
    rtc::CritScope cs(&volume_settings_critsect_);
    output_gain = _outputGain;
    left_pan = _panLeft;
    right_pan = _panRight;
  }

  // Output volume scaling
  if (output_gain < 0.99f || output_gain > 1.01f) {
    AudioFrameOperations::ScaleWithSat(output_gain, *audioFrame);
  }

  // Scale left and/or right channel(s) if stereo and master balance is
  // active

  if (left_pan != 1.0f || right_pan != 1.0f) {
    if (audioFrame->num_channels_ == 1) {
      // Emulate stereo mode since panning is active.
      // The mono signal is copied to both left and right channels here.
      AudioFrameOperations::MonoToStereo(audioFrame);
    }
    // For true stereo mode (when we are receiving a stereo signal), no
    // action is needed.

    // Do the panning operation (the audio frame contains stereo at this
    // stage)
    AudioFrameOperations::Scale(left_pan, right_pan, *audioFrame);
  }

  // Mix decoded PCM output with file if file mixing is enabled
  if (state.output_file_playing) {
    MixAudioWithFile(*audioFrame, audioFrame->sample_rate_hz_);
    muted = false;  // We may have added non-zero samples.
  }

  // Record playout if enabled
  {
    rtc::CritScope cs(&_fileCritSect);

    if (_outputFileRecording && output_file_recorder_) {
      output_file_recorder_->RecordAudioToFile(*audioFrame);
    }
  }

  // Measure audio level (0-9)
  // TODO(henrik.lundin) Use the |muted| information here too.
  _outputAudioLevel.ComputeLevel(*audioFrame);

  if (capture_start_rtp_time_stamp_ < 0 && audioFrame->timestamp_ != 0) {
    // The first frame with a valid rtp timestamp.
    capture_start_rtp_time_stamp_ = audioFrame->timestamp_;
  }

  if (capture_start_rtp_time_stamp_ >= 0) {
    // audioFrame.timestamp_ should be valid from now on.

    // Compute elapsed time.
    int64_t unwrap_timestamp =
        rtp_ts_wraparound_handler_->Unwrap(audioFrame->timestamp_);
    audioFrame->elapsed_time_ms_ =
        (unwrap_timestamp - capture_start_rtp_time_stamp_) /
        (GetRtpTimestampRateHz() / 1000);

    {
      rtc::CritScope lock(&ts_stats_lock_);
      // Compute ntp time.
      audioFrame->ntp_time_ms_ =
          ntp_estimator_.Estimate(audioFrame->timestamp_);
      // |ntp_time_ms_| won't be valid until at least 2 RTCP SRs are received.
      if (audioFrame->ntp_time_ms_ > 0) {
        // Compute |capture_start_ntp_time_ms_| so that
        // |capture_start_ntp_time_ms_| + |elapsed_time_ms_| == |ntp_time_ms_|
        capture_start_ntp_time_ms_ =
            audioFrame->ntp_time_ms_ - audioFrame->elapsed_time_ms_;
      }
    }
  }

  return muted ? MixerParticipant::AudioFrameInfo::kMuted
               : MixerParticipant::AudioFrameInfo::kNormal;
}

AudioMixer::Source::AudioFrameInfo Channel::GetAudioFrameWithInfo(
    int sample_rate_hz,
    AudioFrame* audio_frame) {
  audio_frame->sample_rate_hz_ = sample_rate_hz;

  const auto frame_info = GetAudioFrameWithMuted(-1, audio_frame);

  using FrameInfo = AudioMixer::Source::AudioFrameInfo;
  FrameInfo new_audio_frame_info = FrameInfo::kError;
  switch (frame_info) {
    case MixerParticipant::AudioFrameInfo::kNormal:
      new_audio_frame_info = FrameInfo::kNormal;
      break;
    case MixerParticipant::AudioFrameInfo::kMuted:
      new_audio_frame_info = FrameInfo::kMuted;
      break;
    case MixerParticipant::AudioFrameInfo::kError:
      new_audio_frame_info = FrameInfo::kError;
      break;
  }
  return new_audio_frame_info;
}

int32_t Channel::NeededFrequency(int32_t id) const {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::NeededFrequency(id=%d)", id);

  int highestNeeded = 0;

  // Determine highest needed receive frequency
  int32_t receiveFrequency = audio_coding_->ReceiveFrequency();

  // Return the bigger of playout and receive frequency in the ACM.
  if (audio_coding_->PlayoutFrequency() > receiveFrequency) {
    highestNeeded = audio_coding_->PlayoutFrequency();
  } else {
    highestNeeded = receiveFrequency;
  }

  // Special case, if we're playing a file on the playout side
  // we take that frequency into consideration as well
  // This is not needed on sending side, since the codec will
  // limit the spectrum anyway.
  if (channel_state_.Get().output_file_playing) {
    rtc::CritScope cs(&_fileCritSect);
    if (output_file_player_) {
      if (output_file_player_->Frequency() > highestNeeded) {
        highestNeeded = output_file_player_->Frequency();
      }
    }
  }

  return (highestNeeded);
}

int32_t Channel::CreateChannel(
    Channel*& channel,
    int32_t channelId,
    uint32_t instanceId,
    const VoEBase::ChannelConfig& config) {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(instanceId, channelId),
               "Channel::CreateChannel(channelId=%d, instanceId=%d)", channelId,
               instanceId);

  channel = new Channel(channelId, instanceId, config);
  if (channel == NULL) {
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(instanceId, channelId),
                 "Channel::CreateChannel() unable to allocate memory for"
                 " channel");
    return -1;
  }
  return 0;
}

void Channel::PlayNotification(int32_t id, uint32_t durationMs) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::PlayNotification(id=%d, durationMs=%d)", id,
               durationMs);

  // Not implement yet
}

void Channel::RecordNotification(int32_t id, uint32_t durationMs) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::RecordNotification(id=%d, durationMs=%d)", id,
               durationMs);

  // Not implement yet
}

void Channel::PlayFileEnded(int32_t id) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::PlayFileEnded(id=%d)", id);

  if (id == _inputFilePlayerId) {
    channel_state_.SetInputFilePlaying(false);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::PlayFileEnded() => input file player module is"
                 " shutdown");
  } else if (id == _outputFilePlayerId) {
    channel_state_.SetOutputFilePlaying(false);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::PlayFileEnded() => output file player module is"
                 " shutdown");
  }
}

void Channel::RecordFileEnded(int32_t id) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::RecordFileEnded(id=%d)", id);

  assert(id == _outputFileRecorderId);

  rtc::CritScope cs(&_fileCritSect);

  _outputFileRecording = false;
  WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::RecordFileEnded() => output file recorder module is"
               " shutdown");
}

Channel::Channel(int32_t channelId,
                 uint32_t instanceId,
                 const VoEBase::ChannelConfig& config)
    : _instanceId(instanceId),
      _channelId(channelId),
      event_log_proxy_(new RtcEventLogProxy()),
      rtcp_rtt_stats_proxy_(new RtcpRttStatsProxy()),
      rtp_header_parser_(RtpHeaderParser::Create()),
      rtp_payload_registry_(new RTPPayloadRegistry()),
      rtp_receive_statistics_(
          ReceiveStatistics::Create(Clock::GetRealTimeClock())),
      rtp_receiver_(
          RtpReceiver::CreateAudioReceiver(Clock::GetRealTimeClock(),
                                           this,
                                           this,
                                           rtp_payload_registry_.get())),
      telephone_event_handler_(rtp_receiver_->GetTelephoneEventHandler()),
      _outputAudioLevel(),
      _externalTransport(false),
      // Avoid conflict with other channels by adding 1024 - 1026,
      // won't use as much as 1024 channels.
      _inputFilePlayerId(VoEModuleId(instanceId, channelId) + 1024),
      _outputFilePlayerId(VoEModuleId(instanceId, channelId) + 1025),
      _outputFileRecorderId(VoEModuleId(instanceId, channelId) + 1026),
      _outputFileRecording(false),
      _timeStamp(0),  // This is just an offset, RTP module will add it's own
                      // random offset
      ntp_estimator_(Clock::GetRealTimeClock()),
      playout_timestamp_rtp_(0),
      playout_delay_ms_(0),
      send_sequence_number_(0),
      rtp_ts_wraparound_handler_(new rtc::TimestampWrapAroundHandler()),
      capture_start_rtp_time_stamp_(-1),
      capture_start_ntp_time_ms_(-1),
      _engineStatisticsPtr(NULL),
      _outputMixerPtr(NULL),
      _moduleProcessThreadPtr(NULL),
      _audioDeviceModulePtr(NULL),
      _voiceEngineObserverPtr(NULL),
      _callbackCritSectPtr(NULL),
      _transportPtr(NULL),
      _sendFrameType(0),
      _mixFileWithMicrophone(false),
      input_mute_(false),
      previous_frame_muted_(false),
      _panLeft(1.0f),
      _panRight(1.0f),
      _outputGain(1.0f),
      _lastLocalTimeStamp(0),
      _lastPayloadType(0),
      _includeAudioLevelIndication(false),
      transport_overhead_per_packet_(0),
      rtp_overhead_per_packet_(0),
      _outputSpeechType(AudioFrame::kNormalSpeech),
      restored_packet_in_use_(false),
      rtcp_observer_(new VoERtcpObserver(this)),
      associate_send_channel_(ChannelOwner(nullptr)),
      pacing_enabled_(config.enable_voice_pacing),
      feedback_observer_proxy_(new TransportFeedbackProxy()),
      seq_num_allocator_proxy_(new TransportSequenceNumberProxy()),
      rtp_packet_sender_proxy_(new RtpPacketSenderProxy()),
      retransmission_rate_limiter_(new RateLimiter(Clock::GetRealTimeClock(),
                                                   kMaxRetransmissionWindowMs)),
      decoder_factory_(config.acm_config.decoder_factory) {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::Channel() - ctor");
  AudioCodingModule::Config acm_config(config.acm_config);
  acm_config.id = VoEModuleId(instanceId, channelId);
  acm_config.neteq_config.enable_muted_state = true;
  audio_coding_.reset(AudioCodingModule::Create(acm_config));

  _outputAudioLevel.Clear();

  RtpRtcp::Configuration configuration;
  configuration.audio = true;
  configuration.outgoing_transport = this;
  configuration.overhead_observer = this;
  configuration.receive_statistics = rtp_receive_statistics_.get();
  configuration.bandwidth_callback = rtcp_observer_.get();
  if (pacing_enabled_) {
    configuration.paced_sender = rtp_packet_sender_proxy_.get();
    configuration.transport_sequence_number_allocator =
        seq_num_allocator_proxy_.get();
    configuration.transport_feedback_callback = feedback_observer_proxy_.get();
  }
  configuration.event_log = &(*event_log_proxy_);
  configuration.rtt_stats = &(*rtcp_rtt_stats_proxy_);
  configuration.retransmission_rate_limiter =
      retransmission_rate_limiter_.get();

  _rtpRtcpModule.reset(RtpRtcp::CreateRtpRtcp(configuration));
  _rtpRtcpModule->SetSendingMediaStatus(false);
}

Channel::~Channel() {
  rtp_receive_statistics_->RegisterRtcpStatisticsCallback(NULL);
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::~Channel() - dtor");

  StopSend();
  StopPlayout();

  {
    rtc::CritScope cs(&_fileCritSect);
    if (input_file_player_) {
      input_file_player_->RegisterModuleFileCallback(NULL);
      input_file_player_->StopPlayingFile();
    }
    if (output_file_player_) {
      output_file_player_->RegisterModuleFileCallback(NULL);
      output_file_player_->StopPlayingFile();
    }
    if (output_file_recorder_) {
      output_file_recorder_->RegisterModuleFileCallback(NULL);
      output_file_recorder_->StopRecording();
    }
  }

  // The order to safely shutdown modules in a channel is:
  // 1. De-register callbacks in modules
  // 2. De-register modules in process thread
  // 3. Destroy modules
  if (audio_coding_->RegisterTransportCallback(NULL) == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                 "~Channel() failed to de-register transport callback"
                 " (Audio coding module)");
  }
  if (audio_coding_->RegisterVADCallback(NULL) == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                 "~Channel() failed to de-register VAD callback"
                 " (Audio coding module)");
  }
  // De-register modules in process thread
  _moduleProcessThreadPtr->DeRegisterModule(_rtpRtcpModule.get());

  // End of modules shutdown
}

int32_t Channel::Init() {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::Init()");

  channel_state_.Reset();

  // --- Initial sanity

  if ((_engineStatisticsPtr == NULL) || (_moduleProcessThreadPtr == NULL)) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::Init() must call SetEngineInformation() first");
    return -1;
  }

  // --- Add modules to process thread (for periodic schedulation)

  _moduleProcessThreadPtr->RegisterModule(_rtpRtcpModule.get(), RTC_FROM_HERE);

  // --- ACM initialization

  if (audio_coding_->InitializeReceiver() == -1) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "Channel::Init() unable to initialize the ACM - 1");
    return -1;
  }

  // --- RTP/RTCP module initialization

  // Ensure that RTCP is enabled by default for the created channel.
  // Note that, the module will keep generating RTCP until it is explicitly
  // disabled by the user.
  // After StopListen (when no sockets exists), RTCP packets will no longer
  // be transmitted since the Transport object will then be invalid.
  telephone_event_handler_->SetTelephoneEventForwardToDecoder(true);
  // RTCP is enabled by default.
  _rtpRtcpModule->SetRTCPStatus(RtcpMode::kCompound);
  // --- Register all permanent callbacks
  const bool fail = (audio_coding_->RegisterTransportCallback(this) == -1) ||
                    (audio_coding_->RegisterVADCallback(this) == -1);

  if (fail) {
    _engineStatisticsPtr->SetLastError(
        VE_CANNOT_INIT_CHANNEL, kTraceError,
        "Channel::Init() callbacks not registered");
    return -1;
  }

  // --- Register all supported codecs to the receiving side of the
  // RTP/RTCP module

  CodecInst codec;
  const uint8_t nSupportedCodecs = AudioCodingModule::NumberOfCodecs();

  for (int idx = 0; idx < nSupportedCodecs; idx++) {
    // Open up the RTP/RTCP receiver for all supported codecs
    if ((audio_coding_->Codec(idx, &codec) == -1) ||
        (rtp_receiver_->RegisterReceivePayload(codec) == -1)) {
      WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                   "Channel::Init() unable to register %s "
                   "(%d/%d/%" PRIuS "/%d) to RTP/RTCP receiver",
                   codec.plname, codec.pltype, codec.plfreq, codec.channels,
                   codec.rate);
    } else {
      WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                   "Channel::Init() %s (%d/%d/%" PRIuS
                   "/%d) has been "
                   "added to the RTP/RTCP receiver",
                   codec.plname, codec.pltype, codec.plfreq, codec.channels,
                   codec.rate);
    }

    // Ensure that PCMU is used as default codec on the sending side
    if (!STR_CASE_CMP(codec.plname, "PCMU") && (codec.channels == 1)) {
      SetSendCodec(codec);
    }

    // Register default PT for outband 'telephone-event'
    if (!STR_CASE_CMP(codec.plname, "telephone-event")) {
      if (_rtpRtcpModule->RegisterSendPayload(codec) == -1 ||
          !audio_coding_->RegisterReceiveCodec(codec.pltype,
                                               CodecInstToSdp(codec))) {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                     "Channel::Init() failed to register outband "
                     "'telephone-event' (%d/%d) correctly",
                     codec.pltype, codec.plfreq);
      }
    }

    if (!STR_CASE_CMP(codec.plname, "CN")) {
      if (!codec_manager_.RegisterEncoder(codec) ||
          !codec_manager_.MakeEncoder(&rent_a_codec_, audio_coding_.get()) ||
          !audio_coding_->RegisterReceiveCodec(codec.pltype,
                                               CodecInstToSdp(codec)) ||
          _rtpRtcpModule->RegisterSendPayload(codec) == -1) {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                     "Channel::Init() failed to register CN (%d/%d) "
                     "correctly - 1",
                     codec.pltype, codec.plfreq);
      }
    }
  }

  return 0;
}

int32_t Channel::SetEngineInformation(Statistics& engineStatistics,
                                      OutputMixer& outputMixer,
                                      ProcessThread& moduleProcessThread,
                                      AudioDeviceModule& audioDeviceModule,
                                      VoiceEngineObserver* voiceEngineObserver,
                                      rtc::CriticalSection* callbackCritSect) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetEngineInformation()");
  _engineStatisticsPtr = &engineStatistics;
  _outputMixerPtr = &outputMixer;
  _moduleProcessThreadPtr = &moduleProcessThread;
  _audioDeviceModulePtr = &audioDeviceModule;
  _voiceEngineObserverPtr = voiceEngineObserver;
  _callbackCritSectPtr = callbackCritSect;
  return 0;
}

int32_t Channel::UpdateLocalTimeStamp() {
  _timeStamp += static_cast<uint32_t>(_audioFrame.samples_per_channel_);
  return 0;
}

void Channel::SetSink(std::unique_ptr<AudioSinkInterface> sink) {
  rtc::CritScope cs(&_callbackCritSect);
  audio_sink_ = std::move(sink);
}

const rtc::scoped_refptr<AudioDecoderFactory>&
Channel::GetAudioDecoderFactory() const {
  return decoder_factory_;
}

int32_t Channel::StartPlayout() {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::StartPlayout()");
  if (channel_state_.Get().playing) {
    return 0;
  }

  // Add participant as candidates for mixing.
  if (_outputMixerPtr->SetMixabilityStatus(*this, true) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CONF_MIX_MODULE_ERROR, kTraceError,
        "StartPlayout() failed to add participant to mixer");
    return -1;
  }

  channel_state_.SetPlaying(true);
  if (RegisterFilePlayingToMixer() != 0)
    return -1;

  return 0;
}

int32_t Channel::StopPlayout() {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::StopPlayout()");
  if (!channel_state_.Get().playing) {
    return 0;
  }

  // Remove participant as candidates for mixing
  if (_outputMixerPtr->SetMixabilityStatus(*this, false) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CONF_MIX_MODULE_ERROR, kTraceError,
        "StopPlayout() failed to remove participant from mixer");
    return -1;
  }

  channel_state_.SetPlaying(false);
  _outputAudioLevel.Clear();

  return 0;
}

int32_t Channel::StartSend() {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::StartSend()");
  if (channel_state_.Get().sending) {
    return 0;
  }
  channel_state_.SetSending(true);

  // Resume the previous sequence number which was reset by StopSend(). This
  // needs to be done before |sending| is set to true on the RTP/RTCP module.
  if (send_sequence_number_) {
    _rtpRtcpModule->SetSequenceNumber(send_sequence_number_);
  }
  _rtpRtcpModule->SetSendingMediaStatus(true);
  if (_rtpRtcpModule->SetSendingStatus(true) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_RTP_RTCP_MODULE_ERROR, kTraceError,
        "StartSend() RTP/RTCP failed to start sending");
    _rtpRtcpModule->SetSendingMediaStatus(false);
    rtc::CritScope cs(&_callbackCritSect);
    channel_state_.SetSending(false);
    return -1;
  }

  return 0;
}

int32_t Channel::StopSend() {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::StopSend()");
  if (!channel_state_.Get().sending) {
    return 0;
  }
  channel_state_.SetSending(false);

  // Store the sequence number to be able to pick up the same sequence for
  // the next StartSend(). This is needed for restarting device, otherwise
  // it might cause libSRTP to complain about packets being replayed.
  // TODO(xians): Remove this workaround after RtpRtcpModule's refactoring
  // CL is landed. See issue
  // https://code.google.com/p/webrtc/issues/detail?id=2111 .
  send_sequence_number_ = _rtpRtcpModule->SequenceNumber();

  // Reset sending SSRC and sequence number and triggers direct transmission
  // of RTCP BYE
  if (_rtpRtcpModule->SetSendingStatus(false) == -1) {
    _engineStatisticsPtr->SetLastError(
        VE_RTP_RTCP_MODULE_ERROR, kTraceWarning,
        "StartSend() RTP/RTCP failed to stop sending");
  }
  _rtpRtcpModule->SetSendingMediaStatus(false);

  return 0;
}

int32_t Channel::RegisterVoiceEngineObserver(VoiceEngineObserver& observer) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::RegisterVoiceEngineObserver()");
  rtc::CritScope cs(&_callbackCritSect);

  if (_voiceEngineObserverPtr) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_OPERATION, kTraceError,
        "RegisterVoiceEngineObserver() observer already enabled");
    return -1;
  }
  _voiceEngineObserverPtr = &observer;
  return 0;
}

int32_t Channel::DeRegisterVoiceEngineObserver() {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::DeRegisterVoiceEngineObserver()");
  rtc::CritScope cs(&_callbackCritSect);

  if (!_voiceEngineObserverPtr) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_OPERATION, kTraceWarning,
        "DeRegisterVoiceEngineObserver() observer already disabled");
    return 0;
  }
  _voiceEngineObserverPtr = NULL;
  return 0;
}

int32_t Channel::GetSendCodec(CodecInst& codec) {
  auto send_codec = codec_manager_.GetCodecInst();
  if (send_codec) {
    codec = *send_codec;
    return 0;
  }
  return -1;
}

int32_t Channel::GetRecCodec(CodecInst& codec) {
  return (audio_coding_->ReceiveCodec(&codec));
}

int32_t Channel::SetSendCodec(const CodecInst& codec) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetSendCodec()");

  if (!codec_manager_.RegisterEncoder(codec) ||
      !codec_manager_.MakeEncoder(&rent_a_codec_, audio_coding_.get())) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, _channelId),
                 "SetSendCodec() failed to register codec to ACM");
    return -1;
  }

  if (_rtpRtcpModule->RegisterSendPayload(codec) != 0) {
    _rtpRtcpModule->DeRegisterSendPayload(codec.pltype);
    if (_rtpRtcpModule->RegisterSendPayload(codec) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, _channelId),
                   "SetSendCodec() failed to register codec to"
                   " RTP/RTCP module");
      return -1;
    }
  }

  return 0;
}

void Channel::SetBitRate(int bitrate_bps, int64_t probing_interval_ms) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetBitRate(bitrate_bps=%d)", bitrate_bps);
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->OnReceivedUplinkBandwidth(
          bitrate_bps, rtc::Optional<int64_t>(probing_interval_ms));
    }
  });
  retransmission_rate_limiter_->SetMaxRate(bitrate_bps);
}

void Channel::OnIncomingFractionLoss(int fraction_lost) {
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder)
      (*encoder)->OnReceivedUplinkPacketLossFraction(fraction_lost / 255.0f);
  });
}

int32_t Channel::SetVADStatus(bool enableVAD,
                              ACMVADMode mode,
                              bool disableDTX) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetVADStatus(mode=%d)", mode);
  RTC_DCHECK(!(disableDTX && enableVAD));  // disableDTX mode is deprecated.
  if (!codec_manager_.SetVAD(enableVAD, mode) ||
      !codec_manager_.MakeEncoder(&rent_a_codec_, audio_coding_.get())) {
    _engineStatisticsPtr->SetLastError(VE_AUDIO_CODING_MODULE_ERROR,
                                       kTraceError,
                                       "SetVADStatus() failed to set VAD");
    return -1;
  }
  return 0;
}

int32_t Channel::GetVADStatus(bool& enabledVAD,
                              ACMVADMode& mode,
                              bool& disabledDTX) {
  const auto* params = codec_manager_.GetStackParams();
  enabledVAD = params->use_cng;
  mode = params->vad_mode;
  disabledDTX = !params->use_cng;
  return 0;
}

int32_t Channel::SetRecPayloadType(const CodecInst& codec) {
  return SetRecPayloadType(codec.pltype, CodecInstToSdp(codec));
}

int32_t Channel::SetRecPayloadType(int payload_type,
                                   const SdpAudioFormat& format) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetRecPayloadType()");

  if (channel_state_.Get().playing) {
    _engineStatisticsPtr->SetLastError(
        VE_ALREADY_PLAYING, kTraceError,
        "SetRecPayloadType() unable to set PT while playing");
    return -1;
  }

  const CodecInst codec = SdpToCodecInst(payload_type, format);

  if (payload_type == -1) {
    // De-register the selected codec (RTP/RTCP module and ACM)

    int8_t pltype(-1);
    CodecInst rxCodec = codec;

    // Get payload type for the given codec
    rtp_payload_registry_->ReceivePayloadType(rxCodec, &pltype);
    rxCodec.pltype = pltype;

    if (rtp_receiver_->DeRegisterReceivePayload(pltype) != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_RTP_RTCP_MODULE_ERROR, kTraceError,
          "SetRecPayloadType() RTP/RTCP-module deregistration "
          "failed");
      return -1;
    }
    if (audio_coding_->UnregisterReceiveCodec(rxCodec.pltype) != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
          "SetRecPayloadType() ACM deregistration failed - 1");
      return -1;
    }
    return 0;
  }

  if (rtp_receiver_->RegisterReceivePayload(codec) != 0) {
    // First attempt to register failed => de-register and try again
    // TODO(kwiberg): Retrying is probably not necessary, since
    // AcmReceiver::AddCodec also retries.
    rtp_receiver_->DeRegisterReceivePayload(codec.pltype);
    if (rtp_receiver_->RegisterReceivePayload(codec) != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_RTP_RTCP_MODULE_ERROR, kTraceError,
          "SetRecPayloadType() RTP/RTCP-module registration failed");
      return -1;
    }
  }
  if (!audio_coding_->RegisterReceiveCodec(payload_type, format)) {
    audio_coding_->UnregisterReceiveCodec(payload_type);
    if (!audio_coding_->RegisterReceiveCodec(payload_type, format)) {
      _engineStatisticsPtr->SetLastError(
          VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
          "SetRecPayloadType() ACM registration failed - 1");
      return -1;
    }
  }
  return 0;
}

int32_t Channel::GetRecPayloadType(CodecInst& codec) {
  int8_t payloadType(-1);
  if (rtp_payload_registry_->ReceivePayloadType(codec, &payloadType) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_RTP_RTCP_MODULE_ERROR, kTraceWarning,
        "GetRecPayloadType() failed to retrieve RX payload type");
    return -1;
  }
  codec.pltype = payloadType;
  return 0;
}

int32_t Channel::SetSendCNPayloadType(int type, PayloadFrequencies frequency) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetSendCNPayloadType()");

  CodecInst codec;
  int32_t samplingFreqHz(-1);
  const size_t kMono = 1;
  if (frequency == kFreq32000Hz)
    samplingFreqHz = 32000;
  else if (frequency == kFreq16000Hz)
    samplingFreqHz = 16000;

  if (audio_coding_->Codec("CN", &codec, samplingFreqHz, kMono) == -1) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetSendCNPayloadType() failed to retrieve default CN codec "
        "settings");
    return -1;
  }

  // Modify the payload type (must be set to dynamic range)
  codec.pltype = type;

  if (!codec_manager_.RegisterEncoder(codec) ||
      !codec_manager_.MakeEncoder(&rent_a_codec_, audio_coding_.get())) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetSendCNPayloadType() failed to register CN to ACM");
    return -1;
  }

  if (_rtpRtcpModule->RegisterSendPayload(codec) != 0) {
    _rtpRtcpModule->DeRegisterSendPayload(codec.pltype);
    if (_rtpRtcpModule->RegisterSendPayload(codec) != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_RTP_RTCP_MODULE_ERROR, kTraceError,
          "SetSendCNPayloadType() failed to register CN to RTP/RTCP "
          "module");
      return -1;
    }
  }
  return 0;
}

int Channel::SetOpusMaxPlaybackRate(int frequency_hz) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetOpusMaxPlaybackRate()");

  if (audio_coding_->SetOpusMaxPlaybackRate(frequency_hz) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetOpusMaxPlaybackRate() failed to set maximum playback rate");
    return -1;
  }
  return 0;
}

int Channel::SetOpusDtx(bool enable_dtx) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetOpusDtx(%d)", enable_dtx);
  int ret = enable_dtx ? audio_coding_->EnableOpusDtx()
                       : audio_coding_->DisableOpusDtx();
  if (ret != 0) {
    _engineStatisticsPtr->SetLastError(VE_AUDIO_CODING_MODULE_ERROR,
                                       kTraceError, "SetOpusDtx() failed");
    return -1;
  }
  return 0;
}

int Channel::GetOpusDtx(bool* enabled) {
  int success = -1;
  audio_coding_->QueryEncoder([&](AudioEncoder const* encoder) {
    if (encoder) {
      *enabled = encoder->GetDtx();
      success = 0;
    }
  });
  return success;
}

bool Channel::EnableAudioNetworkAdaptor(const std::string& config_string) {
  bool success = false;
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      success = (*encoder)->EnableAudioNetworkAdaptor(
          config_string, event_log_proxy_.get(), Clock::GetRealTimeClock());
    }
  });
  return success;
}

void Channel::DisableAudioNetworkAdaptor() {
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder)
      (*encoder)->DisableAudioNetworkAdaptor();
  });
}

void Channel::SetReceiverFrameLengthRange(int min_frame_length_ms,
                                          int max_frame_length_ms) {
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->SetReceiverFrameLengthRange(min_frame_length_ms,
                                              max_frame_length_ms);
    }
  });
}

int32_t Channel::RegisterExternalTransport(Transport* transport) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::RegisterExternalTransport()");

  rtc::CritScope cs(&_callbackCritSect);
  if (_externalTransport) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_OPERATION, kTraceError,
        "RegisterExternalTransport() external transport already enabled");
    return -1;
  }
  _externalTransport = true;
  _transportPtr = transport;
  return 0;
}

int32_t Channel::DeRegisterExternalTransport() {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::DeRegisterExternalTransport()");

  rtc::CritScope cs(&_callbackCritSect);
  if (_transportPtr) {
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "DeRegisterExternalTransport() all transport is disabled");
  } else {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_OPERATION, kTraceWarning,
        "DeRegisterExternalTransport() external transport already "
        "disabled");
  }
  _externalTransport = false;
  _transportPtr = NULL;
  return 0;
}

// TODO(nisse): Delete this method together with ReceivedRTPPacket.
// It's a temporary hack to support both ReceivedRTPPacket and
// OnRtpPacket interfaces without too much code duplication.
bool Channel::OnRtpPacketWithHeader(const uint8_t* received_packet,
                                    size_t length,
                                    RTPHeader *header) {
  // Store playout timestamp for the received RTP packet
  UpdatePlayoutTimestamp(false);

  header->payload_type_frequency =
      rtp_payload_registry_->GetPayloadTypeFrequency(header->payloadType);
  if (header->payload_type_frequency < 0)
    return false;
  bool in_order = IsPacketInOrder(*header);
  rtp_receive_statistics_->IncomingPacket(
      *header, length, IsPacketRetransmitted(*header, in_order));
  rtp_payload_registry_->SetIncomingPayloadType(*header);

  return ReceivePacket(received_packet, length, *header, in_order);
}

int32_t Channel::ReceivedRTPPacket(const uint8_t* received_packet,
                                   size_t length,
                                   const PacketTime& packet_time) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::ReceivedRTPPacket()");

  RTPHeader header;
  if (!rtp_header_parser_->Parse(received_packet, length, &header)) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVoice, _channelId,
                 "Incoming packet: invalid RTP header");
    return -1;
  }
  return OnRtpPacketWithHeader(received_packet, length, &header) ? 0 : -1;
}

void Channel::OnRtpPacket(const RtpPacketReceived& packet) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::ReceivedRTPPacket()");

  RTPHeader header;
  packet.GetHeader(&header);
  OnRtpPacketWithHeader(packet.data(), packet.size(), &header);
}

bool Channel::ReceivePacket(const uint8_t* packet,
                            size_t packet_length,
                            const RTPHeader& header,
                            bool in_order) {
  if (rtp_payload_registry_->IsRtx(header)) {
    return HandleRtxPacket(packet, packet_length, header);
  }
  const uint8_t* payload = packet + header.headerLength;
  assert(packet_length >= header.headerLength);
  size_t payload_length = packet_length - header.headerLength;
  PayloadUnion payload_specific;
  if (!rtp_payload_registry_->GetPayloadSpecifics(header.payloadType,
                                                  &payload_specific)) {
    return false;
  }
  return rtp_receiver_->IncomingRtpPacket(header, payload, payload_length,
                                          payload_specific, in_order);
}

bool Channel::HandleRtxPacket(const uint8_t* packet,
                              size_t packet_length,
                              const RTPHeader& header) {
  if (!rtp_payload_registry_->IsRtx(header))
    return false;

  // Remove the RTX header and parse the original RTP header.
  if (packet_length < header.headerLength)
    return false;
  if (packet_length > kVoiceEngineMaxIpPacketSizeBytes)
    return false;
  if (restored_packet_in_use_) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVoice, _channelId,
                 "Multiple RTX headers detected, dropping packet");
    return false;
  }
  if (!rtp_payload_registry_->RestoreOriginalPacket(
          restored_packet_, packet, &packet_length, rtp_receiver_->SSRC(),
          header)) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVoice, _channelId,
                 "Incoming RTX packet: invalid RTP header");
    return false;
  }
  restored_packet_in_use_ = true;
  bool ret = OnRecoveredPacket(restored_packet_, packet_length);
  restored_packet_in_use_ = false;
  return ret;
}

bool Channel::IsPacketInOrder(const RTPHeader& header) const {
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(header.ssrc);
  if (!statistician)
    return false;
  return statistician->IsPacketInOrder(header.sequenceNumber);
}

bool Channel::IsPacketRetransmitted(const RTPHeader& header,
                                    bool in_order) const {
  // Retransmissions are handled separately if RTX is enabled.
  if (rtp_payload_registry_->RtxEnabled())
    return false;
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(header.ssrc);
  if (!statistician)
    return false;
  // Check if this is a retransmission.
  int64_t min_rtt = 0;
  _rtpRtcpModule->RTT(rtp_receiver_->SSRC(), NULL, NULL, &min_rtt, NULL);
  return !in_order && statistician->IsRetransmitOfOldPacket(header, min_rtt);
}

int32_t Channel::ReceivedRTCPPacket(const uint8_t* data, size_t length) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::ReceivedRTCPPacket()");
  // Store playout timestamp for the received RTCP packet
  UpdatePlayoutTimestamp(true);

  // Deliver RTCP packet to RTP/RTCP module for parsing
  if (_rtpRtcpModule->IncomingRtcpPacket(data, length) == -1) {
    _engineStatisticsPtr->SetLastError(
        VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceWarning,
        "Channel::IncomingRTPPacket() RTCP packet is invalid");
  }

  int64_t rtt = GetRTT(true);
  if (rtt == 0) {
    // Waiting for valid RTT.
    return 0;
  }

  int64_t nack_window_ms = rtt;
  if (nack_window_ms < kMinRetransmissionWindowMs) {
    nack_window_ms = kMinRetransmissionWindowMs;
  } else if (nack_window_ms > kMaxRetransmissionWindowMs) {
    nack_window_ms = kMaxRetransmissionWindowMs;
  }
  retransmission_rate_limiter_->SetWindowSize(nack_window_ms);

  // Invoke audio encoders OnReceivedRtt().
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder)
      (*encoder)->OnReceivedRtt(rtt);
  });

  uint32_t ntp_secs = 0;
  uint32_t ntp_frac = 0;
  uint32_t rtp_timestamp = 0;
  if (0 !=
      _rtpRtcpModule->RemoteNTP(&ntp_secs, &ntp_frac, NULL, NULL,
                                &rtp_timestamp)) {
    // Waiting for RTCP.
    return 0;
  }

  {
    rtc::CritScope lock(&ts_stats_lock_);
    ntp_estimator_.UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac, rtp_timestamp);
  }
  return 0;
}

int Channel::StartPlayingFileLocally(const char* fileName,
                                     bool loop,
                                     FileFormats format,
                                     int startPosition,
                                     float volumeScaling,
                                     int stopPosition,
                                     const CodecInst* codecInst) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::StartPlayingFileLocally(fileNameUTF8[]=%s, loop=%d,"
               " format=%d, volumeScaling=%5.3f, startPosition=%d, "
               "stopPosition=%d)",
               fileName, loop, format, volumeScaling, startPosition,
               stopPosition);

  if (channel_state_.Get().output_file_playing) {
    _engineStatisticsPtr->SetLastError(
        VE_ALREADY_PLAYING, kTraceError,
        "StartPlayingFileLocally() is already playing");
    return -1;
  }

  {
    rtc::CritScope cs(&_fileCritSect);

    if (output_file_player_) {
      output_file_player_->RegisterModuleFileCallback(NULL);
      output_file_player_.reset();
    }

    output_file_player_ = FilePlayer::CreateFilePlayer(
        _outputFilePlayerId, (const FileFormats)format);

    if (!output_file_player_) {
      _engineStatisticsPtr->SetLastError(
          VE_INVALID_ARGUMENT, kTraceError,
          "StartPlayingFileLocally() filePlayer format is not correct");
      return -1;
    }

    const uint32_t notificationTime(0);

    if (output_file_player_->StartPlayingFile(
            fileName, loop, startPosition, volumeScaling, notificationTime,
            stopPosition, (const CodecInst*)codecInst) != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_BAD_FILE, kTraceError,
          "StartPlayingFile() failed to start file playout");
      output_file_player_->StopPlayingFile();
      output_file_player_.reset();
      return -1;
    }
    output_file_player_->RegisterModuleFileCallback(this);
    channel_state_.SetOutputFilePlaying(true);
  }

  if (RegisterFilePlayingToMixer() != 0)
    return -1;

  return 0;
}

int Channel::StartPlayingFileLocally(InStream* stream,
                                     FileFormats format,
                                     int startPosition,
                                     float volumeScaling,
                                     int stopPosition,
                                     const CodecInst* codecInst) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::StartPlayingFileLocally(format=%d,"
               " volumeScaling=%5.3f, startPosition=%d, stopPosition=%d)",
               format, volumeScaling, startPosition, stopPosition);

  if (stream == NULL) {
    _engineStatisticsPtr->SetLastError(
        VE_BAD_FILE, kTraceError,
        "StartPlayingFileLocally() NULL as input stream");
    return -1;
  }

  if (channel_state_.Get().output_file_playing) {
    _engineStatisticsPtr->SetLastError(
        VE_ALREADY_PLAYING, kTraceError,
        "StartPlayingFileLocally() is already playing");
    return -1;
  }

  {
    rtc::CritScope cs(&_fileCritSect);

    // Destroy the old instance
    if (output_file_player_) {
      output_file_player_->RegisterModuleFileCallback(NULL);
      output_file_player_.reset();
    }

    // Create the instance
    output_file_player_ = FilePlayer::CreateFilePlayer(
        _outputFilePlayerId, (const FileFormats)format);

    if (!output_file_player_) {
      _engineStatisticsPtr->SetLastError(
          VE_INVALID_ARGUMENT, kTraceError,
          "StartPlayingFileLocally() filePlayer format isnot correct");
      return -1;
    }

    const uint32_t notificationTime(0);

    if (output_file_player_->StartPlayingFile(stream, startPosition,
                                              volumeScaling, notificationTime,
                                              stopPosition, codecInst) != 0) {
      _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
                                         "StartPlayingFile() failed to "
                                         "start file playout");
      output_file_player_->StopPlayingFile();
      output_file_player_.reset();
      return -1;
    }
    output_file_player_->RegisterModuleFileCallback(this);
    channel_state_.SetOutputFilePlaying(true);
  }

  if (RegisterFilePlayingToMixer() != 0)
    return -1;

  return 0;
}

int Channel::StopPlayingFileLocally() {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::StopPlayingFileLocally()");

  if (!channel_state_.Get().output_file_playing) {
    return 0;
  }

  {
    rtc::CritScope cs(&_fileCritSect);

    if (output_file_player_->StopPlayingFile() != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_STOP_RECORDING_FAILED, kTraceError,
          "StopPlayingFile() could not stop playing");
      return -1;
    }
    output_file_player_->RegisterModuleFileCallback(NULL);
    output_file_player_.reset();
    channel_state_.SetOutputFilePlaying(false);
  }
  // _fileCritSect cannot be taken while calling
  // SetAnonymousMixibilityStatus. Refer to comments in
  // StartPlayingFileLocally(const char* ...) for more details.
  if (_outputMixerPtr->SetAnonymousMixabilityStatus(*this, false) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CONF_MIX_MODULE_ERROR, kTraceError,
        "StopPlayingFile() failed to stop participant from playing as"
        "file in the mixer");
    return -1;
  }

  return 0;
}

int Channel::IsPlayingFileLocally() const {
  return channel_state_.Get().output_file_playing;
}

int Channel::RegisterFilePlayingToMixer() {
  // Return success for not registering for file playing to mixer if:
  // 1. playing file before playout is started on that channel.
  // 2. starting playout without file playing on that channel.
  if (!channel_state_.Get().playing ||
      !channel_state_.Get().output_file_playing) {
    return 0;
  }

  // |_fileCritSect| cannot be taken while calling
  // SetAnonymousMixabilityStatus() since as soon as the participant is added
  // frames can be pulled by the mixer. Since the frames are generated from
  // the file, _fileCritSect will be taken. This would result in a deadlock.
  if (_outputMixerPtr->SetAnonymousMixabilityStatus(*this, true) != 0) {
    channel_state_.SetOutputFilePlaying(false);
    rtc::CritScope cs(&_fileCritSect);
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CONF_MIX_MODULE_ERROR, kTraceError,
        "StartPlayingFile() failed to add participant as file to mixer");
    output_file_player_->StopPlayingFile();
    output_file_player_.reset();
    return -1;
  }

  return 0;
}

int Channel::StartPlayingFileAsMicrophone(const char* fileName,
                                          bool loop,
                                          FileFormats format,
                                          int startPosition,
                                          float volumeScaling,
                                          int stopPosition,
                                          const CodecInst* codecInst) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::StartPlayingFileAsMicrophone(fileNameUTF8[]=%s, "
               "loop=%d, format=%d, volumeScaling=%5.3f, startPosition=%d, "
               "stopPosition=%d)",
               fileName, loop, format, volumeScaling, startPosition,
               stopPosition);

  rtc::CritScope cs(&_fileCritSect);

  if (channel_state_.Get().input_file_playing) {
    _engineStatisticsPtr->SetLastError(
        VE_ALREADY_PLAYING, kTraceWarning,
        "StartPlayingFileAsMicrophone() filePlayer is playing");
    return 0;
  }

  // Destroy the old instance
  if (input_file_player_) {
    input_file_player_->RegisterModuleFileCallback(NULL);
    input_file_player_.reset();
  }

  // Create the instance
  input_file_player_ = FilePlayer::CreateFilePlayer(_inputFilePlayerId,
                                                    (const FileFormats)format);

  if (!input_file_player_) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_ARGUMENT, kTraceError,
        "StartPlayingFileAsMicrophone() filePlayer format isnot correct");
    return -1;
  }

  const uint32_t notificationTime(0);

  if (input_file_player_->StartPlayingFile(
          fileName, loop, startPosition, volumeScaling, notificationTime,
          stopPosition, (const CodecInst*)codecInst) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_BAD_FILE, kTraceError,
        "StartPlayingFile() failed to start file playout");
    input_file_player_->StopPlayingFile();
    input_file_player_.reset();
    return -1;
  }
  input_file_player_->RegisterModuleFileCallback(this);
  channel_state_.SetInputFilePlaying(true);

  return 0;
}

int Channel::StartPlayingFileAsMicrophone(InStream* stream,
                                          FileFormats format,
                                          int startPosition,
                                          float volumeScaling,
                                          int stopPosition,
                                          const CodecInst* codecInst) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::StartPlayingFileAsMicrophone(format=%d, "
               "volumeScaling=%5.3f, startPosition=%d, stopPosition=%d)",
               format, volumeScaling, startPosition, stopPosition);

  if (stream == NULL) {
    _engineStatisticsPtr->SetLastError(
        VE_BAD_FILE, kTraceError,
        "StartPlayingFileAsMicrophone NULL as input stream");
    return -1;
  }

  rtc::CritScope cs(&_fileCritSect);

  if (channel_state_.Get().input_file_playing) {
    _engineStatisticsPtr->SetLastError(
        VE_ALREADY_PLAYING, kTraceWarning,
        "StartPlayingFileAsMicrophone() is playing");
    return 0;
  }

  // Destroy the old instance
  if (input_file_player_) {
    input_file_player_->RegisterModuleFileCallback(NULL);
    input_file_player_.reset();
  }

  // Create the instance
  input_file_player_ = FilePlayer::CreateFilePlayer(_inputFilePlayerId,
                                                    (const FileFormats)format);

  if (!input_file_player_) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_ARGUMENT, kTraceError,
        "StartPlayingInputFile() filePlayer format isnot correct");
    return -1;
  }

  const uint32_t notificationTime(0);

  if (input_file_player_->StartPlayingFile(stream, startPosition, volumeScaling,
                                           notificationTime, stopPosition,
                                           codecInst) != 0) {
    _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
                                       "StartPlayingFile() failed to start "
                                       "file playout");
    input_file_player_->StopPlayingFile();
    input_file_player_.reset();
    return -1;
  }

  input_file_player_->RegisterModuleFileCallback(this);
  channel_state_.SetInputFilePlaying(true);

  return 0;
}

int Channel::StopPlayingFileAsMicrophone() {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::StopPlayingFileAsMicrophone()");

  rtc::CritScope cs(&_fileCritSect);

  if (!channel_state_.Get().input_file_playing) {
    return 0;
  }

  if (input_file_player_->StopPlayingFile() != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_STOP_RECORDING_FAILED, kTraceError,
        "StopPlayingFile() could not stop playing");
    return -1;
  }
  input_file_player_->RegisterModuleFileCallback(NULL);
  input_file_player_.reset();
  channel_state_.SetInputFilePlaying(false);

  return 0;
}

int Channel::IsPlayingFileAsMicrophone() const {
  return channel_state_.Get().input_file_playing;
}

int Channel::StartRecordingPlayout(const char* fileName,
                                   const CodecInst* codecInst) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::StartRecordingPlayout(fileName=%s)", fileName);

  if (_outputFileRecording) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                 "StartRecordingPlayout() is already recording");
    return 0;
  }

  FileFormats format;
  const uint32_t notificationTime(0);  // Not supported in VoE
  CodecInst dummyCodec = {100, "L16", 16000, 320, 1, 320000};

  if ((codecInst != NULL) &&
      ((codecInst->channels < 1) || (codecInst->channels > 2))) {
    _engineStatisticsPtr->SetLastError(
        VE_BAD_ARGUMENT, kTraceError,
        "StartRecordingPlayout() invalid compression");
    return (-1);
  }
  if (codecInst == NULL) {
    format = kFileFormatPcm16kHzFile;
    codecInst = &dummyCodec;
  } else if ((STR_CASE_CMP(codecInst->plname, "L16") == 0) ||
             (STR_CASE_CMP(codecInst->plname, "PCMU") == 0) ||
             (STR_CASE_CMP(codecInst->plname, "PCMA") == 0)) {
    format = kFileFormatWavFile;
  } else {
    format = kFileFormatCompressedFile;
  }

  rtc::CritScope cs(&_fileCritSect);

  // Destroy the old instance
  if (output_file_recorder_) {
    output_file_recorder_->RegisterModuleFileCallback(NULL);
    output_file_recorder_.reset();
  }

  output_file_recorder_ = FileRecorder::CreateFileRecorder(
      _outputFileRecorderId, (const FileFormats)format);
  if (!output_file_recorder_) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_ARGUMENT, kTraceError,
        "StartRecordingPlayout() fileRecorder format isnot correct");
    return -1;
  }

  if (output_file_recorder_->StartRecordingAudioFile(
          fileName, (const CodecInst&)*codecInst, notificationTime) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_BAD_FILE, kTraceError,
        "StartRecordingAudioFile() failed to start file recording");
    output_file_recorder_->StopRecording();
    output_file_recorder_.reset();
    return -1;
  }
  output_file_recorder_->RegisterModuleFileCallback(this);
  _outputFileRecording = true;

  return 0;
}

int Channel::StartRecordingPlayout(OutStream* stream,
                                   const CodecInst* codecInst) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::StartRecordingPlayout()");

  if (_outputFileRecording) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                 "StartRecordingPlayout() is already recording");
    return 0;
  }

  FileFormats format;
  const uint32_t notificationTime(0);  // Not supported in VoE
  CodecInst dummyCodec = {100, "L16", 16000, 320, 1, 320000};

  if (codecInst != NULL && codecInst->channels != 1) {
    _engineStatisticsPtr->SetLastError(
        VE_BAD_ARGUMENT, kTraceError,
        "StartRecordingPlayout() invalid compression");
    return (-1);
  }
  if (codecInst == NULL) {
    format = kFileFormatPcm16kHzFile;
    codecInst = &dummyCodec;
  } else if ((STR_CASE_CMP(codecInst->plname, "L16") == 0) ||
             (STR_CASE_CMP(codecInst->plname, "PCMU") == 0) ||
             (STR_CASE_CMP(codecInst->plname, "PCMA") == 0)) {
    format = kFileFormatWavFile;
  } else {
    format = kFileFormatCompressedFile;
  }

  rtc::CritScope cs(&_fileCritSect);

  // Destroy the old instance
  if (output_file_recorder_) {
    output_file_recorder_->RegisterModuleFileCallback(NULL);
    output_file_recorder_.reset();
  }

  output_file_recorder_ = FileRecorder::CreateFileRecorder(
      _outputFileRecorderId, (const FileFormats)format);
  if (!output_file_recorder_) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_ARGUMENT, kTraceError,
        "StartRecordingPlayout() fileRecorder format isnot correct");
    return -1;
  }

  if (output_file_recorder_->StartRecordingAudioFile(stream, *codecInst,
                                                     notificationTime) != 0) {
    _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
                                       "StartRecordingPlayout() failed to "
                                       "start file recording");
    output_file_recorder_->StopRecording();
    output_file_recorder_.reset();
    return -1;
  }

  output_file_recorder_->RegisterModuleFileCallback(this);
  _outputFileRecording = true;

  return 0;
}

int Channel::StopRecordingPlayout() {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
               "Channel::StopRecordingPlayout()");

  if (!_outputFileRecording) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, -1),
                 "StopRecordingPlayout() isnot recording");
    return -1;
  }

  rtc::CritScope cs(&_fileCritSect);

  if (output_file_recorder_->StopRecording() != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_STOP_RECORDING_FAILED, kTraceError,
        "StopRecording() could not stop recording");
    return (-1);
  }
  output_file_recorder_->RegisterModuleFileCallback(NULL);
  output_file_recorder_.reset();
  _outputFileRecording = false;

  return 0;
}

void Channel::SetMixWithMicStatus(bool mix) {
  rtc::CritScope cs(&_fileCritSect);
  _mixFileWithMicrophone = mix;
}

int Channel::GetSpeechOutputLevel(uint32_t& level) const {
  int8_t currentLevel = _outputAudioLevel.Level();
  level = static_cast<int32_t>(currentLevel);
  return 0;
}

int Channel::GetSpeechOutputLevelFullRange(uint32_t& level) const {
  int16_t currentLevel = _outputAudioLevel.LevelFullRange();
  level = static_cast<int32_t>(currentLevel);
  return 0;
}

int Channel::SetInputMute(bool enable) {
  rtc::CritScope cs(&volume_settings_critsect_);
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetMute(enable=%d)", enable);
  input_mute_ = enable;
  return 0;
}

bool Channel::InputMute() const {
  rtc::CritScope cs(&volume_settings_critsect_);
  return input_mute_;
}

int Channel::SetOutputVolumePan(float left, float right) {
  rtc::CritScope cs(&volume_settings_critsect_);
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetOutputVolumePan()");
  _panLeft = left;
  _panRight = right;
  return 0;
}

int Channel::GetOutputVolumePan(float& left, float& right) const {
  rtc::CritScope cs(&volume_settings_critsect_);
  left = _panLeft;
  right = _panRight;
  return 0;
}

int Channel::SetChannelOutputVolumeScaling(float scaling) {
  rtc::CritScope cs(&volume_settings_critsect_);
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetChannelOutputVolumeScaling()");
  _outputGain = scaling;
  return 0;
}

int Channel::GetChannelOutputVolumeScaling(float& scaling) const {
  rtc::CritScope cs(&volume_settings_critsect_);
  scaling = _outputGain;
  return 0;
}

int Channel::SendTelephoneEventOutband(int event, int duration_ms) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SendTelephoneEventOutband(...)");
  RTC_DCHECK_LE(0, event);
  RTC_DCHECK_GE(255, event);
  RTC_DCHECK_LE(0, duration_ms);
  RTC_DCHECK_GE(65535, duration_ms);
  if (!Sending()) {
    return -1;
  }
  if (_rtpRtcpModule->SendTelephoneEventOutband(
      event, duration_ms, kTelephoneEventAttenuationdB) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_SEND_DTMF_FAILED, kTraceWarning,
        "SendTelephoneEventOutband() failed to send event");
    return -1;
  }
  return 0;
}

int Channel::SetSendTelephoneEventPayloadType(int payload_type,
                                              int payload_frequency) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetSendTelephoneEventPayloadType()");
  RTC_DCHECK_LE(0, payload_type);
  RTC_DCHECK_GE(127, payload_type);
  CodecInst codec = {0};
  codec.pltype = payload_type;
  codec.plfreq = payload_frequency;
  memcpy(codec.plname, "telephone-event", 16);
  if (_rtpRtcpModule->RegisterSendPayload(codec) != 0) {
    _rtpRtcpModule->DeRegisterSendPayload(codec.pltype);
    if (_rtpRtcpModule->RegisterSendPayload(codec) != 0) {
      _engineStatisticsPtr->SetLastError(
          VE_RTP_RTCP_MODULE_ERROR, kTraceError,
          "SetSendTelephoneEventPayloadType() failed to register send"
          "payload type");
      return -1;
    }
  }
  return 0;
}

int Channel::VoiceActivityIndicator(int& activity) {
  activity = _sendFrameType;
  return 0;
}

int Channel::SetLocalSSRC(unsigned int ssrc) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetLocalSSRC()");
  if (channel_state_.Get().sending) {
    _engineStatisticsPtr->SetLastError(VE_ALREADY_SENDING, kTraceError,
                                       "SetLocalSSRC() already sending");
    return -1;
  }
  _rtpRtcpModule->SetSSRC(ssrc);
  return 0;
}

int Channel::GetLocalSSRC(unsigned int& ssrc) {
  ssrc = _rtpRtcpModule->SSRC();
  return 0;
}

int Channel::GetRemoteSSRC(unsigned int& ssrc) {
  ssrc = rtp_receiver_->SSRC();
  return 0;
}

int Channel::SetSendAudioLevelIndicationStatus(bool enable, unsigned char id) {
  _includeAudioLevelIndication = enable;
  return SetSendRtpHeaderExtension(enable, kRtpExtensionAudioLevel, id);
}

int Channel::SetReceiveAudioLevelIndicationStatus(bool enable,
                                                  unsigned char id) {
  rtp_header_parser_->DeregisterRtpHeaderExtension(kRtpExtensionAudioLevel);
  if (enable &&
      !rtp_header_parser_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                      id)) {
    return -1;
  }
  return 0;
}

void Channel::EnableSendTransportSequenceNumber(int id) {
  int ret =
      SetSendRtpHeaderExtension(true, kRtpExtensionTransportSequenceNumber, id);
  RTC_DCHECK_EQ(0, ret);
}

void Channel::EnableReceiveTransportSequenceNumber(int id) {
  rtp_header_parser_->DeregisterRtpHeaderExtension(
      kRtpExtensionTransportSequenceNumber);
  bool ret = rtp_header_parser_->RegisterRtpHeaderExtension(
      kRtpExtensionTransportSequenceNumber, id);
  RTC_DCHECK(ret);
}

void Channel::RegisterSenderCongestionControlObjects(
    RtpPacketSender* rtp_packet_sender,
    TransportFeedbackObserver* transport_feedback_observer,
    PacketRouter* packet_router,
    RtcpBandwidthObserver* bandwidth_observer) {
  RTC_DCHECK(rtp_packet_sender);
  RTC_DCHECK(transport_feedback_observer);
  RTC_DCHECK(packet_router && !packet_router_);
  rtcp_observer_->SetBandwidthObserver(bandwidth_observer);
  feedback_observer_proxy_->SetTransportFeedbackObserver(
      transport_feedback_observer);
  seq_num_allocator_proxy_->SetSequenceNumberAllocator(packet_router);
  rtp_packet_sender_proxy_->SetPacketSender(rtp_packet_sender);
  _rtpRtcpModule->SetStorePacketsStatus(true, 600);
  packet_router->AddRtpModule(_rtpRtcpModule.get());
  packet_router_ = packet_router;
}

void Channel::RegisterReceiverCongestionControlObjects(
    PacketRouter* packet_router) {
  RTC_DCHECK(packet_router && !packet_router_);
  packet_router->AddRtpModule(_rtpRtcpModule.get());
  packet_router_ = packet_router;
}

void Channel::ResetCongestionControlObjects() {
  RTC_DCHECK(packet_router_);
  _rtpRtcpModule->SetStorePacketsStatus(false, 600);
  rtcp_observer_->SetBandwidthObserver(nullptr);
  feedback_observer_proxy_->SetTransportFeedbackObserver(nullptr);
  seq_num_allocator_proxy_->SetSequenceNumberAllocator(nullptr);
  packet_router_->RemoveRtpModule(_rtpRtcpModule.get());
  packet_router_ = nullptr;
  rtp_packet_sender_proxy_->SetPacketSender(nullptr);
}

void Channel::SetRTCPStatus(bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetRTCPStatus()");
  _rtpRtcpModule->SetRTCPStatus(enable ? RtcpMode::kCompound : RtcpMode::kOff);
}

int Channel::GetRTCPStatus(bool& enabled) {
  RtcpMode method = _rtpRtcpModule->RTCP();
  enabled = (method != RtcpMode::kOff);
  return 0;
}

int Channel::SetRTCP_CNAME(const char cName[256]) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetRTCP_CNAME()");
  if (_rtpRtcpModule->SetCNAME(cName) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_RTP_RTCP_MODULE_ERROR, kTraceError,
        "SetRTCP_CNAME() failed to set RTCP CNAME");
    return -1;
  }
  return 0;
}

int Channel::GetRemoteRTCP_CNAME(char cName[256]) {
  if (cName == NULL) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_ARGUMENT, kTraceError,
        "GetRemoteRTCP_CNAME() invalid CNAME input buffer");
    return -1;
  }
  char cname[RTCP_CNAME_SIZE];
  const uint32_t remoteSSRC = rtp_receiver_->SSRC();
  if (_rtpRtcpModule->RemoteCNAME(remoteSSRC, cname) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_CANNOT_RETRIEVE_CNAME, kTraceError,
        "GetRemoteRTCP_CNAME() failed to retrieve remote RTCP CNAME");
    return -1;
  }
  strcpy(cName, cname);
  return 0;
}

int Channel::SendApplicationDefinedRTCPPacket(
    unsigned char subType,
    unsigned int name,
    const char* data,
    unsigned short dataLengthInBytes) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SendApplicationDefinedRTCPPacket()");
  if (!channel_state_.Get().sending) {
    _engineStatisticsPtr->SetLastError(
        VE_NOT_SENDING, kTraceError,
        "SendApplicationDefinedRTCPPacket() not sending");
    return -1;
  }
  if (NULL == data) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_ARGUMENT, kTraceError,
        "SendApplicationDefinedRTCPPacket() invalid data value");
    return -1;
  }
  if (dataLengthInBytes % 4 != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_ARGUMENT, kTraceError,
        "SendApplicationDefinedRTCPPacket() invalid length value");
    return -1;
  }
  RtcpMode status = _rtpRtcpModule->RTCP();
  if (status == RtcpMode::kOff) {
    _engineStatisticsPtr->SetLastError(
        VE_RTCP_ERROR, kTraceError,
        "SendApplicationDefinedRTCPPacket() RTCP is disabled");
    return -1;
  }

  // Create and schedule the RTCP APP packet for transmission
  if (_rtpRtcpModule->SetRTCPApplicationSpecificData(
          subType, name, (const unsigned char*)data, dataLengthInBytes) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_SEND_ERROR, kTraceError,
        "SendApplicationDefinedRTCPPacket() failed to send RTCP packet");
    return -1;
  }
  return 0;
}

int Channel::GetRemoteRTCPReportBlocks(
    std::vector<ReportBlock>* report_blocks) {
  if (report_blocks == NULL) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_ARGUMENT, kTraceError,
        "GetRemoteRTCPReportBlock()s invalid report_blocks.");
    return -1;
  }

  // Get the report blocks from the latest received RTCP Sender or Receiver
  // Report. Each element in the vector contains the sender's SSRC and a
  // report block according to RFC 3550.
  std::vector<RTCPReportBlock> rtcp_report_blocks;
  if (_rtpRtcpModule->RemoteRTCPStat(&rtcp_report_blocks) != 0) {
    return -1;
  }

  if (rtcp_report_blocks.empty())
    return 0;

  std::vector<RTCPReportBlock>::const_iterator it = rtcp_report_blocks.begin();
  for (; it != rtcp_report_blocks.end(); ++it) {
    ReportBlock report_block;
    report_block.sender_SSRC = it->remoteSSRC;
    report_block.source_SSRC = it->sourceSSRC;
    report_block.fraction_lost = it->fractionLost;
    report_block.cumulative_num_packets_lost = it->cumulativeLost;
    report_block.extended_highest_sequence_number = it->extendedHighSeqNum;
    report_block.interarrival_jitter = it->jitter;
    report_block.last_SR_timestamp = it->lastSR;
    report_block.delay_since_last_SR = it->delaySinceLastSR;
    report_blocks->push_back(report_block);
  }
  return 0;
}

int Channel::GetRTPStatistics(CallStatistics& stats) {
  // --- RtcpStatistics

  // The jitter statistics is updated for each received RTP packet and is
  // based on received packets.
  RtcpStatistics statistics;
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(rtp_receiver_->SSRC());
  if (statistician) {
    statistician->GetStatistics(&statistics,
                                _rtpRtcpModule->RTCP() == RtcpMode::kOff);
  }

  stats.fractionLost = statistics.fraction_lost;
  stats.cumulativeLost = statistics.cumulative_lost;
  stats.extendedMax = statistics.extended_max_sequence_number;
  stats.jitterSamples = statistics.jitter;

  // --- RTT
  stats.rttMs = GetRTT(true);

  // --- Data counters

  size_t bytesSent(0);
  uint32_t packetsSent(0);
  size_t bytesReceived(0);
  uint32_t packetsReceived(0);

  if (statistician) {
    statistician->GetDataCounters(&bytesReceived, &packetsReceived);
  }

  if (_rtpRtcpModule->DataCountersRTP(&bytesSent, &packetsSent) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                 "GetRTPStatistics() failed to retrieve RTP datacounters =>"
                 " output will not be complete");
  }

  stats.bytesSent = bytesSent;
  stats.packetsSent = packetsSent;
  stats.bytesReceived = bytesReceived;
  stats.packetsReceived = packetsReceived;

  // --- Timestamps
  {
    rtc::CritScope lock(&ts_stats_lock_);
    stats.capture_start_ntp_time_ms_ = capture_start_ntp_time_ms_;
  }
  return 0;
}

int Channel::SetCodecFECStatus(bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetCodecFECStatus()");

  if (!codec_manager_.SetCodecFEC(enable) ||
      !codec_manager_.MakeEncoder(&rent_a_codec_, audio_coding_.get())) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetCodecFECStatus() failed to set FEC state");
    return -1;
  }
  return 0;
}

bool Channel::GetCodecFECStatus() {
  return codec_manager_.GetStackParams()->use_codec_fec;
}

void Channel::SetNACKStatus(bool enable, int maxNumberOfPackets) {
  // None of these functions can fail.
  // If pacing is enabled we always store packets.
  if (!pacing_enabled_)
    _rtpRtcpModule->SetStorePacketsStatus(enable, maxNumberOfPackets);
  rtp_receive_statistics_->SetMaxReorderingThreshold(maxNumberOfPackets);
  if (enable)
    audio_coding_->EnableNack(maxNumberOfPackets);
  else
    audio_coding_->DisableNack();
}

// Called when we are missing one or more packets.
int Channel::ResendPackets(const uint16_t* sequence_numbers, int length) {
  return _rtpRtcpModule->SendNACK(sequence_numbers, length);
}

uint32_t Channel::Demultiplex(const AudioFrame& audioFrame) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::Demultiplex()");
  _audioFrame.CopyFrom(audioFrame);
  _audioFrame.id_ = _channelId;
  return 0;
}

void Channel::Demultiplex(const int16_t* audio_data,
                          int sample_rate,
                          size_t number_of_frames,
                          size_t number_of_channels) {
  CodecInst codec;
  GetSendCodec(codec);

  // Never upsample or upmix the capture signal here. This should be done at the
  // end of the send chain.
  _audioFrame.sample_rate_hz_ = std::min(codec.plfreq, sample_rate);
  _audioFrame.num_channels_ = std::min(number_of_channels, codec.channels);
  RemixAndResample(audio_data, number_of_frames, number_of_channels,
                   sample_rate, &input_resampler_, &_audioFrame);
}

uint32_t Channel::PrepareEncodeAndSend(int mixingFrequency) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::PrepareEncodeAndSend()");

  if (_audioFrame.samples_per_channel_ == 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::PrepareEncodeAndSend() invalid audio frame");
    return 0xFFFFFFFF;
  }

  if (channel_state_.Get().input_file_playing) {
    MixOrReplaceAudioWithFile(mixingFrequency);
  }

  bool is_muted = InputMute();  // Cache locally as InputMute() takes a lock.
  AudioFrameOperations::Mute(&_audioFrame, previous_frame_muted_, is_muted);

  if (_includeAudioLevelIndication) {
    size_t length =
        _audioFrame.samples_per_channel_ * _audioFrame.num_channels_;
    RTC_CHECK_LE(length, sizeof(_audioFrame.data_));
    if (is_muted && previous_frame_muted_) {
      rms_level_.AnalyzeMuted(length);
    } else {
      rms_level_.Analyze(
          rtc::ArrayView<const int16_t>(_audioFrame.data_, length));
    }
  }
  previous_frame_muted_ = is_muted;

  return 0;
}

uint32_t Channel::EncodeAndSend() {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::EncodeAndSend()");

  assert(_audioFrame.num_channels_ <= 2);
  if (_audioFrame.samples_per_channel_ == 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::EncodeAndSend() invalid audio frame");
    return 0xFFFFFFFF;
  }

  _audioFrame.id_ = _channelId;

  // --- Add 10ms of raw (PCM) audio data to the encoder @ 32kHz.

  // The ACM resamples internally.
  _audioFrame.timestamp_ = _timeStamp;
  // This call will trigger AudioPacketizationCallback::SendData if encoding
  // is done and payload is ready for packetization and transmission.
  // Otherwise, it will return without invoking the callback.
  if (audio_coding_->Add10MsData((AudioFrame&)_audioFrame) < 0) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::EncodeAndSend() ACM encoding failed");
    return 0xFFFFFFFF;
  }

  _timeStamp += static_cast<uint32_t>(_audioFrame.samples_per_channel_);
  return 0;
}

void Channel::set_associate_send_channel(const ChannelOwner& channel) {
  RTC_DCHECK(!channel.channel() ||
             channel.channel()->ChannelId() != _channelId);
  rtc::CritScope lock(&assoc_send_channel_lock_);
  associate_send_channel_ = channel;
}

void Channel::DisassociateSendChannel(int channel_id) {
  rtc::CritScope lock(&assoc_send_channel_lock_);
  Channel* channel = associate_send_channel_.channel();
  if (channel && channel->ChannelId() == channel_id) {
    // If this channel is associated with a send channel of the specified
    // Channel ID, disassociate with it.
    ChannelOwner ref(NULL);
    associate_send_channel_ = ref;
  }
}

void Channel::SetRtcEventLog(RtcEventLog* event_log) {
  event_log_proxy_->SetEventLog(event_log);
}

void Channel::SetRtcpRttStats(RtcpRttStats* rtcp_rtt_stats) {
  rtcp_rtt_stats_proxy_->SetRtcpRttStats(rtcp_rtt_stats);
}

void Channel::UpdateOverheadForEncoder() {
  size_t overhead_per_packet =
      transport_overhead_per_packet_ + rtp_overhead_per_packet_;
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->OnReceivedOverhead(overhead_per_packet);
    }
  });
}

void Channel::SetTransportOverhead(size_t transport_overhead_per_packet) {
  rtc::CritScope cs(&overhead_per_packet_lock_);
  transport_overhead_per_packet_ = transport_overhead_per_packet;
  UpdateOverheadForEncoder();
}

// TODO(solenberg): Make AudioSendStream an OverheadObserver instead.
void Channel::OnOverheadChanged(size_t overhead_bytes_per_packet) {
  rtc::CritScope cs(&overhead_per_packet_lock_);
  rtp_overhead_per_packet_ = overhead_bytes_per_packet;
  UpdateOverheadForEncoder();
}

int Channel::GetNetworkStatistics(NetworkStatistics& stats) {
  return audio_coding_->GetNetworkStatistics(&stats);
}

void Channel::GetDecodingCallStatistics(AudioDecodingCallStats* stats) const {
  audio_coding_->GetDecodingCallStatistics(stats);
}

uint32_t Channel::GetDelayEstimate() const {
  rtc::CritScope lock(&video_sync_lock_);
  return audio_coding_->FilteredCurrentDelayMs() + playout_delay_ms_;
}

int Channel::SetMinimumPlayoutDelay(int delayMs) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetMinimumPlayoutDelay()");
  if ((delayMs < kVoiceEngineMinMinPlayoutDelayMs) ||
      (delayMs > kVoiceEngineMaxMinPlayoutDelayMs)) {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_ARGUMENT, kTraceError,
        "SetMinimumPlayoutDelay() invalid min delay");
    return -1;
  }
  if (audio_coding_->SetMinimumPlayoutDelay(delayMs) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetMinimumPlayoutDelay() failed to set min playout delay");
    return -1;
  }
  return 0;
}

int Channel::GetPlayoutTimestamp(unsigned int& timestamp) {
  uint32_t playout_timestamp_rtp = 0;
  {
    rtc::CritScope lock(&video_sync_lock_);
    playout_timestamp_rtp = playout_timestamp_rtp_;
  }
  if (playout_timestamp_rtp == 0) {
    _engineStatisticsPtr->SetLastError(
        VE_CANNOT_RETRIEVE_VALUE, kTraceStateInfo,
        "GetPlayoutTimestamp() failed to retrieve timestamp");
    return -1;
  }
  timestamp = playout_timestamp_rtp;
  return 0;
}

int Channel::GetRtpRtcp(RtpRtcp** rtpRtcpModule,
                        RtpReceiver** rtp_receiver) const {
  *rtpRtcpModule = _rtpRtcpModule.get();
  *rtp_receiver = rtp_receiver_.get();
  return 0;
}

// TODO(andrew): refactor Mix functions here and in transmit_mixer.cc to use
// a shared helper.
int32_t Channel::MixOrReplaceAudioWithFile(int mixingFrequency) {
  std::unique_ptr<int16_t[]> fileBuffer(new int16_t[640]);
  size_t fileSamples(0);

  {
    rtc::CritScope cs(&_fileCritSect);

    if (!input_file_player_) {
      WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                   "Channel::MixOrReplaceAudioWithFile() fileplayer"
                   " doesnt exist");
      return -1;
    }

    if (input_file_player_->Get10msAudioFromFile(fileBuffer.get(), &fileSamples,
                                                 mixingFrequency) == -1) {
      WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                   "Channel::MixOrReplaceAudioWithFile() file mixing "
                   "failed");
      return -1;
    }
    if (fileSamples == 0) {
      WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                   "Channel::MixOrReplaceAudioWithFile() file is ended");
      return 0;
    }
  }

  assert(_audioFrame.samples_per_channel_ == fileSamples);

  if (_mixFileWithMicrophone) {
    // Currently file stream is always mono.
    // TODO(xians): Change the code when FilePlayer supports real stereo.
    MixWithSat(_audioFrame.data_, _audioFrame.num_channels_, fileBuffer.get(),
               1, fileSamples);
  } else {
    // Replace ACM audio with file.
    // Currently file stream is always mono.
    // TODO(xians): Change the code when FilePlayer supports real stereo.
    _audioFrame.UpdateFrame(
        _channelId, 0xFFFFFFFF, fileBuffer.get(), fileSamples, mixingFrequency,
        AudioFrame::kNormalSpeech, AudioFrame::kVadUnknown, 1);
  }
  return 0;
}

int32_t Channel::MixAudioWithFile(AudioFrame& audioFrame, int mixingFrequency) {
  assert(mixingFrequency <= 48000);

  std::unique_ptr<int16_t[]> fileBuffer(new int16_t[960]);
  size_t fileSamples(0);

  {
    rtc::CritScope cs(&_fileCritSect);

    if (!output_file_player_) {
      WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                   "Channel::MixAudioWithFile() file mixing failed");
      return -1;
    }

    // We should get the frequency we ask for.
    if (output_file_player_->Get10msAudioFromFile(
            fileBuffer.get(), &fileSamples, mixingFrequency) == -1) {
      WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                   "Channel::MixAudioWithFile() file mixing failed");
      return -1;
    }
  }

  if (audioFrame.samples_per_channel_ == fileSamples) {
    // Currently file stream is always mono.
    // TODO(xians): Change the code when FilePlayer supports real stereo.
    MixWithSat(audioFrame.data_, audioFrame.num_channels_, fileBuffer.get(), 1,
               fileSamples);
  } else {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::MixAudioWithFile() samples_per_channel_(%" PRIuS
                 ") != "
                 "fileSamples(%" PRIuS ")",
                 audioFrame.samples_per_channel_, fileSamples);
    return -1;
  }

  return 0;
}

void Channel::UpdatePlayoutTimestamp(bool rtcp) {
  jitter_buffer_playout_timestamp_ = audio_coding_->PlayoutTimestamp();

  if (!jitter_buffer_playout_timestamp_) {
    // This can happen if this channel has not received any RTP packets. In
    // this case, NetEq is not capable of computing a playout timestamp.
    return;
  }

  uint16_t delay_ms = 0;
  if (_audioDeviceModulePtr->PlayoutDelay(&delay_ms) == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::UpdatePlayoutTimestamp() failed to read playout"
                 " delay from the ADM");
    _engineStatisticsPtr->SetLastError(
        VE_CANNOT_RETRIEVE_VALUE, kTraceError,
        "UpdatePlayoutTimestamp() failed to retrieve playout delay");
    return;
  }

  RTC_DCHECK(jitter_buffer_playout_timestamp_);
  uint32_t playout_timestamp = *jitter_buffer_playout_timestamp_;

  // Remove the playout delay.
  playout_timestamp -= (delay_ms * (GetRtpTimestampRateHz() / 1000));

  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::UpdatePlayoutTimestamp() => playoutTimestamp = %lu",
               playout_timestamp);

  {
    rtc::CritScope lock(&video_sync_lock_);
    if (!rtcp) {
      playout_timestamp_rtp_ = playout_timestamp;
    }
    playout_delay_ms_ = delay_ms;
  }
}

void Channel::RegisterReceiveCodecsToRTPModule() {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::RegisterReceiveCodecsToRTPModule()");

  CodecInst codec;
  const uint8_t nSupportedCodecs = AudioCodingModule::NumberOfCodecs();

  for (int idx = 0; idx < nSupportedCodecs; idx++) {
    // Open up the RTP/RTCP receiver for all supported codecs
    if ((audio_coding_->Codec(idx, &codec) == -1) ||
        (rtp_receiver_->RegisterReceivePayload(codec) == -1)) {
      WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, _channelId),
                   "Channel::RegisterReceiveCodecsToRTPModule() unable"
                   " to register %s (%d/%d/%" PRIuS
                   "/%d) to RTP/RTCP "
                   "receiver",
                   codec.plname, codec.pltype, codec.plfreq, codec.channels,
                   codec.rate);
    } else {
      WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                   "Channel::RegisterReceiveCodecsToRTPModule() %s "
                   "(%d/%d/%" PRIuS
                   "/%d) has been added to the RTP/RTCP "
                   "receiver",
                   codec.plname, codec.pltype, codec.plfreq, codec.channels,
                   codec.rate);
    }
  }
}

int Channel::SetSendRtpHeaderExtension(bool enable,
                                       RTPExtensionType type,
                                       unsigned char id) {
  int error = 0;
  _rtpRtcpModule->DeregisterSendRtpHeaderExtension(type);
  if (enable) {
    error = _rtpRtcpModule->RegisterSendRtpHeaderExtension(type, id);
  }
  return error;
}

int Channel::GetRtpTimestampRateHz() const {
  const auto format = audio_coding_->ReceiveFormat();
  // Default to the playout frequency if we've not gotten any packets yet.
  // TODO(ossu): Zero clockrate can only happen if we've added an external
  // decoder for a format we don't support internally. Remove once that way of
  // adding decoders is gone!
  return (format && format->clockrate_hz != 0)
             ? format->clockrate_hz
             : audio_coding_->PlayoutFrequency();
}

int64_t Channel::GetRTT(bool allow_associate_channel) const {
  RtcpMode method = _rtpRtcpModule->RTCP();
  if (method == RtcpMode::kOff) {
    return 0;
  }
  std::vector<RTCPReportBlock> report_blocks;
  _rtpRtcpModule->RemoteRTCPStat(&report_blocks);

  int64_t rtt = 0;
  if (report_blocks.empty()) {
    if (allow_associate_channel) {
      rtc::CritScope lock(&assoc_send_channel_lock_);
      Channel* channel = associate_send_channel_.channel();
      // Tries to get RTT from an associated channel. This is important for
      // receive-only channels.
      if (channel) {
        // To prevent infinite recursion and deadlock, calling GetRTT of
        // associate channel should always use "false" for argument:
        // |allow_associate_channel|.
        rtt = channel->GetRTT(false);
      }
    }
    return rtt;
  }

  uint32_t remoteSSRC = rtp_receiver_->SSRC();
  std::vector<RTCPReportBlock>::const_iterator it = report_blocks.begin();
  for (; it != report_blocks.end(); ++it) {
    if (it->remoteSSRC == remoteSSRC)
      break;
  }
  if (it == report_blocks.end()) {
    // We have not received packets with SSRC matching the report blocks.
    // To calculate RTT we try with the SSRC of the first report block.
    // This is very important for send-only channels where we don't know
    // the SSRC of the other end.
    remoteSSRC = report_blocks[0].remoteSSRC;
  }

  int64_t avg_rtt = 0;
  int64_t max_rtt = 0;
  int64_t min_rtt = 0;
  if (_rtpRtcpModule->RTT(remoteSSRC, &rtt, &avg_rtt, &min_rtt, &max_rtt) !=
      0) {
    return 0;
  }
  return rtt;
}

}  // namespace voe
}  // namespace webrtc
