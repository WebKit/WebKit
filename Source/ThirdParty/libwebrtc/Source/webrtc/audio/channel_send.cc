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
#include "api/crypto/frameencryptorinterface.h"
#include "audio/utility/audio_frame_operations.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/pacing/packet_router.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
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

}  // namespace

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
        cricket::MEDIA_TYPE_AUDIO, payloadSize);
    encrypted_audio_payload.SetSize(max_ciphertext_size);

    // Encrypt the audio payload into the buffer.
    size_t bytes_written = 0;
    int encrypt_status = frame_encryptor_->Encrypt(
        cricket::MEDIA_TYPE_AUDIO, _rtpRtcpModule->SSRC(),
        /*additional_data=*/nullptr,
        rtc::ArrayView<const uint8_t>(payloadData, payloadSize),
        encrypted_audio_payload, &bytes_written);
    if (encrypt_status != 0) {
      RTC_DLOG(LS_ERROR) << "Channel::SendData() failed encrypt audio payload: "
                         << encrypt_status;
      return -1;
    }
    // Resize the buffer to the exact number of bytes actually used.
    encrypted_audio_payload.SetSize(bytes_written);
    // Rewrite the payloadData and size to the new encrypted payload.
    payloadData = encrypted_audio_payload.data();
    payloadSize = encrypted_audio_payload.size();
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
    RTC_DLOG(LS_ERROR)
        << "ChannelSend::SendData() failed to send data to RTP/RTCP module";
    return -1;
  }

  return 0;
}

bool ChannelSend::SendRtp(const uint8_t* data,
                          size_t len,
                          const PacketOptions& options) {
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

int ChannelSend::PreferredSampleRate() const {
  // Return the bigger of playout and receive frequency in the ACM.
  return std::max(audio_coding_->ReceiveFrequency(),
                  audio_coding_->PlayoutFrequency());
}

ChannelSend::ChannelSend(rtc::TaskQueue* encoder_queue,
                         ProcessThread* module_process_thread,
                         RtcpRttStats* rtcp_rtt_stats,
                         RtcEventLog* rtc_event_log,
                         FrameEncryptorInterface* frame_encryptor)
    : event_log_(rtc_event_log),
      _timeStamp(0),  // This is just an offset, RTP module will add it's own
                      // random offset
      send_sequence_number_(0),
      _moduleProcessThreadPtr(module_process_thread),
      _transportPtr(NULL),
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
      frame_encryptor_(frame_encryptor) {
  RTC_DCHECK(module_process_thread);
  RTC_DCHECK(encoder_queue);
  audio_coding_.reset(AudioCodingModule::Create(AudioCodingModule::Config()));

  RtpRtcp::Configuration configuration;
  configuration.audio = true;
  configuration.outgoing_transport = this;
  configuration.overhead_observer = this;
  configuration.bandwidth_callback = rtcp_observer_.get();

  configuration.paced_sender = rtp_packet_sender_proxy_.get();
  configuration.transport_sequence_number_allocator =
      seq_num_allocator_proxy_.get();
  configuration.transport_feedback_callback = feedback_observer_proxy_.get();

  configuration.event_log = event_log_;
  configuration.rtt_stats = rtcp_rtt_stats;
  configuration.retransmission_rate_limiter =
      retransmission_rate_limiter_.get();

  _rtpRtcpModule.reset(RtpRtcp::CreateRtpRtcp(configuration));
  _rtpRtcpModule->SetSendingMediaStatus(false);
  Init();
}

ChannelSend::~ChannelSend() {
  Terminate();
  RTC_DCHECK(!channel_state_.Get().sending);
}

void ChannelSend::Init() {
  channel_state_.Reset();

  // --- Add modules to process thread (for periodic schedulation)
  _moduleProcessThreadPtr->RegisterModule(_rtpRtcpModule.get(), RTC_FROM_HERE);

  // --- ACM initialization
  int error = audio_coding_->InitializeReceiver();
  RTC_DCHECK_EQ(0, error);

  // --- RTP/RTCP module initialization

  // Ensure that RTCP is enabled by default for the created channel.
  // Note that, the module will keep generating RTCP until it is explicitly
  // disabled by the user.
  // After StopListen (when no sockets exists), RTCP packets will no longer
  // be transmitted since the Transport object will then be invalid.
  // RTCP is enabled by default.
  _rtpRtcpModule->SetRTCPStatus(RtcpMode::kCompound);

  // --- Register all permanent callbacks
  error = audio_coding_->RegisterTransportCallback(this);
  RTC_DCHECK_EQ(0, error);
}

void ChannelSend::Terminate() {
  RTC_DCHECK(construction_thread_.CalledOnValidThread());
  // Must be called on the same thread as Init().

  StopSend();

  // The order to safely shutdown modules in a channel is:
  // 1. De-register callbacks in modules
  // 2. De-register modules in process thread
  // 3. Destroy modules
  int error = audio_coding_->RegisterTransportCallback(NULL);
  RTC_DCHECK_EQ(0, error);

  // De-register modules in process thread
  if (_moduleProcessThreadPtr)
    _moduleProcessThreadPtr->DeRegisterModule(_rtpRtcpModule.get());

  // End of modules shutdown
}

int32_t ChannelSend::StartSend() {
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
    RTC_DLOG(LS_ERROR) << "StartSend() RTP/RTCP failed to start sending";
    _rtpRtcpModule->SetSendingMediaStatus(false);
    rtc::CritScope cs(&_callbackCritSect);
    channel_state_.SetSending(false);
    return -1;
  }
  {
    // It is now OK to start posting tasks to the encoder task queue.
    rtc::CritScope cs(&encoder_queue_lock_);
    encoder_queue_is_active_ = true;
  }
  return 0;
}

void ChannelSend::StopSend() {
  if (!channel_state_.Get().sending) {
    return;
  }
  channel_state_.SetSending(false);

  // Post a task to the encoder thread which sets an event when the task is
  // executed. We know that no more encoding tasks will be added to the task
  // queue for this channel since sending is now deactivated. It means that,
  // if we wait for the event to bet set, we know that no more pending tasks
  // exists and it is therfore guaranteed that the task queue will never try
  // to acccess and invalid channel object.
  RTC_DCHECK(encoder_queue_);

  rtc::Event flush(false, false);
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

  audio_coding_->SetEncoder(std::move(encoder));
  return true;
}

void ChannelSend::ModifyEncoder(
    rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier) {
  audio_coding_->ModifyEncoder(modifier);
}

void ChannelSend::SetBitRate(int bitrate_bps, int64_t probing_interval_ms) {
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->OnReceivedUplinkBandwidth(bitrate_bps, probing_interval_ms);
    }
  });
  retransmission_rate_limiter_->SetMaxRate(bitrate_bps);
}

void ChannelSend::OnTwccBasedUplinkPacketLossRate(float packet_loss_rate) {
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

bool ChannelSend::EnableAudioNetworkAdaptor(const std::string& config_string) {
  bool success = false;
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      success =
          (*encoder)->EnableAudioNetworkAdaptor(config_string, event_log_);
    }
  });
  return success;
}

void ChannelSend::DisableAudioNetworkAdaptor() {
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder)
      (*encoder)->DisableAudioNetworkAdaptor();
  });
}

void ChannelSend::SetReceiverFrameLengthRange(int min_frame_length_ms,
                                              int max_frame_length_ms) {
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->SetReceiverFrameLengthRange(min_frame_length_ms,
                                              max_frame_length_ms);
    }
  });
}

void ChannelSend::RegisterTransport(Transport* transport) {
  rtc::CritScope cs(&_callbackCritSect);
  _transportPtr = transport;
}

int32_t ChannelSend::ReceivedRTCPPacket(const uint8_t* data, size_t length) {
  // Deliver RTCP packet to RTP/RTCP module for parsing
  _rtpRtcpModule->IncomingRtcpPacket(data, length);

  int64_t rtt = GetRTT();
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

  return 0;
}

void ChannelSend::SetInputMute(bool enable) {
  rtc::CritScope cs(&volume_settings_critsect_);
  input_mute_ = enable;
}

bool ChannelSend::InputMute() const {
  rtc::CritScope cs(&volume_settings_critsect_);
  return input_mute_;
}

int ChannelSend::SendTelephoneEventOutband(int event, int duration_ms) {
  RTC_DCHECK_LE(0, event);
  RTC_DCHECK_GE(255, event);
  RTC_DCHECK_LE(0, duration_ms);
  RTC_DCHECK_GE(65535, duration_ms);
  if (!Sending()) {
    return -1;
  }
  if (_rtpRtcpModule->SendTelephoneEventOutband(
          event, duration_ms, kTelephoneEventAttenuationdB) != 0) {
    RTC_DLOG(LS_ERROR) << "SendTelephoneEventOutband() failed to send event";
    return -1;
  }
  return 0;
}

int ChannelSend::SetSendTelephoneEventPayloadType(int payload_type,
                                                  int payload_frequency) {
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
      return -1;
    }
  }
  return 0;
}

int ChannelSend::SetLocalSSRC(unsigned int ssrc) {
  if (channel_state_.Get().sending) {
    RTC_DLOG(LS_ERROR) << "SetLocalSSRC() already sending";
    return -1;
  }
  _rtpRtcpModule->SetSSRC(ssrc);
  return 0;
}

void ChannelSend::SetMid(const std::string& mid, int extension_id) {
  int ret = SetSendRtpHeaderExtension(true, kRtpExtensionMid, extension_id);
  RTC_DCHECK_EQ(0, ret);
  _rtpRtcpModule->SetMid(mid);
}

int ChannelSend::SetSendAudioLevelIndicationStatus(bool enable,
                                                   unsigned char id) {
  _includeAudioLevelIndication = enable;
  return SetSendRtpHeaderExtension(enable, kRtpExtensionAudioLevel, id);
}

void ChannelSend::EnableSendTransportSequenceNumber(int id) {
  int ret =
      SetSendRtpHeaderExtension(true, kRtpExtensionTransportSequenceNumber, id);
  RTC_DCHECK_EQ(0, ret);
}

void ChannelSend::RegisterSenderCongestionControlObjects(
    RtpTransportControllerSendInterface* transport,
    RtcpBandwidthObserver* bandwidth_observer) {
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
  RTC_DCHECK(packet_router_);
  _rtpRtcpModule->SetStorePacketsStatus(false, 600);
  rtcp_observer_->SetBandwidthObserver(nullptr);
  feedback_observer_proxy_->SetTransportFeedbackObserver(nullptr);
  seq_num_allocator_proxy_->SetSequenceNumberAllocator(nullptr);
  packet_router_->RemoveSendRtpModule(_rtpRtcpModule.get());
  packet_router_ = nullptr;
  rtp_packet_sender_proxy_->SetPacketSender(nullptr);
}

void ChannelSend::SetRTCPStatus(bool enable) {
  _rtpRtcpModule->SetRTCPStatus(enable ? RtcpMode::kCompound : RtcpMode::kOff);
}

int ChannelSend::SetRTCP_CNAME(const char cName[256]) {
  if (_rtpRtcpModule->SetCNAME(cName) != 0) {
    RTC_DLOG(LS_ERROR) << "SetRTCP_CNAME() failed to set RTCP CNAME";
    return -1;
  }
  return 0;
}

int ChannelSend::GetRemoteRTCPReportBlocks(
    std::vector<ReportBlock>* report_blocks) {
  if (report_blocks == NULL) {
    RTC_DLOG(LS_ERROR) << "GetRemoteRTCPReportBlock()s invalid report_blocks.";
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
    report_block.sender_SSRC = it->sender_ssrc;
    report_block.source_SSRC = it->source_ssrc;
    report_block.fraction_lost = it->fraction_lost;
    report_block.cumulative_num_packets_lost = it->packets_lost;
    report_block.extended_highest_sequence_number =
        it->extended_highest_sequence_number;
    report_block.interarrival_jitter = it->jitter;
    report_block.last_SR_timestamp = it->last_sender_report_timestamp;
    report_block.delay_since_last_SR = it->delay_since_last_sender_report;
    report_blocks->push_back(report_block);
  }
  return 0;
}

int ChannelSend::GetRTPStatistics(CallSendStatistics& stats) {
  // --- RtcpStatistics

  // --- RTT
  stats.rttMs = GetRTT();

  // --- Data counters

  size_t bytesSent(0);
  uint32_t packetsSent(0);

  if (_rtpRtcpModule->DataCountersRTP(&bytesSent, &packetsSent) != 0) {
    RTC_DLOG(LS_WARNING)
        << "GetRTPStatistics() failed to retrieve RTP datacounters"
        << " => output will not be complete";
  }

  stats.bytesSent = bytesSent;
  stats.packetsSent = packetsSent;

  return 0;
}

void ChannelSend::SetNACKStatus(bool enable, int maxNumberOfPackets) {
  // None of these functions can fail.
  if (enable)
    audio_coding_->EnableNack(maxNumberOfPackets);
  else
    audio_coding_->DisableNack();
}

// Called when we are missing one or more packets.
int ChannelSend::ResendPackets(const uint16_t* sequence_numbers, int length) {
  return _rtpRtcpModule->SendNACK(sequence_numbers, length);
}

void ChannelSend::ProcessAndEncodeAudio(
    std::unique_ptr<AudioFrame> audio_frame) {
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
  return audio_coding_->GetANAStats();
}

RtpRtcp* ChannelSend::GetRtpRtcp() const {
  return _rtpRtcpModule.get();
}

int ChannelSend::SetSendRtpHeaderExtension(bool enable,
                                           RTPExtensionType type,
                                           unsigned char id) {
  int error = 0;
  _rtpRtcpModule->DeregisterSendRtpHeaderExtension(type);
  if (enable) {
    error = _rtpRtcpModule->RegisterSendRtpHeaderExtension(type, id);
  }
  return error;
}

int ChannelSend::GetRtpTimestampRateHz() const {
  const auto format = audio_coding_->ReceiveFormat();
  // Default to the playout frequency if we've not gotten any packets yet.
  // TODO(ossu): Zero clockrate can only happen if we've added an external
  // decoder for a format we don't support internally. Remove once that way of
  // adding decoders is gone!
  return (format && format->clockrate_hz != 0)
             ? format->clockrate_hz
             : audio_coding_->PlayoutFrequency();
}

int64_t ChannelSend::GetRTT() const {
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

void ChannelSend::SetFrameEncryptor(FrameEncryptorInterface* frame_encryptor) {
  rtc::CritScope cs(&encoder_queue_lock_);
  if (encoder_queue_is_active_) {
    encoder_queue_->PostTask([this, frame_encryptor]() {
      this->frame_encryptor_ = frame_encryptor;
    });
  } else {
    frame_encryptor_ = frame_encryptor;
  }
}

}  // namespace voe
}  // namespace webrtc
