/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_format_vp8.h"

#include <stdint.h>
#include <string.h>  // memcpy
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/video_coding/codecs/interface/common_constants.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr int kXBit = 0x80;
constexpr int kNBit = 0x20;
constexpr int kSBit = 0x10;
constexpr int kKeyIdxField = 0x1F;
constexpr int kIBit = 0x80;
constexpr int kLBit = 0x40;
constexpr int kTBit = 0x20;
constexpr int kKBit = 0x10;
constexpr int kYBit = 0x20;

int ParseVP8PictureID(RTPVideoHeaderVP8* vp8,
                      const uint8_t** data,
                      size_t* data_length,
                      size_t* parsed_bytes) {
  if (*data_length == 0)
    return -1;

  vp8->pictureId = (**data & 0x7F);
  if (**data & 0x80) {
    (*data)++;
    (*parsed_bytes)++;
    if (--(*data_length) == 0)
      return -1;
    // PictureId is 15 bits
    vp8->pictureId = (vp8->pictureId << 8) + **data;
  }
  (*data)++;
  (*parsed_bytes)++;
  (*data_length)--;
  return 0;
}

int ParseVP8Tl0PicIdx(RTPVideoHeaderVP8* vp8,
                      const uint8_t** data,
                      size_t* data_length,
                      size_t* parsed_bytes) {
  if (*data_length == 0)
    return -1;

  vp8->tl0PicIdx = **data;
  (*data)++;
  (*parsed_bytes)++;
  (*data_length)--;
  return 0;
}

int ParseVP8TIDAndKeyIdx(RTPVideoHeaderVP8* vp8,
                         const uint8_t** data,
                         size_t* data_length,
                         size_t* parsed_bytes,
                         bool has_tid,
                         bool has_key_idx) {
  if (*data_length == 0)
    return -1;

  if (has_tid) {
    vp8->temporalIdx = ((**data >> 6) & 0x03);
    vp8->layerSync = (**data & 0x20) ? true : false;  // Y bit
  }
  if (has_key_idx) {
    vp8->keyIdx = (**data & 0x1F);
  }
  (*data)++;
  (*parsed_bytes)++;
  (*data_length)--;
  return 0;
}

int ParseVP8Extension(RTPVideoHeaderVP8* vp8,
                      const uint8_t* data,
                      size_t data_length) {
  RTC_DCHECK_GT(data_length, 0);
  size_t parsed_bytes = 0;
  // Optional X field is present.
  bool has_picture_id = (*data & 0x80) ? true : false;   // I bit
  bool has_tl0_pic_idx = (*data & 0x40) ? true : false;  // L bit
  bool has_tid = (*data & 0x20) ? true : false;          // T bit
  bool has_key_idx = (*data & 0x10) ? true : false;      // K bit

  // Advance data and decrease remaining payload size.
  data++;
  parsed_bytes++;
  data_length--;

  if (has_picture_id) {
    if (ParseVP8PictureID(vp8, &data, &data_length, &parsed_bytes) != 0) {
      return -1;
    }
  }

  if (has_tl0_pic_idx) {
    if (ParseVP8Tl0PicIdx(vp8, &data, &data_length, &parsed_bytes) != 0) {
      return -1;
    }
  }

  if (has_tid || has_key_idx) {
    if (ParseVP8TIDAndKeyIdx(vp8, &data, &data_length, &parsed_bytes, has_tid,
                             has_key_idx) != 0) {
      return -1;
    }
  }
  return static_cast<int>(parsed_bytes);
}

int ParseVP8FrameSize(RtpDepacketizer::ParsedPayload* parsed_payload,
                      const uint8_t* data,
                      size_t data_length) {
  if (parsed_payload->frame_type != kVideoFrameKey) {
    // Included in payload header for I-frames.
    return 0;
  }
  if (data_length < 10) {
    // For an I-frame we should always have the uncompressed VP8 header
    // in the beginning of the partition.
    return -1;
  }
  parsed_payload->video_header().width = ((data[7] << 8) + data[6]) & 0x3FFF;
  parsed_payload->video_header().height = ((data[9] << 8) + data[8]) & 0x3FFF;
  return 0;
}

bool ValidateHeader(const RTPVideoHeaderVP8& hdr_info) {
  if (hdr_info.pictureId != kNoPictureId) {
    RTC_DCHECK_GE(hdr_info.pictureId, 0);
    RTC_DCHECK_LE(hdr_info.pictureId, 0x7FFF);
  }
  if (hdr_info.tl0PicIdx != kNoTl0PicIdx) {
    RTC_DCHECK_GE(hdr_info.tl0PicIdx, 0);
    RTC_DCHECK_LE(hdr_info.tl0PicIdx, 0xFF);
  }
  if (hdr_info.temporalIdx != kNoTemporalIdx) {
    RTC_DCHECK_GE(hdr_info.temporalIdx, 0);
    RTC_DCHECK_LE(hdr_info.temporalIdx, 3);
  } else {
    RTC_DCHECK(!hdr_info.layerSync);
  }
  if (hdr_info.keyIdx != kNoKeyIdx) {
    RTC_DCHECK_GE(hdr_info.keyIdx, 0);
    RTC_DCHECK_LE(hdr_info.keyIdx, 0x1F);
  }
  return true;
}

}  // namespace

RtpPacketizerVp8::RtpPacketizerVp8(rtc::ArrayView<const uint8_t> payload,
                                   PayloadSizeLimits limits,
                                   const RTPVideoHeaderVP8& hdr_info)
    : hdr_(BuildHeader(hdr_info)), remaining_payload_(payload) {
  limits.max_payload_len -= hdr_.size();
  payload_sizes_ = SplitAboutEqually(payload.size(), limits);
  current_packet_ = payload_sizes_.begin();
}

RtpPacketizerVp8::~RtpPacketizerVp8() = default;

size_t RtpPacketizerVp8::NumPackets() const {
  return payload_sizes_.end() - current_packet_;
}

bool RtpPacketizerVp8::NextPacket(RtpPacketToSend* packet) {
  RTC_DCHECK(packet);
  if (current_packet_ == payload_sizes_.end()) {
    return false;
  }

  size_t packet_payload_len = *current_packet_;
  ++current_packet_;

  uint8_t* buffer = packet->AllocatePayload(hdr_.size() + packet_payload_len);
  RTC_CHECK(buffer);

  memcpy(buffer, hdr_.data(), hdr_.size());
  memcpy(buffer + hdr_.size(), remaining_payload_.data(), packet_payload_len);

  remaining_payload_ = remaining_payload_.subview(packet_payload_len);
  hdr_[0] &= (~kSBit);  //  Clear 'Start of partition' bit.
  packet->SetMarker(current_packet_ == payload_sizes_.end());
  return true;
}

// Write the VP8 payload descriptor.
//       0
//       0 1 2 3 4 5 6 7 8
//      +-+-+-+-+-+-+-+-+-+
//      |X| |N|S| PART_ID |
//      +-+-+-+-+-+-+-+-+-+
// X:   |I|L|T|K|         | (mandatory if any of the below are used)
//      +-+-+-+-+-+-+-+-+-+
// I:   |PictureID   (16b)| (optional)
//      +-+-+-+-+-+-+-+-+-+
// L:   |   TL0PIC_IDX    | (optional)
//      +-+-+-+-+-+-+-+-+-+
// T/K: |TID:Y|  KEYIDX   | (optional)
//      +-+-+-+-+-+-+-+-+-+
RtpPacketizerVp8::RawHeader RtpPacketizerVp8::BuildHeader(
    const RTPVideoHeaderVP8& header) {
  RTC_DCHECK(ValidateHeader(header));

  RawHeader result;
  bool tid_present = header.temporalIdx != kNoTemporalIdx;
  bool keyid_present = header.keyIdx != kNoKeyIdx;
  bool tl0_pid_present = header.tl0PicIdx != kNoTl0PicIdx;
  bool pid_present = header.pictureId != kNoPictureId;
  uint8_t x_field = 0;
  if (pid_present)
    x_field |= kIBit;
  if (tl0_pid_present)
    x_field |= kLBit;
  if (tid_present)
    x_field |= kTBit;
  if (keyid_present)
    x_field |= kKBit;

  uint8_t flags = 0;
  if (x_field != 0)
    flags |= kXBit;
  if (header.nonReference)
    flags |= kNBit;
  // Create header as first packet in the frame. NextPacket() will clear it
  // after first use.
  flags |= kSBit;
  result.push_back(flags);
  if (x_field == 0) {
    return result;
  }
  result.push_back(x_field);
  if (pid_present) {
    const uint16_t pic_id = static_cast<uint16_t>(header.pictureId);
    result.push_back(0x80 | ((pic_id >> 8) & 0x7F));
    result.push_back(pic_id & 0xFF);
  }
  if (tl0_pid_present) {
    result.push_back(header.tl0PicIdx);
  }
  if (tid_present || keyid_present) {
    uint8_t data_field = 0;
    if (tid_present) {
      data_field |= header.temporalIdx << 6;
      if (header.layerSync)
        data_field |= kYBit;
    }
    if (keyid_present) {
      data_field |= (header.keyIdx & kKeyIdxField);
    }
    result.push_back(data_field);
  }
  return result;
}

//
// VP8 format:
//
// Payload descriptor
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |X|R|N|S|PartID | (REQUIRED)
//      +-+-+-+-+-+-+-+-+
// X:   |I|L|T|K|  RSV  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// I:   |   PictureID   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// L:   |   TL0PICIDX   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// T/K: |TID:Y| KEYIDX  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
//
// Payload header (considered part of the actual payload, sent to decoder)
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |Size0|H| VER |P|
//      +-+-+-+-+-+-+-+-+
//      |      ...      |
//      +               +
bool RtpDepacketizerVp8::Parse(ParsedPayload* parsed_payload,
                               const uint8_t* payload_data,
                               size_t payload_data_length) {
  RTC_DCHECK(parsed_payload);
  if (payload_data_length == 0) {
    RTC_LOG(LS_ERROR) << "Empty payload.";
    return false;
  }

  // Parse mandatory first byte of payload descriptor.
  bool extension = (*payload_data & 0x80) ? true : false;               // X bit
  bool beginning_of_partition = (*payload_data & 0x10) ? true : false;  // S bit
  int partition_id = (*payload_data & 0x0F);  // PartID field

  parsed_payload->video_header().width = 0;
  parsed_payload->video_header().height = 0;
  parsed_payload->video_header().is_first_packet_in_frame =
      beginning_of_partition && (partition_id == 0);
  parsed_payload->video_header().simulcastIdx = 0;
  parsed_payload->video_header().codec = kVideoCodecVP8;
  auto& vp8_header = parsed_payload->video_header()
                         .video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.nonReference = (*payload_data & 0x20) ? true : false;  // N bit
  vp8_header.partitionId = partition_id;
  vp8_header.beginningOfPartition = beginning_of_partition;
  vp8_header.pictureId = kNoPictureId;
  vp8_header.tl0PicIdx = kNoTl0PicIdx;
  vp8_header.temporalIdx = kNoTemporalIdx;
  vp8_header.layerSync = false;
  vp8_header.keyIdx = kNoKeyIdx;

  if (partition_id > 8) {
    // Weak check for corrupt payload_data: PartID MUST NOT be larger than 8.
    return false;
  }

  // Advance payload_data and decrease remaining payload size.
  payload_data++;
  if (payload_data_length <= 1) {
    RTC_LOG(LS_ERROR) << "Error parsing VP8 payload descriptor!";
    return false;
  }
  payload_data_length--;

  if (extension) {
    const int parsed_bytes =
        ParseVP8Extension(&vp8_header, payload_data, payload_data_length);
    if (parsed_bytes < 0)
      return false;
    payload_data += parsed_bytes;
    payload_data_length -= parsed_bytes;
    if (payload_data_length == 0) {
      RTC_LOG(LS_ERROR) << "Error parsing VP8 payload descriptor!";
      return false;
    }
  }

  // Read P bit from payload header (only at beginning of first partition).
  if (beginning_of_partition && partition_id == 0) {
    parsed_payload->frame_type =
        (*payload_data & 0x01) ? kVideoFrameDelta : kVideoFrameKey;
  } else {
    parsed_payload->frame_type = kVideoFrameDelta;
  }

  if (ParseVP8FrameSize(parsed_payload, payload_data, payload_data_length) !=
      0) {
    return false;
  }

  parsed_payload->payload = payload_data;
  parsed_payload->payload_length = payload_data_length;
  return true;
}
}  // namespace webrtc
