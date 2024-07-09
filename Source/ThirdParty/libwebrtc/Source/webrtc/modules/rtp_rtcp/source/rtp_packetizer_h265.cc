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
#include "common_video/h264/h264_common.h"
#include "common_video/h265/h265_common.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_packet_h265_common.h"
#include "rtc_base/logging.h"

namespace webrtc {

RtpPacketizerH265::RtpPacketizerH265(rtc::ArrayView<const uint8_t> payload,
                                     PayloadSizeLimits limits)
    : limits_(limits), num_packets_left_(0) {
  for (const auto& nalu :
       H264::FindNaluIndices(payload.data(), payload.size())) {
#if WEBRTC_WEBKIT_BUILD
    if (!nalu.payload_size) {
      input_fragments_.clear();
      return;
    }
#endif
    input_fragments_.push_back(
        payload.subview(nalu.payload_start_offset, nalu.payload_size));
  }

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
      i = PacketizeAp(i);
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
  limits.max_payload_len -=
      kH265FuHeaderSizeBytes + kH265PayloadHeaderSizeBytes;

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
  size_t payload_left = fragment.size() - kH265NalHeaderSizeBytes;
  int offset = kH265NalHeaderSizeBytes;

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

    fragment_headers_length = kH265LengthFieldSizeBytes;
    // If we are going to try to aggregate more fragments into this packet
    // we need to add the AP NALU header and a length field for the first
    // NALU of this packet.
    if (aggregated_fragments == 0) {
      fragment_headers_length +=
          kH265PayloadHeaderSizeBytes + kH265LengthFieldSizeBytes;
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
  RTC_CHECK_GE(payload_capacity, kH265PayloadHeaderSizeBytes);
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

  int index = kH265PayloadHeaderSizeBytes;
  bool is_last_fragment = packet->last_fragment;
  while (packet->aggregated) {
    // Add NAL unit length field.
    rtc::ArrayView<const uint8_t> fragment = packet->source_fragment;
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[index], fragment.size());
    index += kH265LengthFieldSizeBytes;
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
      kH265FuHeaderSizeBytes + kH265PayloadHeaderSizeBytes + fragment.size());
  RTC_CHECK(buffer);
  buffer[0] = payload_hdr_h;
  buffer[1] = payload_hdr_l;
  buffer[2] = fu_header;

  // Do not support DONL for fragmentation units, DONL field is not present.
  memcpy(buffer + kH265FuHeaderSizeBytes + kH265PayloadHeaderSizeBytes,
         fragment.data(), fragment.size());
  if (packet->last_fragment) {
    input_fragments_.pop_front();
  }
  packets_.pop();
}

}  // namespace webrtc
