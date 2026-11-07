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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/rtp_header_extension_id.h"
#include "api/rtp_headers.h"
#include "api/rtp_packet_sender.h"
#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/corruption_detection_extension.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extension_size.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_history.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/random.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

namespace {
constexpr size_t kMinAudioPaddingLength = 50;
constexpr size_t kRtpHeaderLength = 12;

// Min size needed to get payload padding from packet history.
constexpr int kMinPayloadPaddingBytes = 50;

// Determines how much larger a payload padding packet may be, compared to the
// requested padding size.
constexpr double kMaxPaddingSizeFactor = 3.0;

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
    CreateMaxExtensionSize<CorruptionDetectionExtension>(),
    {.type = RtpGenericFrameDescriptorExtension00::kId,
     .value_size = RtpGenericFrameDescriptorExtension00::kMaxSizeBytes},
};

// Size info for header extensions that might be used in audio packets.
constexpr RtpExtensionSize kAudioExtensionSizes[] = {
    CreateExtensionSize<AbsoluteSendTime>(),
    CreateExtensionSize<AbsoluteCaptureTimeExtension>(),
    CreateExtensionSize<AudioLevelExtension>(),
    CreateExtensionSize<InbandComfortNoiseExtension>(),
    CreateExtensionSize<TransmissionOffset>(),
    CreateExtensionSize<TransportSequenceNumber>(),
    CreateMaxExtensionSize<RtpMid>(),
};

// Non-volatile extensions can be expected on all packets, if registered.
// Volatile ones, such as VideoContentTypeExtension which is only set on
// key-frames, are removed to simplify overhead calculations at the expense of
// some accuracy.
bool IsNonVolatile(RTPExtensionType type) {
  switch (type) {
    case kRtpExtensionTransmissionTimeOffset:
    case kRtpExtensionAudioLevel:
    case kRtpExtensionCsrcAudioLevel:
    case kRtpExtensionAbsoluteSendTime:
    case kRtpExtensionTransportSequenceNumber:
    case kRtpExtensionTransportSequenceNumber02:
    case kRtpExtensionRtpStreamId:
    case kRtpExtensionRepairedRtpStreamId:
    case kRtpExtensionMid:
    case kRtpExtensionGenericFrameDescriptor:
    case kRtpExtensionDependencyDescriptor:
      return true;
    case kRtpExtensionInbandComfortNoise:
    case kRtpExtensionAbsoluteCaptureTime:
    case kRtpExtensionVideoRotation:
    case kRtpExtensionPlayoutDelay:
    case kRtpExtensionVideoContentType:
    case kRtpExtensionVideoLayersAllocation:
    case kRtpExtensionVideoTiming:
    case kRtpExtensionColorSpace:
    case kRtpExtensionVideoFrameTrackingId:
    case kRtpExtensionCorruptionDetection:
      return false;
    case kRtpExtensionNone:
    case kRtpExtensionNumberOfExtensions:
      RTC_DCHECK_NOTREACHED();
      return false;
  }
  RTC_CHECK_NOTREACHED();
}

// Returns the number of padding bytes to put in each padding packet when
// `bytes_left` bytes of padding are left to generate.
size_t PaddingBytesInPacket(bool audio,
                            size_t bytes_left,
                            size_t max_payload_size) {
  if (audio) {
    // Allow smaller padding packets for audio.
    return SafeClamp<size_t>(bytes_left, kMinAudioPaddingLength,
                             SafeMin(max_payload_size, kMaxPaddingLength));
  }
  // Always send full padding packets. This is accounted for by the
  // RtpPacketSender, which will make sure we don't send too much padding even
  // if a single packet is larger than requested.
  // We do this to avoid frequently sending small packets on higher bitrates.
  return SafeMin(max_payload_size, kMaxPaddingLength);
}

bool HasBweExtension(const RtpHeaderExtensionMap& extensions_map) {
  return extensions_map.IsRegistered(kRtpExtensionTransportSequenceNumber) ||
         extensions_map.IsRegistered(kRtpExtensionTransportSequenceNumber02) ||
         extensions_map.IsRegistered(kRtpExtensionAbsoluteSendTime) ||
         extensions_map.IsRegistered(kRtpExtensionTransmissionTimeOffset);
}

// Returns whether `payload_type` fits the 7 bit payload type field of the RTP
// header.
bool IsValidPayloadType(int payload_type) {
  return payload_type >= 0 && payload_type <= 127;
}

// Maps `associated_payload_type` to `rtx_payload_type` in
// `rtx_payload_type_map`. Logs an error and returns false if either of the
// payload types is invalid.
bool AddRtxPayloadType(int rtx_payload_type,
                       int associated_payload_type,
                       flat_map<int8_t, int8_t>& rtx_payload_type_map) {
  if (!IsValidPayloadType(rtx_payload_type) ||
      !IsValidPayloadType(associated_payload_type)) {
    RTC_LOG(LS_ERROR) << "Invalid RTX payload type mapping: "
                      << associated_payload_type << " -> " << rtx_payload_type
                      << ".";
    return false;
  }
  rtx_payload_type_map[associated_payload_type] = rtx_payload_type;
  return true;
}

// Creates the mapping from associated payload type to RTX payload type. The
// mapping is empty if any of the payload types is invalid, so that RTX can't be
// enabled with it.
flat_map<int8_t, int8_t> CreateRtxPayloadTypeMap(
    const flat_map<int, int>& rtx_payload_types) {
  flat_map<int8_t, int8_t> rtx_payload_type_map;
  for (const auto& [associated_payload_type, rtx_payload_type] :
       rtx_payload_types) {
    if (!AddRtxPayloadType(rtx_payload_type, associated_payload_type,
                           rtx_payload_type_map)) {
      return {};
    }
  }
  return rtx_payload_type_map;
}

// Returns `mode` if RTX can be used with `mode`, `rtx_ssrc` and
// `rtx_payload_type_map`. Otherwise logs an error and returns `kRtxOff`.
int ValidatedRtxMode(int mode,
                     std::optional<uint32_t> rtx_ssrc,
                     const flat_map<int8_t, int8_t>& rtx_payload_type_map) {
  if (mode != kRtxOff &&
      (!rtx_ssrc.has_value() || rtx_payload_type_map.empty())) {
    RTC_LOG(LS_ERROR)
        << "Failed to enable RTX without RTX SSRC or payload types.";
    return kRtxOff;
  }
  return mode;
}

}  // namespace

RTPSender::RTPSender(const Environment& env,
                     const RtpRtcpInterface::Configuration& config,
                     RtpPacketHistory* packet_history,
                     RtpPacketSender* packet_sender)
    : clock_(&env.clock()),
      audio_configured_(config.audio),
      ssrc_(config.local_media_ssrc),
      rtx_ssrc_(config.rtx_send_ssrc),
      flexfec_ssrc_(config.fec_generator ? config.fec_generator->FecSsrc()
                                         : std::nullopt),
      packet_history_(packet_history),
      paced_sender_(packet_sender),
      sending_media_(true),                   // Default to sending media.
      max_packet_size_(IP_PACKET_SIZE - 28),  // Default is IP-v4/UDP.
      rtp_header_extension_map_(config.extmap_allow_mixed),
      // RTP variables
      // This random initialization is not intended to be cryptographically
      // strong.
      timestamp_offset_(
          config.rtp_state.has_value()
              ? config.rtp_state->start_timestamp
              : Random(clock_->TimeInMicroseconds()).Rand<uint32_t>()),
      rid_(config.rid),
      mid_(config.mid),
      always_send_mid_and_rid_(config.always_send_mid_and_rid),
      ssrc_has_acked_(config.rtp_state.has_value() &&
                      config.rtp_state->ssrc_has_acked),
      rtx_ssrc_has_acked_(config.rtx_rtp_state.has_value() &&
                          config.rtx_rtp_state->ssrc_has_acked),
      rtx_payload_type_map_(CreateRtxPayloadTypeMap(config.rtx_payload_types)),
      rtx_(ValidatedRtxMode(config.rtx_mode, rtx_ssrc_, rtx_payload_type_map_)),
      supports_bwe_extension_(false),
      retransmission_rate_limiter_(config.retransmission_rate_limiter) {
  RTC_DCHECK(paced_sender_);
  RTC_DCHECK(packet_history_);
  RTC_DCHECK_LE(rid_.size(), RtpStreamId::kMaxValueSizeBytes);
  RTC_DCHECK_LE(mid_.size(), RtpMid::kMaxValueSizeBytes);

  UpdateHeaderSizes();
}

RTPSender::~RTPSender() = default;

std::span<const RtpExtensionSize> RTPSender::FecExtensionSizes() {
  return kFecOrPaddingExtensionSizes;
}

std::span<const RtpExtensionSize> RTPSender::VideoExtensionSizes() {
  return kVideoExtensionSizes;
}

std::span<const RtpExtensionSize> RTPSender::AudioExtensionSizes() {
  return kAudioExtensionSizes;
}

void RTPSender::SetExtmapAllowMixed(bool extmap_allow_mixed) {
  MutexLock lock(&send_mutex_);
  rtp_header_extension_map_.SetExtmapAllowMixed(extmap_allow_mixed);
}

bool RTPSender::RegisterRtpHeaderExtension(absl::string_view uri,
                                           RtpHeaderExtensionId id) {
  RTC_DCHECK_RUN_ON(&worker_checker_);
  MutexLock lock(&send_mutex_);
  bool registered = rtp_header_extension_map_.RegisterByUri(id, uri);
  supports_bwe_extension_ = HasBweExtension(rtp_header_extension_map_);
  UpdateHeaderSizes();
  return registered;
}

void RTPSender::DeregisterRtpHeaderExtension(absl::string_view uri) {
  RTC_DCHECK_RUN_ON(&worker_checker_);
  MutexLock lock(&send_mutex_);
  rtp_header_extension_map_.Deregister(uri);
  supports_bwe_extension_ = HasBweExtension(rtp_header_extension_map_);
  UpdateHeaderSizes();
}

void RTPSender::SetMaxRtpPacketSize(size_t max_packet_size) {
  RTC_DCHECK_GE(max_packet_size, 100);
  RTC_DCHECK_LE(max_packet_size, IP_PACKET_SIZE);
  MutexLock lock(&send_mutex_);
  max_packet_size_ = max_packet_size;
}

size_t RTPSender::MaxRtpPacketSize() const {
  MutexLock lock(&send_mutex_);
  return max_packet_size_;
}

int32_t RTPSender::ReSendPacket(uint16_t packet_id) {
  int32_t packet_size = 0;
  const bool rtx = (RtxStatus() & kRtxRetransmitted) > 0;

  std::unique_ptr<RtpPacketToSend> packet =
      packet_history_->GetPacketAndMarkAsPending(
          packet_id, [&](const RtpPacketToSend& stored_packet) {
            // Check if we're overusing retransmission bitrate.
            // TODO(sprang): Add histograms for nack success or failure
            // reasons.
            packet_size = stored_packet.size();
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
              retransmit_packet->set_original_ssrc(stored_packet.Ssrc());
            }
            return retransmit_packet;
          });
  if (packet_size == 0) {
    // Packet not found or already queued for retransmission, ignore.
    RTC_DCHECK(!packet);
    return 0;
  }
  if (!packet) {
    // Packet was found, but lambda helper above chose not to create
    // `retransmit_packet` out of it.
    return -1;
  }
  packet->set_packet_type(RtpPacketMediaType::kRetransmission);
  packet->set_fec_protect_packet(false);
  std::vector<std::unique_ptr<RtpPacketToSend>> packets;
  packets.emplace_back(std::move(packet));
  paced_sender_->EnqueuePackets(std::move(packets));

  return packet_size;
}

void RTPSender::OnReceivedAckOnSsrc(
    int64_t /* extended_highest_sequence_number */) {
  MutexLock lock(&send_mutex_);
  bool update_required = !ssrc_has_acked_;
  ssrc_has_acked_ = true;
  if (update_required) {
    UpdateHeaderSizes();
  }
}

void RTPSender::OnReceivedAckOnRtxSsrc(
    int64_t /* extended_highest_sequence_number */) {
  MutexLock lock(&send_mutex_);
  bool update_required = !rtx_ssrc_has_acked_;
  rtx_ssrc_has_acked_ = true;
  if (update_required) {
    UpdateHeaderSizes();
  }
}

void RTPSender::OnReceivedNack(
    const std::vector<uint16_t>& nack_sequence_numbers,
    int64_t avg_rtt) {
  packet_history_->SetRtt(TimeDelta::Millis(5 + avg_rtt));
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
  RTC_DCHECK_RUN_ON(&worker_checker_);
  // Only `sending_media_` requires `send_mutex_`, so check it last.
  return supports_bwe_extension_ && SendingMedia();
}

bool RTPSender::SupportsRtxPayloadPadding() const {
  RTC_DCHECK_RUN_ON(&worker_checker_);
  // Only `sending_media_` requires `send_mutex_`, so check it last.
  return supports_bwe_extension_ && (rtx_ & kRtxRedundantPayloads) &&
         SendingMedia();
}

std::vector<std::unique_ptr<RtpPacketToSend>> RTPSender::GeneratePadding(
    size_t target_size_bytes,
    bool media_has_been_sent,
    bool can_send_padding_on_media_ssrc) {
  RTC_DCHECK_RUN_ON(&worker_checker_);
  // This method does not actually send packets, it just generates
  // them and returns them to the caller, which puts them in the pacer queue.

  std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets;
  size_t bytes_left = target_size_bytes;
  if (SupportsRtxPayloadPadding()) {
    bytes_left = AppendPayloadPadding(target_size_bytes, padding_packets);
  }

  MutexLock lock(&send_mutex_);
  if (!sending_media_) {
    return {};
  }
  const bool rtx = rtx_ != kRtxOff;
  const size_t max_payload_size =
      max_packet_size_ - max_padding_fec_packet_header_;

  if (rtx) {
    // Without abs-send-time or transport sequence number a media packet
    // must be sent before padding so that the timestamps used for
    // estimation are correct.
    if (!media_has_been_sent &&
        !(rtp_header_extension_map_.IsRegistered(AbsoluteSendTime::kId) ||
          rtp_header_extension_map_.IsRegistered(
              TransportSequenceNumber::kId))) {
      return padding_packets;
    }
  } else if (!can_send_padding_on_media_ssrc) {
    return padding_packets;
  }

  const size_t padding_bytes_in_packet =
      PaddingBytesInPacket(audio_configured_, bytes_left, max_payload_size);
  while (bytes_left > 0) {
    padding_packets.push_back(BuildPaddingPacket(rtx, padding_bytes_in_packet));
    bytes_left -= std::min(bytes_left, padding_bytes_in_packet);
  }

  return padding_packets;
}

size_t RTPSender::AppendPayloadPadding(
    size_t target_size_bytes,
    std::vector<std::unique_ptr<RtpPacketToSend>>& padding_packets) {
  size_t bytes_left = target_size_bytes;
  while (bytes_left >= kMinPayloadPaddingBytes) {
    std::unique_ptr<RtpPacketToSend> packet =
        packet_history_->GetPayloadPaddingPacket(
            [&](const RtpPacketToSend& packet)
                -> std::unique_ptr<RtpPacketToSend> {
              // Limit overshoot, generate <= `kMaxPaddingSizeFactor` *
              // `target_size_bytes`.
              const size_t max_overshoot_bytes = static_cast<size_t>(
                  ((kMaxPaddingSizeFactor - 1.0) * target_size_bytes) + 0.5);
              if (packet.payload_size() + kRtxHeaderSize >
                  max_overshoot_bytes + bytes_left) {
                return nullptr;
              }
              return BuildRtxPacket(packet);
            });
    if (!packet) {
      break;
    }

    bytes_left -= std::min(bytes_left, packet->payload_size());
    packet->set_packet_type(RtpPacketMediaType::kPadding);
    padding_packets.push_back(std::move(packet));
  }
  return bytes_left;
}

std::unique_ptr<RtpPacketToSend> RTPSender::BuildPaddingPacket(
    bool rtx,
    size_t padding_size) const {
  auto padding_packet =
      std::make_unique<RtpPacketToSend>(&rtp_header_extension_map_);
  padding_packet->set_packet_type(RtpPacketMediaType::kPadding);
  padding_packet->SetMarker(false);
  if (rtx) {
    RTC_DCHECK(rtx_ssrc_);
    RTC_DCHECK(!rtx_payload_type_map_.empty());
    padding_packet->SetSsrc(*rtx_ssrc_);
    padding_packet->SetPayloadType(rtx_payload_type_map_.begin()->second);
  } else {
    padding_packet->SetSsrc(ssrc_);
  }
  AttachMidAndRid(rtx, *padding_packet);

  padding_packet->ReserveExtension<TransportSequenceNumber>();
  padding_packet->ReserveExtension<TransmissionOffset>();
  padding_packet->ReserveExtension<AbsoluteSendTime>();

  padding_packet->SetPadding(padding_size);
  return padding_packet;
}

void RTPSender::EnqueuePackets(
    std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
  RTC_DCHECK(!packets.empty());
  Timestamp now = clock_->CurrentTime();
  for (auto& packet : packets) {
    RTC_DCHECK(packet);
    RTC_CHECK(packet->packet_type().has_value())
        << "Packet type must be set before sending.";
    if (packet->capture_time() <= Timestamp::Zero()) {
      packet->set_capture_time(now);
    }
  }

  paced_sender_->EnqueuePackets(std::move(packets));
}

size_t RTPSender::FecOrPaddingPacketMaxRtpHeaderLength() const {
  MutexLock lock(&send_mutex_);
  return max_padding_fec_packet_header_;
}

size_t RTPSender::ExpectedPerPacketOverhead() const {
  MutexLock lock(&send_mutex_);
  return max_media_packet_header_;
}

std::unique_ptr<RtpPacketToSend> RTPSender::AllocatePacket(
    std::span<const uint32_t> csrcs) {
  MutexLock lock(&send_mutex_);
  UpdateMaxNumCsrcs(csrcs.size());
  return BuildMediaPacket(csrcs);
}

std::unique_ptr<RtpPacketToSend> RTPSender::BuildMediaPacket(
    std::span<const uint32_t> csrcs) const {
  auto packet = std::make_unique<RtpPacketToSend>(&rtp_header_extension_map_,
                                                  max_packet_size_);
  packet->SetSsrc(ssrc_);
  packet->SetCsrcs(csrcs);

  // Reserve extensions, if registered, RtpSender set in SendToNetwork.
  packet->ReserveExtension<AbsoluteSendTime>();
  packet->ReserveExtension<TransmissionOffset>();
  packet->ReserveExtension<TransportSequenceNumber>();

  AttachMidAndRid(/*rtx=*/false, *packet);
  return packet;
}

size_t RTPSender::RtxPacketOverhead() const {
  MutexLock lock(&send_mutex_);
  if (rtx_ == kRtxOff) {
    return 0;
  }
  size_t overhead = 0;

  // Count space for the RTP header extensions that might need to be added to
  // the RTX packet.
  if (!always_send_mid_and_rid_ && (!rtx_ssrc_has_acked_ && ssrc_has_acked_)) {
    // Prefer to reserve extra byte in case two byte header rtp header
    // extensions are used.
    static constexpr int kRtpExtensionHeaderSize = 2;

    // Rtx packets hasn't been acked and would need to have mid and rrsid rtp
    // header extensions, while media packets no longer needs to include mid and
    // rsid extensions.
    if (!mid_.empty()) {
      overhead += (kRtpExtensionHeaderSize + mid_.size());
    }
    if (!rid_.empty()) {
      overhead += (kRtpExtensionHeaderSize + rid_.size());
    }
    // RTP header extensions are rounded up to 4 bytes. Depending on already
    // present extensions adding mid & rrsid may add up to 3 bytes of padding.
    overhead += 3;
  }

  // Add two bytes for the original sequence number in the RTP payload.
  overhead += kRtxHeaderSize;
  return overhead;
}

void RTPSender::SetSendingMediaStatus(bool enabled) {
  MutexLock lock(&send_mutex_);
  sending_media_ = enabled;
}

bool RTPSender::SendingMedia() const {
  MutexLock lock(&send_mutex_);
  return sending_media_;
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
  rtx_packet->SetCsrcs(packet.Csrcs());
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

    // Empty extensions should be supported, so not checking `source.empty()`.
    if (!packet.HasExtension(extension)) {
      continue;
    }

    std::span<const uint8_t> source = packet.FindExtension(extension);

    std::span<uint8_t> destination =
        rtx_packet->AllocateExtension(extension, source.size());

    // Could happen if any:
    // 1. Extension has 0 length.
    // 2. Extension is not registered in destination.
    // 3. Allocating extension in destination failed.
    if (destination.empty() || source.size() != destination.size()) {
      continue;
    }

    std::memcpy(destination.data(), source.data(), destination.size());
  }
}

std::unique_ptr<RtpPacketToSend> RTPSender::BuildRtxPacket(
    const RtpPacketToSend& packet) {
  // `rtx_payload_type_map_` is const, so the RTX payload type is looked up
  // without holding `send_mutex_`.
  auto kv = rtx_payload_type_map_.find(packet.PayloadType());
  if (kv == rtx_payload_type_map_.end())
    return nullptr;

  std::unique_ptr<RtpPacketToSend> rtx_packet;

  // Add original RTP header.
  {
    MutexLock lock(&send_mutex_);
    if (!sending_media_)
      return nullptr;

    RTC_DCHECK(rtx_ssrc_);

    rtx_packet = std::make_unique<RtpPacketToSend>(&rtp_header_extension_map_,
                                                   max_packet_size_);

    // Replace payload type.
    rtx_packet->SetPayloadType(kv->second);

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
    AttachMidAndRid(/*rtx=*/true, *rtx_packet);
  }
  RTC_DCHECK(rtx_packet);

  uint8_t* rtx_payload =
      rtx_packet->AllocatePayload(packet.payload_size() + kRtxHeaderSize);
  RTC_CHECK(rtx_payload);

  // Add OSN (original sequence number).
  ByteWriter<uint16_t>::WriteBigEndian(rtx_payload, packet.SequenceNumber());

  // Add original payload data.
  auto payload = packet.payload();
  if (!payload.empty()) {
    memcpy(rtx_payload + kRtxHeaderSize, payload.data(), payload.size());
  }

  // Add original additional data.
  rtx_packet->set_additional_data(packet.additional_data());

  // Copy capture time so e.g. TransmissionOffset is correctly set.
  rtx_packet->set_capture_time(packet.capture_time());

  return rtx_packet;
}

RtpState RTPSender::GetRtpState() const {
  MutexLock lock(&send_mutex_);

  RtpState state;
  state.start_timestamp = timestamp_offset_;
  state.ssrc_has_acked = ssrc_has_acked_;
  return state;
}

RtpState RTPSender::GetRtxRtpState() const {
  MutexLock lock(&send_mutex_);

  RtpState state;
  state.start_timestamp = timestamp_offset_;
  state.ssrc_has_acked = rtx_ssrc_has_acked_;

  return state;
}

void RTPSender::UpdateHeaderSizes() {
  const size_t rtp_header_length =
      kRtpHeaderLength + sizeof(uint32_t) * max_num_csrcs_;

  max_padding_fec_packet_header_ =
      rtp_header_length + RtpHeaderExtensionSize(kFecOrPaddingExtensionSizes,
                                                 rtp_header_extension_map_);

  // RtpStreamId, Mid and RepairedRtpStreamId are treated specially in that
  // we check if they currently are being sent. RepairedRtpStreamId can be
  // sent instead of RtpStreamID on RTX packets and may share the same space.
  // When the primary SSRC has already been acked but the RTX SSRC has not
  // yet been acked, RepairedRtpStreamId needs to be taken into account
  // separately.
  const bool send_mid_rid_on_rtx =
      rtx_ssrc_.has_value() &&
      (always_send_mid_and_rid_ || !rtx_ssrc_has_acked_);
  const bool send_mid_rid = always_send_mid_and_rid_ || !ssrc_has_acked_;
  std::vector<RtpExtensionSize> non_volatile_extensions;
  for (auto& extension :
       audio_configured_ ? AudioExtensionSizes() : VideoExtensionSizes()) {
    if (IsNonVolatile(extension.type)) {
      switch (extension.type) {
        case RTPExtensionType::kRtpExtensionMid:
          if ((send_mid_rid || send_mid_rid_on_rtx) && !mid_.empty()) {
            non_volatile_extensions.push_back(extension);
          }
          break;
        case RTPExtensionType::kRtpExtensionRtpStreamId:
          if (send_mid_rid && !rid_.empty()) {
            non_volatile_extensions.push_back(extension);
          }
          break;
        case RTPExtensionType::kRtpExtensionRepairedRtpStreamId:
          if (send_mid_rid_on_rtx && !send_mid_rid && !rid_.empty()) {
            non_volatile_extensions.push_back(extension);
          }
          break;
        default:
          non_volatile_extensions.push_back(extension);
      }
    }
  }
  max_media_packet_header_ =
      rtp_header_length + RtpHeaderExtensionSize(non_volatile_extensions,
                                                 rtp_header_extension_map_);
  // Reserve extra bytes if packet might be resent in an rtx packet.
  if (rtx_ssrc_.has_value()) {
    max_media_packet_header_ += kRtxHeaderSize;
  }
}

void RTPSender::UpdateMaxNumCsrcs(size_t num_csrcs) {
  RTC_DCHECK_LE(num_csrcs, kRtpCsrcSize);
  if (num_csrcs > max_num_csrcs_) {
    max_num_csrcs_ = num_csrcs;
    UpdateHeaderSizes();
  }
}

void RTPSender::AttachMidAndRid(bool rtx, RtpPacketToSend& packet) const {
  // BUNDLE requires that the receiver "bind" the received SSRC to the values
  // in the MID and/or (R)RID header extensions if present. Therefore, the
  // sender can reduce overhead by omitting these header extensions once it
  // knows that the receiver has "bound" the SSRC.
  // This optimization can be configured by setting
  // `always_send_mid_and_rid_` appropriately.
  //
  // The algorithm here is fairly simple: Always attach a MID and/or RID (if
  // configured) to the outgoing packets until an RTCP receiver report comes
  // back for this SSRC. That feedback indicates the receiver must have
  // received a packet with the SSRC and header extension(s), so the sender
  // then stops attaching the MID and RID.
  const bool has_acked = rtx ? rtx_ssrc_has_acked_ : ssrc_has_acked_;
  const bool send_mid_and_rid = always_send_mid_and_rid_ || !has_acked;
  if (!send_mid_and_rid) {
    return;
  }
  // These are no-ops if the corresponding header extension is not registered.
  if (!mid_.empty()) {
    packet.SetExtension<RtpMid>(mid_);
  }
  if (!rid_.empty()) {
    if (rtx) {
      packet.SetExtension<RepairedRtpStreamId>(rid_);
    } else {
      packet.SetExtension<RtpStreamId>(rid_);
    }
  }
}
}  // namespace webrtc
