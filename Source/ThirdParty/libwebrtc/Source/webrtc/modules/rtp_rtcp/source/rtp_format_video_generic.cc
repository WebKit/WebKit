/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <string.h>

#include "absl/types/optional.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

static const size_t kGenericHeaderLength = 1;
static const size_t kExtendedHeaderLength = 2;

RtpPacketizerGeneric::RtpPacketizerGeneric(
    rtc::ArrayView<const uint8_t> payload,
    PayloadSizeLimits limits,
    const RTPVideoHeader& rtp_video_header,
    FrameType frame_type)
    : remaining_payload_(payload) {
  BuildHeader(rtp_video_header, frame_type);

  limits.max_payload_len -= header_size_;
  payload_sizes_ = SplitAboutEqually(payload.size(), limits);
  current_packet_ = payload_sizes_.begin();
}

RtpPacketizerGeneric::~RtpPacketizerGeneric() = default;

size_t RtpPacketizerGeneric::NumPackets() const {
  return payload_sizes_.end() - current_packet_;
}

bool RtpPacketizerGeneric::NextPacket(RtpPacketToSend* packet) {
  RTC_DCHECK(packet);
  if (current_packet_ == payload_sizes_.end())
    return false;

  size_t next_packet_payload_len = *current_packet_;

  uint8_t* out_ptr =
      packet->AllocatePayload(header_size_ + next_packet_payload_len);
  RTC_CHECK(out_ptr);

  memcpy(out_ptr, header_, header_size_);
  memcpy(out_ptr + header_size_, remaining_payload_.data(),
         next_packet_payload_len);

  // Remove first-packet bit, following packets are intermediate.
  header_[0] &= ~RtpFormatVideoGeneric::kFirstPacketBit;

  remaining_payload_ = remaining_payload_.subview(next_packet_payload_len);

  ++current_packet_;

  // Packets left to produce and data left to split should end at the same time.
  RTC_DCHECK_EQ(current_packet_ == payload_sizes_.end(),
                remaining_payload_.empty());

  packet->SetMarker(remaining_payload_.empty());
  return true;
}

void RtpPacketizerGeneric::BuildHeader(const RTPVideoHeader& rtp_video_header,
                                       FrameType frame_type) {
  header_size_ = kGenericHeaderLength;
  header_[0] = RtpFormatVideoGeneric::kFirstPacketBit;
  if (frame_type == kVideoFrameKey) {
    header_[0] |= RtpFormatVideoGeneric::kKeyFrameBit;
  }
  if (rtp_video_header.generic.has_value()) {
    // Store bottom 15 bits of the the picture id. Only 15 bits are used for
    // compatibility with other packetizer implemenetations.
    uint16_t picture_id = rtp_video_header.generic->frame_id & 0x7FFF;
    header_[0] |= RtpFormatVideoGeneric::kExtendedHeaderBit;
    header_[1] = (picture_id >> 8) & 0x7F;
    header_[2] = picture_id & 0xFF;
    header_size_ += kExtendedHeaderLength;
  }
}

RtpDepacketizerGeneric::~RtpDepacketizerGeneric() = default;

bool RtpDepacketizerGeneric::Parse(ParsedPayload* parsed_payload,
                                   const uint8_t* payload_data,
                                   size_t payload_data_length) {
  assert(parsed_payload != NULL);
  if (payload_data_length == 0) {
    RTC_LOG(LS_WARNING) << "Empty payload.";
    return false;
  }

  uint8_t generic_header = *payload_data++;
  --payload_data_length;

  parsed_payload->frame_type =
      ((generic_header & RtpFormatVideoGeneric::kKeyFrameBit) != 0)
          ? kVideoFrameKey
          : kVideoFrameDelta;
  parsed_payload->video_header().is_first_packet_in_frame =
      (generic_header & RtpFormatVideoGeneric::kFirstPacketBit) != 0;
  parsed_payload->video_header().codec = kVideoCodecGeneric;
  parsed_payload->video_header().width = 0;
  parsed_payload->video_header().height = 0;

  if (generic_header & RtpFormatVideoGeneric::kExtendedHeaderBit) {
    if (payload_data_length < kExtendedHeaderLength) {
      RTC_LOG(LS_WARNING) << "Too short payload for generic header.";
      return false;
    }
    parsed_payload->video_header().generic.emplace();
    parsed_payload->video_header().generic->frame_id =
        ((payload_data[0] & 0x7F) << 8) | payload_data[1];
    payload_data += kExtendedHeaderLength;
    payload_data_length -= kExtendedHeaderLength;
  }

  parsed_payload->payload = payload_data;
  parsed_payload->payload_length = payload_data_length;
  return true;
}
}  // namespace webrtc
