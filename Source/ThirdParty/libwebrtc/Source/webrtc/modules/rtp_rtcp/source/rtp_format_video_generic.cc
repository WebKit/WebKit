/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/logging.h"

namespace webrtc {

static const size_t kGenericHeaderLength = 1;

RtpPacketizerGeneric::RtpPacketizerGeneric(FrameType frame_type,
                                           size_t max_payload_len,
                                           size_t last_packet_reduction_len)
    : payload_data_(NULL),
      payload_size_(0),
      max_payload_len_(max_payload_len - kGenericHeaderLength),
      last_packet_reduction_len_(last_packet_reduction_len),
      frame_type_(frame_type),
      num_packets_left_(0),
      num_larger_packets_(0) {}

RtpPacketizerGeneric::~RtpPacketizerGeneric() {
}

size_t RtpPacketizerGeneric::SetPayloadData(
    const uint8_t* payload_data,
    size_t payload_size,
    const RTPFragmentationHeader* fragmentation) {
  payload_data_ = payload_data;
  payload_size_ = payload_size;

  // Fragment packets such that they are almost the same size, even accounting
  // for larger header in the last packet.
  // Since we are given how much extra space is occupied by the longer header
  // in the last packet, we can pretend that RTP headers are the same, but
  // there's last_packet_reduction_len_ virtual payload, to be put at the end of
  // the last packet.
  //
  size_t total_bytes = payload_size_ + last_packet_reduction_len_;

  // Minimum needed number of packets to fit payload and virtual payload in the
  // last packet.
  num_packets_left_ = (total_bytes + max_payload_len_ - 1) / max_payload_len_;
  // Given number of packets, calculate average size rounded down.
  payload_len_per_packet_ = total_bytes / num_packets_left_;
  // If we can't divide everything perfectly evenly, we put 1 extra byte in some
  // last packets: 14 bytes in 4 packets would be split as 3+3+4+4.
  num_larger_packets_ = total_bytes % num_packets_left_;
  RTC_DCHECK_LE(payload_len_per_packet_, max_payload_len_);

  generic_header_ = RtpFormatVideoGeneric::kFirstPacketBit;
  if (frame_type_ == kVideoFrameKey) {
    generic_header_ |= RtpFormatVideoGeneric::kKeyFrameBit;
  }
  return num_packets_left_;
}

bool RtpPacketizerGeneric::NextPacket(RtpPacketToSend* packet) {
  RTC_DCHECK(packet);
  if (num_packets_left_ == 0)
    return false;
  // Last larger_packets_ packets are 1 byte larger than previous packets.
  // Increase per packet payload once needed.
  if (num_packets_left_ == num_larger_packets_)
    ++payload_len_per_packet_;
  size_t next_packet_payload_len = payload_len_per_packet_;
  if (payload_size_ <= next_packet_payload_len) {
    // Whole payload fits into this packet.
    next_packet_payload_len = payload_size_;
    if (num_packets_left_ == 2) {
      // This is the penultimate packet. Leave at least 1 payload byte for the
      // last packet.
      --next_packet_payload_len;
      RTC_DCHECK_GT(next_packet_payload_len, 0);
    }
  }
  RTC_DCHECK_LE(next_packet_payload_len, max_payload_len_);

  uint8_t* out_ptr =
      packet->AllocatePayload(kGenericHeaderLength + next_packet_payload_len);
  // Put generic header in packet.
  out_ptr[0] = generic_header_;
  // Remove first-packet bit, following packets are intermediate.
  generic_header_ &= ~RtpFormatVideoGeneric::kFirstPacketBit;

  // Put payload in packet.
  memcpy(out_ptr + kGenericHeaderLength, payload_data_,
         next_packet_payload_len);
  payload_data_ += next_packet_payload_len;
  payload_size_ -= next_packet_payload_len;
  --num_packets_left_;
  // Packets left to produce and data left to split should end at the same time.
  RTC_DCHECK_EQ(num_packets_left_ == 0, payload_size_ == 0);

  packet->SetMarker(payload_size_ == 0);

  return true;
}

std::string RtpPacketizerGeneric::ToString() {
  return "RtpPacketizerGeneric";
}

bool RtpDepacketizerGeneric::Parse(ParsedPayload* parsed_payload,
                                   const uint8_t* payload_data,
                                   size_t payload_data_length) {
  assert(parsed_payload != NULL);
  if (payload_data_length == 0) {
    RTC_LOG(LS_ERROR) << "Empty payload.";
    return false;
  }

  uint8_t generic_header = *payload_data++;
  --payload_data_length;

  parsed_payload->frame_type =
      ((generic_header & RtpFormatVideoGeneric::kKeyFrameBit) != 0)
          ? kVideoFrameKey
          : kVideoFrameDelta;
  parsed_payload->type.Video.is_first_packet_in_frame =
      (generic_header & RtpFormatVideoGeneric::kFirstPacketBit) != 0;
  parsed_payload->type.Video.codec = kRtpVideoGeneric;
  parsed_payload->type.Video.width = 0;
  parsed_payload->type.Video.height = 0;

  parsed_payload->payload = payload_data;
  parsed_payload->payload_length = payload_data_length;
  return true;
}
}  // namespace webrtc
