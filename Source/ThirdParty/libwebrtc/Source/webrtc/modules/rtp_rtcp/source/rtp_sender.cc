/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "api/array_view.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/time_util.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

namespace {
// Max in the RFC 3550 is 255 bytes, we limit it to be modulus 32 for SRTP.
constexpr size_t kMaxPaddingLength = 224;
constexpr size_t kMinAudioPaddingLength = 50;
constexpr size_t kRtpHeaderLength = 12;
constexpr uint16_t kMaxInitRtpSeqNumber = 32767;  // 2^15 -1.
constexpr uint32_t kTimestampTicksPerMs = 90;

// Min size needed to get payload padding from packet history.
constexpr int kMinPayloadPaddingBytes = 50;

template <typename Extension>
constexpr RtpExtensionSize CreateExtensionSize() {
  return {Extension::kId, Extension::kValueSizeBytes};
}

template <typename Extension>
constexpr RtpExtensionSize CreateMaxExtensionSize() {
  return {Extension::kId, Extension::kMaxValueSizeBytes};
}

// Size info for header extensions that might be used in padding or FEC packets.
constexpr RtpExtensionSize kFecOrPaddingExtensionSizes[] = {
    CreateExtensionSize<AbsoluteSendTime>(),
    CreateExtensionSize<TransmissionOffset>(),
    CreateExtensionSize<TransportSequenceNumber>(),
    CreateExtensionSize<PlayoutDelayLimits>(),
    CreateMaxExtensionSize<RtpMid>(),
    CreateExtensionSize<VideoTimingExtension>(),
};

// Size info for header extensions that might be used in video packets.
constexpr RtpExtensionSize kVideoExtensionSizes[] = {
    CreateExtensionSize<AbsoluteSendTime>(),
    CreateExtensionSize<AbsoluteCaptureTimeExtension>(),
    CreateExtensionSize<TransmissionOffset>(),
    CreateExtensionSize<TransportSequenceNumber>(),
    CreateExtensionSize<PlayoutDelayLimits>(),
    CreateExtensionSize<VideoOrientation>(),
    CreateExtensionSize<VideoContentTypeExtension>(),
    CreateExtensionSize<VideoTimingExtension>(),
    CreateMaxExtensionSize<RtpStreamId>(),
    CreateMaxExtensionSize<RepairedRtpStreamId>(),
    CreateMaxExtensionSize<RtpMid>(),
    {RtpGenericFrameDescriptorExtension00::kId,
     RtpGenericFrameDescriptorExtension00::kMaxSizeBytes},
    {RtpGenericFrameDescriptorExtension01::kId,
     RtpGenericFrameDescriptorExtension01::kMaxSizeBytes},
};

bool HasBweExtension(const RtpHeaderExtensionMap& extensions_map) {
  return extensions_map.IsRegistered(kRtpExtensionTransportSequenceNumber) ||
         extensions_map.IsRegistered(kRtpExtensionTransportSequenceNumber02) ||
         extensions_map.IsRegistered(kRtpExtensionAbsoluteSendTime) ||
         extensions_map.IsRegistered(kRtpExtensionTransmissionTimeOffset);
}

}  // namespace

RTPSender::RTPSender(const RtpRtcp::Configuration& config,
                     RtpPacketHistory* packet_history,
                     RtpPacketSender* packet_sender)
    : clock_(config.clock),
      random_(clock_->TimeInMicroseconds()),
      audio_configured_(config.audio),
      ssrc_(config.local_media_ssrc),
      rtx_ssrc_(config.rtx_send_ssrc),
      flexfec_ssrc_(config.fec_generator ? config.fec_generator->FecSsrc()
                                         : absl::nullopt),
      packet_history_(packet_history),
      paced_sender_(packet_sender),
      sending_media_(true),                   // Default to sending media.
      max_packet_size_(IP_PACKET_SIZE - 28),  // Default is IP-v4/UDP.
      last_payload_type_(-1),
      rtp_header_extension_map_(config.extmap_allow_mixed),
      // RTP variables
      sequence_number_forced_(false),
      always_send_mid_and_rid_(config.always_send_mid_and_rid),
      ssrc_has_acked_(false),
      rtx_ssrc_has_acked_(false),
      last_rtp_timestamp_(0),
      capture_time_ms_(0),
      last_timestamp_time_ms_(0),
      last_packet_marker_bit_(false),
      csrcs_(),
      rtx_(kRtxOff),
      supports_bwe_extension_(false),
      retransmission_rate_limiter_(config.retransmission_rate_limiter) {
  // This random initialization is not intended to be cryptographic strong.
  timestamp_offset_ = random_.Rand<uint32_t>();
  // Random start, 16 bits. Can't be 0.
  sequence_number_rtx_ = random_.Rand(1, kMaxInitRtpSeqNumber);
  sequence_number_ = random_.Rand(1, kMaxInitRtpSeqNumber);

  RTC_DCHECK(paced_sender_);
  RTC_DCHECK(packet_history_);
}

RTPSender::~RTPSender() {
  // TODO(tommi): Use a thread checker to ensure the object is created and
  // deleted on the same thread.  At the moment this isn't possible due to
  // voe::ChannelOwner in voice engine.  To reproduce, run:
  // voe_auto_test --automated --gtest_filter=*MixManyChannelsForStressOpus

  // TODO(tommi,holmer): We don't grab locks in the dtor before accessing member
  // variables but we grab them in all other methods. (what's the design?)
  // Start documenting what thread we're on in what method so that it's easier
  // to understand performance attributes and possibly remove locks.
}

rtc::ArrayView<const RtpExtensionSize> RTPSender::FecExtensionSizes() {
  return rtc::MakeArrayView(kFecOrPaddingExtensionSizes,
                            arraysize(kFecOrPaddingExtensionSizes));
}

rtc::ArrayView<const RtpExtensionSize> RTPSender::VideoExtensionSizes() {
  return rtc::MakeArrayView(kVideoExtensionSizes,
                            arraysize(kVideoExtensionSizes));
}

void RTPSender::SetExtmapAllowMixed(bool extmap_allow_mixed) {
  rtc::CritScope lock(&send_critsect_);
  rtp_header_extension_map_.SetExtmapAllowMixed(extmap_allow_mixed);
}

int32_t RTPSender::RegisterRtpHeaderExtension(RTPExtensionType type,
                                              uint8_t id) {
  rtc::CritScope lock(&send_critsect_);
  bool registered = rtp_header_extension_map_.RegisterByType(id, type);
  supports_bwe_extension_ = HasBweExtension(rtp_header_extension_map_);
  return registered ? 0 : -1;
}

bool RTPSender::RegisterRtpHeaderExtension(absl::string_view uri, int id) {
  rtc::CritScope lock(&send_critsect_);
  bool registered = rtp_header_extension_map_.RegisterByUri(id, uri);
  supports_bwe_extension_ = HasBweExtension(rtp_header_extension_map_);
  return registered;
}

bool RTPSender::IsRtpHeaderExtensionRegistered(RTPExtensionType type) const {
  rtc::CritScope lock(&send_critsect_);
  return rtp_header_extension_map_.IsRegistered(type);
}

int32_t RTPSender::DeregisterRtpHeaderExtension(RTPExtensionType type) {
  rtc::CritScope lock(&send_critsect_);
  int32_t deregistered = rtp_header_extension_map_.Deregister(type);
  supports_bwe_extension_ = HasBweExtension(rtp_header_extension_map_);
  return deregistered;
}

void RTPSender::DeregisterRtpHeaderExtension(absl::string_view uri) {
  rtc::CritScope lock(&send_critsect_);
  rtp_header_extension_map_.Deregister(uri);
  supports_bwe_extension_ = HasBweExtension(rtp_header_extension_map_);
}

void RTPSender::SetMaxRtpPacketSize(size_t max_packet_size) {
  RTC_DCHECK_GE(max_packet_size, 100);
  RTC_DCHECK_LE(max_packet_size, IP_PACKET_SIZE);
  rtc::CritScope lock(&send_critsect_);
  max_packet_size_ = max_packet_size;
}

size_t RTPSender::MaxRtpPacketSize() const {
  return max_packet_size_;
}

void RTPSender::SetRtxStatus(int mode) {
  rtc::CritScope lock(&send_critsect_);
  rtx_ = mode;
}

int RTPSender::RtxStatus() const {
  rtc::CritScope lock(&send_critsect_);
  return rtx_;
}

void RTPSender::SetRtxPayloadType(int payload_type,
                                  int associated_payload_type) {
  rtc::CritScope lock(&send_critsect_);
  RTC_DCHECK_LE(payload_type, 127);
  RTC_DCHECK_LE(associated_payload_type, 127);
  if (payload_type < 0) {
    RTC_LOG(LS_ERROR) << "Invalid RTX payload type: " << payload_type << ".";
    return;
  }

  rtx_payload_type_map_[associated_payload_type] = payload_type;
}

int32_t RTPSender::ReSendPacket(uint16_t packet_id) {
  // Try to find packet in RTP packet history. Also verify RTT here, so that we
  // don't retransmit too often.
  absl::optional<RtpPacketHistory::PacketState> stored_packet =
      packet_history_->GetPacketState(packet_id);
  if (!stored_packet || stored_packet->pending_transmission) {
    // Packet not found or already queued for retransmission, ignore.
    return 0;
  }

  const int32_t packet_size = static_cast<int32_t>(stored_packet->packet_size);
  const bool rtx = (RtxStatus() & kRtxRetransmitted) > 0;

  std::unique_ptr<RtpPacketToSend> packet =
      packet_history_->GetPacketAndMarkAsPending(
          packet_id, [&](const RtpPacketToSend& stored_packet) {
            // Check if we're overusing retransmission bitrate.
            // TODO(sprang): Add histograms for nack success or failure
            // reasons.
            std::unique_ptr<RtpPacketToSend> retransmit_packet;
            if (retransmission_rate_limiter_ &&
                !retransmission_rate_limiter_->TryUseRate(packet_size)) {
              return retransmit_packet;
            }
            if (rtx) {
              retransmit_packet = BuildRtxPacket(stored_packet);
            } else {
              retransmit_packet =
                  std::make_unique<RtpPacketToSend>(stored_packet);
            }
            if (retransmit_packet) {
              retransmit_packet->set_retransmitted_sequence_number(
                  stored_packet.SequenceNumber());
            }
            return retransmit_packet;
          });
  if (!packet) {
    return -1;
  }
  packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  std::vector<std::unique_ptr<RtpPacketToSend>> packets;
  packets.emplace_back(std::move(packet));
  paced_sender_->EnqueuePackets(std::move(packets));

  return packet_size;
}

void RTPSender::OnReceivedAckOnSsrc(int64_t extended_highest_sequence_number) {
  rtc::CritScope lock(&send_critsect_);
  ssrc_has_acked_ = true;
}

void RTPSender::OnReceivedAckOnRtxSsrc(
    int64_t extended_highest_sequence_number) {
  rtc::CritScope lock(&send_critsect_);
  rtx_ssrc_has_acked_ = true;
}

void RTPSender::OnReceivedNack(
    const std::vector<uint16_t>& nack_sequence_numbers,
    int64_t avg_rtt) {
  packet_history_->SetRtt(5 + avg_rtt);
  for (uint16_t seq_no : nack_sequence_numbers) {
    const int32_t bytes_sent = ReSendPacket(seq_no);
    if (bytes_sent < 0) {
      // Failed to send one Sequence number. Give up the rest in this nack.
      RTC_LOG(LS_WARNING) << "Failed resending RTP packet " << seq_no
                          << ", Discard rest of packets.";
      break;
    }
  }
}

bool RTPSender::SupportsPadding() const {
  rtc::CritScope lock(&send_critsect_);
  return sending_media_ && supports_bwe_extension_;
}

bool RTPSender::SupportsRtxPayloadPadding() const {
  rtc::CritScope lock(&send_critsect_);
  return sending_media_ && supports_bwe_extension_ &&
         (rtx_ & kRtxRedundantPayloads);
}

std::vector<std::unique_ptr<RtpPacketToSend>> RTPSender::GeneratePadding(
    size_t target_size_bytes,
    bool media_has_been_sent) {
  // This method does not actually send packets, it just generates
  // them and puts them in the pacer queue. Since this should incur
  // low overhead, keep the lock for the scope of the method in order
  // to make the code more readable.

  std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets;
  size_t bytes_left = target_size_bytes;
  if (SupportsRtxPayloadPadding()) {
    while (bytes_left >= kMinPayloadPaddingBytes) {
      std::unique_ptr<RtpPacketToSend> packet =
          packet_history_->GetPayloadPaddingPacket(
              [&](const RtpPacketToSend& packet)
                  -> std::unique_ptr<RtpPacketToSend> {
                return BuildRtxPacket(packet);
              });
      if (!packet) {
        break;
      }

      bytes_left -= std::min(bytes_left, packet->payload_size());
      packet->set_packet_type(RtpPacketMediaType::kPadding);
      padding_packets.push_back(std::move(packet));
    }
  }

  rtc::CritScope lock(&send_critsect_);
  if (!sending_media_) {
    return {};
  }

  size_t padding_bytes_in_packet;
  const size_t max_payload_size = max_packet_size_ - RtpHeaderLength();
  if (audio_configured_) {
    // Allow smaller padding packets for audio.
    padding_bytes_in_packet = rtc::SafeClamp<size_t>(
        bytes_left, kMinAudioPaddingLength,
        rtc::SafeMin(max_payload_size, kMaxPaddingLength));
  } else {
    // Always send full padding packets. This is accounted for by the
    // RtpPacketSender, which will make sure we don't send too much padding even
    // if a single packet is larger than requested.
    // We do this to avoid frequently sending small packets on higher bitrates.
    padding_bytes_in_packet = rtc::SafeMin(max_payload_size, kMaxPaddingLength);
  }

  while (bytes_left > 0) {
    auto padding_packet =
        std::make_unique<RtpPacketToSend>(&rtp_header_extension_map_);
    padding_packet->set_packet_type(RtpPacketMediaType::kPadding);
    padding_packet->SetMarker(false);
    padding_packet->SetTimestamp(last_rtp_timestamp_);
    padding_packet->set_capture_time_ms(capture_time_ms_);
    if (rtx_ == kRtxOff) {
      if (last_payload_type_ == -1) {
        break;
      }
      // Without RTX we can't send padding in the middle of frames.
      // For audio marker bits doesn't mark the end of a frame and frames
      // are usually a single packet, so for now we don't apply this rule
      // for audio.
      if (!audio_configured_ && !last_packet_marker_bit_) {
        break;
      }

      padding_packet->SetSsrc(ssrc_);
      padding_packet->SetPayloadType(last_payload_type_);
      padding_packet->SetSequenceNumber(sequence_number_++);
    } else {
      // Without abs-send-time or transport sequence number a media packet
      // must be sent before padding so that the timestamps used for
      // estimation are correct.
      if (!media_has_been_sent &&
          !(rtp_header_extension_map_.IsRegistered(AbsoluteSendTime::kId) ||
            rtp_header_extension_map_.IsRegistered(
                TransportSequenceNumber::kId))) {
        break;
      }
      // Only change the timestamp of padding packets sent over RTX.
      // Padding only packets over RTP has to be sent as part of a media
      // frame (and therefore the same timestamp).
      int64_t now_ms = clock_->TimeInMilliseconds();
      if (last_timestamp_time_ms_ > 0) {
        padding_packet->SetTimestamp(padding_packet->Timestamp() +
                                     (now_ms - last_timestamp_time_ms_) *
                                         kTimestampTicksPerMs);
        padding_packet->set_capture_time_ms(padding_packet->capture_time_ms() +
                                            (now_ms - last_timestamp_time_ms_));
      }
      RTC_DCHECK(rtx_ssrc_);
      padding_packet->SetSsrc(*rtx_ssrc_);
      padding_packet->SetSequenceNumber(sequence_number_rtx_++);
      padding_packet->SetPayloadType(rtx_payload_type_map_.begin()->second);
    }

    if (rtp_header_extension_map_.IsRegistered(TransportSequenceNumber::kId)) {
      padding_packet->ReserveExtension<TransportSequenceNumber>();
    }
    if (rtp_header_extension_map_.IsRegistered(TransmissionOffset::kId)) {
      padding_packet->ReserveExtension<TransmissionOffset>();
    }
    if (rtp_header_extension_map_.IsRegistered(AbsoluteSendTime::kId)) {
      padding_packet->ReserveExtension<AbsoluteSendTime>();
    }

    padding_packet->SetPadding(padding_bytes_in_packet);
    bytes_left -= std::min(bytes_left, padding_bytes_in_packet);
    padding_packets.push_back(std::move(padding_packet));
  }

  return padding_packets;
}

bool RTPSender::SendToNetwork(std::unique_ptr<RtpPacketToSend> packet) {
  RTC_DCHECK(packet);
  int64_t now_ms = clock_->TimeInMilliseconds();

  auto packet_type = packet->packet_type();
  RTC_CHECK(packet_type) << "Packet type must be set before sending.";

  if (packet->capture_time_ms() <= 0) {
    packet->set_capture_time_ms(now_ms);
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> packets;
  packets.emplace_back(std::move(packet));
  paced_sender_->EnqueuePackets(std::move(packets));

  return true;
}

void RTPSender::EnqueuePackets(
    std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
  RTC_DCHECK(!packets.empty());
  int64_t now_ms = clock_->TimeInMilliseconds();
  for (auto& packet : packets) {
    RTC_DCHECK(packet);
    RTC_CHECK(packet->packet_type().has_value())
        << "Packet type must be set before sending.";
    if (packet->capture_time_ms() <= 0) {
      packet->set_capture_time_ms(now_ms);
    }
  }

  paced_sender_->EnqueuePackets(std::move(packets));
}

size_t RTPSender::RtpHeaderLength() const {
  rtc::CritScope lock(&send_critsect_);
  size_t rtp_header_length = kRtpHeaderLength;
  rtp_header_length += sizeof(uint32_t) * csrcs_.size();
  rtp_header_length += RtpHeaderExtensionSize(kFecOrPaddingExtensionSizes,
                                              rtp_header_extension_map_);
  return rtp_header_length;
}

uint16_t RTPSender::AllocateSequenceNumber(uint16_t packets_to_send) {
  rtc::CritScope lock(&send_critsect_);
  uint16_t first_allocated_sequence_number = sequence_number_;
  sequence_number_ += packets_to_send;
  return first_allocated_sequence_number;
}

std::unique_ptr<RtpPacketToSend> RTPSender::AllocatePacket() const {
  rtc::CritScope lock(&send_critsect_);
  // TODO(danilchap): Find better motivator and value for extra capacity.
  // RtpPacketizer might slightly miscalulate needed size,
  // SRTP may benefit from extra space in the buffer and do encryption in place
  // saving reallocation.
  // While sending slightly oversized packet increase chance of dropped packet,
  // it is better than crash on drop packet without trying to send it.
  static constexpr int kExtraCapacity = 16;
  auto packet = std::make_unique<RtpPacketToSend>(
      &rtp_header_extension_map_, max_packet_size_ + kExtraCapacity);
  packet->SetSsrc(ssrc_);
  packet->SetCsrcs(csrcs_);
  // Reserve extensions, if registered, RtpSender set in SendToNetwork.
  packet->ReserveExtension<AbsoluteSendTime>();
  packet->ReserveExtension<TransmissionOffset>();
  packet->ReserveExtension<TransportSequenceNumber>();

  // BUNDLE requires that the receiver "bind" the received SSRC to the values
  // in the MID and/or (R)RID header extensions if present. Therefore, the
  // sender can reduce overhead by omitting these header extensions once it
  // knows that the receiver has "bound" the SSRC.
  // This optimization can be configured by setting
  // |always_send_mid_and_rid_| appropriately.
  //
  // The algorithm here is fairly simple: Always attach a MID and/or RID (if
  // configured) to the outgoing packets until an RTCP receiver report comes
  // back for this SSRC. That feedback indicates the receiver must have
  // received a packet with the SSRC and header extension(s), so the sender
  // then stops attaching the MID and RID.
  if (always_send_mid_and_rid_ || !ssrc_has_acked_) {
    // These are no-ops if the corresponding header extension is not registered.
    if (!mid_.empty()) {
      packet->SetExtension<RtpMid>(mid_);
    }
    if (!rid_.empty()) {
      packet->SetExtension<RtpStreamId>(rid_);
    }
  }
  return packet;
}

bool RTPSender::AssignSequenceNumber(RtpPacketToSend* packet) {
  rtc::CritScope lock(&send_critsect_);
  if (!sending_media_)
    return false;
  RTC_DCHECK(packet->Ssrc() == ssrc_);
  packet->SetSequenceNumber(sequence_number_++);

  // Remember marker bit to determine if padding can be inserted with
  // sequence number following |packet|.
  last_packet_marker_bit_ = packet->Marker();
  // Remember payload type to use in the padding packet if rtx is disabled.
  last_payload_type_ = packet->PayloadType();
  // Save timestamps to generate timestamp field and extensions for the padding.
  last_rtp_timestamp_ = packet->Timestamp();
  last_timestamp_time_ms_ = clock_->TimeInMilliseconds();
  capture_time_ms_ = packet->capture_time_ms();
  return true;
}

void RTPSender::SetSendingMediaStatus(bool enabled) {
  rtc::CritScope lock(&send_critsect_);
  sending_media_ = enabled;
}

bool RTPSender::SendingMedia() const {
  rtc::CritScope lock(&send_critsect_);
  return sending_media_;
}

bool RTPSender::IsAudioConfigured() const {
  return audio_configured_;
}

void RTPSender::SetTimestampOffset(uint32_t timestamp) {
  rtc::CritScope lock(&send_critsect_);
  timestamp_offset_ = timestamp;
}

uint32_t RTPSender::TimestampOffset() const {
  rtc::CritScope lock(&send_critsect_);
  return timestamp_offset_;
}

void RTPSender::SetRid(const std::string& rid) {
  // RID is used in simulcast scenario when multiple layers share the same mid.
  rtc::CritScope lock(&send_critsect_);
  RTC_DCHECK_LE(rid.length(), RtpStreamId::kMaxValueSizeBytes);
  rid_ = rid;
}

void RTPSender::SetMid(const std::string& mid) {
  // This is configured via the API.
  rtc::CritScope lock(&send_critsect_);
  RTC_DCHECK_LE(mid.length(), RtpMid::kMaxValueSizeBytes);
  mid_ = mid;
}

void RTPSender::SetCsrcs(const std::vector<uint32_t>& csrcs) {
  RTC_DCHECK_LE(csrcs.size(), kRtpCsrcSize);
  rtc::CritScope lock(&send_critsect_);
  csrcs_ = csrcs;
}

void RTPSender::SetSequenceNumber(uint16_t seq) {
  bool updated_sequence_number = false;
  {
    rtc::CritScope lock(&send_critsect_);
    sequence_number_forced_ = true;
    if (sequence_number_ != seq) {
      updated_sequence_number = true;
    }
    sequence_number_ = seq;
  }

  if (updated_sequence_number) {
    // Sequence number series has been reset to a new value, clear RTP packet
    // history, since any packets there may conflict with new ones.
    packet_history_->Clear();
  }
}

uint16_t RTPSender::SequenceNumber() const {
  rtc::CritScope lock(&send_critsect_);
  return sequence_number_;
}

static void CopyHeaderAndExtensionsToRtxPacket(const RtpPacketToSend& packet,
                                               RtpPacketToSend* rtx_packet) {
  // Set the relevant fixed packet headers. The following are not set:
  // * Payload type - it is replaced in rtx packets.
  // * Sequence number - RTX has a separate sequence numbering.
  // * SSRC - RTX stream has its own SSRC.
  rtx_packet->SetMarker(packet.Marker());
  rtx_packet->SetTimestamp(packet.Timestamp());

  // Set the variable fields in the packet header:
  // * CSRCs - must be set before header extensions.
  // * Header extensions - replace Rid header with RepairedRid header.
  const std::vector<uint32_t> csrcs = packet.Csrcs();
  rtx_packet->SetCsrcs(csrcs);
  for (int extension_num = kRtpExtensionNone + 1;
       extension_num < kRtpExtensionNumberOfExtensions; ++extension_num) {
    auto extension = static_cast<RTPExtensionType>(extension_num);

    // Stream ID header extensions (MID, RSID) are sent per-SSRC. Since RTX
    // operates on a different SSRC, the presence and values of these header
    // extensions should be determined separately and not blindly copied.
    if (extension == kRtpExtensionMid ||
        extension == kRtpExtensionRtpStreamId) {
      continue;
    }

    // Empty extensions should be supported, so not checking |source.empty()|.
    if (!packet.HasExtension(extension)) {
      continue;
    }

    rtc::ArrayView<const uint8_t> source = packet.FindExtension(extension);

    rtc::ArrayView<uint8_t> destination =
        rtx_packet->AllocateExtension(extension, source.size());

    // Could happen if any:
    // 1. Extension has 0 length.
    // 2. Extension is not registered in destination.
    // 3. Allocating extension in destination failed.
    if (destination.empty() || source.size() != destination.size()) {
      continue;
    }

    std::memcpy(destination.begin(), source.begin(), destination.size());
  }
}

std::unique_ptr<RtpPacketToSend> RTPSender::BuildRtxPacket(
    const RtpPacketToSend& packet) {
  std::unique_ptr<RtpPacketToSend> rtx_packet;

  // Add original RTP header.
  {
    rtc::CritScope lock(&send_critsect_);
    if (!sending_media_)
      return nullptr;

    RTC_DCHECK(rtx_ssrc_);

    // Replace payload type.
    auto kv = rtx_payload_type_map_.find(packet.PayloadType());
    if (kv == rtx_payload_type_map_.end())
      return nullptr;

    rtx_packet = std::make_unique<RtpPacketToSend>(&rtp_header_extension_map_,
                                                   max_packet_size_);

    rtx_packet->SetPayloadType(kv->second);

    // Replace sequence number.
    rtx_packet->SetSequenceNumber(sequence_number_rtx_++);

    // Replace SSRC.
    rtx_packet->SetSsrc(*rtx_ssrc_);

    CopyHeaderAndExtensionsToRtxPacket(packet, rtx_packet.get());

    // RTX packets are sent on an SSRC different from the main media, so the
    // decision to attach MID and/or RRID header extensions is completely
    // separate from that of the main media SSRC.
    //
    // Note that RTX packets must used the RepairedRtpStreamId (RRID) header
    // extension instead of the RtpStreamId (RID) header extension even though
    // the payload is identical.
    if (always_send_mid_and_rid_ || !rtx_ssrc_has_acked_) {
      // These are no-ops if the corresponding header extension is not
      // registered.
      if (!mid_.empty()) {
        rtx_packet->SetExtension<RtpMid>(mid_);
      }
      if (!rid_.empty()) {
        rtx_packet->SetExtension<RepairedRtpStreamId>(rid_);
      }
    }
  }
  RTC_DCHECK(rtx_packet);

  uint8_t* rtx_payload =
      rtx_packet->AllocatePayload(packet.payload_size() + kRtxHeaderSize);
  if (rtx_payload == nullptr)
    return nullptr;

  // Add OSN (original sequence number).
  ByteWriter<uint16_t>::WriteBigEndian(rtx_payload, packet.SequenceNumber());

  // Add original payload data.
  auto payload = packet.payload();
  memcpy(rtx_payload + kRtxHeaderSize, payload.data(), payload.size());

  // Add original application data.
  rtx_packet->set_application_data(packet.application_data());

  // Copy capture time so e.g. TransmissionOffset is correctly set.
  rtx_packet->set_capture_time_ms(packet.capture_time_ms());

  return rtx_packet;
}

void RTPSender::SetRtpState(const RtpState& rtp_state) {
  rtc::CritScope lock(&send_critsect_);
  sequence_number_ = rtp_state.sequence_number;
  sequence_number_forced_ = true;
  timestamp_offset_ = rtp_state.start_timestamp;
  last_rtp_timestamp_ = rtp_state.timestamp;
  capture_time_ms_ = rtp_state.capture_time_ms;
  last_timestamp_time_ms_ = rtp_state.last_timestamp_time_ms;
  ssrc_has_acked_ = rtp_state.ssrc_has_acked;
}

RtpState RTPSender::GetRtpState() const {
  rtc::CritScope lock(&send_critsect_);

  RtpState state;
  state.sequence_number = sequence_number_;
  state.start_timestamp = timestamp_offset_;
  state.timestamp = last_rtp_timestamp_;
  state.capture_time_ms = capture_time_ms_;
  state.last_timestamp_time_ms = last_timestamp_time_ms_;
  state.ssrc_has_acked = ssrc_has_acked_;
  return state;
}

void RTPSender::SetRtxRtpState(const RtpState& rtp_state) {
  rtc::CritScope lock(&send_critsect_);
  sequence_number_rtx_ = rtp_state.sequence_number;
  rtx_ssrc_has_acked_ = rtp_state.ssrc_has_acked;
}

RtpState RTPSender::GetRtxRtpState() const {
  rtc::CritScope lock(&send_critsect_);

  RtpState state;
  state.sequence_number = sequence_number_rtx_;
  state.start_timestamp = timestamp_offset_;
  state.ssrc_has_acked = rtx_ssrc_has_acked_;

  return state;
}

int64_t RTPSender::LastTimestampTimeMs() const {
  rtc::CritScope lock(&send_critsect_);
  return last_timestamp_time_ms_;
}
}  // namespace webrtc
