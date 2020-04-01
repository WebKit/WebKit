/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/call/transport.h"
#include "api/transport/webrtc_key_value_config.h"
#include "modules/rtp_rtcp/include/flexfec_sender.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_packet_sender.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_history.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/deprecation.h"
#include "rtc_base/random.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class FrameEncryptorInterface;
class OverheadObserver;
class RateLimiter;
class RtcEventLog;
class RtpPacketToSend;

class RTPSender {
 public:
  RTPSender(const RtpRtcp::Configuration& config,
            RtpPacketHistory* packet_history,
            RtpPacketSender* packet_sender);

  ~RTPSender();

  void SetSendingMediaStatus(bool enabled);
  bool SendingMedia() const;
  bool IsAudioConfigured() const;

  uint32_t TimestampOffset() const;
  void SetTimestampOffset(uint32_t timestamp);

  void SetRid(const std::string& rid);

  void SetMid(const std::string& mid);

  uint16_t SequenceNumber() const;
  void SetSequenceNumber(uint16_t seq);

  void SetCsrcs(const std::vector<uint32_t>& csrcs);

  void SetMaxRtpPacketSize(size_t max_packet_size);

  void SetExtmapAllowMixed(bool extmap_allow_mixed);

  // RTP header extension
  int32_t RegisterRtpHeaderExtension(RTPExtensionType type, uint8_t id);
  bool RegisterRtpHeaderExtension(absl::string_view uri, int id);
  bool IsRtpHeaderExtensionRegistered(RTPExtensionType type) const;
  int32_t DeregisterRtpHeaderExtension(RTPExtensionType type);
  void DeregisterRtpHeaderExtension(absl::string_view uri);

  bool SupportsPadding() const;
  bool SupportsRtxPayloadPadding() const;

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t target_size_bytes,
      bool media_has_been_sent);

  // NACK.
  void OnReceivedNack(const std::vector<uint16_t>& nack_sequence_numbers,
                      int64_t avg_rtt);

  int32_t ReSendPacket(uint16_t packet_id);

  // ACK.
  void OnReceivedAckOnSsrc(int64_t extended_highest_sequence_number);
  void OnReceivedAckOnRtxSsrc(int64_t extended_highest_sequence_number);

  // RTX.
  void SetRtxStatus(int mode);
  int RtxStatus() const;
  absl::optional<uint32_t> RtxSsrc() const { return rtx_ssrc_; }

  void SetRtxPayloadType(int payload_type, int associated_payload_type);

  // Size info for header extensions used by FEC packets.
  static rtc::ArrayView<const RtpExtensionSize> FecExtensionSizes();

  // Size info for header extensions used by video packets.
  static rtc::ArrayView<const RtpExtensionSize> VideoExtensionSizes();

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

  uint32_t SSRC() const { return ssrc_; }

  absl::optional<uint32_t> FlexfecSsrc() const { return flexfec_ssrc_; }

  // Sends packet to |transport_| or to the pacer, depending on configuration.
  // TODO(bugs.webrtc.org/XXX): Remove in favor of EnqueuePackets().
  bool SendToNetwork(std::unique_ptr<RtpPacketToSend> packet);

  // Pass a set of packets to RtpPacketSender instance, for paced or immediate
  // sending to the network.
  void EnqueuePackets(std::vector<std::unique_ptr<RtpPacketToSend>> packets);

  void SetRtpState(const RtpState& rtp_state);
  RtpState GetRtpState() const;
  void SetRtxRtpState(const RtpState& rtp_state);
  RtpState GetRtxRtpState() const;

  int64_t LastTimestampTimeMs() const;

 private:
  std::unique_ptr<RtpPacketToSend> BuildRtxPacket(
      const RtpPacketToSend& packet);

  bool IsFecPacket(const RtpPacketToSend& packet) const;

  Clock* const clock_;
  Random random_ RTC_GUARDED_BY(send_critsect_);

  const bool audio_configured_;

  const uint32_t ssrc_;
  const absl::optional<uint32_t> rtx_ssrc_;
  const absl::optional<uint32_t> flexfec_ssrc_;

  RtpPacketHistory* const packet_history_;
  RtpPacketSender* const paced_sender_;

  rtc::CriticalSection send_critsect_;

  bool sending_media_ RTC_GUARDED_BY(send_critsect_);
  size_t max_packet_size_;

  int8_t last_payload_type_ RTC_GUARDED_BY(send_critsect_);

  RtpHeaderExtensionMap rtp_header_extension_map_
      RTC_GUARDED_BY(send_critsect_);

  // RTP variables
  uint32_t timestamp_offset_ RTC_GUARDED_BY(send_critsect_);
  bool sequence_number_forced_ RTC_GUARDED_BY(send_critsect_);
  uint16_t sequence_number_ RTC_GUARDED_BY(send_critsect_);
  uint16_t sequence_number_rtx_ RTC_GUARDED_BY(send_critsect_);
  // RID value to send in the RID or RepairedRID header extension.
  std::string rid_ RTC_GUARDED_BY(send_critsect_);
  // MID value to send in the MID header extension.
  std::string mid_ RTC_GUARDED_BY(send_critsect_);
  // Should we send MID/RID even when ACKed? (see below).
  const bool always_send_mid_and_rid_;
  // Track if any ACK has been received on the SSRC and RTX SSRC to indicate
  // when to stop sending the MID and RID header extensions.
  bool ssrc_has_acked_ RTC_GUARDED_BY(send_critsect_);
  bool rtx_ssrc_has_acked_ RTC_GUARDED_BY(send_critsect_);
  uint32_t last_rtp_timestamp_ RTC_GUARDED_BY(send_critsect_);
  int64_t capture_time_ms_ RTC_GUARDED_BY(send_critsect_);
  int64_t last_timestamp_time_ms_ RTC_GUARDED_BY(send_critsect_);
  bool last_packet_marker_bit_ RTC_GUARDED_BY(send_critsect_);
  std::vector<uint32_t> csrcs_ RTC_GUARDED_BY(send_critsect_);
  int rtx_ RTC_GUARDED_BY(send_critsect_);
  // Mapping rtx_payload_type_map_[associated] = rtx.
  std::map<int8_t, int8_t> rtx_payload_type_map_ RTC_GUARDED_BY(send_critsect_);
  bool supports_bwe_extension_ RTC_GUARDED_BY(send_critsect_);

  RateLimiter* const retransmission_rate_limiter_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RTPSender);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
