/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_video.h"

#include <stdlib.h>
#include <string.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

namespace {
constexpr size_t kRedForFecHeaderLength = 1;
constexpr size_t kRtpSequenceNumberMapMaxEntries = 1 << 13;
constexpr int64_t kMaxUnretransmittableFrameIntervalMs = 33 * 4;

// This is experimental field trial to exclude transport sequence number from
// FEC packets and should only be used in conjunction with datagram transport.
// Datagram transport removes transport sequence numbers from RTP packets and
// uses datagram feedback loop to re-generate RTCP feedback packets, but FEC
// contorol packets are calculated before sequence number is removed and as a
// result recovered packets will be corrupt unless we also remove transport
// sequence number during FEC calculation.
//
// TODO(sukhanov): We need to find find better way to implement FEC with
// datagram transport, probably moving FEC to datagram integration layter. We
// should also remove special field trial once we switch datagram path from
// RTCConfiguration flags to field trial and use the same field trial for FEC
// workaround.
const char kExcludeTransportSequenceNumberFromFecFieldTrial[] =
    "WebRTC-ExcludeTransportSequenceNumberFromFec";

void BuildRedPayload(const RtpPacketToSend& media_packet,
                     RtpPacketToSend* red_packet) {
  uint8_t* red_payload = red_packet->AllocatePayload(
      kRedForFecHeaderLength + media_packet.payload_size());
  RTC_DCHECK(red_payload);
  red_payload[0] = media_packet.PayloadType();

  auto media_payload = media_packet.payload();
  memcpy(&red_payload[kRedForFecHeaderLength], media_payload.data(),
         media_payload.size());
}

void AddRtpHeaderExtensions(const RTPVideoHeader& video_header,
                            const absl::optional<PlayoutDelay>& playout_delay,
                            VideoFrameType frame_type,
                            bool set_video_rotation,
                            bool set_color_space,
                            bool set_frame_marking,
                            bool first_packet,
                            bool last_packet,
                            RtpPacketToSend* packet) {
  // Color space requires two-byte header extensions if HDR metadata is
  // included. Therefore, it's best to add this extension first so that the
  // other extensions in the same packet are written as two-byte headers at
  // once.
  if (last_packet && set_color_space && video_header.color_space)
    packet->SetExtension<ColorSpaceExtension>(video_header.color_space.value());

  if (last_packet && set_video_rotation)
    packet->SetExtension<VideoOrientation>(video_header.rotation);

  // Report content type only for key frames.
  if (last_packet && frame_type == VideoFrameType::kVideoFrameKey &&
      video_header.content_type != VideoContentType::UNSPECIFIED)
    packet->SetExtension<VideoContentTypeExtension>(video_header.content_type);

  if (last_packet &&
      video_header.video_timing.flags != VideoSendTiming::kInvalid)
    packet->SetExtension<VideoTimingExtension>(video_header.video_timing);

  // If transmitted, add to all packets; ack logic depends on this.
  if (playout_delay) {
    packet->SetExtension<PlayoutDelayLimits>(*playout_delay);
  }

  if (set_frame_marking) {
    FrameMarking frame_marking = video_header.frame_marking;
    frame_marking.start_of_frame = first_packet;
    frame_marking.end_of_frame = last_packet;
    packet->SetExtension<FrameMarkingExtension>(frame_marking);
  }

  if (video_header.generic) {
    RtpGenericFrameDescriptor generic_descriptor;
    generic_descriptor.SetFirstPacketInSubFrame(first_packet);
    generic_descriptor.SetLastPacketInSubFrame(last_packet);
    generic_descriptor.SetDiscardable(video_header.generic->discardable);

    if (first_packet) {
      generic_descriptor.SetFrameId(
          static_cast<uint16_t>(video_header.generic->frame_id));
      for (int64_t dep : video_header.generic->dependencies) {
        generic_descriptor.AddFrameDependencyDiff(
            video_header.generic->frame_id - dep);
      }

      uint8_t spatial_bimask = 1 << video_header.generic->spatial_index;
      for (int layer : video_header.generic->higher_spatial_layers) {
        RTC_DCHECK_GT(layer, video_header.generic->spatial_index);
        RTC_DCHECK_LT(layer, 8);
        spatial_bimask |= 1 << layer;
      }
      generic_descriptor.SetSpatialLayersBitmask(spatial_bimask);

      generic_descriptor.SetTemporalLayer(video_header.generic->temporal_index);

      if (frame_type == VideoFrameType::kVideoFrameKey) {
        generic_descriptor.SetResolution(video_header.width,
                                         video_header.height);
      }
    }

    if (!packet->SetExtension<RtpGenericFrameDescriptorExtension01>(
            generic_descriptor)) {
      packet->SetExtension<RtpGenericFrameDescriptorExtension00>(
          generic_descriptor);
    }
  }
}

bool MinimizeDescriptor(const RTPVideoHeader& full, RTPVideoHeader* minimized) {
  if (full.codec == VideoCodecType::kVideoCodecVP8) {
    minimized->codec = VideoCodecType::kVideoCodecVP8;
    const auto& vp8 = absl::get<RTPVideoHeaderVP8>(full.video_type_header);
    // Set minimum fields the RtpPacketizer is using to create vp8 packets.
    auto& min_vp8 = minimized->video_type_header.emplace<RTPVideoHeaderVP8>();
    min_vp8.InitRTPVideoHeaderVP8();
    min_vp8.nonReference = vp8.nonReference;
    return true;
  }
  // TODO(danilchap): Reduce vp9 codec specific descriptor too.
  return false;
}

bool IsBaseLayer(const RTPVideoHeader& video_header) {
  switch (video_header.codec) {
    case kVideoCodecVP8: {
      const auto& vp8 =
          absl::get<RTPVideoHeaderVP8>(video_header.video_type_header);
      return (vp8.temporalIdx == 0 || vp8.temporalIdx == kNoTemporalIdx);
    }
    case kVideoCodecVP9: {
      const auto& vp9 =
          absl::get<RTPVideoHeaderVP9>(video_header.video_type_header);
      return (vp9.temporal_idx == 0 || vp9.temporal_idx == kNoTemporalIdx);
    }
    case kVideoCodecH264:
      // TODO(kron): Implement logic for H264 once WebRTC supports temporal
      // layers for H264.
      break;
    default:
      break;
  }
  return true;
}

const char* FrameTypeToString(VideoFrameType frame_type) {
  switch (frame_type) {
    case VideoFrameType::kEmptyFrame:
      return "empty";
    case VideoFrameType::kVideoFrameKey:
      return "video_key";
    case VideoFrameType::kVideoFrameDelta:
      return "video_delta";
    default:
      RTC_NOTREACHED();
      return "";
  }
}

}  // namespace

RTPSenderVideo::RTPSenderVideo(Clock* clock,
                               RTPSender* rtp_sender,
                               FlexfecSender* flexfec_sender,
                               PlayoutDelayOracle* playout_delay_oracle,
                               FrameEncryptorInterface* frame_encryptor,
                               bool require_frame_encryption,
                               bool need_rtp_packet_infos,
                               const WebRtcKeyValueConfig& field_trials)
    : rtp_sender_(rtp_sender),
      clock_(clock),
      retransmission_settings_(kRetransmitBaseLayer |
                               kConditionallyRetransmitHigherLayers),
      last_rotation_(kVideoRotation_0),
      transmit_color_space_next_frame_(false),
      playout_delay_oracle_(playout_delay_oracle),
      rtp_sequence_number_map_(need_rtp_packet_infos
                                   ? absl::make_unique<RtpSequenceNumberMap>(
                                         kRtpSequenceNumberMapMaxEntries)
                                   : nullptr),
      red_payload_type_(-1),
      ulpfec_payload_type_(-1),
      flexfec_sender_(flexfec_sender),
      delta_fec_params_{0, 1, kFecMaskRandom},
      key_fec_params_{0, 1, kFecMaskRandom},
      fec_bitrate_(1000, RateStatistics::kBpsScale),
      video_bitrate_(1000, RateStatistics::kBpsScale),
      packetization_overhead_bitrate_(1000, RateStatistics::kBpsScale),
      frame_encryptor_(frame_encryptor),
      require_frame_encryption_(require_frame_encryption),
      generic_descriptor_auth_experiment_(
          field_trials.Lookup("WebRTC-GenericDescriptorAuth").find("Enabled") ==
          0),
      exclude_transport_sequence_number_from_fec_experiment_(
          field_trials.Lookup(kExcludeTransportSequenceNumberFromFecFieldTrial)
              .find("Enabled") == 0) {
  RTC_DCHECK(playout_delay_oracle_);
}

RTPSenderVideo::~RTPSenderVideo() {}

void RTPSenderVideo::RegisterPayloadType(int8_t payload_type,
                                         absl::string_view payload_name,
                                         bool raw_payload) {
  absl::optional<VideoCodecType> video_type;
  if (!raw_payload) {
    if (absl::EqualsIgnoreCase(payload_name, "VP8")) {
      video_type = kVideoCodecVP8;
    } else if (absl::EqualsIgnoreCase(payload_name, "VP9")) {
      video_type = kVideoCodecVP9;
    } else if (absl::EqualsIgnoreCase(payload_name, "H264")) {
      video_type = kVideoCodecH264;
    } else {
      video_type = kVideoCodecGeneric;
    }
  }

  {
    rtc::CritScope cs(&payload_type_crit_);
    payload_type_map_[payload_type] = video_type;
  }

  // Backward compatibility for older receivers without temporal layer logic
  if (absl::EqualsIgnoreCase(payload_name, "H264")) {
    rtc::CritScope cs(&crit_);
    retransmission_settings_ = kRetransmitBaseLayer | kRetransmitHigherLayers;
  }
}

void RTPSenderVideo::SendVideoPacket(std::unique_ptr<RtpPacketToSend> packet) {
  // Remember some values about the packet before sending it away.
  size_t packet_size = packet->size();
  uint16_t seq_num = packet->SequenceNumber();
  packet->set_packet_type(RtpPacketToSend::Type::kVideo);
  if (!LogAndSendToNetwork(std::move(packet))) {
    RTC_LOG(LS_WARNING) << "Failed to send video packet " << seq_num;
    return;
  }
  rtc::CritScope cs(&stats_crit_);
  video_bitrate_.Update(packet_size, clock_->TimeInMilliseconds());
}

void RTPSenderVideo::SendVideoPacketAsRedMaybeWithUlpfec(
    std::unique_ptr<RtpPacketToSend> media_packet,
    bool protect_media_packet) {
  uint16_t media_seq_num = media_packet->SequenceNumber();

  std::unique_ptr<RtpPacketToSend> red_packet(
      new RtpPacketToSend(*media_packet));
  BuildRedPayload(*media_packet, red_packet.get());

  std::vector<std::unique_ptr<RedPacket>> fec_packets;
  {
    // Only protect while creating RED and FEC packets, not when sending.
    rtc::CritScope cs(&crit_);
    red_packet->SetPayloadType(red_payload_type_);
    if (ulpfec_enabled()) {
      if (protect_media_packet) {
        if (exclude_transport_sequence_number_from_fec_experiment_) {
          // See comments at the top of the file why experiment
          // "WebRTC-kExcludeTransportSequenceNumberFromFec" is needed in
          // conjunction with datagram transport.
          // TODO(sukhanov): We may also need to implement it for flexfec_sender
          // if we decide to keep this approach in the future.
          uint16_t transport_senquence_number;
          if (media_packet->GetExtension<webrtc::TransportSequenceNumber>(
                  &transport_senquence_number)) {
            if (!media_packet->RemoveExtension(
                    webrtc::TransportSequenceNumber::kId)) {
              RTC_NOTREACHED()
                  << "Failed to remove transport sequence number, packet="
                  << media_packet->ToString();
            }
          }
        }

        ulpfec_generator_.AddRtpPacketAndGenerateFec(
            media_packet->data(), media_packet->payload_size(),
            media_packet->headers_size());
      }
      uint16_t num_fec_packets = ulpfec_generator_.NumAvailableFecPackets();
      if (num_fec_packets > 0) {
        uint16_t first_fec_sequence_number =
            rtp_sender_->AllocateSequenceNumber(num_fec_packets);
        fec_packets = ulpfec_generator_.GetUlpfecPacketsAsRed(
            red_payload_type_, ulpfec_payload_type_, first_fec_sequence_number);
        RTC_DCHECK_EQ(num_fec_packets, fec_packets.size());
      }
    }
  }
  // Send |red_packet| instead of |packet| for allocated sequence number.
  size_t red_packet_size = red_packet->size();
  red_packet->set_packet_type(RtpPacketToSend::Type::kVideo);
  red_packet->set_allow_retransmission(media_packet->allow_retransmission());
  if (LogAndSendToNetwork(std::move(red_packet))) {
    rtc::CritScope cs(&stats_crit_);
    video_bitrate_.Update(red_packet_size, clock_->TimeInMilliseconds());
  } else {
    RTC_LOG(LS_WARNING) << "Failed to send RED packet " << media_seq_num;
  }
  for (const auto& fec_packet : fec_packets) {
    // TODO(danilchap): Make ulpfec_generator_ generate RtpPacketToSend to avoid
    // reparsing them.
    std::unique_ptr<RtpPacketToSend> rtp_packet(
        new RtpPacketToSend(*media_packet));
    RTC_CHECK(rtp_packet->Parse(fec_packet->data(), fec_packet->length()));
    rtp_packet->set_capture_time_ms(media_packet->capture_time_ms());
    rtp_packet->set_packet_type(RtpPacketToSend::Type::kForwardErrorCorrection);
    uint16_t fec_sequence_number = rtp_packet->SequenceNumber();
    rtp_packet->set_allow_retransmission(false);
    if (LogAndSendToNetwork(std::move(rtp_packet))) {
      rtc::CritScope cs(&stats_crit_);
      fec_bitrate_.Update(fec_packet->length(), clock_->TimeInMilliseconds());
    } else {
      RTC_LOG(LS_WARNING) << "Failed to send ULPFEC packet "
                          << fec_sequence_number;
    }
  }
}

void RTPSenderVideo::SendVideoPacketWithFlexfec(
    std::unique_ptr<RtpPacketToSend> media_packet,
    bool protect_media_packet) {
  RTC_DCHECK(flexfec_sender_);

  if (protect_media_packet)
    flexfec_sender_->AddRtpPacketAndGenerateFec(*media_packet);

  SendVideoPacket(std::move(media_packet));

  if (flexfec_sender_->FecAvailable()) {
    std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets =
        flexfec_sender_->GetFecPackets();
    for (auto& fec_packet : fec_packets) {
      size_t packet_length = fec_packet->size();
      uint16_t seq_num = fec_packet->SequenceNumber();
      fec_packet->set_packet_type(
          RtpPacketToSend::Type::kForwardErrorCorrection);
      fec_packet->set_allow_retransmission(false);
      if (LogAndSendToNetwork(std::move(fec_packet))) {
        rtc::CritScope cs(&stats_crit_);
        fec_bitrate_.Update(packet_length, clock_->TimeInMilliseconds());
      } else {
        RTC_LOG(LS_WARNING) << "Failed to send FlexFEC packet " << seq_num;
      }
    }
  }
}

bool RTPSenderVideo::LogAndSendToNetwork(
    std::unique_ptr<RtpPacketToSend> packet) {
#if BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
  int64_t now_ms = clock_->TimeInMilliseconds();
  BWE_TEST_LOGGING_PLOT_WITH_SSRC(1, "VideoTotBitrate_kbps", now_ms,
                                  rtp_sender_->ActualSendBitrateKbit(),
                                  packet->Ssrc());
  BWE_TEST_LOGGING_PLOT_WITH_SSRC(1, "VideoFecBitrate_kbps", now_ms,
                                  FecOverheadRate() / 1000, packet->Ssrc());
  BWE_TEST_LOGGING_PLOT_WITH_SSRC(1, "VideoNackBitrate_kbps", now_ms,
                                  rtp_sender_->NackOverheadRate() / 1000,
                                  packet->Ssrc());
#endif
  return rtp_sender_->SendToNetwork(std::move(packet));
}

void RTPSenderVideo::SetUlpfecConfig(int red_payload_type,
                                     int ulpfec_payload_type) {
  // Sanity check. Per the definition of UlpfecConfig (see config.h),
  // a payload type of -1 means that the corresponding feature is
  // turned off.
  RTC_DCHECK_GE(red_payload_type, -1);
  RTC_DCHECK_LE(red_payload_type, 127);
  RTC_DCHECK_GE(ulpfec_payload_type, -1);
  RTC_DCHECK_LE(ulpfec_payload_type, 127);

  rtc::CritScope cs(&crit_);
  red_payload_type_ = red_payload_type;
  ulpfec_payload_type_ = ulpfec_payload_type;

  // Must not enable ULPFEC without RED.
  RTC_DCHECK(!(red_enabled() ^ ulpfec_enabled()));

  // Reset FEC parameters.
  delta_fec_params_ = FecProtectionParams{0, 1, kFecMaskRandom};
  key_fec_params_ = FecProtectionParams{0, 1, kFecMaskRandom};
}

size_t RTPSenderVideo::CalculateFecPacketOverhead() const {
  if (flexfec_enabled())
    return flexfec_sender_->MaxPacketOverhead();

  size_t overhead = 0;
  if (red_enabled()) {
    // The RED overhead is due to a small header.
    overhead += kRedForFecHeaderLength;
  }
  if (ulpfec_enabled()) {
    // For ULPFEC, the overhead is the FEC headers plus RED for FEC header
    // (see above) plus anything in RTP header beyond the 12 bytes base header
    // (CSRC list, extensions...)
    // This reason for the header extensions to be included here is that
    // from an FEC viewpoint, they are part of the payload to be protected.
    // (The base RTP header is already protected by the FEC header.)
    overhead += ulpfec_generator_.MaxPacketOverhead() +
                (rtp_sender_->RtpHeaderLength() - kRtpHeaderSize);
  }
  return overhead;
}

void RTPSenderVideo::SetFecParameters(const FecProtectionParams& delta_params,
                                      const FecProtectionParams& key_params) {
  rtc::CritScope cs(&crit_);
  delta_fec_params_ = delta_params;
  key_fec_params_ = key_params;
}

absl::optional<uint32_t> RTPSenderVideo::FlexfecSsrc() const {
  if (flexfec_sender_) {
    return flexfec_sender_->ssrc();
  }
  return absl::nullopt;
}

bool RTPSenderVideo::SendVideo(
    VideoFrameType frame_type,
    int8_t payload_type,
    uint32_t rtp_timestamp,
    int64_t capture_time_ms,
    const uint8_t* payload_data,
    size_t payload_size,
    const RTPFragmentationHeader* fragmentation,
    const RTPVideoHeader* video_header,
    absl::optional<int64_t> expected_retransmission_time_ms) {
  TRACE_EVENT_ASYNC_STEP1("webrtc", "Video", capture_time_ms, "Send", "type",
                          FrameTypeToString(frame_type));

  if (frame_type == VideoFrameType::kEmptyFrame)
    return true;

  if (payload_size == 0)
    return false;
  RTC_CHECK(video_header);

  size_t fec_packet_overhead;
  bool red_enabled;
  int32_t retransmission_settings;
  bool set_video_rotation;
  bool set_color_space = false;
  bool set_frame_marking =
      video_header->codec == kVideoCodecH264 &&
      video_header->frame_marking.temporal_id != kNoTemporalIdx;

  const absl::optional<PlayoutDelay> playout_delay =
      playout_delay_oracle_->PlayoutDelayToSend(video_header->playout_delay);
  {
    rtc::CritScope cs(&crit_);
    // According to
    // http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/
    // ts_126114v120700p.pdf Section 7.4.5:
    // The MTSI client shall add the payload bytes as defined in this clause
    // onto the last RTP packet in each group of packets which make up a key
    // frame (I-frame or IDR frame in H.264 (AVC), or an IRAP picture in H.265
    // (HEVC)). The MTSI client may also add the payload bytes onto the last RTP
    // packet in each group of packets which make up another type of frame
    // (e.g. a P-Frame) only if the current value is different from the previous
    // value sent.
    // Set rotation when key frame or when changed (to follow standard).
    // Or when different from 0 (to follow current receiver implementation).
    set_video_rotation = frame_type == VideoFrameType::kVideoFrameKey ||
                         video_header->rotation != last_rotation_ ||
                         video_header->rotation != kVideoRotation_0;
    last_rotation_ = video_header->rotation;

    // Send color space when changed or if the frame is a key frame. Keep
    // sending color space information until the first base layer frame to
    // guarantee that the information is retrieved by the receiver.
    if (video_header->color_space != last_color_space_) {
      last_color_space_ = video_header->color_space;
      set_color_space = true;
      transmit_color_space_next_frame_ = !IsBaseLayer(*video_header);
    } else {
      set_color_space = frame_type == VideoFrameType::kVideoFrameKey ||
                        transmit_color_space_next_frame_;
      transmit_color_space_next_frame_ = transmit_color_space_next_frame_
                                             ? !IsBaseLayer(*video_header)
                                             : false;
    }

    // FEC settings.
    const FecProtectionParams& fec_params =
        frame_type == VideoFrameType::kVideoFrameKey ? key_fec_params_
                                                     : delta_fec_params_;
    if (flexfec_enabled())
      flexfec_sender_->SetFecParameters(fec_params);
    if (ulpfec_enabled())
      ulpfec_generator_.SetFecParameters(fec_params);

    fec_packet_overhead = CalculateFecPacketOverhead();
    red_enabled = this->red_enabled();
    retransmission_settings = retransmission_settings_;
  }

  // Maximum size of packet including rtp headers.
  // Extra space left in case packet will be resent using fec or rtx.
  int packet_capacity = rtp_sender_->MaxRtpPacketSize() - fec_packet_overhead -
                        (rtp_sender_->RtxStatus() ? kRtxHeaderSize : 0);

  std::unique_ptr<RtpPacketToSend> single_packet =
      rtp_sender_->AllocatePacket();
  RTC_DCHECK_LE(packet_capacity, single_packet->capacity());
  single_packet->SetPayloadType(payload_type);
  single_packet->SetTimestamp(rtp_timestamp);
  single_packet->set_capture_time_ms(capture_time_ms);

  auto first_packet = absl::make_unique<RtpPacketToSend>(*single_packet);
  auto middle_packet = absl::make_unique<RtpPacketToSend>(*single_packet);
  auto last_packet = absl::make_unique<RtpPacketToSend>(*single_packet);
  // Simplest way to estimate how much extensions would occupy is to set them.
  AddRtpHeaderExtensions(*video_header, playout_delay, frame_type,
                         set_video_rotation, set_color_space, set_frame_marking,
                         /*first=*/true, /*last=*/true, single_packet.get());
  AddRtpHeaderExtensions(*video_header, playout_delay, frame_type,
                         set_video_rotation, set_color_space, set_frame_marking,
                         /*first=*/true, /*last=*/false, first_packet.get());
  AddRtpHeaderExtensions(*video_header, playout_delay, frame_type,
                         set_video_rotation, set_color_space, set_frame_marking,
                         /*first=*/false, /*last=*/false, middle_packet.get());
  AddRtpHeaderExtensions(*video_header, playout_delay, frame_type,
                         set_video_rotation, set_color_space, set_frame_marking,
                         /*first=*/false, /*last=*/true, last_packet.get());

  RTC_DCHECK_GT(packet_capacity, single_packet->headers_size());
  RTC_DCHECK_GT(packet_capacity, first_packet->headers_size());
  RTC_DCHECK_GT(packet_capacity, middle_packet->headers_size());
  RTC_DCHECK_GT(packet_capacity, last_packet->headers_size());
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = packet_capacity - middle_packet->headers_size();

  RTC_DCHECK_GE(single_packet->headers_size(), middle_packet->headers_size());
  limits.single_packet_reduction_len =
      single_packet->headers_size() - middle_packet->headers_size();

  RTC_DCHECK_GE(first_packet->headers_size(), middle_packet->headers_size());
  limits.first_packet_reduction_len =
      first_packet->headers_size() - middle_packet->headers_size();

  RTC_DCHECK_GE(last_packet->headers_size(), middle_packet->headers_size());
  limits.last_packet_reduction_len =
      last_packet->headers_size() - middle_packet->headers_size();

  RTPVideoHeader minimized_video_header;
  const RTPVideoHeader* packetize_video_header = video_header;

  rtc::ArrayView<const uint8_t> generic_descriptor_raw_00 =
      first_packet->GetRawExtension<RtpGenericFrameDescriptorExtension00>();
  rtc::ArrayView<const uint8_t> generic_descriptor_raw_01 =
      first_packet->GetRawExtension<RtpGenericFrameDescriptorExtension01>();

  if (!generic_descriptor_raw_00.empty() &&
      !generic_descriptor_raw_01.empty()) {
    RTC_LOG(LS_WARNING) << "Two versions of GFD extension used.";
    return false;
  }

  rtc::ArrayView<const uint8_t> generic_descriptor_raw =
      !generic_descriptor_raw_01.empty() ? generic_descriptor_raw_01
                                         : generic_descriptor_raw_00;
  if (!generic_descriptor_raw.empty()) {
    if (MinimizeDescriptor(*video_header, &minimized_video_header)) {
      packetize_video_header = &minimized_video_header;
    }
  }

  // TODO(benwright@webrtc.org) - Allocate enough to always encrypt inline.
  rtc::Buffer encrypted_video_payload;
  if (frame_encryptor_ != nullptr) {
    if (generic_descriptor_raw.empty()) {
      return false;
    }

    const size_t max_ciphertext_size =
        frame_encryptor_->GetMaxCiphertextByteSize(cricket::MEDIA_TYPE_VIDEO,
                                                   payload_size);
    encrypted_video_payload.SetSize(max_ciphertext_size);

    size_t bytes_written = 0;

    // Only enable header authentication if the field trial is enabled.
    rtc::ArrayView<const uint8_t> additional_data;
    if (generic_descriptor_auth_experiment_) {
      additional_data = generic_descriptor_raw;
    }

    if (frame_encryptor_->Encrypt(
            cricket::MEDIA_TYPE_VIDEO, first_packet->Ssrc(), additional_data,
            rtc::MakeArrayView(payload_data, payload_size),
            encrypted_video_payload, &bytes_written) != 0) {
      return false;
    }

    encrypted_video_payload.SetSize(bytes_written);
    payload_data = encrypted_video_payload.data();
    payload_size = encrypted_video_payload.size();
  } else if (require_frame_encryption_) {
    RTC_LOG(LS_WARNING)
        << "No FrameEncryptor is attached to this video sending stream but "
        << "one is required since require_frame_encryptor is set";
  }

  absl::optional<VideoCodecType> type;
  {
    rtc::CritScope cs(&payload_type_crit_);
    const auto it = payload_type_map_.find(payload_type);
    if (it == payload_type_map_.end()) {
      RTC_LOG(LS_ERROR) << "Payload type " << static_cast<int>(payload_type)
                        << " not registered.";
      return false;
    }
    type = it->second;
  }
  std::unique_ptr<RtpPacketizer> packetizer = RtpPacketizer::Create(
      type, rtc::MakeArrayView(payload_data, payload_size), limits,
      *packetize_video_header, frame_type, fragmentation);

  const uint8_t temporal_id = GetTemporalId(*video_header);
  // TODO(bugs.webrtc.org/10714): retransmission_settings_ should generally be
  // replaced by expected_retransmission_time_ms.has_value(). For now, though,
  // only VP8 with an injected frame buffer controller actually controls it.
  const bool allow_retransmission =
      expected_retransmission_time_ms.has_value()
          ? AllowRetransmission(temporal_id, retransmission_settings,
                                expected_retransmission_time_ms.value())
          : false;
  const size_t num_packets = packetizer->NumPackets();

  size_t unpacketized_payload_size;
  if (fragmentation && fragmentation->fragmentationVectorSize > 0) {
    unpacketized_payload_size = 0;
    for (uint16_t i = 0; i < fragmentation->fragmentationVectorSize; ++i) {
      unpacketized_payload_size += fragmentation->fragmentationLength[i];
    }
  } else {
    unpacketized_payload_size = payload_size;
  }
  size_t packetized_payload_size = 0;

  if (num_packets == 0)
    return false;

  uint16_t first_sequence_number;
  bool first_frame = first_frame_sent_();
  for (size_t i = 0; i < num_packets; ++i) {
    std::unique_ptr<RtpPacketToSend> packet;
    int expected_payload_capacity;
    // Choose right packet template:
    if (num_packets == 1) {
      packet = std::move(single_packet);
      expected_payload_capacity =
          limits.max_payload_len - limits.single_packet_reduction_len;
    } else if (i == 0) {
      packet = std::move(first_packet);
      expected_payload_capacity =
          limits.max_payload_len - limits.first_packet_reduction_len;
    } else if (i == num_packets - 1) {
      packet = std::move(last_packet);
      expected_payload_capacity =
          limits.max_payload_len - limits.last_packet_reduction_len;
    } else {
      packet = absl::make_unique<RtpPacketToSend>(*middle_packet);
      expected_payload_capacity = limits.max_payload_len;
    }

    if (!packetizer->NextPacket(packet.get()))
      return false;
    RTC_DCHECK_LE(packet->payload_size(), expected_payload_capacity);
    if (!rtp_sender_->AssignSequenceNumber(packet.get()))
      return false;
    packetized_payload_size += packet->payload_size();

    if (rtp_sequence_number_map_ && i == 0) {
      first_sequence_number = packet->SequenceNumber();
    }

    if (i == 0) {
      playout_delay_oracle_->OnSentPacket(packet->SequenceNumber(),
                                          playout_delay);
    }
    // No FEC protection for upper temporal layers, if used.
    bool protect_packet = temporal_id == 0 || temporal_id == kNoTemporalIdx;

    packet->set_allow_retransmission(allow_retransmission);

    // Put packetization finish timestamp into extension.
    if (packet->HasExtension<VideoTimingExtension>()) {
      packet->set_packetization_finish_time_ms(clock_->TimeInMilliseconds());
      // TODO(webrtc:10750): wait a couple of months and remove the statement
      // below. For now we can't use packets with VideoTimingFrame extensions in
      // Fec because the extension is modified after FEC is calculated by pacer
      // and network. This may cause corruptions in video payload and header.
      // The fix in receive code is implemented, but until all the receivers
      // are updated, senders can't send potentially breaking packets.
      protect_packet = false;
    }

    if (flexfec_enabled()) {
      // TODO(brandtr): Remove the FlexFEC code path when FlexfecSender
      // is wired up to PacedSender instead.
      SendVideoPacketWithFlexfec(std::move(packet), protect_packet);
    } else if (red_enabled) {
      SendVideoPacketAsRedMaybeWithUlpfec(std::move(packet), protect_packet);
    } else {
      SendVideoPacket(std::move(packet));
    }

    if (first_frame) {
      if (i == 0) {
        RTC_LOG(LS_INFO)
            << "Sent first RTP packet of the first video frame (pre-pacer)";
      }
      if (i == num_packets - 1) {
        RTC_LOG(LS_INFO)
            << "Sent last RTP packet of the first video frame (pre-pacer)";
      }
    }
  }

  if (rtp_sequence_number_map_) {
    const uint32_t timestamp = rtp_timestamp - rtp_sender_->TimestampOffset();
    rtc::CritScope cs(&crit_);
    rtp_sequence_number_map_->InsertFrame(first_sequence_number, num_packets,
                                          timestamp);
  }

  rtc::CritScope cs(&stats_crit_);
  RTC_DCHECK_GE(packetized_payload_size, unpacketized_payload_size);
  packetization_overhead_bitrate_.Update(
      packetized_payload_size - unpacketized_payload_size,
      clock_->TimeInMilliseconds());

  TRACE_EVENT_ASYNC_END1("webrtc", "Video", capture_time_ms, "timestamp",
                         rtp_timestamp);
  return true;
}

uint32_t RTPSenderVideo::VideoBitrateSent() const {
  rtc::CritScope cs(&stats_crit_);
  return video_bitrate_.Rate(clock_->TimeInMilliseconds()).value_or(0);
}

uint32_t RTPSenderVideo::FecOverheadRate() const {
  rtc::CritScope cs(&stats_crit_);
  return fec_bitrate_.Rate(clock_->TimeInMilliseconds()).value_or(0);
}

uint32_t RTPSenderVideo::PacketizationOverheadBps() const {
  rtc::CritScope cs(&stats_crit_);
  return packetization_overhead_bitrate_.Rate(clock_->TimeInMilliseconds())
      .value_or(0);
}

std::vector<RtpSequenceNumberMap::Info> RTPSenderVideo::GetSentRtpPacketInfos(
    rtc::ArrayView<const uint16_t> sequence_numbers) const {
  RTC_DCHECK(!sequence_numbers.empty());

  std::vector<RtpSequenceNumberMap::Info> results;
  if (!rtp_sequence_number_map_) {
    return results;
  }
  results.reserve(sequence_numbers.size());

  {
    rtc::CritScope cs(&crit_);
    for (uint16_t sequence_number : sequence_numbers) {
      const absl::optional<RtpSequenceNumberMap::Info> info =
          rtp_sequence_number_map_->Get(sequence_number);
      if (!info) {
        // The empty vector will be returned. We can delay the clearing
        // of the vector until after we exit the critical section.
        break;
      }
      results.push_back(*info);
    }
  }

  if (results.size() != sequence_numbers.size()) {
    results.clear();  // Some sequence number was not found.
  }

  return results;
}

bool RTPSenderVideo::AllowRetransmission(
    uint8_t temporal_id,
    int32_t retransmission_settings,
    int64_t expected_retransmission_time_ms) {
  if (retransmission_settings == kRetransmitOff)
    return false;

  rtc::CritScope cs(&stats_crit_);
  // Media packet storage.
  if ((retransmission_settings & kConditionallyRetransmitHigherLayers) &&
      UpdateConditionalRetransmit(temporal_id,
                                  expected_retransmission_time_ms)) {
    retransmission_settings |= kRetransmitHigherLayers;
  }

  if (temporal_id == kNoTemporalIdx)
    return true;

  if ((retransmission_settings & kRetransmitBaseLayer) && temporal_id == 0)
    return true;

  if ((retransmission_settings & kRetransmitHigherLayers) && temporal_id > 0)
    return true;

  return false;
}

uint8_t RTPSenderVideo::GetTemporalId(const RTPVideoHeader& header) {
  struct TemporalIdGetter {
    uint8_t operator()(const RTPVideoHeaderVP8& vp8) { return vp8.temporalIdx; }
    uint8_t operator()(const RTPVideoHeaderVP9& vp9) {
      return vp9.temporal_idx;
    }
    uint8_t operator()(const RTPVideoHeaderH264&) { return kNoTemporalIdx; }
    uint8_t operator()(const absl::monostate&) { return kNoTemporalIdx; }
  };
  switch (header.codec) {
    case kVideoCodecH264:
      return header.frame_marking.temporal_id;
    default:
      return absl::visit(TemporalIdGetter(), header.video_type_header);
  }
}

bool RTPSenderVideo::UpdateConditionalRetransmit(
    uint8_t temporal_id,
    int64_t expected_retransmission_time_ms) {
  int64_t now_ms = clock_->TimeInMilliseconds();
  // Update stats for any temporal layer.
  TemporalLayerStats* current_layer_stats =
      &frame_stats_by_temporal_layer_[temporal_id];
  current_layer_stats->frame_rate_fp1000s.Update(1, now_ms);
  int64_t tl_frame_interval = now_ms - current_layer_stats->last_frame_time_ms;
  current_layer_stats->last_frame_time_ms = now_ms;

  // Conditional retransmit only applies to upper layers.
  if (temporal_id != kNoTemporalIdx && temporal_id > 0) {
    if (tl_frame_interval >= kMaxUnretransmittableFrameIntervalMs) {
      // Too long since a retransmittable frame in this layer, enable NACK
      // protection.
      return true;
    } else {
      // Estimate when the next frame of any lower layer will be sent.
      const int64_t kUndefined = std::numeric_limits<int64_t>::max();
      int64_t expected_next_frame_time = kUndefined;
      for (int i = temporal_id - 1; i >= 0; --i) {
        TemporalLayerStats* stats = &frame_stats_by_temporal_layer_[i];
        absl::optional<uint32_t> rate = stats->frame_rate_fp1000s.Rate(now_ms);
        if (rate) {
          int64_t tl_next = stats->last_frame_time_ms + 1000000 / *rate;
          if (tl_next - now_ms > -expected_retransmission_time_ms &&
              tl_next < expected_next_frame_time) {
            expected_next_frame_time = tl_next;
          }
        }
      }

      if (expected_next_frame_time == kUndefined ||
          expected_next_frame_time - now_ms > expected_retransmission_time_ms) {
        // The next frame in a lower layer is expected at a later time (or
        // unable to tell due to lack of data) than a retransmission is
        // estimated to be able to arrive, so allow this packet to be nacked.
        return true;
      }
    }
  }

  return false;
}

}  // namespace webrtc
