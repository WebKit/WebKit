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
#include "api/crypto/frameencryptorinterface.h"
#include "audio/utility/audio_frame_operations.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_processing/rms_level.h"
#include "modules/pacing/packet_router.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace voe {

namespace {

constexpr int64_t kMaxRetransmissionWindowMs = 1000;
constexpr int64_t kMinRetransmissionWindowMs = 30;

MediaTransportEncodedAudioFrame::FrameType
MediaTransportFrameTypeForWebrtcFrameType(webrtc::FrameType frame_type) {
  switch (frame_type) {
    case kAudioFrameSpeech:
      return MediaTransportEncodedAudioFrame::FrameType::kSpeech;
      break;

    case kAudioFrameCN:
      return MediaTransportEncodedAudioFrame::FrameType::
          kDiscontinuousTransmission;
      break;

    default:
      RTC_CHECK(false) << "Unexpected frame type=" << frame_type;
      break;
  }
}

class RtpPacketSenderProxy;
class TransportFeedbackProxy;
class TransportSequenceNumberProxy;
class VoERtcpObserver;

class ChannelSend
    : public ChannelSendInterface,
      public Transport,
      public OverheadObserver,
      public AudioPacketizationCallback,  // receive encoded packets from the
                                          // ACM
      public TargetTransferRateObserver {
 public:
  // TODO(nisse): Make OnUplinkPacketLossRate public, and delete friend
  // declaration.
  friend class VoERtcpObserver;

  ChannelSend(rtc::TaskQueue* encoder_queue,
              ProcessThread* module_process_thread,
              MediaTransportInterface* media_transport,
              Transport* rtp_transport,
              RtcpRttStats* rtcp_rtt_stats,
              RtcEventLog* rtc_event_log,
              FrameEncryptorInterface* frame_encryptor,
              const webrtc::CryptoOptions& crypto_options,
              bool extmap_allow_mixed,
              int rtcp_report_interval_ms);

  ~ChannelSend() override;

  // Send using this encoder, with this payload type.
  bool SetEncoder(int payload_type,
                  std::unique_ptr<AudioEncoder> encoder) override;
  void ModifyEncoder(rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)>
                         modifier) override;

  // API methods
  void StartSend() override;
  void StopSend() override;

  // Codecs
  void OnBitrateAllocation(BitrateAllocationUpdate update) override;
  int GetBitrate() const override;

  // Network
  bool ReceivedRTCPPacket(const uint8_t* data, size_t length) override;

  // Muting, Volume and Level.
  void SetInputMute(bool enable) override;

  // Stats.
  ANAStats GetANAStatistics() const override;

  // Used by AudioSendStream.
  RtpRtcp* GetRtpRtcp() const override;

  // DTMF.
  bool SendTelephoneEventOutband(int event, int duration_ms) override;
  bool SetSendTelephoneEventPayloadType(int payload_type,
                                        int payload_frequency) override;

  // RTP+RTCP
  void SetLocalSSRC(uint32_t ssrc) override;
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

  void SetTransportOverhead(size_t transport_overhead_per_packet) override;

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
  class ProcessAndEncodeAudioTask;

  // From AudioPacketizationCallback in the ACM
  int32_t SendData(FrameType frameType,
                   uint8_t payloadType,
                   uint32_t timeStamp,
                   const uint8_t* payloadData,
                   size_t payloadSize,
                   const RTPFragmentationHeader* fragmentation) override;

  // From Transport (called by the RTP/RTCP module)
  bool SendRtp(const uint8_t* data,
               size_t len,
               const PacketOptions& packet_options) override;
  bool SendRtcp(const uint8_t* data, size_t len) override;

  // From OverheadObserver in the RTP/RTCP module
  void OnOverheadChanged(size_t overhead_bytes_per_packet) override;

  void OnUplinkPacketLossRate(float packet_loss_rate);
  bool InputMute() const;

  int SetSendRtpHeaderExtension(bool enable, RTPExtensionType type, int id);

  void UpdateOverheadForEncoder()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(overhead_per_packet_lock_);

  int32_t SendRtpAudio(FrameType frameType,
                       uint8_t payloadType,
                       uint32_t timeStamp,
                       rtc::ArrayView<const uint8_t> payload,
                       const RTPFragmentationHeader* fragmentation);

  int32_t SendMediaTransportAudio(FrameType frameType,
                                  uint8_t payloadType,
                                  uint32_t timeStamp,
                                  rtc::ArrayView<const uint8_t> payload,
                                  const RTPFragmentationHeader* fragmentation);

  // Return media transport or nullptr if using RTP.
  MediaTransportInterface* media_transport() { return media_transport_; }

  // Called on the encoder task queue when a new input audio frame is ready
  // for encoding.
  void ProcessAndEncodeAudioOnTaskQueue(AudioFrame* audio_input);

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

  rtc::CriticalSection _callbackCritSect;
  rtc::CriticalSection volume_settings_critsect_;

  bool sending_ RTC_GUARDED_BY(&worker_thread_checker_) = false;

  RtcEventLog* const event_log_;

  std::unique_ptr<RtpRtcp> _rtpRtcpModule;

  std::unique_ptr<AudioCodingModule> audio_coding_;
  uint32_t _timeStamp RTC_GUARDED_BY(encoder_queue_);

  uint16_t send_sequence_number_;

  // uses
  ProcessThread* const _moduleProcessThreadPtr;
  Transport* const _transportPtr;  // WebRtc socket or external transport
  RmsLevel rms_level_ RTC_GUARDED_BY(encoder_queue_);
  bool input_mute_ RTC_GUARDED_BY(volume_settings_critsect_);
  bool previous_frame_muted_ RTC_GUARDED_BY(encoder_queue_);
  // VoeRTP_RTCP
  // TODO(henrika): can today be accessed on the main thread and on the
  // task queue; hence potential race.
  bool _includeAudioLevelIndication;
  size_t transport_overhead_per_packet_
      RTC_GUARDED_BY(overhead_per_packet_lock_);
  size_t rtp_overhead_per_packet_ RTC_GUARDED_BY(overhead_per_packet_lock_);
  rtc::CriticalSection overhead_per_packet_lock_;
  // RtcpBandwidthObserver
  const std::unique_ptr<VoERtcpObserver> rtcp_observer_;

  PacketRouter* packet_router_ RTC_GUARDED_BY(&worker_thread_checker_) =
      nullptr;
  const std::unique_ptr<TransportFeedbackProxy> feedback_observer_proxy_;
  const std::unique_ptr<TransportSequenceNumberProxy> seq_num_allocator_proxy_;
  const std::unique_ptr<RtpPacketSenderProxy> rtp_packet_sender_proxy_;
  const std::unique_ptr<RateLimiter> retransmission_rate_limiter_;

  rtc::ThreadChecker construction_thread_;

  const bool use_twcc_plr_for_ana_;

  rtc::CriticalSection encoder_queue_lock_;
  bool encoder_queue_is_active_ RTC_GUARDED_BY(encoder_queue_lock_) = false;
  rtc::TaskQueue* const encoder_queue_ = nullptr;

  MediaTransportInterface* const media_transport_;
  int media_transport_sequence_number_ RTC_GUARDED_BY(encoder_queue_) = 0;

  rtc::CriticalSection media_transport_lock_;
  // Currently set by SetLocalSSRC.
  uint64_t media_transport_channel_id_ RTC_GUARDED_BY(&media_transport_lock_) =
      0;
  // Cache payload type and sampling frequency from most recent call to
  // SetEncoder. Needed to set MediaTransportEncodedAudioFrame metadata, and
  // invalidate on encoder change.
  int media_transport_payload_type_ RTC_GUARDED_BY(&media_transport_lock_);
  int media_transport_sampling_frequency_
      RTC_GUARDED_BY(&media_transport_lock_);

  // E2EE Audio Frame Encryption
  rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor_;
  // E2EE Frame Encryption Options
  const webrtc::CryptoOptions crypto_options_;

  rtc::CriticalSection bitrate_crit_section_;
  int configured_bitrate_bps_ RTC_GUARDED_BY(bitrate_crit_section_) = 0;
};

const int kTelephoneEventAttenuationdB = 10;

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
  void AddPacket(uint32_t ssrc,
                 uint16_t sequence_number,
                 size_t length,
                 const PacedPacketInfo& pacing_info) override {
    RTC_DCHECK(pacer_thread_.CalledOnValidThread());
    rtc::CritScope lock(&crit_);
    if (feedback_observer_)
      feedback_observer_->AddPacket(ssrc, sequence_number, length, pacing_info);
  }

  void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override {
    RTC_DCHECK(network_thread_.CalledOnValidThread());
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
  TransportSequenceNumberAllocator* seq_num_allocator_ RTC_GUARDED_BY(&crit_);
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

  void SetAccountForAudioPackets(bool account_for_audio) override {
    RTC_NOTREACHED();
  }

 private:
  rtc::ThreadChecker thread_checker_;
  rtc::CriticalSection crit_;
  RtpPacketSender* rtp_packet_sender_ RTC_GUARDED_BY(&crit_);
};

class VoERtcpObserver : public RtcpBandwidthObserver {
 public:
  explicit VoERtcpObserver(ChannelSend* owner)
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

class ChannelSend::ProcessAndEncodeAudioTask : public rtc::QueuedTask {
 public:
  ProcessAndEncodeAudioTask(std::unique_ptr<AudioFrame> audio_frame,
                            ChannelSend* channel)
      : audio_frame_(std::move(audio_frame)), channel_(channel) {
    RTC_DCHECK(channel_);
  }

 private:
  bool Run() override {
    RTC_DCHECK_RUN_ON(channel_->encoder_queue_);
    channel_->ProcessAndEncodeAudioOnTaskQueue(audio_frame_.get());
    return true;
  }

  std::unique_ptr<AudioFrame> audio_frame_;
  ChannelSend* const channel_;
};

int32_t ChannelSend::SendData(FrameType frameType,
                              uint8_t payloadType,
                              uint32_t timeStamp,
                              const uint8_t* payloadData,
                              size_t payloadSize,
                              const RTPFragmentationHeader* fragmentation) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  rtc::ArrayView<const uint8_t> payload(payloadData, payloadSize);

  if (media_transport() != nullptr) {
    return SendMediaTransportAudio(frameType, payloadType, timeStamp, payload,
                                   fragmentation);
  } else {
    return SendRtpAudio(frameType, payloadType, timeStamp, payload,
                        fragmentation);
  }
}

int32_t ChannelSend::SendRtpAudio(FrameType frameType,
                                  uint8_t payloadType,
                                  uint32_t timeStamp,
                                  rtc::ArrayView<const uint8_t> payload,
                                  const RTPFragmentationHeader* fragmentation) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  if (_includeAudioLevelIndication) {
    // Store current audio level in the RTP/RTCP module.
    // The level will be used in combination with voice-activity state
    // (frameType) to add an RTP header extension
    _rtpRtcpModule->SetAudioLevel(rms_level_.Average());
  }

  // E2EE Custom Audio Frame Encryption (This is optional).
  // Keep this buffer around for the lifetime of the send call.
  rtc::Buffer encrypted_audio_payload;
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
      RTC_DLOG(LS_ERROR) << "Channel::SendData() failed encrypt audio payload: "
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

  // Push data from ACM to RTP/RTCP-module to deliver audio frame for
  // packetization.
  // This call will trigger Transport::SendPacket() from the RTP/RTCP module.
  if (!_rtpRtcpModule->SendOutgoingData((FrameType&)frameType, payloadType,
                                        timeStamp,
                                        // Leaving the time when this frame was
                                        // received from the capture device as
                                        // undefined for voice for now.
                                        -1, payload.data(), payload.size(),
                                        fragmentation, nullptr, nullptr)) {
    RTC_DLOG(LS_ERROR)
        << "ChannelSend::SendData() failed to send data to RTP/RTCP module";
    return -1;
  }

  return 0;
}

int32_t ChannelSend::SendMediaTransportAudio(
    FrameType frameType,
    uint8_t payloadType,
    uint32_t timeStamp,
    rtc::ArrayView<const uint8_t> payload,
    const RTPFragmentationHeader* fragmentation) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
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
  const MediaTransportEncodedAudioFrame frame(
      /*sampling_rate_hz=*/sampling_rate_hz,

      // TODO(nisse): Timestamp and sample index are the same for all supported
      // audio codecs except G722. Refactor audio coding module to only use
      // sample index, and leave translation to RTP time, when needed, for
      // RTP-specific code.
      /*starting_sample_index=*/timeStamp,

      // Sample count isn't conveniently available from the AudioCodingModule,
      // and needs some refactoring to wire up in a good way. For now, left as
      // zero.
      /*sample_count=*/0,

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

bool ChannelSend::SendRtp(const uint8_t* data,
                          size_t len,
                          const PacketOptions& options) {
  // We should not be sending RTP packets if media transport is available.
  RTC_CHECK(!media_transport());

  rtc::CritScope cs(&_callbackCritSect);

  if (_transportPtr == NULL) {
    RTC_DLOG(LS_ERROR)
        << "ChannelSend::SendPacket() failed to send RTP packet due to"
        << " invalid transport object";
    return false;
  }

  if (!_transportPtr->SendRtp(data, len, options)) {
    RTC_DLOG(LS_ERROR) << "ChannelSend::SendPacket() RTP transmission failed";
    return false;
  }
  return true;
}

bool ChannelSend::SendRtcp(const uint8_t* data, size_t len) {
  rtc::CritScope cs(&_callbackCritSect);
  if (_transportPtr == NULL) {
    RTC_DLOG(LS_ERROR)
        << "ChannelSend::SendRtcp() failed to send RTCP packet due to"
        << " invalid transport object";
    return false;
  }

  int n = _transportPtr->SendRtcp(data, len);
  if (n < 0) {
    RTC_DLOG(LS_ERROR) << "ChannelSend::SendRtcp() transmission failed";
    return false;
  }
  return true;
}

ChannelSend::ChannelSend(rtc::TaskQueue* encoder_queue,
                         ProcessThread* module_process_thread,
                         MediaTransportInterface* media_transport,
                         Transport* rtp_transport,
                         RtcpRttStats* rtcp_rtt_stats,
                         RtcEventLog* rtc_event_log,
                         FrameEncryptorInterface* frame_encryptor,
                         const webrtc::CryptoOptions& crypto_options,
                         bool extmap_allow_mixed,
                         int rtcp_report_interval_ms)
    : event_log_(rtc_event_log),
      _timeStamp(0),  // This is just an offset, RTP module will add it's own
                      // random offset
      send_sequence_number_(0),
      _moduleProcessThreadPtr(module_process_thread),
      _transportPtr(rtp_transport),
      input_mute_(false),
      previous_frame_muted_(false),
      _includeAudioLevelIndication(false),
      transport_overhead_per_packet_(0),
      rtp_overhead_per_packet_(0),
      rtcp_observer_(new VoERtcpObserver(this)),
      feedback_observer_proxy_(new TransportFeedbackProxy()),
      seq_num_allocator_proxy_(new TransportSequenceNumberProxy()),
      rtp_packet_sender_proxy_(new RtpPacketSenderProxy()),
      retransmission_rate_limiter_(new RateLimiter(Clock::GetRealTimeClock(),
                                                   kMaxRetransmissionWindowMs)),
      use_twcc_plr_for_ana_(
          webrtc::field_trial::FindFullName("UseTwccPlrForAna") == "Enabled"),
      encoder_queue_(encoder_queue),
      media_transport_(media_transport),
      frame_encryptor_(frame_encryptor),
      crypto_options_(crypto_options) {
  RTC_DCHECK(module_process_thread);
  RTC_DCHECK(encoder_queue);
  module_process_thread_checker_.DetachFromThread();

  audio_coding_.reset(AudioCodingModule::Create(AudioCodingModule::Config()));

  RtpRtcp::Configuration configuration;

  // We gradually remove codepaths that depend on RTP when using media
  // transport. All of this logic should be moved to the future
  // RTPMediaTransport. In this case it means that overhead and bandwidth
  // observers should not be called when using media transport.
  if (!media_transport_) {
    configuration.overhead_observer = this;
    configuration.bandwidth_callback = rtcp_observer_.get();
    configuration.transport_feedback_callback = feedback_observer_proxy_.get();
  }

  configuration.audio = true;
  configuration.outgoing_transport = this;

  configuration.paced_sender = rtp_packet_sender_proxy_.get();
  configuration.transport_sequence_number_allocator =
      seq_num_allocator_proxy_.get();

  configuration.event_log = event_log_;
  configuration.rtt_stats = rtcp_rtt_stats;
  configuration.retransmission_rate_limiter =
      retransmission_rate_limiter_.get();
  configuration.extmap_allow_mixed = extmap_allow_mixed;
  configuration.rtcp_report_interval_ms = rtcp_report_interval_ms;

  _rtpRtcpModule.reset(RtpRtcp::CreateRtpRtcp(configuration));
  _rtpRtcpModule->SetSendingMediaStatus(false);

  // We want to invoke the 'TargetRateObserver' and |OnOverheadChanged|
  // callbacks after the audio_coding_ is fully initialized.
  if (media_transport_) {
    RTC_DLOG(LS_INFO) << "Setting media_transport_ rate observers.";
    media_transport_->AddTargetTransferRateObserver(this);
    OnOverheadChanged(media_transport_->GetAudioPacketOverhead());
  } else {
    RTC_DLOG(LS_INFO) << "Not setting media_transport_ rate observers.";
  }

  _moduleProcessThreadPtr->RegisterModule(_rtpRtcpModule.get(), RTC_FROM_HERE);

  // Ensure that RTCP is enabled by default for the created channel.
  // Note that, the module will keep generating RTCP until it is explicitly
  // disabled by the user.
  // After StopListen (when no sockets exists), RTCP packets will no longer
  // be transmitted since the Transport object will then be invalid.
  // RTCP is enabled by default.
  _rtpRtcpModule->SetRTCPStatus(RtcpMode::kCompound);

  int error = audio_coding_->RegisterTransportCallback(this);
  RTC_DCHECK_EQ(0, error);
}

ChannelSend::~ChannelSend() {
  RTC_DCHECK(construction_thread_.CalledOnValidThread());

  if (media_transport_) {
    media_transport_->RemoveTargetTransferRateObserver(this);
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

  // Resume the previous sequence number which was reset by StopSend(). This
  // needs to be done before |sending| is set to true on the RTP/RTCP module.
  if (send_sequence_number_) {
    _rtpRtcpModule->SetSequenceNumber(send_sequence_number_);
  }
  _rtpRtcpModule->SetSendingMediaStatus(true);
  int ret = _rtpRtcpModule->SetSendingStatus(true);
  RTC_DCHECK_EQ(0, ret);
  {
    // It is now OK to start posting tasks to the encoder task queue.
    rtc::CritScope cs(&encoder_queue_lock_);
    encoder_queue_is_active_ = true;
  }
}

void ChannelSend::StopSend() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!sending_) {
    return;
  }
  sending_ = false;

  // Post a task to the encoder thread which sets an event when the task is
  // executed. We know that no more encoding tasks will be added to the task
  // queue for this channel since sending is now deactivated. It means that,
  // if we wait for the event to bet set, we know that no more pending tasks
  // exists and it is therfore guaranteed that the task queue will never try
  // to acccess and invalid channel object.
  RTC_DCHECK(encoder_queue_);

  rtc::Event flush;
  {
    // Clear |encoder_queue_is_active_| under lock to prevent any other tasks
    // than this final "flush task" to be posted on the queue.
    rtc::CritScope cs(&encoder_queue_lock_);
    encoder_queue_is_active_ = false;
    encoder_queue_->PostTask([&flush]() { flush.Set(); });
  }
  flush.Wait(rtc::Event::kForever);

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
    RTC_DLOG(LS_ERROR) << "StartSend() RTP/RTCP failed to stop sending";
  }
  _rtpRtcpModule->SetSendingMediaStatus(false);
}

bool ChannelSend::SetEncoder(int payload_type,
                             std::unique_ptr<AudioEncoder> encoder) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK_GE(payload_type, 0);
  RTC_DCHECK_LE(payload_type, 127);
  // TODO(ossu): Make CodecInsts up, for now: one for the RTP/RTCP module and
  // one for for us to keep track of sample rate and number of channels, etc.

  // The RTP/RTCP module needs to know the RTP timestamp rate (i.e. clockrate)
  // as well as some other things, so we collect this info and send it along.
  CodecInst rtp_codec;
  rtp_codec.pltype = payload_type;
  strncpy(rtp_codec.plname, "audio", sizeof(rtp_codec.plname));
  rtp_codec.plname[sizeof(rtp_codec.plname) - 1] = 0;
  // Seems unclear if it should be clock rate or sample rate. CodecInst
  // supposedly carries the sample rate, but only clock rate seems sensible to
  // send to the RTP/RTCP module.
  rtp_codec.plfreq = encoder->RtpTimestampRateHz();
  rtp_codec.pacsize = rtc::CheckedDivExact(
      static_cast<int>(encoder->Max10MsFramesInAPacket() * rtp_codec.plfreq),
      100);
  rtp_codec.channels = encoder->NumChannels();
  rtp_codec.rate = 0;

  if (_rtpRtcpModule->RegisterSendPayload(rtp_codec) != 0) {
    _rtpRtcpModule->DeRegisterSendPayload(payload_type);
    if (_rtpRtcpModule->RegisterSendPayload(rtp_codec) != 0) {
      RTC_DLOG(LS_ERROR)
          << "SetEncoder() failed to register codec to RTP/RTCP module";
      return false;
    }
  }

  if (media_transport_) {
    rtc::CritScope cs(&media_transport_lock_);
    media_transport_payload_type_ = payload_type;
    // TODO(nisse): Currently broken for G722, since timestamps passed through
    // encoder use RTP clock rather than sample count, and they differ for G722.
    media_transport_sampling_frequency_ = encoder->RtpTimestampRateHz();
  }
  audio_coding_->SetEncoder(std::move(encoder));
  return true;
}

void ChannelSend::ModifyEncoder(
    rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  audio_coding_->ModifyEncoder(modifier);
}

void ChannelSend::OnBitrateAllocation(BitrateAllocationUpdate update) {
  // This method can be called on the worker thread, module process thread
  // or on a TaskQueue via VideoSendStreamImpl::OnEncoderConfigurationChanged.
  // TODO(solenberg): Figure out a good way to check this or enforce calling
  // rules.
  // RTC_DCHECK(worker_thread_checker_.CalledOnValidThread() ||
  //            module_process_thread_checker_.CalledOnValidThread());
  rtc::CritScope lock(&bitrate_crit_section_);

  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->OnReceivedUplinkAllocation(update);
    }
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
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->OnReceivedUplinkPacketLossFraction(packet_loss_rate);
    }
  });
}

void ChannelSend::OnRecoverableUplinkPacketLossRate(
    float recoverable_packet_loss_rate) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->OnReceivedUplinkRecoverablePacketLossFraction(
          recoverable_packet_loss_rate);
    }
  });
}

void ChannelSend::OnUplinkPacketLossRate(float packet_loss_rate) {
  if (use_twcc_plr_for_ana_)
    return;
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->OnReceivedUplinkPacketLossFraction(packet_loss_rate);
    }
  });
}

// TODO(nisse): Delete always-true return value.
bool ChannelSend::ReceivedRTCPPacket(const uint8_t* data, size_t length) {
  // May be called on either worker thread or network thread.
  if (media_transport_) {
    // Ignore RTCP packets while media transport is used.
    // Those packets should not arrive, but we are seeing occasional packets.
    return 0;
  }

  // Deliver RTCP packet to RTP/RTCP module for parsing
  _rtpRtcpModule->IncomingRtcpPacket(data, length);

  int64_t rtt = GetRTT();
  if (rtt == 0) {
    // Waiting for valid RTT.
    return true;
  }

  int64_t nack_window_ms = rtt;
  if (nack_window_ms < kMinRetransmissionWindowMs) {
    nack_window_ms = kMinRetransmissionWindowMs;
  } else if (nack_window_ms > kMaxRetransmissionWindowMs) {
    nack_window_ms = kMaxRetransmissionWindowMs;
  }
  retransmission_rate_limiter_->SetWindowSize(nack_window_ms);

  OnReceivedRtt(rtt);
  return true;
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
  if (_rtpRtcpModule->SendTelephoneEventOutband(
          event, duration_ms, kTelephoneEventAttenuationdB) != 0) {
    RTC_DLOG(LS_ERROR) << "SendTelephoneEventOutband() failed to send event";
    return false;
  }
  return true;
}

bool ChannelSend::SetSendTelephoneEventPayloadType(int payload_type,
                                                   int payload_frequency) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK_LE(0, payload_type);
  RTC_DCHECK_GE(127, payload_type);
  CodecInst codec = {0};
  codec.pltype = payload_type;
  codec.plfreq = payload_frequency;
  memcpy(codec.plname, "telephone-event", 16);
  if (_rtpRtcpModule->RegisterSendPayload(codec) != 0) {
    _rtpRtcpModule->DeRegisterSendPayload(codec.pltype);
    if (_rtpRtcpModule->RegisterSendPayload(codec) != 0) {
      RTC_DLOG(LS_ERROR)
          << "SetSendTelephoneEventPayloadType() failed to register "
             "send payload type";
      return false;
    }
  }
  return true;
}

void ChannelSend::SetLocalSSRC(uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK(!sending_);

  if (media_transport_) {
    rtc::CritScope cs(&media_transport_lock_);
    media_transport_channel_id_ = ssrc;
  }
  _rtpRtcpModule->SetSSRC(ssrc);
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
  RtpPacketSender* rtp_packet_sender = transport->packet_sender();
  TransportFeedbackObserver* transport_feedback_observer =
      transport->transport_feedback_observer();
  PacketRouter* packet_router = transport->packet_router();

  RTC_DCHECK(rtp_packet_sender);
  RTC_DCHECK(transport_feedback_observer);
  RTC_DCHECK(packet_router);
  RTC_DCHECK(!packet_router_);
  rtcp_observer_->SetBandwidthObserver(bandwidth_observer);
  feedback_observer_proxy_->SetTransportFeedbackObserver(
      transport_feedback_observer);
  seq_num_allocator_proxy_->SetSequenceNumberAllocator(packet_router);
  rtp_packet_sender_proxy_->SetPacketSender(rtp_packet_sender);
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
  seq_num_allocator_proxy_->SetSequenceNumberAllocator(nullptr);
  packet_router_->RemoveSendRtpModule(_rtpRtcpModule.get());
  packet_router_ = nullptr;
  rtp_packet_sender_proxy_->SetPacketSender(nullptr);
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

  size_t bytesSent(0);
  uint32_t packetsSent(0);

  if (_rtpRtcpModule->DataCountersRTP(&bytesSent, &packetsSent) != 0) {
    RTC_DLOG(LS_WARNING)
        << "GetRTPStatistics() failed to retrieve RTP datacounters"
        << " => output will not be complete";
  }

  stats.bytesSent = bytesSent;
  stats.packetsSent = packetsSent;

  return stats;
}

void ChannelSend::ProcessAndEncodeAudio(
    std::unique_ptr<AudioFrame> audio_frame) {
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  // Avoid posting any new tasks if sending was already stopped in StopSend().
  rtc::CritScope cs(&encoder_queue_lock_);
  if (!encoder_queue_is_active_) {
    return;
  }
  // Profile time between when the audio frame is added to the task queue and
  // when the task is actually executed.
  audio_frame->UpdateProfileTimeStamp();
  encoder_queue_->PostTask(std::unique_ptr<rtc::QueuedTask>(
      new ProcessAndEncodeAudioTask(std::move(audio_frame), this)));
}

void ChannelSend::ProcessAndEncodeAudioOnTaskQueue(AudioFrame* audio_input) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  RTC_DCHECK_GT(audio_input->samples_per_channel_, 0);
  RTC_DCHECK_LE(audio_input->num_channels_, 2);

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

void ChannelSend::UpdateOverheadForEncoder() {
  size_t overhead_per_packet =
      transport_overhead_per_packet_ + rtp_overhead_per_packet_;
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->OnReceivedOverhead(overhead_per_packet);
    }
  });
}

void ChannelSend::SetTransportOverhead(size_t transport_overhead_per_packet) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  rtc::CritScope cs(&overhead_per_packet_lock_);
  transport_overhead_per_packet_ = transport_overhead_per_packet;
  UpdateOverheadForEncoder();
}

// TODO(solenberg): Make AudioSendStream an OverheadObserver instead.
void ChannelSend::OnOverheadChanged(size_t overhead_bytes_per_packet) {
  rtc::CritScope cs(&overhead_per_packet_lock_);
  rtp_overhead_per_packet_ = overhead_bytes_per_packet;
  UpdateOverheadForEncoder();
}

ANAStats ChannelSend::GetANAStatistics() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return audio_coding_->GetANAStats();
}

RtpRtcp* ChannelSend::GetRtpRtcp() const {
  RTC_DCHECK(module_process_thread_checker_.CalledOnValidThread());
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
  if (media_transport_) {
    // GetRTT is generally used in the RTCP codepath, where media transport is
    // not present and so it shouldn't be needed. But it's also invoked in
    // 'GetStats' method, and for now returning media transport RTT here gives
    // us "free" rtt stats for media transport.
    auto target_rate = media_transport_->GetLatestTargetTransferRate();
    if (target_rate.has_value()) {
      return target_rate.value().network_estimate.round_trip_time.ms();
    }

    return 0;
  }
  RtcpMode method = _rtpRtcpModule->RTCP();
  if (method == RtcpMode::kOff) {
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
  rtc::CritScope cs(&encoder_queue_lock_);
  if (encoder_queue_is_active_) {
    encoder_queue_->PostTask([this, frame_encryptor]() {
      this->frame_encryptor_ = std::move(frame_encryptor);
    });
  } else {
    frame_encryptor_ = std::move(frame_encryptor);
  }
}

void ChannelSend::OnTargetTransferRate(TargetTransferRate rate) {
  RTC_DCHECK(media_transport_);
  OnReceivedRtt(rate.network_estimate.round_trip_time.ms());
}

void ChannelSend::OnReceivedRtt(int64_t rtt_ms) {
  // Invoke audio encoders OnReceivedRtt().
  audio_coding_->ModifyEncoder(
      [rtt_ms](std::unique_ptr<AudioEncoder>* encoder) {
        if (*encoder) {
          (*encoder)->OnReceivedRtt(rtt_ms);
        }
      });
}

}  // namespace

std::unique_ptr<ChannelSendInterface> CreateChannelSend(
    rtc::TaskQueue* encoder_queue,
    ProcessThread* module_process_thread,
    MediaTransportInterface* media_transport,
    Transport* rtp_transport,
    RtcpRttStats* rtcp_rtt_stats,
    RtcEventLog* rtc_event_log,
    FrameEncryptorInterface* frame_encryptor,
    const webrtc::CryptoOptions& crypto_options,
    bool extmap_allow_mixed,
    int rtcp_report_interval_ms) {
  return absl::make_unique<ChannelSend>(
      encoder_queue, module_process_thread, media_transport, rtp_transport,
      rtcp_rtt_stats, rtc_event_log, frame_encryptor, crypto_options,
      extmap_allow_mixed, rtcp_report_interval_ms);
}

}  // namespace voe
}  // namespace webrtc
