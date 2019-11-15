/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_send.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "api/call/transport.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "audio/utility/audio_frame_operations.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_processing/rms_level.h"
#include "modules/pacing/packet_router.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace voe {

namespace {

constexpr int64_t kMaxRetransmissionWindowMs = 1000;
constexpr int64_t kMinRetransmissionWindowMs = 30;

// Field trial which controls whether to report standard-compliant bytes
// sent/received per stream.  If enabled, padding and headers are not included
// in bytes sent or received.
constexpr char kUseStandardBytesStats[] = "WebRTC-UseStandardBytesStats";

MediaTransportEncodedAudioFrame::FrameType
MediaTransportFrameTypeForWebrtcFrameType(webrtc::AudioFrameType frame_type) {
  switch (frame_type) {
    case AudioFrameType::kAudioFrameSpeech:
      return MediaTransportEncodedAudioFrame::FrameType::kSpeech;
      break;

    case AudioFrameType::kAudioFrameCN:
      return MediaTransportEncodedAudioFrame::FrameType::
          kDiscontinuousTransmission;
      break;

    default:
      RTC_CHECK(false) << "Unexpected frame type="
                       << static_cast<int>(frame_type);
      break;
  }
}

class RtpPacketSenderProxy;
class TransportFeedbackProxy;
class TransportSequenceNumberProxy;
class VoERtcpObserver;

class ChannelSend : public ChannelSendInterface,
                    public AudioPacketizationCallback,  // receive encoded
                                                        // packets from the ACM
                    public TargetTransferRateObserver {
 public:
  // TODO(nisse): Make OnUplinkPacketLossRate public, and delete friend
  // declaration.
  friend class VoERtcpObserver;

  ChannelSend(Clock* clock,
              TaskQueueFactory* task_queue_factory,
              ProcessThread* module_process_thread,
              const MediaTransportConfig& media_transport_config,
              OverheadObserver* overhead_observer,
              Transport* rtp_transport,
              RtcpRttStats* rtcp_rtt_stats,
              RtcEventLog* rtc_event_log,
              FrameEncryptorInterface* frame_encryptor,
              const webrtc::CryptoOptions& crypto_options,
              bool extmap_allow_mixed,
              int rtcp_report_interval_ms,
              uint32_t ssrc);

  ~ChannelSend() override;

  // Send using this encoder, with this payload type.
  void SetEncoder(int payload_type,
                  std::unique_ptr<AudioEncoder> encoder) override;
  void ModifyEncoder(rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)>
                         modifier) override;
  void CallEncoder(rtc::FunctionView<void(AudioEncoder*)> modifier) override;

  // API methods
  void StartSend() override;
  void StopSend() override;

  // Codecs
  void OnBitrateAllocation(BitrateAllocationUpdate update) override;
  int GetBitrate() const override;

  // Network
  void ReceivedRTCPPacket(const uint8_t* data, size_t length) override;

  // Muting, Volume and Level.
  void SetInputMute(bool enable) override;

  // Stats.
  ANAStats GetANAStatistics() const override;

  // Used by AudioSendStream.
  RtpRtcp* GetRtpRtcp() const override;

  void RegisterCngPayloadType(int payload_type, int payload_frequency) override;

  // DTMF.
  bool SendTelephoneEventOutband(int event, int duration_ms) override;
  void SetSendTelephoneEventPayloadType(int payload_type,
                                        int payload_frequency) override;

  // RTP+RTCP
  void SetRid(const std::string& rid,
              int extension_id,
              int repaired_extension_id) override;
  void SetMid(const std::string& mid, int extension_id) override;
  void SetExtmapAllowMixed(bool extmap_allow_mixed) override;
  void SetSendAudioLevelIndicationStatus(bool enable, int id) override;
  void EnableSendTransportSequenceNumber(int id) override;

  void RegisterSenderCongestionControlObjects(
      RtpTransportControllerSendInterface* transport,
      RtcpBandwidthObserver* bandwidth_observer) override;
  void ResetSenderCongestionControlObjects() override;
  void SetRTCP_CNAME(absl::string_view c_name) override;
  std::vector<ReportBlock> GetRemoteRTCPReportBlocks() const override;
  CallSendStatistics GetRTCPStatistics() const override;

  // ProcessAndEncodeAudio() posts a task on the shared encoder task queue,
  // which in turn calls (on the queue) ProcessAndEncodeAudioOnTaskQueue() where
  // the actual processing of the audio takes place. The processing mainly
  // consists of encoding and preparing the result for sending by adding it to a
  // send queue.
  // The main reason for using a task queue here is to release the native,
  // OS-specific, audio capture thread as soon as possible to ensure that it
  // can go back to sleep and be prepared to deliver an new captured audio
  // packet.
  void ProcessAndEncodeAudio(std::unique_ptr<AudioFrame> audio_frame) override;

  // The existence of this function alongside OnUplinkPacketLossRate is
  // a compromise. We want the encoder to be agnostic of the PLR source, but
  // we also don't want it to receive conflicting information from TWCC and
  // from RTCP-XR.
  void OnTwccBasedUplinkPacketLossRate(float packet_loss_rate) override;

  void OnRecoverableUplinkPacketLossRate(
      float recoverable_packet_loss_rate) override;

  int64_t GetRTT() const override;

  // E2EE Custom Audio Frame Encryption
  void SetFrameEncryptor(
      rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor) override;

 private:
  // From AudioPacketizationCallback in the ACM
  int32_t SendData(AudioFrameType frameType,
                   uint8_t payloadType,
                   uint32_t timeStamp,
                   const uint8_t* payloadData,
                   size_t payloadSize) override;

  void OnUplinkPacketLossRate(float packet_loss_rate);
  bool InputMute() const;

  int SetSendRtpHeaderExtension(bool enable, RTPExtensionType type, int id);

  int32_t SendRtpAudio(AudioFrameType frameType,
                       uint8_t payloadType,
                       uint32_t timeStamp,
                       rtc::ArrayView<const uint8_t> payload)
      RTC_RUN_ON(encoder_queue_);

  int32_t SendMediaTransportAudio(AudioFrameType frameType,
                                  uint8_t payloadType,
                                  uint32_t timeStamp,
                                  rtc::ArrayView<const uint8_t> payload)
      RTC_RUN_ON(encoder_queue_);

  // Return media transport or nullptr if using RTP.
  MediaTransportInterface* media_transport() {
    return media_transport_config_.media_transport;
  }

  // Called on the encoder task queue when a new input audio frame is ready
  // for encoding.
  void ProcessAndEncodeAudioOnTaskQueue(AudioFrame* audio_input)
      RTC_RUN_ON(encoder_queue_);

  void OnReceivedRtt(int64_t rtt_ms);

  void OnTargetTransferRate(TargetTransferRate) override;

  // Thread checkers document and lock usage of some methods on voe::Channel to
  // specific threads we know about. The goal is to eventually split up
  // voe::Channel into parts with single-threaded semantics, and thereby reduce
  // the need for locks.
  rtc::ThreadChecker worker_thread_checker_;
  rtc::ThreadChecker module_process_thread_checker_;
  // Methods accessed from audio and video threads are checked for sequential-
  // only access. We don't necessarily own and control these threads, so thread
  // checkers cannot be used. E.g. Chromium may transfer "ownership" from one
  // audio thread to another, but access is still sequential.
  rtc::RaceChecker audio_thread_race_checker_;

  rtc::CriticalSection volume_settings_critsect_;

  bool sending_ RTC_GUARDED_BY(&worker_thread_checker_) = false;

  RtcEventLog* const event_log_;

  std::unique_ptr<RtpRtcp> _rtpRtcpModule;
  std::unique_ptr<RTPSenderAudio> rtp_sender_audio_;

  std::unique_ptr<AudioCodingModule> audio_coding_;
  uint32_t _timeStamp RTC_GUARDED_BY(encoder_queue_);

  // uses
  ProcessThread* const _moduleProcessThreadPtr;
  RmsLevel rms_level_ RTC_GUARDED_BY(encoder_queue_);
  bool input_mute_ RTC_GUARDED_BY(volume_settings_critsect_);
  bool previous_frame_muted_ RTC_GUARDED_BY(encoder_queue_);
  // VoeRTP_RTCP
  // TODO(henrika): can today be accessed on the main thread and on the
  // task queue; hence potential race.
  bool _includeAudioLevelIndication;

  // RtcpBandwidthObserver
  const std::unique_ptr<VoERtcpObserver> rtcp_observer_;

  PacketRouter* packet_router_ RTC_GUARDED_BY(&worker_thread_checker_) =
      nullptr;
  const std::unique_ptr<TransportFeedbackProxy> feedback_observer_proxy_;
  const std::unique_ptr<RtpPacketSenderProxy> rtp_packet_pacer_proxy_;
  const std::unique_ptr<RateLimiter> retransmission_rate_limiter_;

  rtc::ThreadChecker construction_thread_;

  const bool use_twcc_plr_for_ana_;
  const bool use_standard_bytes_stats_;

  bool encoder_queue_is_active_ RTC_GUARDED_BY(encoder_queue_) = false;

  MediaTransportConfig media_transport_config_;
  int media_transport_sequence_number_ RTC_GUARDED_BY(encoder_queue_) = 0;

  rtc::CriticalSection media_transport_lock_;
  // Currently set to local SSRC at construction.
  uint64_t media_transport_channel_id_ RTC_GUARDED_BY(&media_transport_lock_) =
      0;
  // Cache payload type and sampling frequency from most recent call to
  // SetEncoder. Needed to set MediaTransportEncodedAudioFrame metadata, and
  // invalidate on encoder change.
  int media_transport_payload_type_ RTC_GUARDED_BY(&media_transport_lock_);
  int media_transport_sampling_frequency_
      RTC_GUARDED_BY(&media_transport_lock_);

  // E2EE Audio Frame Encryption
  rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor_
      RTC_GUARDED_BY(encoder_queue_);
  // E2EE Frame Encryption Options
  const webrtc::CryptoOptions crypto_options_;

  rtc::CriticalSection bitrate_crit_section_;
  int configured_bitrate_bps_ RTC_GUARDED_BY(bitrate_crit_section_) = 0;

  // Defined last to ensure that there are no running tasks when the other
  // members are destroyed.
  rtc::TaskQueue encoder_queue_;
};

const int kTelephoneEventAttenuationdB = 10;

class TransportFeedbackProxy : public TransportFeedbackObserver {
 public:
  TransportFeedbackProxy() : feedback_observer_(nullptr) {
    pacer_thread_.Detach();
    network_thread_.Detach();
  }

  void SetTransportFeedbackObserver(
      TransportFeedbackObserver* feedback_observer) {
    RTC_DCHECK(thread_checker_.IsCurrent());
    rtc::CritScope lock(&crit_);
    feedback_observer_ = feedback_observer;
  }

  // Implements TransportFeedbackObserver.
  void OnAddPacket(const RtpPacketSendInfo& packet_info) override {
    RTC_DCHECK(pacer_thread_.IsCurrent());
    rtc::CritScope lock(&crit_);
    if (feedback_observer_)
      feedback_observer_->OnAddPacket(packet_info);
  }

  void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override {
    RTC_DCHECK(network_thread_.IsCurrent());
    rtc::CritScope lock(&crit_);
    if (feedback_observer_)
      feedback_observer_->OnTransportFeedback(feedback);
  }

 private:
  rtc::CriticalSection crit_;
  rtc::ThreadChecker thread_checker_;
  rtc::ThreadChecker pacer_thread_;
  rtc::ThreadChecker network_thread_;
  TransportFeedbackObserver* feedback_observer_ RTC_GUARDED_BY(&crit_);
};

class RtpPacketSenderProxy : public RtpPacketSender {
 public:
  RtpPacketSenderProxy() : rtp_packet_pacer_(nullptr) {}

  void SetPacketPacer(RtpPacketSender* rtp_packet_pacer) {
    RTC_DCHECK(thread_checker_.IsCurrent());
    rtc::CritScope lock(&crit_);
    rtp_packet_pacer_ = rtp_packet_pacer;
  }

  void EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet) override {
    rtc::CritScope lock(&crit_);
    rtp_packet_pacer_->EnqueuePacket(std::move(packet));
  }

 private:
  rtc::ThreadChecker thread_checker_;
  rtc::CriticalSection crit_;
  RtpPacketSender* rtp_packet_pacer_ RTC_GUARDED_BY(&crit_);
};

class VoERtcpObserver : public RtcpBandwidthObserver {
 public:
  explicit VoERtcpObserver(ChannelSend* owner)
      : owner_(owner), bandwidth_observer_(nullptr) {}
  ~VoERtcpObserver() override {}

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
          extended_max_sequence_number_.find(block_it->source_ssrc);
      int number_of_packets = 0;
      if (seq_num_it != extended_max_sequence_number_.end()) {
        number_of_packets =
            block_it->extended_highest_sequence_number - seq_num_it->second;
      }
      fraction_lost_aggregate += number_of_packets * block_it->fraction_lost;
      total_number_of_packets += number_of_packets;

      extended_max_sequence_number_[block_it->source_ssrc] =
          block_it->extended_highest_sequence_number;
    }
    int weighted_fraction_lost = 0;
    if (total_number_of_packets > 0) {
      weighted_fraction_lost =
          (fraction_lost_aggregate + total_number_of_packets / 2) /
          total_number_of_packets;
    }
    owner_->OnUplinkPacketLossRate(weighted_fraction_lost / 255.0f);
  }

 private:
  ChannelSend* owner_;
  // Maps remote side ssrc to extended highest sequence number received.
  std::map<uint32_t, uint32_t> extended_max_sequence_number_;
  rtc::CriticalSection crit_;
  RtcpBandwidthObserver* bandwidth_observer_ RTC_GUARDED_BY(crit_);
};

int32_t ChannelSend::SendData(AudioFrameType frameType,
                              uint8_t payloadType,
                              uint32_t timeStamp,
                              const uint8_t* payloadData,
                              size_t payloadSize) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  rtc::ArrayView<const uint8_t> payload(payloadData, payloadSize);

  if (media_transport() != nullptr) {
    if (frameType == AudioFrameType::kEmptyFrame) {
      // TODO(bugs.webrtc.org/9719): Media transport Send doesn't support
      // sending empty frames.
      return 0;
    }

    return SendMediaTransportAudio(frameType, payloadType, timeStamp, payload);
  } else {
    return SendRtpAudio(frameType, payloadType, timeStamp, payload);
  }
}

int32_t ChannelSend::SendRtpAudio(AudioFrameType frameType,
                                  uint8_t payloadType,
                                  uint32_t timeStamp,
                                  rtc::ArrayView<const uint8_t> payload) {
  if (_includeAudioLevelIndication) {
    // Store current audio level in the RTP sender.
    // The level will be used in combination with voice-activity state
    // (frameType) to add an RTP header extension
    rtp_sender_audio_->SetAudioLevel(rms_level_.Average());
  }

  // E2EE Custom Audio Frame Encryption (This is optional).
  // Keep this buffer around for the lifetime of the send call.
  rtc::Buffer encrypted_audio_payload;
  // We don't invoke encryptor if payload is empty, which means we are to send
  // DTMF, or the encoder entered DTX.
  // TODO(minyue): see whether DTMF packets should be encrypted or not. In
  // current implementation, they are not.
  if (!payload.empty()) {
    if (frame_encryptor_ != nullptr) {
      // TODO(benwright@webrtc.org) - Allocate enough to always encrypt inline.
      // Allocate a buffer to hold the maximum possible encrypted payload.
      size_t max_ciphertext_size = frame_encryptor_->GetMaxCiphertextByteSize(
          cricket::MEDIA_TYPE_AUDIO, payload.size());
      encrypted_audio_payload.SetSize(max_ciphertext_size);

      // Encrypt the audio payload into the buffer.
      size_t bytes_written = 0;
      int encrypt_status = frame_encryptor_->Encrypt(
          cricket::MEDIA_TYPE_AUDIO, _rtpRtcpModule->SSRC(),
          /*additional_data=*/nullptr, payload, encrypted_audio_payload,
          &bytes_written);
      if (encrypt_status != 0) {
        RTC_DLOG(LS_ERROR)
            << "Channel::SendData() failed encrypt audio payload: "
            << encrypt_status;
        return -1;
      }
      // Resize the buffer to the exact number of bytes actually used.
      encrypted_audio_payload.SetSize(bytes_written);
      // Rewrite the payloadData and size to the new encrypted payload.
      payload = encrypted_audio_payload;
    } else if (crypto_options_.sframe.require_frame_encryption) {
      RTC_DLOG(LS_ERROR) << "Channel::SendData() failed sending audio payload: "
                         << "A frame encryptor is required but one is not set.";
      return -1;
    }
  }

  // Push data from ACM to RTP/RTCP-module to deliver audio frame for
  // packetization.
  if (!_rtpRtcpModule->OnSendingRtpFrame(timeStamp,
                                         // Leaving the time when this frame was
                                         // received from the capture device as
                                         // undefined for voice for now.
                                         -1, payloadType,
                                         /*force_sender_report=*/false)) {
    return false;
  }

  // RTCPSender has it's own copy of the timestamp offset, added in
  // RTCPSender::BuildSR, hence we must not add the in the offset for the above
  // call.
  // TODO(nisse): Delete RTCPSender:timestamp_offset_, and see if we can confine
  // knowledge of the offset to a single place.
  const uint32_t rtp_timestamp = timeStamp + _rtpRtcpModule->StartTimestamp();
  // This call will trigger Transport::SendPacket() from the RTP/RTCP module.
  if (!rtp_sender_audio_->SendAudio(frameType, payloadType, rtp_timestamp,
                                    payload.data(), payload.size())) {
    RTC_DLOG(LS_ERROR)
        << "ChannelSend::SendData() failed to send data to RTP/RTCP module";
    return -1;
  }

  return 0;
}

int32_t ChannelSend::SendMediaTransportAudio(
    AudioFrameType frameType,
    uint8_t payloadType,
    uint32_t timeStamp,
    rtc::ArrayView<const uint8_t> payload) {
  // TODO(nisse): Use null _transportPtr for MediaTransport.
  // RTC_DCHECK(_transportPtr == nullptr);
  uint64_t channel_id;
  int sampling_rate_hz;
  {
    rtc::CritScope cs(&media_transport_lock_);
    if (media_transport_payload_type_ != payloadType) {
      // Payload type is being changed, media_transport_sampling_frequency_,
      // no longer current.
      return -1;
    }
    sampling_rate_hz = media_transport_sampling_frequency_;
    channel_id = media_transport_channel_id_;
  }
  MediaTransportEncodedAudioFrame frame(
      /*sampling_rate_hz=*/sampling_rate_hz,

      // TODO(nisse): Timestamp and sample index are the same for all supported
      // audio codecs except G722. Refactor audio coding module to only use
      // sample index, and leave translation to RTP time, when needed, for
      // RTP-specific code.
      /*starting_sample_index=*/timeStamp,

      // Sample count isn't conveniently available from the AudioCodingModule,
      // and needs some refactoring to wire up in a good way. For now, left as
      // zero.
      /*samples_per_channel=*/0,

      /*sequence_number=*/media_transport_sequence_number_,
      MediaTransportFrameTypeForWebrtcFrameType(frameType), payloadType,
      std::vector<uint8_t>(payload.begin(), payload.end()));

  // TODO(nisse): Introduce a MediaTransportSender object bound to a specific
  // channel id.
  RTCError rtc_error =
      media_transport()->SendAudioFrame(channel_id, std::move(frame));

  if (!rtc_error.ok()) {
    RTC_LOG(LS_ERROR) << "Failed to send frame, rtc_error="
                      << ToString(rtc_error.type()) << ", "
                      << rtc_error.message();
    return -1;
  }

  ++media_transport_sequence_number_;

  return 0;
}

ChannelSend::ChannelSend(Clock* clock,
                         TaskQueueFactory* task_queue_factory,
                         ProcessThread* module_process_thread,
                         const MediaTransportConfig& media_transport_config,
                         OverheadObserver* overhead_observer,
                         Transport* rtp_transport,
                         RtcpRttStats* rtcp_rtt_stats,
                         RtcEventLog* rtc_event_log,
                         FrameEncryptorInterface* frame_encryptor,
                         const webrtc::CryptoOptions& crypto_options,
                         bool extmap_allow_mixed,
                         int rtcp_report_interval_ms,
                         uint32_t ssrc)
    : event_log_(rtc_event_log),
      _timeStamp(0),  // This is just an offset, RTP module will add it's own
                      // random offset
      _moduleProcessThreadPtr(module_process_thread),
      input_mute_(false),
      previous_frame_muted_(false),
      _includeAudioLevelIndication(false),
      rtcp_observer_(new VoERtcpObserver(this)),
      feedback_observer_proxy_(new TransportFeedbackProxy()),
      rtp_packet_pacer_proxy_(new RtpPacketSenderProxy()),
      retransmission_rate_limiter_(
          new RateLimiter(clock, kMaxRetransmissionWindowMs)),
      use_twcc_plr_for_ana_(
          webrtc::field_trial::FindFullName("UseTwccPlrForAna") == "Enabled"),
      use_standard_bytes_stats_(
          webrtc::field_trial::IsEnabled(kUseStandardBytesStats)),
      media_transport_config_(media_transport_config),
      frame_encryptor_(frame_encryptor),
      crypto_options_(crypto_options),
      encoder_queue_(task_queue_factory->CreateTaskQueue(
          "AudioEncoder",
          TaskQueueFactory::Priority::NORMAL)) {
  RTC_DCHECK(module_process_thread);
  module_process_thread_checker_.Detach();

  audio_coding_.reset(AudioCodingModule::Create(AudioCodingModule::Config()));

  RtpRtcp::Configuration configuration;

  // We gradually remove codepaths that depend on RTP when using media
  // transport. All of this logic should be moved to the future
  // RTPMediaTransport. In this case it means that overhead and bandwidth
  // observers should not be called when using media transport.
  if (!media_transport_config.media_transport) {
    configuration.overhead_observer = overhead_observer;
    configuration.bandwidth_callback = rtcp_observer_.get();
    configuration.transport_feedback_callback = feedback_observer_proxy_.get();
  }

  configuration.clock = clock;
  configuration.audio = true;
  configuration.clock = Clock::GetRealTimeClock();
  configuration.outgoing_transport = rtp_transport;

  configuration.paced_sender = rtp_packet_pacer_proxy_.get();

  configuration.event_log = event_log_;
  configuration.rtt_stats = rtcp_rtt_stats;
  configuration.retransmission_rate_limiter =
      retransmission_rate_limiter_.get();
  configuration.extmap_allow_mixed = extmap_allow_mixed;
  configuration.rtcp_report_interval_ms = rtcp_report_interval_ms;

  configuration.local_media_ssrc = ssrc;
  if (media_transport_config_.media_transport) {
    rtc::CritScope cs(&media_transport_lock_);
    media_transport_channel_id_ = ssrc;
  }

  _rtpRtcpModule = RtpRtcp::Create(configuration);
  _rtpRtcpModule->SetSendingMediaStatus(false);

  rtp_sender_audio_ = absl::make_unique<RTPSenderAudio>(
      configuration.clock, _rtpRtcpModule->RtpSender());

  // We want to invoke the 'TargetRateObserver' and |OnOverheadChanged|
  // callbacks after the audio_coding_ is fully initialized.
  if (media_transport_config.media_transport) {
    RTC_DLOG(LS_INFO) << "Setting media_transport_ rate observers.";
    media_transport_config.media_transport->AddTargetTransferRateObserver(this);
    media_transport_config.media_transport->SetAudioOverheadObserver(
        overhead_observer);
  } else {
    RTC_DLOG(LS_INFO) << "Not setting media_transport_ rate observers.";
  }

  _moduleProcessThreadPtr->RegisterModule(_rtpRtcpModule.get(), RTC_FROM_HERE);

  // Ensure that RTCP is enabled by default for the created channel.
  _rtpRtcpModule->SetRTCPStatus(RtcpMode::kCompound);

  int error = audio_coding_->RegisterTransportCallback(this);
  RTC_DCHECK_EQ(0, error);
}

ChannelSend::~ChannelSend() {
  RTC_DCHECK(construction_thread_.IsCurrent());

  if (media_transport_config_.media_transport) {
    media_transport_config_.media_transport->RemoveTargetTransferRateObserver(
        this);
    media_transport_config_.media_transport->SetAudioOverheadObserver(nullptr);
  }

  StopSend();
  int error = audio_coding_->RegisterTransportCallback(NULL);
  RTC_DCHECK_EQ(0, error);

  if (_moduleProcessThreadPtr)
    _moduleProcessThreadPtr->DeRegisterModule(_rtpRtcpModule.get());
}

void ChannelSend::StartSend() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK(!sending_);
  sending_ = true;

  _rtpRtcpModule->SetSendingMediaStatus(true);
  int ret = _rtpRtcpModule->SetSendingStatus(true);
  RTC_DCHECK_EQ(0, ret);
  // It is now OK to start processing on the encoder task queue.
  encoder_queue_.PostTask([this] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    encoder_queue_is_active_ = true;
  });
}

void ChannelSend::StopSend() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!sending_) {
    return;
  }
  sending_ = false;

  rtc::Event flush;
  encoder_queue_.PostTask([this, &flush]() {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    encoder_queue_is_active_ = false;
    flush.Set();
  });
  flush.Wait(rtc::Event::kForever);

  // Reset sending SSRC and sequence number and triggers direct transmission
  // of RTCP BYE
  if (_rtpRtcpModule->SetSendingStatus(false) == -1) {
    RTC_DLOG(LS_ERROR) << "StartSend() RTP/RTCP failed to stop sending";
  }
  _rtpRtcpModule->SetSendingMediaStatus(false);
}

void ChannelSend::SetEncoder(int payload_type,
                             std::unique_ptr<AudioEncoder> encoder) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK_GE(payload_type, 0);
  RTC_DCHECK_LE(payload_type, 127);

  // The RTP/RTCP module needs to know the RTP timestamp rate (i.e. clockrate)
  // as well as some other things, so we collect this info and send it along.
  _rtpRtcpModule->RegisterSendPayloadFrequency(payload_type,
                                               encoder->RtpTimestampRateHz());
  rtp_sender_audio_->RegisterAudioPayload("audio", payload_type,
                                          encoder->RtpTimestampRateHz(),
                                          encoder->NumChannels(), 0);

  if (media_transport_config_.media_transport) {
    rtc::CritScope cs(&media_transport_lock_);
    media_transport_payload_type_ = payload_type;
    // TODO(nisse): Currently broken for G722, since timestamps passed through
    // encoder use RTP clock rather than sample count, and they differ for G722.
    media_transport_sampling_frequency_ = encoder->RtpTimestampRateHz();
  }
  audio_coding_->SetEncoder(std::move(encoder));
}

void ChannelSend::ModifyEncoder(
    rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier) {
  // This method can be called on the worker thread, module process thread
  // or network thread. Audio coding is thread safe, so we do not need to
  // enforce the calling thread.
  audio_coding_->ModifyEncoder(modifier);
}

void ChannelSend::CallEncoder(rtc::FunctionView<void(AudioEncoder*)> modifier) {
  ModifyEncoder([modifier](std::unique_ptr<AudioEncoder>* encoder_ptr) {
    if (*encoder_ptr) {
      modifier(encoder_ptr->get());
    } else {
      RTC_DLOG(LS_WARNING) << "Trying to call unset encoder.";
    }
  });
}

void ChannelSend::OnBitrateAllocation(BitrateAllocationUpdate update) {
  // This method can be called on the worker thread, module process thread
  // or on a TaskQueue via VideoSendStreamImpl::OnEncoderConfigurationChanged.
  // TODO(solenberg): Figure out a good way to check this or enforce calling
  // rules.
  // RTC_DCHECK(worker_thread_checker_.IsCurrent() ||
  //            module_process_thread_checker_.IsCurrent());
  rtc::CritScope lock(&bitrate_crit_section_);

  CallEncoder([&](AudioEncoder* encoder) {
    encoder->OnReceivedUplinkAllocation(update);
  });
  retransmission_rate_limiter_->SetMaxRate(update.target_bitrate.bps());
  configured_bitrate_bps_ = update.target_bitrate.bps();
}

int ChannelSend::GetBitrate() const {
  rtc::CritScope lock(&bitrate_crit_section_);
  return configured_bitrate_bps_;
}

void ChannelSend::OnTwccBasedUplinkPacketLossRate(float packet_loss_rate) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!use_twcc_plr_for_ana_)
    return;
  CallEncoder([&](AudioEncoder* encoder) {
    encoder->OnReceivedUplinkPacketLossFraction(packet_loss_rate);
  });
}

void ChannelSend::OnRecoverableUplinkPacketLossRate(
    float recoverable_packet_loss_rate) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  CallEncoder([&](AudioEncoder* encoder) {
    encoder->OnReceivedUplinkRecoverablePacketLossFraction(
        recoverable_packet_loss_rate);
  });
}

void ChannelSend::OnUplinkPacketLossRate(float packet_loss_rate) {
  if (use_twcc_plr_for_ana_)
    return;
  CallEncoder([&](AudioEncoder* encoder) {
    encoder->OnReceivedUplinkPacketLossFraction(packet_loss_rate);
  });
}

void ChannelSend::ReceivedRTCPPacket(const uint8_t* data, size_t length) {
  // May be called on either worker thread or network thread.
  if (media_transport_config_.media_transport) {
    // Ignore RTCP packets while media transport is used.
    // Those packets should not arrive, but we are seeing occasional packets.
    return;
  }

  // Deliver RTCP packet to RTP/RTCP module for parsing
  _rtpRtcpModule->IncomingRtcpPacket(data, length);

  int64_t rtt = GetRTT();
  if (rtt == 0) {
    // Waiting for valid RTT.
    return;
  }

  int64_t nack_window_ms = rtt;
  if (nack_window_ms < kMinRetransmissionWindowMs) {
    nack_window_ms = kMinRetransmissionWindowMs;
  } else if (nack_window_ms > kMaxRetransmissionWindowMs) {
    nack_window_ms = kMaxRetransmissionWindowMs;
  }
  retransmission_rate_limiter_->SetWindowSize(nack_window_ms);

  OnReceivedRtt(rtt);
}

void ChannelSend::SetInputMute(bool enable) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  rtc::CritScope cs(&volume_settings_critsect_);
  input_mute_ = enable;
}

bool ChannelSend::InputMute() const {
  rtc::CritScope cs(&volume_settings_critsect_);
  return input_mute_;
}

bool ChannelSend::SendTelephoneEventOutband(int event, int duration_ms) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK_LE(0, event);
  RTC_DCHECK_GE(255, event);
  RTC_DCHECK_LE(0, duration_ms);
  RTC_DCHECK_GE(65535, duration_ms);
  if (!sending_) {
    return false;
  }
  if (rtp_sender_audio_->SendTelephoneEvent(
          event, duration_ms, kTelephoneEventAttenuationdB) != 0) {
    RTC_DLOG(LS_ERROR) << "SendTelephoneEvent() failed to send event";
    return false;
  }
  return true;
}

void ChannelSend::RegisterCngPayloadType(int payload_type,
                                         int payload_frequency) {
  _rtpRtcpModule->RegisterSendPayloadFrequency(payload_type, payload_frequency);
  rtp_sender_audio_->RegisterAudioPayload("CN", payload_type, payload_frequency,
                                          1, 0);
}

void ChannelSend::SetSendTelephoneEventPayloadType(int payload_type,
                                                   int payload_frequency) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK_LE(0, payload_type);
  RTC_DCHECK_GE(127, payload_type);
  _rtpRtcpModule->RegisterSendPayloadFrequency(payload_type, payload_frequency);
  rtp_sender_audio_->RegisterAudioPayload("telephone-event", payload_type,
                                          payload_frequency, 0, 0);
}

void ChannelSend::SetRid(const std::string& rid,
                         int extension_id,
                         int repaired_extension_id) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (extension_id != 0) {
    int ret = SetSendRtpHeaderExtension(!rid.empty(), kRtpExtensionRtpStreamId,
                                        extension_id);
    RTC_DCHECK_EQ(0, ret);
  }
  if (repaired_extension_id != 0) {
    int ret = SetSendRtpHeaderExtension(!rid.empty(), kRtpExtensionRtpStreamId,
                                        repaired_extension_id);
    RTC_DCHECK_EQ(0, ret);
  }
  _rtpRtcpModule->SetRid(rid);
}

void ChannelSend::SetMid(const std::string& mid, int extension_id) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  int ret = SetSendRtpHeaderExtension(true, kRtpExtensionMid, extension_id);
  RTC_DCHECK_EQ(0, ret);
  _rtpRtcpModule->SetMid(mid);
}

void ChannelSend::SetExtmapAllowMixed(bool extmap_allow_mixed) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  _rtpRtcpModule->SetExtmapAllowMixed(extmap_allow_mixed);
}

void ChannelSend::SetSendAudioLevelIndicationStatus(bool enable, int id) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  _includeAudioLevelIndication = enable;
  int ret = SetSendRtpHeaderExtension(enable, kRtpExtensionAudioLevel, id);
  RTC_DCHECK_EQ(0, ret);
}

void ChannelSend::EnableSendTransportSequenceNumber(int id) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  int ret =
      SetSendRtpHeaderExtension(true, kRtpExtensionTransportSequenceNumber, id);
  RTC_DCHECK_EQ(0, ret);
}

void ChannelSend::RegisterSenderCongestionControlObjects(
    RtpTransportControllerSendInterface* transport,
    RtcpBandwidthObserver* bandwidth_observer) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RtpPacketSender* rtp_packet_pacer = transport->packet_sender();
  TransportFeedbackObserver* transport_feedback_observer =
      transport->transport_feedback_observer();
  PacketRouter* packet_router = transport->packet_router();

  RTC_DCHECK(rtp_packet_pacer);
  RTC_DCHECK(transport_feedback_observer);
  RTC_DCHECK(packet_router);
  RTC_DCHECK(!packet_router_);
  rtcp_observer_->SetBandwidthObserver(bandwidth_observer);
  feedback_observer_proxy_->SetTransportFeedbackObserver(
      transport_feedback_observer);
  rtp_packet_pacer_proxy_->SetPacketPacer(rtp_packet_pacer);
  _rtpRtcpModule->SetStorePacketsStatus(true, 600);
  constexpr bool remb_candidate = false;
  packet_router->AddSendRtpModule(_rtpRtcpModule.get(), remb_candidate);
  packet_router_ = packet_router;
}

void ChannelSend::ResetSenderCongestionControlObjects() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK(packet_router_);
  _rtpRtcpModule->SetStorePacketsStatus(false, 600);
  rtcp_observer_->SetBandwidthObserver(nullptr);
  feedback_observer_proxy_->SetTransportFeedbackObserver(nullptr);
  packet_router_->RemoveSendRtpModule(_rtpRtcpModule.get());
  packet_router_ = nullptr;
  rtp_packet_pacer_proxy_->SetPacketPacer(nullptr);
}

void ChannelSend::SetRTCP_CNAME(absl::string_view c_name) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  // Note: SetCNAME() accepts a c string of length at most 255.
  const std::string c_name_limited(c_name.substr(0, 255));
  int ret = _rtpRtcpModule->SetCNAME(c_name_limited.c_str()) != 0;
  RTC_DCHECK_EQ(0, ret) << "SetRTCP_CNAME() failed to set RTCP CNAME";
}

std::vector<ReportBlock> ChannelSend::GetRemoteRTCPReportBlocks() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  // Get the report blocks from the latest received RTCP Sender or Receiver
  // Report. Each element in the vector contains the sender's SSRC and a
  // report block according to RFC 3550.
  std::vector<RTCPReportBlock> rtcp_report_blocks;

  int ret = _rtpRtcpModule->RemoteRTCPStat(&rtcp_report_blocks);
  RTC_DCHECK_EQ(0, ret);

  std::vector<ReportBlock> report_blocks;

  std::vector<RTCPReportBlock>::const_iterator it = rtcp_report_blocks.begin();
  for (; it != rtcp_report_blocks.end(); ++it) {
    ReportBlock report_block;
    report_block.sender_SSRC = it->sender_ssrc;
    report_block.source_SSRC = it->source_ssrc;
    report_block.fraction_lost = it->fraction_lost;
    report_block.cumulative_num_packets_lost = it->packets_lost;
    report_block.extended_highest_sequence_number =
        it->extended_highest_sequence_number;
    report_block.interarrival_jitter = it->jitter;
    report_block.last_SR_timestamp = it->last_sender_report_timestamp;
    report_block.delay_since_last_SR = it->delay_since_last_sender_report;
    report_blocks.push_back(report_block);
  }
  return report_blocks;
}

CallSendStatistics ChannelSend::GetRTCPStatistics() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  CallSendStatistics stats = {0};
  stats.rttMs = GetRTT();

  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  _rtpRtcpModule->GetSendStreamDataCounters(&rtp_stats, &rtx_stats);
  if (use_standard_bytes_stats_) {
    stats.bytesSent = rtp_stats.transmitted.payload_bytes +
                      rtx_stats.transmitted.payload_bytes;
  } else {
    stats.bytesSent = rtp_stats.transmitted.payload_bytes +
                      rtp_stats.transmitted.padding_bytes +
                      rtp_stats.transmitted.header_bytes +
                      rtx_stats.transmitted.payload_bytes +
                      rtx_stats.transmitted.padding_bytes +
                      rtx_stats.transmitted.header_bytes;
  }
  // TODO(https://crbug.com/webrtc/10555): RTX retransmissions should show up in
  // separate outbound-rtp stream objects.
  stats.retransmitted_bytes_sent = rtp_stats.retransmitted.payload_bytes;
  stats.packetsSent =
      rtp_stats.transmitted.packets + rtx_stats.transmitted.packets;
  stats.retransmitted_packets_sent = rtp_stats.retransmitted.packets;
  stats.report_block_datas = _rtpRtcpModule->GetLatestReportBlockData();

  return stats;
}

void ChannelSend::ProcessAndEncodeAudio(
    std::unique_ptr<AudioFrame> audio_frame) {
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  struct ProcessAndEncodeAudio {
    void operator()() {
      RTC_DCHECK_RUN_ON(&channel->encoder_queue_);
      if (!channel->encoder_queue_is_active_) {
        return;
      }
      channel->ProcessAndEncodeAudioOnTaskQueue(audio_frame.get());
    }
    std::unique_ptr<AudioFrame> audio_frame;
    ChannelSend* const channel;
  };
  // Profile time between when the audio frame is added to the task queue and
  // when the task is actually executed.
  audio_frame->UpdateProfileTimeStamp();
  encoder_queue_.PostTask(ProcessAndEncodeAudio{std::move(audio_frame), this});
}

void ChannelSend::ProcessAndEncodeAudioOnTaskQueue(AudioFrame* audio_input) {
  RTC_DCHECK_GT(audio_input->samples_per_channel_, 0);
  RTC_DCHECK_LE(audio_input->num_channels_, 8);

  // Measure time between when the audio frame is added to the task queue and
  // when the task is actually executed. Goal is to keep track of unwanted
  // extra latency added by the task queue.
  RTC_HISTOGRAM_COUNTS_10000("WebRTC.Audio.EncodingTaskQueueLatencyMs",
                             audio_input->ElapsedProfileTimeMs());

  bool is_muted = InputMute();
  AudioFrameOperations::Mute(audio_input, previous_frame_muted_, is_muted);

  if (_includeAudioLevelIndication) {
    size_t length =
        audio_input->samples_per_channel_ * audio_input->num_channels_;
    RTC_CHECK_LE(length, AudioFrame::kMaxDataSizeBytes);
    if (is_muted && previous_frame_muted_) {
      rms_level_.AnalyzeMuted(length);
    } else {
      rms_level_.Analyze(
          rtc::ArrayView<const int16_t>(audio_input->data(), length));
    }
  }
  previous_frame_muted_ = is_muted;

  // Add 10ms of raw (PCM) audio data to the encoder @ 32kHz.

  // The ACM resamples internally.
  audio_input->timestamp_ = _timeStamp;
  // This call will trigger AudioPacketizationCallback::SendData if encoding
  // is done and payload is ready for packetization and transmission.
  // Otherwise, it will return without invoking the callback.
  if (audio_coding_->Add10MsData(*audio_input) < 0) {
    RTC_DLOG(LS_ERROR) << "ACM::Add10MsData() failed.";
    return;
  }

  _timeStamp += static_cast<uint32_t>(audio_input->samples_per_channel_);
}

ANAStats ChannelSend::GetANAStatistics() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return audio_coding_->GetANAStats();
}

RtpRtcp* ChannelSend::GetRtpRtcp() const {
  RTC_DCHECK(module_process_thread_checker_.IsCurrent());
  return _rtpRtcpModule.get();
}

int ChannelSend::SetSendRtpHeaderExtension(bool enable,
                                           RTPExtensionType type,
                                           int id) {
  int error = 0;
  _rtpRtcpModule->DeregisterSendRtpHeaderExtension(type);
  if (enable) {
    // TODO(nisse): RtpRtcp::RegisterSendRtpHeaderExtension to take an int
    // argument. Currently it wants an uint8_t.
    error = _rtpRtcpModule->RegisterSendRtpHeaderExtension(
        type, rtc::dchecked_cast<uint8_t>(id));
  }
  return error;
}

int64_t ChannelSend::GetRTT() const {
  if (media_transport_config_.media_transport) {
    // GetRTT is generally used in the RTCP codepath, where media transport is
    // not present and so it shouldn't be needed. But it's also invoked in
    // 'GetStats' method, and for now returning media transport RTT here gives
    // us "free" rtt stats for media transport.
    auto target_rate =
        media_transport_config_.media_transport->GetLatestTargetTransferRate();
    if (target_rate.has_value()) {
      return target_rate.value().network_estimate.round_trip_time.ms();
    }

    return 0;
  }
  std::vector<RTCPReportBlock> report_blocks;
  _rtpRtcpModule->RemoteRTCPStat(&report_blocks);

  if (report_blocks.empty()) {
    return 0;
  }

  int64_t rtt = 0;
  int64_t avg_rtt = 0;
  int64_t max_rtt = 0;
  int64_t min_rtt = 0;
  // We don't know in advance the remote ssrc used by the other end's receiver
  // reports, so use the SSRC of the first report block for calculating the RTT.
  if (_rtpRtcpModule->RTT(report_blocks[0].sender_ssrc, &rtt, &avg_rtt,
                          &min_rtt, &max_rtt) != 0) {
    return 0;
  }
  return rtt;
}

void ChannelSend::SetFrameEncryptor(
    rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  encoder_queue_.PostTask([this, frame_encryptor]() mutable {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    frame_encryptor_ = std::move(frame_encryptor);
  });
}

// TODO(sukhanov): Consider moving TargetTransferRate observer to
// AudioSendStream. Since AudioSendStream owns encoder and configures ANA, it
// makes sense to consolidate all rate (and overhead) calculation there.
void ChannelSend::OnTargetTransferRate(TargetTransferRate rate) {
  RTC_DCHECK(media_transport_config_.media_transport);
  OnReceivedRtt(rate.network_estimate.round_trip_time.ms());
}

void ChannelSend::OnReceivedRtt(int64_t rtt_ms) {
  // Invoke audio encoders OnReceivedRtt().
  CallEncoder(
      [rtt_ms](AudioEncoder* encoder) { encoder->OnReceivedRtt(rtt_ms); });
}

}  // namespace

std::unique_ptr<ChannelSendInterface> CreateChannelSend(
    Clock* clock,
    TaskQueueFactory* task_queue_factory,
    ProcessThread* module_process_thread,
    const MediaTransportConfig& media_transport_config,
    OverheadObserver* overhead_observer,
    Transport* rtp_transport,
    RtcpRttStats* rtcp_rtt_stats,
    RtcEventLog* rtc_event_log,
    FrameEncryptorInterface* frame_encryptor,
    const webrtc::CryptoOptions& crypto_options,
    bool extmap_allow_mixed,
    int rtcp_report_interval_ms,
    uint32_t ssrc) {
  return absl::make_unique<ChannelSend>(
      clock, task_queue_factory, module_process_thread, media_transport_config,
      overhead_observer, rtp_transport, rtcp_rtt_stats, rtc_event_log,
      frame_encryptor, crypto_options, extmap_allow_mixed,
      rtcp_report_interval_ms, ssrc);
}

}  // namespace voe
}  // namespace webrtc
