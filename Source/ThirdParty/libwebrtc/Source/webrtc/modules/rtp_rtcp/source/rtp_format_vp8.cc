/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp8.h"

#include <assert.h>  // assert
#include <string.h>  // memcpy

#include <vector>

#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/source/vp8_partition_aggregator.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_to_send.h"

namespace webrtc {
namespace {
int ParseVP8PictureID(RTPVideoHeaderVP8* vp8,
                      const uint8_t** data,
                      size_t* data_length,
                      size_t* parsed_bytes) {
  assert(vp8 != NULL);
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
  assert(vp8 != NULL);
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
  assert(vp8 != NULL);
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
  assert(vp8 != NULL);
  assert(data_length > 0);
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
  assert(parsed_payload != NULL);
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
}  // namespace

// Define how the VP8PacketizerModes are implemented.
// Modes are: kStrict, kAggregate, kEqualSize.
const RtpPacketizerVp8::AggregationMode RtpPacketizerVp8::aggr_modes_
    [kNumModes] = {kAggrNone, kAggrPartitions, kAggrFragments};
const bool RtpPacketizerVp8::balance_modes_[kNumModes] = {true, true, true};
const bool RtpPacketizerVp8::separate_first_modes_[kNumModes] = {true, false,
                                                                 false};

RtpPacketizerVp8::RtpPacketizerVp8(const RTPVideoHeaderVP8& hdr_info,
                                   size_t max_payload_len,
                                   VP8PacketizerMode mode)
    : payload_data_(NULL),
      payload_size_(0),
      vp8_fixed_payload_descriptor_bytes_(1),
      aggr_mode_(aggr_modes_[mode]),
      balance_(balance_modes_[mode]),
      separate_first_(separate_first_modes_[mode]),
      hdr_info_(hdr_info),
      num_partitions_(0),
      max_payload_len_(max_payload_len),
      packets_calculated_(false) {
}

RtpPacketizerVp8::RtpPacketizerVp8(const RTPVideoHeaderVP8& hdr_info,
                                   size_t max_payload_len)
    : payload_data_(NULL),
      payload_size_(0),
      part_info_(),
      vp8_fixed_payload_descriptor_bytes_(1),
      aggr_mode_(aggr_modes_[kEqualSize]),
      balance_(balance_modes_[kEqualSize]),
      separate_first_(separate_first_modes_[kEqualSize]),
      hdr_info_(hdr_info),
      num_partitions_(0),
      max_payload_len_(max_payload_len),
      packets_calculated_(false) {
}

RtpPacketizerVp8::~RtpPacketizerVp8() {
}

void RtpPacketizerVp8::SetPayloadData(
    const uint8_t* payload_data,
    size_t payload_size,
    const RTPFragmentationHeader* fragmentation) {
  payload_data_ = payload_data;
  payload_size_ = payload_size;
  if (fragmentation) {
    part_info_.CopyFrom(*fragmentation);
    num_partitions_ = fragmentation->fragmentationVectorSize;
  } else {
    part_info_.VerifyAndAllocateFragmentationHeader(1);
    part_info_.fragmentationLength[0] = payload_size;
    part_info_.fragmentationOffset[0] = 0;
    num_partitions_ = part_info_.fragmentationVectorSize;
  }
}

bool RtpPacketizerVp8::NextPacket(RtpPacketToSend* packet, bool* last_packet) {
  RTC_DCHECK(packet);
  RTC_DCHECK(last_packet);
  if (!packets_calculated_) {
    int ret = 0;
    if (aggr_mode_ == kAggrPartitions && balance_) {
      ret = GeneratePacketsBalancedAggregates();
    } else {
      ret = GeneratePackets();
    }
    if (ret < 0) {
      return false;
    }
  }
  if (packets_.empty()) {
    return false;
  }
  InfoStruct packet_info = packets_.front();
  packets_.pop();

  uint8_t* buffer = packet->AllocatePayload(max_payload_len_);
  int bytes = WriteHeaderAndPayload(packet_info, buffer, max_payload_len_);
  if (bytes < 0) {
    return false;
  }
  packet->SetPayloadSize(bytes);
  *last_packet = packets_.empty();
  packet->SetMarker(*last_packet);
  return true;
}

ProtectionType RtpPacketizerVp8::GetProtectionType() {
  bool protect =
      hdr_info_.temporalIdx == 0 || hdr_info_.temporalIdx == kNoTemporalIdx;
  return protect ? kProtectedPacket : kUnprotectedPacket;
}

StorageType RtpPacketizerVp8::GetStorageType(uint32_t retransmission_settings) {
  if (hdr_info_.temporalIdx == 0 &&
      !(retransmission_settings & kRetransmitBaseLayer)) {
    return kDontRetransmit;
  }
  if (hdr_info_.temporalIdx != kNoTemporalIdx &&
             hdr_info_.temporalIdx > 0 &&
             !(retransmission_settings & kRetransmitHigherLayers)) {
    return kDontRetransmit;
  }
  return kAllowRetransmission;
}

std::string RtpPacketizerVp8::ToString() {
  return "RtpPacketizerVp8";
}

size_t RtpPacketizerVp8::CalcNextSize(size_t max_payload_len,
                                      size_t remaining_bytes,
                                      bool split_payload) const {
  if (max_payload_len == 0 || remaining_bytes == 0) {
    return 0;
  }
  if (!split_payload) {
    return max_payload_len >= remaining_bytes ? remaining_bytes : 0;
  }

  if (balance_) {
    // Balance payload sizes to produce (almost) equal size
    // fragments.
    // Number of fragments for remaining_bytes:
    size_t num_frags = remaining_bytes / max_payload_len + 1;
    // Number of bytes in this fragment:
    return static_cast<size_t>(
        static_cast<double>(remaining_bytes) / num_frags + 0.5);
  } else {
    return max_payload_len >= remaining_bytes ? remaining_bytes
                                              : max_payload_len;
  }
}

int RtpPacketizerVp8::GeneratePackets() {
  if (max_payload_len_ < vp8_fixed_payload_descriptor_bytes_ +
                             PayloadDescriptorExtraLength() + 1) {
    // The provided payload length is not long enough for the payload
    // descriptor and one payload byte. Return an error.
    return -1;
  }
  size_t total_bytes_processed = 0;
  bool start_on_new_fragment = true;
  bool beginning = true;
  size_t part_ix = 0;
  while (total_bytes_processed < payload_size_) {
    size_t packet_bytes = 0;    // How much data to send in this packet.
    bool split_payload = true;  // Splitting of partitions is initially allowed.
    size_t remaining_in_partition = part_info_.fragmentationOffset[part_ix] -
                                 total_bytes_processed +
                                 part_info_.fragmentationLength[part_ix];
    size_t rem_payload_len =
        max_payload_len_ -
        (vp8_fixed_payload_descriptor_bytes_ + PayloadDescriptorExtraLength());
    size_t first_partition_in_packet = part_ix;

    while (size_t next_size = CalcNextSize(
               rem_payload_len, remaining_in_partition, split_payload)) {
      packet_bytes += next_size;
      rem_payload_len -= next_size;
      remaining_in_partition -= next_size;

      if (remaining_in_partition == 0 && !(beginning && separate_first_)) {
        // Advance to next partition?
        // Check that there are more partitions; verify that we are either
        // allowed to aggregate fragments, or that we are allowed to
        // aggregate intact partitions and that we started this packet
        // with an intact partition (indicated by first_fragment_ == true).
        if (part_ix + 1 < num_partitions_ &&
            ((aggr_mode_ == kAggrFragments) ||
             (aggr_mode_ == kAggrPartitions && start_on_new_fragment))) {
          assert(part_ix < num_partitions_);
          remaining_in_partition = part_info_.fragmentationLength[++part_ix];
          // Disallow splitting unless kAggrFragments. In kAggrPartitions,
          // we can only aggregate intact partitions.
          split_payload = (aggr_mode_ == kAggrFragments);
        }
      } else if (balance_ && remaining_in_partition > 0) {
        break;
      }
    }
    if (remaining_in_partition == 0) {
      ++part_ix;  // Advance to next partition.
    }
    assert(packet_bytes > 0);

    QueuePacket(total_bytes_processed,
                packet_bytes,
                first_partition_in_packet,
                start_on_new_fragment);
    total_bytes_processed += packet_bytes;
    start_on_new_fragment = (remaining_in_partition == 0);
    beginning = false;  // Next packet cannot be first packet in frame.
  }
  packets_calculated_ = true;
  assert(total_bytes_processed == payload_size_);
  return 0;
}

int RtpPacketizerVp8::GeneratePacketsBalancedAggregates() {
  if (max_payload_len_ < vp8_fixed_payload_descriptor_bytes_ +
                             PayloadDescriptorExtraLength() + 1) {
    // The provided payload length is not long enough for the payload
    // descriptor and one payload byte. Return an error.
    return -1;
  }
  std::vector<int> partition_decision;
  const size_t overhead =
      vp8_fixed_payload_descriptor_bytes_ + PayloadDescriptorExtraLength();
  const size_t max_payload_len = max_payload_len_ - overhead;
  int min_size, max_size;
  AggregateSmallPartitions(&partition_decision, &min_size, &max_size);

  size_t total_bytes_processed = 0;
  size_t part_ix = 0;
  while (part_ix < num_partitions_) {
    if (partition_decision[part_ix] == -1) {
      // Split large partitions.
      size_t remaining_partition = part_info_.fragmentationLength[part_ix];
      size_t num_fragments = Vp8PartitionAggregator::CalcNumberOfFragments(
          remaining_partition, max_payload_len, overhead, min_size, max_size);
      const size_t packet_bytes =
          (remaining_partition + num_fragments - 1) / num_fragments;
      for (size_t n = 0; n < num_fragments; ++n) {
        const size_t this_packet_bytes = packet_bytes < remaining_partition
                                             ? packet_bytes
                                             : remaining_partition;
        QueuePacket(
            total_bytes_processed, this_packet_bytes, part_ix, (n == 0));
        remaining_partition -= this_packet_bytes;
        total_bytes_processed += this_packet_bytes;
        if (static_cast<int>(this_packet_bytes) < min_size) {
          min_size = this_packet_bytes;
        }
        if (static_cast<int>(this_packet_bytes) > max_size) {
          max_size = this_packet_bytes;
        }
      }
      assert(remaining_partition == 0);
      ++part_ix;
    } else {
      size_t this_packet_bytes = 0;
      const size_t first_partition_in_packet = part_ix;
      const int aggregation_index = partition_decision[part_ix];
      while (part_ix < partition_decision.size() &&
             partition_decision[part_ix] == aggregation_index) {
        // Collect all partitions that were aggregated into the same packet.
        this_packet_bytes += part_info_.fragmentationLength[part_ix];
        ++part_ix;
      }
      QueuePacket(total_bytes_processed,
                  this_packet_bytes,
                  first_partition_in_packet,
                  true);
      total_bytes_processed += this_packet_bytes;
    }
  }
  packets_calculated_ = true;
  return 0;
}

void RtpPacketizerVp8::AggregateSmallPartitions(std::vector<int>* partition_vec,
                                                int* min_size,
                                                int* max_size) {
  assert(min_size && max_size);
  *min_size = -1;
  *max_size = -1;
  assert(partition_vec);
  partition_vec->assign(num_partitions_, -1);
  const size_t overhead =
      vp8_fixed_payload_descriptor_bytes_ + PayloadDescriptorExtraLength();
  const size_t max_payload_len = max_payload_len_ - overhead;
  size_t first_in_set = 0;
  size_t last_in_set = 0;
  int num_aggregate_packets = 0;
  // Find sets of partitions smaller than max_payload_len_.
  while (first_in_set < num_partitions_) {
    if (part_info_.fragmentationLength[first_in_set] < max_payload_len) {
      // Found start of a set.
      last_in_set = first_in_set;
      while (last_in_set + 1 < num_partitions_ &&
             part_info_.fragmentationLength[last_in_set + 1] <
                 max_payload_len) {
        ++last_in_set;
      }
      // Found end of a set. Run optimized aggregator. It is ok if start == end.
      Vp8PartitionAggregator aggregator(part_info_, first_in_set, last_in_set);
      if (*min_size >= 0 && *max_size >= 0) {
        aggregator.SetPriorMinMax(*min_size, *max_size);
      }
      Vp8PartitionAggregator::ConfigVec optimal_config =
          aggregator.FindOptimalConfiguration(max_payload_len, overhead);
      aggregator.CalcMinMax(optimal_config, min_size, max_size);
      for (size_t i = first_in_set, j = 0; i <= last_in_set; ++i, ++j) {
        // Transfer configuration for this set of partitions to the joint
        // partition vector representing all partitions in the frame.
        (*partition_vec)[i] = num_aggregate_packets + optimal_config[j];
      }
      num_aggregate_packets += optimal_config.back() + 1;
      first_in_set = last_in_set;
    }
    ++first_in_set;
  }
}

void RtpPacketizerVp8::QueuePacket(size_t start_pos,
                                   size_t packet_size,
                                   size_t first_partition_in_packet,
                                   bool start_on_new_fragment) {
  // Write info to packet info struct and store in packet info queue.
  InfoStruct packet_info;
  packet_info.payload_start_pos = start_pos;
  packet_info.size = packet_size;
  packet_info.first_partition_ix = first_partition_in_packet;
  packet_info.first_fragment = start_on_new_fragment;
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

  assert(packet_info.size > 0);
  buffer[0] = 0;
  if (XFieldPresent())
    buffer[0] |= kXBit;
  if (hdr_info_.nonReference)
    buffer[0] |= kNBit;
  if (packet_info.first_fragment)
    buffer[0] |= kSBit;
  buffer[0] |= (packet_info.first_partition_ix & kPartIdField);

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
    assert(extension_length == PayloadDescriptorExtraLength());
  }
  return static_cast<int>(extension_length);
}

int RtpPacketizerVp8::WritePictureIDFields(uint8_t* x_field,
                                           uint8_t* buffer,
                                           size_t buffer_length,
                                           size_t* extension_length) const {
  *x_field |= kIBit;
  assert(buffer_length >=
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
    assert(hdr_info_.temporalIdx <= 3);
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
  if (hdr_info_.pictureId <= 0x7F) {
    return 1;
  }
  return 2;
}

bool RtpPacketizerVp8::XFieldPresent() const {
  return (TIDFieldPresent() || TL0PicIdxFieldPresent() || PictureIdPresent() ||
          KeyIdxFieldPresent());
}

bool RtpPacketizerVp8::TIDFieldPresent() const {
  assert((hdr_info_.layerSync == false) ||
         (hdr_info_.temporalIdx != kNoTemporalIdx));
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
  assert(parsed_payload != NULL);
  if (payload_data_length == 0) {
    LOG(LS_ERROR) << "Empty payload.";
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
    LOG(LS_ERROR) << "Error parsing VP8 payload descriptor!";
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
      LOG(LS_ERROR) << "Error parsing VP8 payload descriptor!";
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
