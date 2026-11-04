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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/rtp_header_extension_id.h"
#include "api/rtp_packet_sender.h"
#include "api/sequence_checker.h"
#include "modules/rtp_rtcp/include/flexfec_sender.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_header_extension_size.h"
#include "modules/rtp_rtcp/source/rtp_packet_history.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_interface.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RateLimiter;
class RtpPacketToSend;

// Maximum amount of padding in RFC 3550 is 255 bytes.
constexpr size_t kMaxPaddingLength = 255;

// Threading model:
// `RTPSender` is used from two kinds of threads:
// * The worker thread. This is where the object is configured and, when used
//   via `Call`, where it is constructed, where the pacer runs and where RTCP
//   feedback is handled. State that is only used on this sequence, i.e.
//   whether a header extension usable for bandwidth estimation has been
//   registered, is guarded by `worker_checker_`.
//   Methods that access that state must be called on the worker thread.
//   `worker_checker_` is detached at construction and binds to the sequence of
//   the first call to such a method, so the object may be constructed on a
//   different thread.
// * Media threads, such as encoder and frame transformer queues, which call
//   methods needed for packetization, e.g. `AllocatePacket()`,
//   `MaxRtpPacketSize()`, `TimestampOffset()`, `RtxPacketOverhead()`,
//   `FecOrPaddingPacketMaxRtpHeaderLength()` and `EnqueuePackets()`. Note that
//   these threads do not necessarily form a single sequence.
// State that is accessed from both kinds of threads is guarded by
// `send_mutex_`. State that is set at construction and doesn't change
// afterwards, such as the timestamp offset, the MID and the RTX
// configuration, is const and can be read from any thread.
class RTPSender {
 public:
  RTPSender(const Environment& env,
            const RtpRtcpInterface::Configuration& config,
            RtpPacketHistory* packet_history,
            RtpPacketSender* packet_sender);

  RTPSender(const RTPSender&) = delete;
  RTPSender& operator=(const RTPSender&) = delete;

  ~RTPSender();

  void SetSendingMediaStatus(bool enabled) RTC_LOCKS_EXCLUDED(send_mutex_);
  bool SendingMedia() const RTC_LOCKS_EXCLUDED(send_mutex_);
  bool IsAudioConfigured() const { return audio_configured_; }

  uint32_t TimestampOffset() const { return timestamp_offset_; }

  void SetMaxRtpPacketSize(size_t max_packet_size)
      RTC_LOCKS_EXCLUDED(send_mutex_);

  void SetExtmapAllowMixed(bool extmap_allow_mixed)
      RTC_LOCKS_EXCLUDED(send_mutex_);

  // RTP header extension
  bool RegisterRtpHeaderExtension(absl::string_view uri,
                                  RtpHeaderExtensionId id)
      RTC_LOCKS_EXCLUDED(send_mutex_);
  void DeregisterRtpHeaderExtension(absl::string_view uri)
      RTC_LOCKS_EXCLUDED(send_mutex_);

  bool SupportsPadding() const RTC_LOCKS_EXCLUDED(send_mutex_);
  bool SupportsRtxPayloadPadding() const RTC_LOCKS_EXCLUDED(send_mutex_);

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t target_size_bytes,
      bool media_has_been_sent,
      bool can_send_padding_on_media_ssrc) RTC_LOCKS_EXCLUDED(send_mutex_);

  // NACK.
  void OnReceivedNack(const std::vector<uint16_t>& nack_sequence_numbers,
                      int64_t avg_rtt) RTC_LOCKS_EXCLUDED(send_mutex_);

  int32_t ReSendPacket(uint16_t packet_id) RTC_LOCKS_EXCLUDED(send_mutex_);

  // ACK.
  void OnReceivedAckOnSsrc(int64_t extended_highest_sequence_number)
      RTC_LOCKS_EXCLUDED(send_mutex_);
  void OnReceivedAckOnRtxSsrc(int64_t extended_highest_sequence_number)
      RTC_LOCKS_EXCLUDED(send_mutex_);

  // RTX.
  int RtxStatus() const { return rtx_; }
  std::optional<uint32_t> RtxSsrc() const { return rtx_ssrc_; }
  // Returns expected size difference between an RTX packet and media packet
  // that RTX packet is created from. Returns 0 if RTX is disabled.
  size_t RtxPacketOverhead() const RTC_LOCKS_EXCLUDED(send_mutex_);

  // Size info for header extensions used by FEC packets.
  static std::span<const RtpExtensionSize> FecExtensionSizes();

  // Size info for header extensions used by video packets.
  static std::span<const RtpExtensionSize> VideoExtensionSizes();

  // Size info for header extensions used by audio packets.
  static std::span<const RtpExtensionSize> AudioExtensionSizes();

  // Create empty packet, fills ssrc, csrcs and reserve place for header
  // extensions RtpSender updates before sending.
  std::unique_ptr<RtpPacketToSend> AllocatePacket(
      std::span<const uint32_t> csrcs = {}) RTC_LOCKS_EXCLUDED(send_mutex_);

  // Maximum header overhead per fec/padding packet.
  size_t FecOrPaddingPacketMaxRtpHeaderLength() const
      RTC_LOCKS_EXCLUDED(send_mutex_);
  // Expected header overhead per media packet.
  size_t ExpectedPerPacketOverhead() const RTC_LOCKS_EXCLUDED(send_mutex_);
  // Including RTP headers.
  size_t MaxRtpPacketSize() const RTC_LOCKS_EXCLUDED(send_mutex_);

  uint32_t SSRC() const { return ssrc_; }

  std::string Rid() const { return rid_; }

  std::optional<uint32_t> FlexfecSsrc() const { return flexfec_ssrc_; }

  // Pass a set of packets to RtpPacketSender instance, for paced or immediate
  // sending to the network.
  void EnqueuePackets(std::vector<std::unique_ptr<RtpPacketToSend>> packets)
      RTC_LOCKS_EXCLUDED(send_mutex_);

  RtpState GetRtpState() const RTC_LOCKS_EXCLUDED(send_mutex_);
  RtpState GetRtxRtpState() const RTC_LOCKS_EXCLUDED(send_mutex_);

 private:
  // Builds a packet that is sent on the media SSRC.
  std::unique_ptr<RtpPacketToSend> BuildMediaPacket(
      std::span<const uint32_t> csrcs) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(send_mutex_);

  std::unique_ptr<RtpPacketToSend> BuildRtxPacket(
      const RtpPacketToSend& packet);

  // Appends padding packets to `padding_packets` that are built from packets in
  // the packet history and sent on the RTX SSRC. Returns the number of bytes of
  // `target_size_bytes` that are left to generate.
  size_t AppendPayloadPadding(
      size_t target_size_bytes,
      std::vector<std::unique_ptr<RtpPacketToSend>>& padding_packets)
      RTC_LOCKS_EXCLUDED(send_mutex_);

  // Builds a packet with `padding_size` bytes of padding. It is sent on the
  // RTX SSRC if `rtx` is true, or on the media SSRC otherwise.
  std::unique_ptr<RtpPacketToSend> BuildPaddingPacket(bool rtx,
                                                      size_t padding_size) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(send_mutex_);

  // Attaches the MID and RID header extensions to `packet` if needed.
  void AttachMidAndRid(bool rtx, RtpPacketToSend& packet) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(send_mutex_);

  void UpdateHeaderSizes() RTC_EXCLUSIVE_LOCKS_REQUIRED(send_mutex_);

  // Updates the header sizes if `num_csrcs` is larger than the number of CSRCs
  // that any packet has been allocated with so far.
  void UpdateMaxNumCsrcs(size_t num_csrcs)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(send_mutex_);

  Clock* const clock_;

  const bool audio_configured_;

  const uint32_t ssrc_;
  const std::optional<uint32_t> rtx_ssrc_;
  const std::optional<uint32_t> flexfec_ssrc_;

  RtpPacketHistory* const packet_history_;
  RtpPacketSender* const paced_sender_;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_checker_;
  mutable Mutex send_mutex_;

  bool sending_media_ RTC_GUARDED_BY(send_mutex_);
  size_t max_packet_size_ RTC_GUARDED_BY(send_mutex_);

  RtpHeaderExtensionMap rtp_header_extension_map_ RTC_GUARDED_BY(send_mutex_);
  size_t max_media_packet_header_ RTC_GUARDED_BY(send_mutex_);
  size_t max_padding_fec_packet_header_ RTC_GUARDED_BY(send_mutex_);

  // RTP variables
  const uint32_t timestamp_offset_;
  // RID value to send in the RID or RepairedRID header extension.
  const std::string rid_;
  // MID value to send in the MID header extension.
  const std::string mid_;
  // Should we send MID/RID even when ACKed? (see below).
  const bool always_send_mid_and_rid_;
  // Track if any ACK has been received on the SSRC and RTX SSRC to indicate
  // when to stop sending the MID and RID header extensions.
  bool ssrc_has_acked_ RTC_GUARDED_BY(send_mutex_);
  bool rtx_ssrc_has_acked_ RTC_GUARDED_BY(send_mutex_);
  // Maximum number of csrcs this sender is used with.
  size_t max_num_csrcs_ RTC_GUARDED_BY(send_mutex_) = 0;
  // Mapping rtx_payload_type_map_[associated] = rtx.
  const flat_map<int8_t, int8_t> rtx_payload_type_map_;
  // Declared after `rtx_payload_type_map_`, since its initial value depends on
  // the mapping.
  const int rtx_;
  bool supports_bwe_extension_ RTC_GUARDED_BY(worker_checker_);

  RateLimiter* const retransmission_rate_limiter_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
