/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "webrtc/api/call/transport.h"
#include "webrtc/base/array_view.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/deprecation.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/random.h"
#include "webrtc/base/rate_statistics.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/flexfec_sender.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/playout_delay_oracle.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_history.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {

class OverheadObserver;
class RateLimiter;
class RtcEventLog;
class RtpPacketToSend;
class RTPSenderAudio;
class RTPSenderVideo;

class RTPSender {
 public:
  RTPSender(bool audio,
            Clock* clock,
            Transport* transport,
            RtpPacketSender* paced_sender,
            // TODO(brandtr): Remove |flexfec_sender| when that is hooked up
            // to PacedSender instead.
            FlexfecSender* flexfec_sender,
            TransportSequenceNumberAllocator* sequence_number_allocator,
            TransportFeedbackObserver* transport_feedback_callback,
            BitrateStatisticsObserver* bitrate_callback,
            FrameCountObserver* frame_count_observer,
            SendSideDelayObserver* send_side_delay_observer,
            RtcEventLog* event_log,
            SendPacketObserver* send_packet_observer,
            RateLimiter* nack_rate_limiter,
            OverheadObserver* overhead_observer);

  ~RTPSender();

  void ProcessBitrate();

  uint16_t ActualSendBitrateKbit() const;

  uint32_t VideoBitrateSent() const;
  uint32_t FecOverheadRate() const;
  uint32_t NackOverheadRate() const;

  int32_t RegisterPayload(const char* payload_name,
                          const int8_t payload_type,
                          const uint32_t frequency,
                          const size_t channels,
                          const uint32_t rate);

  int32_t DeRegisterSendPayload(const int8_t payload_type);

  void SetSendPayloadType(int8_t payload_type);

  void SetSendingMediaStatus(bool enabled);
  bool SendingMedia() const;

  void GetDataCounters(StreamDataCounters* rtp_stats,
                       StreamDataCounters* rtx_stats) const;

  uint32_t TimestampOffset() const;
  void SetTimestampOffset(uint32_t timestamp);

  void SetSSRC(uint32_t ssrc);

  uint16_t SequenceNumber() const;
  void SetSequenceNumber(uint16_t seq);

  void SetCsrcs(const std::vector<uint32_t>& csrcs);

  void SetMaxRtpPacketSize(size_t max_packet_size);

  bool SendOutgoingData(FrameType frame_type,
                        int8_t payload_type,
                        uint32_t timestamp,
                        int64_t capture_time_ms,
                        const uint8_t* payload_data,
                        size_t payload_size,
                        const RTPFragmentationHeader* fragmentation,
                        const RTPVideoHeader* rtp_header,
                        uint32_t* transport_frame_id_out);

  // RTP header extension
  int32_t RegisterRtpHeaderExtension(RTPExtensionType type, uint8_t id);
  bool IsRtpHeaderExtensionRegistered(RTPExtensionType type) const;
  int32_t DeregisterRtpHeaderExtension(RTPExtensionType type);

  bool TimeToSendPacket(uint32_t ssrc,
                        uint16_t sequence_number,
                        int64_t capture_time_ms,
                        bool retransmission,
                        const PacedPacketInfo& pacing_info);
  size_t TimeToSendPadding(size_t bytes, const PacedPacketInfo& pacing_info);

  // NACK.
  int SelectiveRetransmissions() const;
  int SetSelectiveRetransmissions(uint8_t settings);
  void OnReceivedNack(const std::vector<uint16_t>& nack_sequence_numbers,
                      int64_t avg_rtt);

  void SetStorePacketsStatus(bool enable, uint16_t number_to_store);

  bool StorePackets() const;

  int32_t ReSendPacket(uint16_t packet_id, int64_t min_resend_time = 0);

  // Feedback to decide when to stop sending playout delay.
  void OnReceivedRtcpReportBlocks(const ReportBlockList& report_blocks);

  // RTX.
  void SetRtxStatus(int mode);
  int RtxStatus() const;

  uint32_t RtxSsrc() const;
  void SetRtxSsrc(uint32_t ssrc);

  void SetRtxPayloadType(int payload_type, int associated_payload_type);

  // Size info for header extensions used by FEC packets.
  static rtc::ArrayView<const RtpExtensionSize> FecExtensionSizes();

  // Create empty packet, fills ssrc, csrcs and reserve place for header
  // extensions RtpSender updates before sending.
  std::unique_ptr<RtpPacketToSend> AllocatePacket() const;
  // Allocate sequence number for provided packet.
  // Save packet's fields to generate padding that doesn't break media stream.
  // Return false if sending was turned off.
  bool AssignSequenceNumber(RtpPacketToSend* packet);

  // Used for padding and FEC packets only.
  size_t RtpHeaderLength() const;
  uint16_t AllocateSequenceNumber(uint16_t packets_to_send);
  // Including RTP headers.
  size_t MaxRtpPacketSize() const;

  uint32_t SSRC() const;

  rtc::Optional<uint32_t> FlexfecSsrc() const;

  bool SendToNetwork(std::unique_ptr<RtpPacketToSend> packet,
                     StorageType storage,
                     RtpPacketSender::Priority priority);

  // Audio.

  // Send a DTMF tone using RFC 2833 (4733).
  int32_t SendTelephoneEvent(uint8_t key, uint16_t time_ms, uint8_t level);

  // This function is deprecated. It was previously used to determine when it
  // was time to send a DTMF packet in silence (CNG).
  RTC_DEPRECATED int32_t SetAudioPacketSize(uint16_t packet_size_samples);

  // Store the audio level in d_bov for
  // header-extension-for-audio-level-indication.
  int32_t SetAudioLevel(uint8_t level_d_bov);

  RtpVideoCodecTypes VideoCodecType() const;

  uint32_t MaxConfiguredBitrateVideo() const;

  // ULPFEC.
  void SetUlpfecConfig(int red_payload_type, int ulpfec_payload_type);

  bool SetFecParameters(const FecProtectionParams& delta_params,
                        const FecProtectionParams& key_params);

  // Called on update of RTP statistics.
  void RegisterRtpStatisticsCallback(StreamDataCountersCallback* callback);
  StreamDataCountersCallback* GetRtpStatisticsCallback() const;

  uint32_t BitrateSent() const;

  void SetRtpState(const RtpState& rtp_state);
  RtpState GetRtpState() const;
  void SetRtxRtpState(const RtpState& rtp_state);
  RtpState GetRtxRtpState() const;

 protected:
  int32_t CheckPayloadType(int8_t payload_type, RtpVideoCodecTypes* video_type);

 private:
  // Maps capture time in milliseconds to send-side delay in milliseconds.
  // Send-side delay is the difference between transmission time and capture
  // time.
  typedef std::map<int64_t, int> SendDelayMap;

  size_t SendPadData(size_t bytes, const PacedPacketInfo& pacing_info);

  bool PrepareAndSendPacket(std::unique_ptr<RtpPacketToSend> packet,
                            bool send_over_rtx,
                            bool is_retransmit,
                            const PacedPacketInfo& pacing_info);

  // Return the number of bytes sent.  Note that both of these functions may
  // return a larger value that their argument.
  size_t TrySendRedundantPayloads(size_t bytes,
                                  const PacedPacketInfo& pacing_info);

  std::unique_ptr<RtpPacketToSend> BuildRtxPacket(
      const RtpPacketToSend& packet);

  bool SendPacketToNetwork(const RtpPacketToSend& packet,
                           const PacketOptions& options,
                           const PacedPacketInfo& pacing_info);

  void UpdateDelayStatistics(int64_t capture_time_ms, int64_t now_ms);
  void UpdateOnSendPacket(int packet_id,
                          int64_t capture_time_ms,
                          uint32_t ssrc);

  bool UpdateTransportSequenceNumber(RtpPacketToSend* packet,
                                     int* packet_id) const;

  void UpdateRtpStats(const RtpPacketToSend& packet,
                      bool is_rtx,
                      bool is_retransmit);
  bool IsFecPacket(const RtpPacketToSend& packet) const;

  void AddPacketToTransportFeedback(uint16_t packet_id,
                                    const RtpPacketToSend& packet,
                                    const PacedPacketInfo& pacing_info);

  void UpdateRtpOverhead(const RtpPacketToSend& packet);

  Clock* const clock_;
  const int64_t clock_delta_ms_;
  Random random_ GUARDED_BY(send_critsect_);

  const bool audio_configured_;
  const std::unique_ptr<RTPSenderAudio> audio_;
  const std::unique_ptr<RTPSenderVideo> video_;

  RtpPacketSender* const paced_sender_;
  TransportSequenceNumberAllocator* const transport_sequence_number_allocator_;
  TransportFeedbackObserver* const transport_feedback_observer_;
  int64_t last_capture_time_ms_sent_;
  rtc::CriticalSection send_critsect_;

  Transport* transport_;
  bool sending_media_ GUARDED_BY(send_critsect_);

  size_t max_packet_size_;

  int8_t payload_type_ GUARDED_BY(send_critsect_);
  std::map<int8_t, RtpUtility::Payload*> payload_type_map_;

  RtpHeaderExtensionMap rtp_header_extension_map_ GUARDED_BY(send_critsect_);

  // Tracks the current request for playout delay limits from application
  // and decides whether the current RTP frame should include the playout
  // delay extension on header.
  PlayoutDelayOracle playout_delay_oracle_;

  RtpPacketHistory packet_history_;
  // TODO(brandtr): Remove |flexfec_packet_history_| when the FlexfecSender
  // is hooked up to the PacedSender.
  RtpPacketHistory flexfec_packet_history_;

  // Statistics
  rtc::CriticalSection statistics_crit_;
  SendDelayMap send_delays_ GUARDED_BY(statistics_crit_);
  FrameCounts frame_counts_ GUARDED_BY(statistics_crit_);
  StreamDataCounters rtp_stats_ GUARDED_BY(statistics_crit_);
  StreamDataCounters rtx_rtp_stats_ GUARDED_BY(statistics_crit_);
  StreamDataCountersCallback* rtp_stats_callback_ GUARDED_BY(statistics_crit_);
  RateStatistics total_bitrate_sent_ GUARDED_BY(statistics_crit_);
  RateStatistics nack_bitrate_sent_ GUARDED_BY(statistics_crit_);
  FrameCountObserver* const frame_count_observer_;
  SendSideDelayObserver* const send_side_delay_observer_;
  RtcEventLog* const event_log_;
  SendPacketObserver* const send_packet_observer_;
  BitrateStatisticsObserver* const bitrate_callback_;

  // RTP variables
  uint32_t timestamp_offset_ GUARDED_BY(send_critsect_);
  uint32_t remote_ssrc_ GUARDED_BY(send_critsect_);
  bool sequence_number_forced_ GUARDED_BY(send_critsect_);
  uint16_t sequence_number_ GUARDED_BY(send_critsect_);
  uint16_t sequence_number_rtx_ GUARDED_BY(send_critsect_);
  // Must be explicitly set by the application, use of rtc::Optional
  // only to keep track of correct use.
  rtc::Optional<uint32_t> ssrc_ GUARDED_BY(send_critsect_);
  uint32_t last_rtp_timestamp_ GUARDED_BY(send_critsect_);
  int64_t capture_time_ms_ GUARDED_BY(send_critsect_);
  int64_t last_timestamp_time_ms_ GUARDED_BY(send_critsect_);
  bool media_has_been_sent_ GUARDED_BY(send_critsect_);
  bool last_packet_marker_bit_ GUARDED_BY(send_critsect_);
  std::vector<uint32_t> csrcs_ GUARDED_BY(send_critsect_);
  int rtx_ GUARDED_BY(send_critsect_);
  rtc::Optional<uint32_t> ssrc_rtx_ GUARDED_BY(send_critsect_);
  // Mapping rtx_payload_type_map_[associated] = rtx.
  std::map<int8_t, int8_t> rtx_payload_type_map_ GUARDED_BY(send_critsect_);
  size_t rtp_overhead_bytes_per_packet_ GUARDED_BY(send_critsect_);

  RateLimiter* const retransmission_rate_limiter_;
  OverheadObserver* overhead_observer_;

  const bool send_side_bwe_with_overhead_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RTPSender);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
