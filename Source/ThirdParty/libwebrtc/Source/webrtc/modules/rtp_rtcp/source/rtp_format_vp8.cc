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

#include <string.h>  // memcpy

#include <limits>
#include <utility>
#include <vector>

#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
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
    if (ParseVP8TIDAndKeyIdx(
            vp8, &data, &data_length, &parsed_bytes, has_tid, has_key_idx) !=
        0) {
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
  parsed_payload->type.Video.width = ((data[7] << 8) + data[6]) & 0x3FFF;
  parsed_payload->type.Video.height = ((data[9] << 8) + data[8]) & 0x3FFF;
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

RtpPacketizerVp8::RtpPacketizerVp8(const RTPVideoHeaderVP8& hdr_info,
                                   size_t max_payload_len,
                                   size_t last_packet_reduction_len)
    : payload_data_(NULL),
      payload_size_(0),
      vp8_fixed_payload_descriptor_bytes_(1),
      hdr_info_(hdr_info),
      max_payload_len_(max_payload_len),
      last_packet_reduction_len_(last_packet_reduction_len) {
  RTC_DCHECK(ValidateHeader(hdr_info));
}

RtpPacketizerVp8::~RtpPacketizerVp8() {
}

size_t RtpPacketizerVp8::SetPayloadData(
    const uint8_t* payload_data,
    size_t payload_size,
    const RTPFragmentationHeader* /* fragmentation */) {
  payload_data_ = payload_data;
  payload_size_ = payload_size;
  if (GeneratePackets() < 0) {
    return 0;
  }
  return packets_.size();
}

bool RtpPacketizerVp8::NextPacket(RtpPacketToSend* packet) {
  RTC_DCHECK(packet);
  if (packets_.empty()) {
    return false;
  }
  InfoStruct packet_info = packets_.front();
  packets_.pop();

  uint8_t* buffer = packet->AllocatePayload(
      packets_.empty() ? max_payload_len_ - last_packet_reduction_len_
                       : max_payload_len_);
  int bytes = WriteHeaderAndPayload(packet_info, buffer, max_payload_len_);
  if (bytes < 0) {
    return false;
  }
  packet->SetPayloadSize(bytes);
  packet->SetMarker(packets_.empty());
  return true;
}

std::string RtpPacketizerVp8::ToString() {
  return "RtpPacketizerVp8";
}

int RtpPacketizerVp8::GeneratePackets() {
  if (max_payload_len_ < vp8_fixed_payload_descriptor_bytes_ +
                             PayloadDescriptorExtraLength() + 1 +
                             last_packet_reduction_len_) {
    // The provided payload length is not long enough for the payload
    // descriptor and one payload byte in the last packet.
    // Return an error.
    return -1;
  }

  size_t per_packet_capacity =
      max_payload_len_ -
      (vp8_fixed_payload_descriptor_bytes_ + PayloadDescriptorExtraLength());

  GeneratePacketsSplitPayloadBalanced(payload_size_, per_packet_capacity);
  return 0;
}

void RtpPacketizerVp8::GeneratePacketsSplitPayloadBalanced(size_t payload_len,
                                                           size_t capacity) {
  // Last packet of the last partition is smaller. Pretend that it's the same
  // size, but we must write more payload to it.
  size_t total_bytes = payload_len + last_packet_reduction_len_;
  // Integer divisions with rounding up.
  size_t num_packets_left = (total_bytes + capacity - 1) / capacity;
  size_t bytes_per_packet = total_bytes / num_packets_left;
  size_t num_larger_packets = total_bytes % num_packets_left;
  size_t remaining_data = payload_len;
  while (remaining_data > 0) {
    // Last num_larger_packets are 1 byte wider than the rest. Increase
    // per-packet payload size when needed.
    if (num_packets_left == num_larger_packets)
      ++bytes_per_packet;
    size_t current_packet_bytes = bytes_per_packet;
    if (current_packet_bytes > remaining_data) {
      current_packet_bytes = remaining_data;
    }
    // This is not the last packet in the whole payload, but there's no data
    // left for the last packet. Leave at least one byte for the last packet.
    if (num_packets_left == 2 && current_packet_bytes == remaining_data) {
      --current_packet_bytes;
    }
    QueuePacket(payload_len - remaining_data,
                current_packet_bytes, remaining_data == payload_len);
    remaining_data -= current_packet_bytes;
    --num_packets_left;
  }
}

void RtpPacketizerVp8::QueuePacket(size_t start_pos,
                                   size_t packet_size,
                                   bool first_packet) {
  // Write info to packet info struct and store in packet info queue.
  InfoStruct packet_info;
  packet_info.payload_start_pos = start_pos;
  packet_info.size = packet_size;
  packet_info.first_packet = first_packet;
  packets_.push(packet_info);
}

int RtpPacketizerVp8::WriteHeaderAndPayload(const InfoStruct& packet_info,
                                            uint8_t* buffer,
                                            size_t buffer_length) const {
  // Write the VP8 payload descriptor.
  //       0
  //       0 1 2 3 4 5 6 7 8
  //      +-+-+-+-+-+-+-+-+-+
  //      |X| |N|S| PART_ID |
  //      +-+-+-+-+-+-+-+-+-+
  // X:   |I|L|T|K|         | (mandatory if any of the below are used)
  //      +-+-+-+-+-+-+-+-+-+
  // I:   |PictureID (8/16b)| (optional)
  //      +-+-+-+-+-+-+-+-+-+
  // L:   |   TL0PIC_IDX    | (optional)
  //      +-+-+-+-+-+-+-+-+-+
  // T/K: |TID:Y|  KEYIDX   | (optional)
  //      +-+-+-+-+-+-+-+-+-+

  RTC_DCHECK_GT(packet_info.size, 0);
  buffer[0] = 0;
  if (XFieldPresent())
    buffer[0] |= kXBit;
  if (hdr_info_.nonReference)
    buffer[0] |= kNBit;
  if (packet_info.first_packet)
    buffer[0] |= kSBit;

  const int extension_length = WriteExtensionFields(buffer, buffer_length);
  if (extension_length < 0)
    return -1;

  memcpy(&buffer[vp8_fixed_payload_descriptor_bytes_ + extension_length],
         &payload_data_[packet_info.payload_start_pos],
         packet_info.size);

  // Return total length of written data.
  return packet_info.size + vp8_fixed_payload_descriptor_bytes_ +
         extension_length;
}

int RtpPacketizerVp8::WriteExtensionFields(uint8_t* buffer,
                                           size_t buffer_length) const {
  size_t extension_length = 0;
  if (XFieldPresent()) {
    uint8_t* x_field = buffer + vp8_fixed_payload_descriptor_bytes_;
    *x_field = 0;
    extension_length = 1;  // One octet for the X field.
    if (PictureIdPresent()) {
      if (WritePictureIDFields(
              x_field, buffer, buffer_length, &extension_length) < 0) {
        return -1;
      }
    }
    if (TL0PicIdxFieldPresent()) {
      if (WriteTl0PicIdxFields(
              x_field, buffer, buffer_length, &extension_length) < 0) {
        return -1;
      }
    }
    if (TIDFieldPresent() || KeyIdxFieldPresent()) {
      if (WriteTIDAndKeyIdxFields(
              x_field, buffer, buffer_length, &extension_length) < 0) {
        return -1;
      }
    }
    RTC_DCHECK_EQ(extension_length, PayloadDescriptorExtraLength());
  }
  return static_cast<int>(extension_length);
}

int RtpPacketizerVp8::WritePictureIDFields(uint8_t* x_field,
                                           uint8_t* buffer,
                                           size_t buffer_length,
                                           size_t* extension_length) const {
  *x_field |= kIBit;
  RTC_DCHECK_GE(buffer_length,
                vp8_fixed_payload_descriptor_bytes_ + *extension_length);
  const int pic_id_length = WritePictureID(
      buffer + vp8_fixed_payload_descriptor_bytes_ + *extension_length,
      buffer_length - vp8_fixed_payload_descriptor_bytes_ - *extension_length);
  if (pic_id_length < 0)
    return -1;
  *extension_length += pic_id_length;
  return 0;
}

int RtpPacketizerVp8::WritePictureID(uint8_t* buffer,
                                     size_t buffer_length) const {
  const uint16_t pic_id = static_cast<uint16_t>(hdr_info_.pictureId);
  size_t picture_id_len = PictureIdLength();
  if (picture_id_len > buffer_length)
    return -1;
  if (picture_id_len == 2) {
    buffer[0] = 0x80 | ((pic_id >> 8) & 0x7F);
    buffer[1] = pic_id & 0xFF;
  } else if (picture_id_len == 1) {
    buffer[0] = pic_id & 0x7F;
  }
  return static_cast<int>(picture_id_len);
}

int RtpPacketizerVp8::WriteTl0PicIdxFields(uint8_t* x_field,
                                           uint8_t* buffer,
                                           size_t buffer_length,
                                           size_t* extension_length) const {
  if (buffer_length <
      vp8_fixed_payload_descriptor_bytes_ + *extension_length + 1) {
    return -1;
  }
  *x_field |= kLBit;
  buffer[vp8_fixed_payload_descriptor_bytes_ + *extension_length] =
      hdr_info_.tl0PicIdx;
  ++*extension_length;
  return 0;
}

int RtpPacketizerVp8::WriteTIDAndKeyIdxFields(uint8_t* x_field,
                                              uint8_t* buffer,
                                              size_t buffer_length,
                                              size_t* extension_length) const {
  if (buffer_length <
      vp8_fixed_payload_descriptor_bytes_ + *extension_length + 1) {
    return -1;
  }
  uint8_t* data_field =
      &buffer[vp8_fixed_payload_descriptor_bytes_ + *extension_length];
  *data_field = 0;
  if (TIDFieldPresent()) {
    *x_field |= kTBit;
    *data_field |= hdr_info_.temporalIdx << 6;
    *data_field |= hdr_info_.layerSync ? kYBit : 0;
  }
  if (KeyIdxFieldPresent()) {
    *x_field |= kKBit;
    *data_field |= (hdr_info_.keyIdx & kKeyIdxField);
  }
  ++*extension_length;
  return 0;
}

size_t RtpPacketizerVp8::PayloadDescriptorExtraLength() const {
  size_t length_bytes = PictureIdLength();
  if (TL0PicIdxFieldPresent())
    ++length_bytes;
  if (TIDFieldPresent() || KeyIdxFieldPresent())
    ++length_bytes;
  if (length_bytes > 0)
    ++length_bytes;  // Include the extension field.
  return length_bytes;
}

size_t RtpPacketizerVp8::PictureIdLength() const {
  if (hdr_info_.pictureId == kNoPictureId) {
    return 0;
  }
  return 2;
}

bool RtpPacketizerVp8::XFieldPresent() const {
  return (TIDFieldPresent() || TL0PicIdxFieldPresent() || PictureIdPresent() ||
          KeyIdxFieldPresent());
}

bool RtpPacketizerVp8::TIDFieldPresent() const {
  return (hdr_info_.temporalIdx != kNoTemporalIdx);
}

bool RtpPacketizerVp8::KeyIdxFieldPresent() const {
  return (hdr_info_.keyIdx != kNoKeyIdx);
}

bool RtpPacketizerVp8::TL0PicIdxFieldPresent() const {
  return (hdr_info_.tl0PicIdx != kNoTl0PicIdx);
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

  parsed_payload->type.Video.width = 0;
  parsed_payload->type.Video.height = 0;
  parsed_payload->type.Video.is_first_packet_in_frame =
      beginning_of_partition && (partition_id == 0);
  parsed_payload->type.Video.simulcastIdx = 0;
  parsed_payload->type.Video.codec = kRtpVideoVp8;
  parsed_payload->type.Video.codecHeader.VP8.nonReference =
      (*payload_data & 0x20) ? true : false;  // N bit
  parsed_payload->type.Video.codecHeader.VP8.partitionId = partition_id;
  parsed_payload->type.Video.codecHeader.VP8.beginningOfPartition =
      beginning_of_partition;
  parsed_payload->type.Video.codecHeader.VP8.pictureId = kNoPictureId;
  parsed_payload->type.Video.codecHeader.VP8.tl0PicIdx = kNoTl0PicIdx;
  parsed_payload->type.Video.codecHeader.VP8.temporalIdx = kNoTemporalIdx;
  parsed_payload->type.Video.codecHeader.VP8.layerSync = false;
  parsed_payload->type.Video.codecHeader.VP8.keyIdx = kNoKeyIdx;

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
        ParseVP8Extension(&parsed_payload->type.Video.codecHeader.VP8,
                          payload_data,
                          payload_data_length);
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
