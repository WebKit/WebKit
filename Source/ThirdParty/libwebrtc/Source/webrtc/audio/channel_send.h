/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_CHANNEL_SEND_H_
#define AUDIO_CHANNEL_SEND_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/audio/audio_frame.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/call/transport.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_processing/rms_level.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"

// TODO(solenberg, nisse): This file contains a few NOLINT marks, to silence
// warnings about use of unsigned short, and non-const reference arguments.
// These need cleanup, in a separate cl.

namespace rtc {
class TimestampWrapAroundHandler;
}

namespace webrtc {

class FrameEncryptorInterface;
class PacketRouter;
class ProcessThread;
class RateLimiter;
class RtcEventLog;
class RtpRtcp;
class RtpTransportControllerSendInterface;

struct SenderInfo;

struct CallSendStatistics {
  int64_t rttMs;
  size_t bytesSent;
  int packetsSent;
};

// See section 6.4.2 in http://www.ietf.org/rfc/rfc3550.txt for details.
struct ReportBlock {
  uint32_t sender_SSRC;  // SSRC of sender
  uint32_t source_SSRC;
  uint8_t fraction_lost;
  int32_t cumulative_num_packets_lost;
  uint32_t extended_highest_sequence_number;
  uint32_t interarrival_jitter;
  uint32_t last_SR_timestamp;
  uint32_t delay_since_last_SR;
};

namespace voe {

class RtpPacketSenderProxy;
class TransportFeedbackProxy;
class TransportSequenceNumberProxy;
class VoERtcpObserver;

// Helper class to simplify locking scheme for members that are accessed from
// multiple threads.
// Example: a member can be set on thread T1 and read by an internal audio
// thread T2. Accessing the member via this class ensures that we are
// safe and also avoid TSan v2 warnings.
class ChannelSendState {
 public:
  struct State {
    bool sending = false;
  };

  ChannelSendState() {}
  virtual ~ChannelSendState() {}

  void Reset() {
    rtc::CritScope lock(&lock_);
    state_ = State();
  }

  State Get() const {
    rtc::CritScope lock(&lock_);
    return state_;
  }

  void SetSending(bool enable) {
    rtc::CritScope lock(&lock_);
    state_.sending = enable;
  }

 private:
  rtc::CriticalSection lock_;
  State state_;
};

class ChannelSend
    : public Transport,
      public AudioPacketizationCallback,  // receive encoded packets from the
                                          // ACM
      public OverheadObserver {
 public:
  // TODO(nisse): Make OnUplinkPacketLossRate public, and delete friend
  // declaration.
  friend class VoERtcpObserver;

  ChannelSend(rtc::TaskQueue* encoder_queue,
              ProcessThread* module_process_thread,
              RtcpRttStats* rtcp_rtt_stats,
              RtcEventLog* rtc_event_log,
              FrameEncryptorInterface* frame_encryptor);

  virtual ~ChannelSend();

  // Send using this encoder, with this payload type.
  bool SetEncoder(int payload_type, std::unique_ptr<AudioEncoder> encoder);
  void ModifyEncoder(
      rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier);

  // API methods

  // VoEBase
  int32_t StartSend();
  void StopSend();

  // Codecs
  void SetBitRate(int bitrate_bps, int64_t probing_interval_ms);
  bool EnableAudioNetworkAdaptor(const std::string& config_string);
  void DisableAudioNetworkAdaptor();

  // TODO(nisse): Modifies decoder, but not used?
  void SetReceiverFrameLengthRange(int min_frame_length_ms,
                                   int max_frame_length_ms);

  // Network
  void RegisterTransport(Transport* transport);
  // TODO(nisse, solenberg): Delete when VoENetwork is deleted.
  int32_t ReceivedRTCPPacket(const uint8_t* data, size_t length);

  // Muting, Volume and Level.
  void SetInputMute(bool enable);

  // Stats.
  ANAStats GetANAStatistics() const;

  // Used by AudioSendStream.
  RtpRtcp* GetRtpRtcp() const;

  // DTMF.
  int SendTelephoneEventOutband(int event, int duration_ms);
  int SetSendTelephoneEventPayloadType(int payload_type, int payload_frequency);

  // RTP+RTCP
  int SetLocalSSRC(unsigned int ssrc);

  void SetMid(const std::string& mid, int extension_id);
  int SetSendAudioLevelIndicationStatus(bool enable, unsigned char id);
  void EnableSendTransportSequenceNumber(int id);

  void RegisterSenderCongestionControlObjects(
      RtpTransportControllerSendInterface* transport,
      RtcpBandwidthObserver* bandwidth_observer);
  void ResetSenderCongestionControlObjects();
  void SetRTCPStatus(bool enable);
  int SetRTCP_CNAME(const char cName[256]);
  int GetRemoteRTCPReportBlocks(std::vector<ReportBlock>* report_blocks);
  int GetRTPStatistics(CallSendStatistics& stats);  // NOLINT
  void SetNACKStatus(bool enable, int maxNumberOfPackets);

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

  int PreferredSampleRate() const;

  bool Sending() const { return channel_state_.Get().sending; }
  RtpRtcp* RtpRtcpModulePtr() const { return _rtpRtcpModule.get(); }

  // ProcessAndEncodeAudio() posts a task on the shared encoder task queue,
  // which in turn calls (on the queue) ProcessAndEncodeAudioOnTaskQueue() where
  // the actual processing of the audio takes place. The processing mainly
  // consists of encoding and preparing the result for sending by adding it to a
  // send queue.
  // The main reason for using a task queue here is to release the native,
  // OS-specific, audio capture thread as soon as possible to ensure that it
  // can go back to sleep and be prepared to deliver an new captured audio
  // packet.
  void ProcessAndEncodeAudio(std::unique_ptr<AudioFrame> audio_frame);

  void SetTransportOverhead(size_t transport_overhead_per_packet);

  // From OverheadObserver in the RTP/RTCP module
  void OnOverheadChanged(size_t overhead_bytes_per_packet) override;

  // The existence of this function alongside OnUplinkPacketLossRate is
  // a compromise. We want the encoder to be agnostic of the PLR source, but
  // we also don't want it to receive conflicting information from TWCC and
  // from RTCP-XR.
  void OnTwccBasedUplinkPacketLossRate(float packet_loss_rate);

  void OnRecoverableUplinkPacketLossRate(float recoverable_packet_loss_rate);

  int64_t GetRTT() const;

  // E2EE Custom Audio Frame Encryption
  void SetFrameEncryptor(FrameEncryptorInterface* frame_encryptor);

 private:
  class ProcessAndEncodeAudioTask;

  void Init();
  void Terminate();

  void OnUplinkPacketLossRate(float packet_loss_rate);
  bool InputMute() const;

  int ResendPackets(const uint16_t* sequence_numbers, int length);

  int SetSendRtpHeaderExtension(bool enable,
                                RTPExtensionType type,
                                unsigned char id);

  void UpdateOverheadForEncoder()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(overhead_per_packet_lock_);

  int GetRtpTimestampRateHz() const;

  // Called on the encoder task queue when a new input audio frame is ready
  // for encoding.
  void ProcessAndEncodeAudioOnTaskQueue(AudioFrame* audio_input);

  rtc::CriticalSection _callbackCritSect;
  rtc::CriticalSection volume_settings_critsect_;

  ChannelSendState channel_state_;

  RtcEventLog* const event_log_;

  std::unique_ptr<RtpRtcp> _rtpRtcpModule;

  std::unique_ptr<AudioCodingModule> audio_coding_;
  uint32_t _timeStamp RTC_GUARDED_BY(encoder_queue_);

  uint16_t send_sequence_number_;

  // uses
  ProcessThread* _moduleProcessThreadPtr;
  Transport* _transportPtr;  // WebRtc socket or external transport
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
  std::unique_ptr<VoERtcpObserver> rtcp_observer_;

  PacketRouter* packet_router_ = nullptr;
  std::unique_ptr<TransportFeedbackProxy> feedback_observer_proxy_;
  std::unique_ptr<TransportSequenceNumberProxy> seq_num_allocator_proxy_;
  std::unique_ptr<RtpPacketSenderProxy> rtp_packet_sender_proxy_;
  std::unique_ptr<RateLimiter> retransmission_rate_limiter_;

  rtc::ThreadChecker construction_thread_;

  const bool use_twcc_plr_for_ana_;

  rtc::CriticalSection encoder_queue_lock_;
  bool encoder_queue_is_active_ RTC_GUARDED_BY(encoder_queue_lock_) = false;
  rtc::TaskQueue* encoder_queue_ = nullptr;

  // E2EE Audio Frame Encryption
  FrameEncryptorInterface* frame_encryptor_ = nullptr;
};

}  // namespace voe
}  // namespace webrtc

#endif  // AUDIO_CHANNEL_SEND_H_
