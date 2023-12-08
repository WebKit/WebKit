/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_packetizer_h265.h"

#include <vector>

#include "absl/types/optional.h"
#ifndef WEBRTC_WEBKIT_BUILD
#include "common_video/h264/h264_common.h"
#endif
#include "common_video/h265/h265_common.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

// The payload header consists of the same
// fields (F, Type, LayerId and TID) as the NAL unit header. Refer to
// section 4.2 in RFC 7798.
constexpr size_t kH265PayloadHeaderSize = 2;
// Unlike H.264, H265 NAL header is 2-bytes.
constexpr size_t kH265NalHeaderSize = 2;
// H265's FU is constructed of 2-byte payload header, 1-byte FU header and FU
// payload.
constexpr size_t kH265FuHeaderSize = 1;
// The NALU size for H265 RTP aggregated packet indicates the size of the NAL
// unit is 2-bytes.
constexpr size_t kH265LengthFieldSize = 2;

enum H265NalHdrMasks {
  kH265FBit = 0x80,
  kH265TypeMask = 0x7E,
  kH265LayerIDHMask = 0x1,
  kH265LayerIDLMask = 0xF8,
  kH265TIDMask = 0x7,
  kH265TypeMaskN = 0x81,
  kH265TypeMaskInFuHeader = 0x3F
};

// Bit masks for FU headers.
enum H265FuBitmasks {
  kH265SBitMask = 0x80,
  kH265EBitMask = 0x40,
  kH265FuTypeBitMask = 0x3F
};

}  // namespace

RtpPacketizerH265::RtpPacketizerH265(rtc::ArrayView<const uint8_t> payload,
                                     PayloadSizeLimits limits)
    : limits_(limits), num_packets_left_(0) {
#ifdef WEBRTC_WEBKIT_BUILD
  for (const auto& nalu :
       H265::FindNaluIndices(payload.data(), payload.size())) {
    input_fragments_.push_back(
        payload.subview(nalu.payload_start_offset, nalu.payload_size));
  }
#else
  for (const auto& nalu :
       H264::FindNaluIndices(payload.data(), payload.size())) {
    input_fragments_.push_back(
        payload.subview(nalu.payload_start_offset, nalu.payload_size));
  }
#endif

  if (!GeneratePackets()) {
    // If failed to generate all the packets, discard already generated
    // packets in case the caller would ignore return value and still try to
    // call NextPacket().
    num_packets_left_ = 0;
    while (!packets_.empty()) {
      packets_.pop();
    }
  }
}

RtpPacketizerH265::~RtpPacketizerH265() = default;

size_t RtpPacketizerH265::NumPackets() const {
  return num_packets_left_;
}

bool RtpPacketizerH265::GeneratePackets() {
  for (size_t i = 0; i < input_fragments_.size();) {
    int fragment_len = input_fragments_[i].size();
    int single_packet_capacity = limits_.max_payload_len;
    if (input_fragments_.size() == 1) {
      single_packet_capacity -= limits_.single_packet_reduction_len;
    } else if (i == 0) {
      single_packet_capacity -= limits_.first_packet_reduction_len;
    } else if (i + 1 == input_fragments_.size()) {
      // Pretend that last fragment is larger instead of making last packet
      // smaller.
      single_packet_capacity -= limits_.last_packet_reduction_len;
    }
    if (fragment_len > single_packet_capacity) {
      if (!PacketizeFu(i)) {
        return false;
      }
      ++i;
    } else {
      if (!PacketizeSingleNalu(i)) {
        return false;
      }
      ++i;
    }
  }
  return true;
}

bool RtpPacketizerH265::PacketizeFu(size_t fragment_index) {
  // Fragment payload into packets (FU).
  // Strip out the original header and leave room for the FU header.
  rtc::ArrayView<const uint8_t> fragment = input_fragments_[fragment_index];
  PayloadSizeLimits limits = limits_;
  // Refer to section 4.4.3 in RFC7798, each FU fragment will have a 2-bytes
  // payload header and a one-byte FU header. DONL is not supported so ignore
  // its size when calculating max_payload_len.
  limits.max_payload_len -= kH265FuHeaderSize + kH265PayloadHeaderSize;

  // Update single/first/last packet reductions unless it is single/first/last
  // fragment.
  if (input_fragments_.size() != 1) {
    // if this fragment is put into a single packet, it might still be the
    // first or the last packet in the whole sequence of packets.
    if (fragment_index == input_fragments_.size() - 1) {
      limits.single_packet_reduction_len = limits_.last_packet_reduction_len;
    } else if (fragment_index == 0) {
      limits.single_packet_reduction_len = limits_.first_packet_reduction_len;
    } else {
      limits.single_packet_reduction_len = 0;
    }
  }
  if (fragment_index != 0) {
    limits.first_packet_reduction_len = 0;
  }
  if (fragment_index != input_fragments_.size() - 1) {
    limits.last_packet_reduction_len = 0;
  }

  // Strip out the original header.
  size_t payload_left = fragment.size() - kH265NalHeaderSize;
  int offset = kH265NalHeaderSize;

  std::vector<int> payload_sizes = SplitAboutEqually(payload_left, limits);
  if (payload_sizes.empty()) {
    return false;
  }

  for (size_t i = 0; i < payload_sizes.size(); ++i) {
    int packet_length = payload_sizes[i];
    RTC_CHECK_GT(packet_length, 0);
    uint16_t header = (fragment[0] << 8) | fragment[1];
    packets_.push({.source_fragment = fragment.subview(offset, packet_length),
                   .first_fragment = (i == 0),
                   .last_fragment = (i == payload_sizes.size() - 1),
                   .aggregated = false,
                   .header = header});
    offset += packet_length;
    payload_left -= packet_length;
  }
  num_packets_left_ += payload_sizes.size();
  RTC_CHECK_EQ(payload_left, 0);
  return true;
}

bool RtpPacketizerH265::PacketizeSingleNalu(size_t fragment_index) {
  // Add a single NALU to the queue, no aggregation.
  size_t payload_size_left = limits_.max_payload_len;
  if (input_fragments_.size() == 1) {
    payload_size_left -= limits_.single_packet_reduction_len;
  } else if (fragment_index == 0) {
    payload_size_left -= limits_.first_packet_reduction_len;
  } else if (fragment_index + 1 == input_fragments_.size()) {
    payload_size_left -= limits_.last_packet_reduction_len;
  }
  rtc::ArrayView<const uint8_t> fragment = input_fragments_[fragment_index];
  if (payload_size_left < fragment.size()) {
    RTC_LOG(LS_ERROR) << "Failed to fit a fragment to packet in SingleNalu "
                         "packetization mode. Payload size left "
                      << payload_size_left << ", fragment length "
                      << fragment.size() << ", packet capacity "
                      << limits_.max_payload_len;
    return false;
  }
#ifdef WEBRTC_WEBKIT_BUILD
  if (fragment.size() == 0u) {
    return false;
  }
#else
  RTC_CHECK_GT(fragment.size(), 0u);
#endif
  packets_.push({.source_fragment = fragment,
                   .first_fragment = true,
                   .last_fragment = true,
                   .aggregated = false,
                   .header = fragment[0]});
  ++num_packets_left_;
  return true;
}

int RtpPacketizerH265::PacketizeAp(size_t fragment_index) {
  // Aggregate fragments into one packet.
  size_t payload_size_left = limits_.max_payload_len;
  if (input_fragments_.size() == 1) {
    payload_size_left -= limits_.single_packet_reduction_len;
  } else if (fragment_index == 0) {
    payload_size_left -= limits_.first_packet_reduction_len;
  }
  int aggregated_fragments = 0;
  size_t fragment_headers_length = 0;
  rtc::ArrayView<const uint8_t> fragment = input_fragments_[fragment_index];
  RTC_CHECK_GE(payload_size_left, fragment.size());
  ++num_packets_left_;

  auto payload_size_needed = [&] {
    size_t fragment_size = fragment.size() + fragment_headers_length;
    if (input_fragments_.size() == 1) {
      // Single fragment, single packet, payload_size_left already adjusted
      // with limits_.single_packet_reduction_len.
      return fragment_size;
    }
    if (fragment_index == input_fragments_.size() - 1) {
      // Last fragment, so this might be the last packet.
      return fragment_size + limits_.last_packet_reduction_len;
    }
    return fragment_size;
  };

  while (payload_size_left >= payload_size_needed()) {
    RTC_CHECK_GT(fragment.size(), 0);
    packets_.push({.source_fragment = fragment,
                   .first_fragment = (aggregated_fragments == 0),
                   .last_fragment = false,
                   .aggregated = true,
                   .header = fragment[0]});
    payload_size_left -= fragment.size();
    payload_size_left -= fragment_headers_length;

    fragment_headers_length = kH265LengthFieldSize;
    // If we are going to try to aggregate more fragments into this packet
    // we need to add the AP NALU header and a length field for the first
    // NALU of this packet.
    if (aggregated_fragments == 0) {
      fragment_headers_length += kH265PayloadHeaderSize + kH265LengthFieldSize;
    }
    ++aggregated_fragments;

    // Next fragment.
    ++fragment_index;
    if (fragment_index == input_fragments_.size()) {
      break;
    }
    fragment = input_fragments_[fragment_index];
  }
  RTC_CHECK_GT(aggregated_fragments, 0);
  packets_.back().last_fragment = true;
  return fragment_index;
}

bool RtpPacketizerH265::NextPacket(RtpPacketToSend* rtp_packet) {
  RTC_DCHECK(rtp_packet);

  if (packets_.empty()) {
    return false;
  }

  PacketUnit packet = packets_.front();

  if (packet.first_fragment && packet.last_fragment) {
    // Single NAL unit packet. Do not support DONL for single NAL unit packets,
    // DONL field is not present.
    size_t bytes_to_send = packet.source_fragment.size();
    uint8_t* buffer = rtp_packet->AllocatePayload(bytes_to_send);
    memcpy(buffer, packet.source_fragment.data(), bytes_to_send);
    packets_.pop();
    input_fragments_.pop_front();
  } else if (packet.aggregated) {
    NextAggregatePacket(rtp_packet);
  } else {
    NextFragmentPacket(rtp_packet);
  }
  rtp_packet->SetMarker(packets_.empty());
  --num_packets_left_;
  return true;
}

void RtpPacketizerH265::NextAggregatePacket(RtpPacketToSend* rtp_packet) {
  size_t payload_capacity = rtp_packet->FreeCapacity();
  RTC_CHECK_GE(payload_capacity, kH265PayloadHeaderSize);
  uint8_t* buffer = rtp_packet->AllocatePayload(payload_capacity);
  RTC_CHECK(buffer);
  PacketUnit* packet = &packets_.front();
  RTC_CHECK(packet->first_fragment);

  /*
   +---------------+---------------+
   |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |F|    Type   |  LayerId  | TID |
   +-------------+-----------------+
  */
  // Refer to section section 4.4.2 for aggregation packets and modify type to
  // 48 in PayloadHdr for aggregate packet. Do not support DONL for aggregation
  // packets, DONL field is not present.
  uint8_t payload_hdr_h = packet->header >> 8;
  uint8_t payload_hdr_l = packet->header & 0xFF;
  uint8_t layer_id_h = payload_hdr_h & kH265LayerIDHMask;
  payload_hdr_h = (payload_hdr_h & kH265TypeMaskN) |
                  (H265::NaluType::kAp << 1) | layer_id_h;
  buffer[0] = payload_hdr_h;
  buffer[1] = payload_hdr_l;

  int index = kH265PayloadHeaderSize;
  bool is_last_fragment = packet->last_fragment;
  while (packet->aggregated) {
    // Add NAL unit length field.
    rtc::ArrayView<const uint8_t> fragment = packet->source_fragment;
#ifdef WEBRTC_WEBKIT_BUILD
    RTC_CHECK_LE(index + kH265LengthFieldSize + fragment.size(), payload_capacity);
#endif
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[index], fragment.size());
    index += kH265LengthFieldSize;
    // Add NAL unit.
    memcpy(&buffer[index], fragment.data(), fragment.size());
    index += fragment.size();
    packets_.pop();
    input_fragments_.pop_front();
    if (is_last_fragment) {
      break;
    }
    packet = &packets_.front();
    is_last_fragment = packet->last_fragment;
  }
  RTC_CHECK(is_last_fragment);
  rtp_packet->SetPayloadSize(index);
}

void RtpPacketizerH265::NextFragmentPacket(RtpPacketToSend* rtp_packet) {
  PacketUnit* packet = &packets_.front();
  // NAL unit fragmented over multiple packets (FU).
  // We do not send original NALU header, so it will be replaced by the
  // PayloadHdr of the first packet.
  /*
   +---------------+---------------+
   |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |F|    Type   |  LayerId  | TID |
   +-------------+-----------------+
  */
  // Refer to section section 4.4.3 for aggregation packets and modify type to
  // 49 in PayloadHdr for aggregate packet.
  uint8_t payload_hdr_h =
      packet->header >> 8;  // 1-bit F, 6-bit type, 1-bit layerID highest-bit
  uint8_t payload_hdr_l = packet->header & 0xFF;
  uint8_t layer_id_h = payload_hdr_h & kH265LayerIDHMask;
  uint8_t fu_header = 0;
  /*
   +---------------+
   |0|1|2|3|4|5|6|7|
   +-+-+-+-+-+-+-+-+
   |S|E|  FuType   |
   +---------------+
  */
  // S bit indicates the start of a fragmented NAL unit.
  // E bit indicates the end of a fragmented NAL unit.
  // FuType must be equal to the field type value of the fragmented NAL unit.
  fu_header |= (packet->first_fragment ? kH265SBitMask : 0);
  fu_header |= (packet->last_fragment ? kH265EBitMask : 0);
  uint8_t type = (payload_hdr_h & kH265TypeMask) >> 1;
  fu_header |= type;
  // Now update payload_hdr_h with FU type.
  payload_hdr_h = (payload_hdr_h & kH265TypeMaskN) |
                  (H265::NaluType::kFu << 1) | layer_id_h;
  rtc::ArrayView<const uint8_t> fragment = packet->source_fragment;
  uint8_t* buffer = rtp_packet->AllocatePayload(
      kH265FuHeaderSize + kH265PayloadHeaderSize + fragment.size());
  RTC_CHECK(buffer);
  buffer[0] = payload_hdr_h;
  buffer[1] = payload_hdr_l;
  buffer[2] = fu_header;

  // Do not support DONL for fragmentation units, DONL field is not present.
  memcpy(buffer + kH265FuHeaderSize + kH265PayloadHeaderSize, fragment.data(),
         fragment.size());
  if (packet->last_fragment) {
    input_fragments_.pop_front();
  }
  packets_.pop();
}

}  // namespace webrtc
