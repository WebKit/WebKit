/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_format_h264.h"

#include <string.h>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common_types.h"  // NOLINT(build/include)
#include "common_video/h264/h264_common.h"
#include "common_video/h264/pps_parser.h"
#include "common_video/h264/sps_parser.h"
#include "common_video/h264/sps_vui_rewriter.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/system/fallthrough.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

static const size_t kNalHeaderSize = 1;
static const size_t kFuAHeaderSize = 2;
static const size_t kLengthFieldSize = 2;
static const size_t kStapAHeaderSize = kNalHeaderSize + kLengthFieldSize;

static const char* kSpsValidHistogramName = "WebRTC.Video.H264.SpsValid";
enum SpsValidEvent {
  kReceivedSpsPocOk = 0,
  kReceivedSpsVuiOk = 1,
  kReceivedSpsRewritten = 2,
  kReceivedSpsParseFailure = 3,
  kSentSpsPocOk = 4,
  kSentSpsVuiOk = 5,
  kSentSpsRewritten = 6,
  kSentSpsParseFailure = 7,
  kSpsRewrittenMax = 8
};

// Bit masks for FU (A and B) indicators.
enum NalDefs : uint8_t { kFBit = 0x80, kNriMask = 0x60, kTypeMask = 0x1F };

// Bit masks for FU (A and B) headers.
enum FuDefs : uint8_t { kSBit = 0x80, kEBit = 0x40, kRBit = 0x20 };

// TODO(pbos): Avoid parsing this here as well as inside the jitter buffer.
bool ParseStapAStartOffsets(const uint8_t* nalu_ptr,
                            size_t length_remaining,
                            std::vector<size_t>* offsets) {
  size_t offset = 0;
  while (length_remaining > 0) {
    // Buffer doesn't contain room for additional nalu length.
    if (length_remaining < sizeof(uint16_t))
      return false;
    uint16_t nalu_size = ByteReader<uint16_t>::ReadBigEndian(nalu_ptr);
    nalu_ptr += sizeof(uint16_t);
    length_remaining -= sizeof(uint16_t);
    if (nalu_size > length_remaining)
      return false;
    nalu_ptr += nalu_size;
    length_remaining -= nalu_size;

    offsets->push_back(offset + kStapAHeaderSize);
    offset += kLengthFieldSize + nalu_size;
  }
  return true;
}

}  // namespace

RtpPacketizerH264::RtpPacketizerH264(
    rtc::ArrayView<const uint8_t> payload,
    PayloadSizeLimits limits,
    H264PacketizationMode packetization_mode,
    const RTPFragmentationHeader& fragmentation)
    : limits_(limits), num_packets_left_(0) {
  // Guard against uninitialized memory in packetization_mode.
  RTC_CHECK(packetization_mode == H264PacketizationMode::NonInterleaved ||
            packetization_mode == H264PacketizationMode::SingleNalUnit);

  for (int i = 0; i < fragmentation.fragmentationVectorSize; ++i) {
    const uint8_t* buffer =
        payload.data() + fragmentation.fragmentationOffset[i];
    size_t length = fragmentation.fragmentationLength[i];

    bool updated_sps = false;
    H264::NaluType nalu_type = H264::ParseNaluType(buffer[0]);
    if (nalu_type == H264::NaluType::kSps) {
      // Check if stream uses picture order count type 0, and if so rewrite it
      // to enable faster decoding. Streams in that format incur additional
      // delay because it allows decode order to differ from render order.
      // The mechanism used is to rewrite (edit or add) the SPS's VUI to contain
      // restrictions on the maximum number of reordered pictures. This reduces
      // latency significantly, though it still adds about a frame of latency to
      // decoding.
      // Note that we do this rewriting both here (send side, in order to
      // protect legacy receive clients) and below in
      // RtpDepacketizerH264::ParseSingleNalu (receive side, in orderer to
      // protect us from unknown or legacy send clients).

      absl::optional<SpsParser::SpsState> sps;

      std::unique_ptr<rtc::Buffer> output_buffer(new rtc::Buffer());
      // Add the type header to the output buffer first, so that the rewriter
      // can append modified payload on top of that.
      output_buffer->AppendData(buffer[0]);
      SpsVuiRewriter::ParseResult result = SpsVuiRewriter::ParseAndRewriteSps(
          buffer + H264::kNaluTypeSize, length - H264::kNaluTypeSize, &sps,
          output_buffer.get());

      switch (result) {
        case SpsVuiRewriter::ParseResult::kVuiRewritten:
          input_fragments_.push_back(
              Fragment(output_buffer->data(), output_buffer->size()));
          input_fragments_.rbegin()->tmp_buffer = std::move(output_buffer);
          updated_sps = true;
          RTC_HISTOGRAM_ENUMERATION(kSpsValidHistogramName,
                                    SpsValidEvent::kSentSpsRewritten,
                                    SpsValidEvent::kSpsRewrittenMax);
          break;
        case SpsVuiRewriter::ParseResult::kPocOk:
          RTC_HISTOGRAM_ENUMERATION(kSpsValidHistogramName,
                                    SpsValidEvent::kSentSpsPocOk,
                                    SpsValidEvent::kSpsRewrittenMax);
          break;
        case SpsVuiRewriter::ParseResult::kVuiOk:
          RTC_HISTOGRAM_ENUMERATION(kSpsValidHistogramName,
                                    SpsValidEvent::kSentSpsVuiOk,
                                    SpsValidEvent::kSpsRewrittenMax);
          break;
        case SpsVuiRewriter::ParseResult::kFailure:
          RTC_HISTOGRAM_ENUMERATION(kSpsValidHistogramName,
                                    SpsValidEvent::kSentSpsParseFailure,
                                    SpsValidEvent::kSpsRewrittenMax);
          break;
      }
    }

    if (!updated_sps)
      input_fragments_.push_back(Fragment(buffer, length));
  }
  if (!GeneratePackets(packetization_mode)) {
    // If failed to generate all the packets, discard already generated
    // packets in case the caller would ignore return value and still try to
    // call NextPacket().
    num_packets_left_ = 0;
    while (!packets_.empty()) {
      packets_.pop();
    }
  }
}

RtpPacketizerH264::~RtpPacketizerH264() = default;

RtpPacketizerH264::Fragment::~Fragment() = default;

RtpPacketizerH264::Fragment::Fragment(const uint8_t* buffer, size_t length)
    : buffer(buffer), length(length) {}
RtpPacketizerH264::Fragment::Fragment(const Fragment& fragment)
    : buffer(fragment.buffer), length(fragment.length) {}

size_t RtpPacketizerH264::NumPackets() const {
  return num_packets_left_;
}

bool RtpPacketizerH264::GeneratePackets(
    H264PacketizationMode packetization_mode) {
  for (size_t i = 0; i < input_fragments_.size();) {
    switch (packetization_mode) {
      case H264PacketizationMode::SingleNalUnit:
        if (!PacketizeSingleNalu(i))
          return false;
        ++i;
        break;
      case H264PacketizationMode::NonInterleaved:
        int fragment_len = input_fragments_[i].length;
        int single_packet_capacity = limits_.max_payload_len;
        if (input_fragments_.size() == 1)
          single_packet_capacity -= limits_.single_packet_reduction_len;
        else if (i == 0)
          single_packet_capacity -= limits_.first_packet_reduction_len;
        else if (i + 1 == input_fragments_.size())
          single_packet_capacity -= limits_.last_packet_reduction_len;

        if (fragment_len > single_packet_capacity) {
          if (!PacketizeFuA(i))
            return false;
          ++i;
        } else {
          i = PacketizeStapA(i);
        }
        break;
    }
  }
  return true;
}

bool RtpPacketizerH264::PacketizeFuA(size_t fragment_index) {
  // Fragment payload into packets (FU-A).
  const Fragment& fragment = input_fragments_[fragment_index];

  PayloadSizeLimits limits = limits_;
  // Leave room for the FU-A header.
  limits.max_payload_len -= kFuAHeaderSize;
  // Ignore single/first/last packet reductions unless it is single/first/last
  // fragment.
  if (input_fragments_.size() != 1)
    limits.single_packet_reduction_len = 0;
  if (fragment_index != 0)
    limits.first_packet_reduction_len = 0;
  if (fragment_index != input_fragments_.size() - 1)
    limits.last_packet_reduction_len = 0;

  // Strip out the original header.
  size_t payload_left = fragment.length - kNalHeaderSize;
  int offset = kNalHeaderSize;

  std::vector<int> payload_sizes = SplitAboutEqually(payload_left, limits);
  if (payload_sizes.empty())
    return false;

  for (size_t i = 0; i < payload_sizes.size(); ++i) {
    int packet_length = payload_sizes[i];
    RTC_CHECK_GT(packet_length, 0);
    packets_.push(PacketUnit(Fragment(fragment.buffer + offset, packet_length),
                             /*first_fragment=*/i == 0,
                             /*last_fragment=*/i == payload_sizes.size() - 1,
                             false, fragment.buffer[0]));
    offset += packet_length;
    payload_left -= packet_length;
  }
  num_packets_left_ += payload_sizes.size();
  RTC_CHECK_EQ(0, payload_left);
  return true;
}

size_t RtpPacketizerH264::PacketizeStapA(size_t fragment_index) {
  // Aggregate fragments into one packet (STAP-A).
  size_t payload_size_left = limits_.max_payload_len;
  if (input_fragments_.size() == 1)
    payload_size_left -= limits_.single_packet_reduction_len;
  else if (fragment_index == 0)
    payload_size_left -= limits_.first_packet_reduction_len;
  int aggregated_fragments = 0;
  size_t fragment_headers_length = 0;
  const Fragment* fragment = &input_fragments_[fragment_index];
  RTC_CHECK_GE(payload_size_left, fragment->length);
  ++num_packets_left_;

  auto payload_size_needed = [&] {
    size_t fragment_size = fragment->length + fragment_headers_length;
    if (input_fragments_.size() == 1) {
      // Single fragment, single packet, payload_size_left already adjusted
      // with limits_.single_packet_reduction_len.
      return fragment_size;
    }
    if (fragment_index == input_fragments_.size() - 1) {
      // Last fragment, so StrapA might be the last packet.
      return fragment_size + limits_.last_packet_reduction_len;
    }
    return fragment_size;
  };

  while (payload_size_left >= payload_size_needed()) {
    RTC_CHECK_GT(fragment->length, 0);
    packets_.push(PacketUnit(*fragment, aggregated_fragments == 0, false, true,
                             fragment->buffer[0]));
    payload_size_left -= fragment->length;
    payload_size_left -= fragment_headers_length;

    fragment_headers_length = kLengthFieldSize;
    // If we are going to try to aggregate more fragments into this packet
    // we need to add the STAP-A NALU header and a length field for the first
    // NALU of this packet.
    if (aggregated_fragments == 0)
      fragment_headers_length += kNalHeaderSize + kLengthFieldSize;
    ++aggregated_fragments;

    // Next fragment.
    ++fragment_index;
    if (fragment_index == input_fragments_.size())
      break;
    fragment = &input_fragments_[fragment_index];
  }
  RTC_CHECK_GT(aggregated_fragments, 0);
  packets_.back().last_fragment = true;
  return fragment_index;
}

bool RtpPacketizerH264::PacketizeSingleNalu(size_t fragment_index) {
  // Add a single NALU to the queue, no aggregation.
  size_t payload_size_left = limits_.max_payload_len;
  if (input_fragments_.size() == 1)
    payload_size_left -= limits_.single_packet_reduction_len;
  else if (fragment_index == 0)
    payload_size_left -= limits_.first_packet_reduction_len;
  else if (fragment_index + 1 == input_fragments_.size())
    payload_size_left -= limits_.last_packet_reduction_len;
  const Fragment* fragment = &input_fragments_[fragment_index];
  if (payload_size_left < fragment->length) {
    RTC_LOG(LS_ERROR) << "Failed to fit a fragment to packet in SingleNalu "
                         "packetization mode. Payload size left "
                      << payload_size_left << ", fragment length "
                      << fragment->length << ", packet capacity "
                      << limits_.max_payload_len;
    return false;
  }
  RTC_CHECK_GT(fragment->length, 0u);
  packets_.push(PacketUnit(*fragment, true /* first */, true /* last */,
                           false /* aggregated */, fragment->buffer[0]));
  ++num_packets_left_;
  return true;
}

bool RtpPacketizerH264::NextPacket(RtpPacketToSend* rtp_packet) {
  RTC_DCHECK(rtp_packet);
  if (packets_.empty()) {
    return false;
  }

  PacketUnit packet = packets_.front();
  if (packet.first_fragment && packet.last_fragment) {
    // Single NAL unit packet.
    size_t bytes_to_send = packet.source_fragment.length;
    uint8_t* buffer = rtp_packet->AllocatePayload(bytes_to_send);
    memcpy(buffer, packet.source_fragment.buffer, bytes_to_send);
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

void RtpPacketizerH264::NextAggregatePacket(RtpPacketToSend* rtp_packet) {
  // Reserve maximum available payload, set actual payload size later.
  size_t payload_capacity = rtp_packet->FreeCapacity();
  RTC_CHECK_GE(payload_capacity, kNalHeaderSize);
  uint8_t* buffer = rtp_packet->AllocatePayload(payload_capacity);
  RTC_DCHECK(buffer);
  PacketUnit* packet = &packets_.front();
  RTC_CHECK(packet->first_fragment);
  // STAP-A NALU header.
  buffer[0] = (packet->header & (kFBit | kNriMask)) | H264::NaluType::kStapA;
  size_t index = kNalHeaderSize;
  bool is_last_fragment = packet->last_fragment;
  while (packet->aggregated) {
    const Fragment& fragment = packet->source_fragment;
    RTC_CHECK_LE(index + kLengthFieldSize + fragment.length, payload_capacity);
    // Add NAL unit length field.
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[index], fragment.length);
    index += kLengthFieldSize;
    // Add NAL unit.
    memcpy(&buffer[index], fragment.buffer, fragment.length);
    index += fragment.length;
    packets_.pop();
    input_fragments_.pop_front();
    if (is_last_fragment)
      break;
    packet = &packets_.front();
    is_last_fragment = packet->last_fragment;
  }
  RTC_CHECK(is_last_fragment);
  rtp_packet->SetPayloadSize(index);
}

void RtpPacketizerH264::NextFragmentPacket(RtpPacketToSend* rtp_packet) {
  PacketUnit* packet = &packets_.front();
  // NAL unit fragmented over multiple packets (FU-A).
  // We do not send original NALU header, so it will be replaced by the
  // FU indicator header of the first packet.
  uint8_t fu_indicator =
      (packet->header & (kFBit | kNriMask)) | H264::NaluType::kFuA;
  uint8_t fu_header = 0;

  // S | E | R | 5 bit type.
  fu_header |= (packet->first_fragment ? kSBit : 0);
  fu_header |= (packet->last_fragment ? kEBit : 0);
  uint8_t type = packet->header & kTypeMask;
  fu_header |= type;
  const Fragment& fragment = packet->source_fragment;
  uint8_t* buffer =
      rtp_packet->AllocatePayload(kFuAHeaderSize + fragment.length);
  buffer[0] = fu_indicator;
  buffer[1] = fu_header;
  memcpy(buffer + kFuAHeaderSize, fragment.buffer, fragment.length);
  if (packet->last_fragment)
    input_fragments_.pop_front();
  packets_.pop();
}

RtpDepacketizerH264::RtpDepacketizerH264() : offset_(0), length_(0) {}
RtpDepacketizerH264::~RtpDepacketizerH264() {}

bool RtpDepacketizerH264::Parse(ParsedPayload* parsed_payload,
                                const uint8_t* payload_data,
                                size_t payload_data_length) {
  RTC_CHECK(parsed_payload != nullptr);
  if (payload_data_length == 0) {
    RTC_LOG(LS_ERROR) << "Empty payload.";
    return false;
  }

  offset_ = 0;
  length_ = payload_data_length;
  modified_buffer_.reset();

  uint8_t nal_type = payload_data[0] & kTypeMask;
  parsed_payload->video_header()
      .video_type_header.emplace<RTPVideoHeaderH264>();
  if (nal_type == H264::NaluType::kFuA) {
    // Fragmented NAL units (FU-A).
    if (!ParseFuaNalu(parsed_payload, payload_data))
      return false;
  } else {
    // We handle STAP-A and single NALU's the same way here. The jitter buffer
    // will depacketize the STAP-A into NAL units later.
    // TODO(sprang): Parse STAP-A offsets here and store in fragmentation vec.
    if (!ProcessStapAOrSingleNalu(parsed_payload, payload_data))
      return false;
  }

  const uint8_t* payload =
      modified_buffer_ ? modified_buffer_->data() : payload_data;

  parsed_payload->payload = payload + offset_;
  parsed_payload->payload_length = length_;
  return true;
}

bool RtpDepacketizerH264::ProcessStapAOrSingleNalu(
    ParsedPayload* parsed_payload,
    const uint8_t* payload_data) {
  parsed_payload->video_header().width = 0;
  parsed_payload->video_header().height = 0;
  parsed_payload->video_header().codec = kVideoCodecH264;
  parsed_payload->video_header().simulcastIdx = 0;
  parsed_payload->video_header().is_first_packet_in_frame = true;
  auto& h264_header = absl::get<RTPVideoHeaderH264>(
      parsed_payload->video_header().video_type_header);

  const uint8_t* nalu_start = payload_data + kNalHeaderSize;
  const size_t nalu_length = length_ - kNalHeaderSize;
  uint8_t nal_type = payload_data[0] & kTypeMask;
  std::vector<size_t> nalu_start_offsets;
  if (nal_type == H264::NaluType::kStapA) {
    // Skip the StapA header (StapA NAL type + length).
    if (length_ <= kStapAHeaderSize) {
      RTC_LOG(LS_ERROR) << "StapA header truncated.";
      return false;
    }

    if (!ParseStapAStartOffsets(nalu_start, nalu_length, &nalu_start_offsets)) {
      RTC_LOG(LS_ERROR) << "StapA packet with incorrect NALU packet lengths.";
      return false;
    }

    h264_header.packetization_type = kH264StapA;
    nal_type = payload_data[kStapAHeaderSize] & kTypeMask;
  } else {
    h264_header.packetization_type = kH264SingleNalu;
    nalu_start_offsets.push_back(0);
  }
  h264_header.nalu_type = nal_type;
  parsed_payload->frame_type = kVideoFrameDelta;

  nalu_start_offsets.push_back(length_ + kLengthFieldSize);  // End offset.
  for (size_t i = 0; i < nalu_start_offsets.size() - 1; ++i) {
    size_t start_offset = nalu_start_offsets[i];
    // End offset is actually start offset for next unit, excluding length field
    // so remove that from this units length.
    size_t end_offset = nalu_start_offsets[i + 1] - kLengthFieldSize;
    if (end_offset - start_offset < H264::kNaluTypeSize) {
      RTC_LOG(LS_ERROR) << "STAP-A packet too short";
      return false;
    }

    NaluInfo nalu;
    nalu.type = payload_data[start_offset] & kTypeMask;
    nalu.sps_id = -1;
    nalu.pps_id = -1;
    start_offset += H264::kNaluTypeSize;

    switch (nalu.type) {
      case H264::NaluType::kSps: {
        // Check if VUI is present in SPS and if it needs to be modified to
        // avoid
        // excessive decoder latency.

        // Copy any previous data first (likely just the first header).
        std::unique_ptr<rtc::Buffer> output_buffer(new rtc::Buffer());
        if (start_offset)
          output_buffer->AppendData(payload_data, start_offset);

        absl::optional<SpsParser::SpsState> sps;

        SpsVuiRewriter::ParseResult result = SpsVuiRewriter::ParseAndRewriteSps(
            &payload_data[start_offset], end_offset - start_offset, &sps,
            output_buffer.get());
        switch (result) {
          case SpsVuiRewriter::ParseResult::kVuiRewritten:
            if (modified_buffer_) {
              RTC_LOG(LS_WARNING)
                  << "More than one H264 SPS NAL units needing "
                     "rewriting found within a single STAP-A packet. "
                     "Keeping the first and rewriting the last.";
            }

            // Rewrite length field to new SPS size.
            if (h264_header.packetization_type == kH264StapA) {
              size_t length_field_offset =
                  start_offset - (H264::kNaluTypeSize + kLengthFieldSize);
              // Stap-A Length includes payload data and type header.
              size_t rewritten_size =
                  output_buffer->size() - start_offset + H264::kNaluTypeSize;
              ByteWriter<uint16_t>::WriteBigEndian(
                  &(*output_buffer)[length_field_offset], rewritten_size);
            }

            // Append rest of packet.
            output_buffer->AppendData(
                &payload_data[end_offset],
                nalu_length + kNalHeaderSize - end_offset);

            modified_buffer_ = std::move(output_buffer);
            length_ = modified_buffer_->size();

            RTC_HISTOGRAM_ENUMERATION(kSpsValidHistogramName,
                                      SpsValidEvent::kReceivedSpsRewritten,
                                      SpsValidEvent::kSpsRewrittenMax);
            break;
          case SpsVuiRewriter::ParseResult::kPocOk:
            RTC_HISTOGRAM_ENUMERATION(kSpsValidHistogramName,
                                      SpsValidEvent::kReceivedSpsPocOk,
                                      SpsValidEvent::kSpsRewrittenMax);
            break;
          case SpsVuiRewriter::ParseResult::kVuiOk:
            RTC_HISTOGRAM_ENUMERATION(kSpsValidHistogramName,
                                      SpsValidEvent::kReceivedSpsVuiOk,
                                      SpsValidEvent::kSpsRewrittenMax);
            break;
          case SpsVuiRewriter::ParseResult::kFailure:
            RTC_HISTOGRAM_ENUMERATION(kSpsValidHistogramName,
                                      SpsValidEvent::kReceivedSpsParseFailure,
                                      SpsValidEvent::kSpsRewrittenMax);
            break;
        }

        if (sps) {
          parsed_payload->video_header().width = sps->width;
          parsed_payload->video_header().height = sps->height;
          nalu.sps_id = sps->id;
        } else {
          RTC_LOG(LS_WARNING) << "Failed to parse SPS id from SPS slice.";
        }
        parsed_payload->frame_type = kVideoFrameKey;
        break;
      }
      case H264::NaluType::kPps: {
        uint32_t pps_id;
        uint32_t sps_id;
        if (PpsParser::ParsePpsIds(&payload_data[start_offset],
                                   end_offset - start_offset, &pps_id,
                                   &sps_id)) {
          nalu.pps_id = pps_id;
          nalu.sps_id = sps_id;
        } else {
          RTC_LOG(LS_WARNING)
              << "Failed to parse PPS id and SPS id from PPS slice.";
        }
        break;
      }
      case H264::NaluType::kIdr:
        parsed_payload->frame_type = kVideoFrameKey;
        RTC_FALLTHROUGH();
      case H264::NaluType::kSlice: {
        absl::optional<uint32_t> pps_id = PpsParser::ParsePpsIdFromSlice(
            &payload_data[start_offset], end_offset - start_offset);
        if (pps_id) {
          nalu.pps_id = *pps_id;
        } else {
          RTC_LOG(LS_WARNING) << "Failed to parse PPS id from slice of type: "
                              << static_cast<int>(nalu.type);
        }
        break;
      }
      // Slices below don't contain SPS or PPS ids.
      case H264::NaluType::kAud:
      case H264::NaluType::kEndOfSequence:
      case H264::NaluType::kEndOfStream:
      case H264::NaluType::kFiller:
      case H264::NaluType::kSei:
        break;
      case H264::NaluType::kStapA:
      case H264::NaluType::kFuA:
        RTC_LOG(LS_WARNING) << "Unexpected STAP-A or FU-A received.";
        return false;
    }

    if (h264_header.nalus_length == kMaxNalusPerPacket) {
      RTC_LOG(LS_WARNING)
          << "Received packet containing more than " << kMaxNalusPerPacket
          << " NAL units. Will not keep track sps and pps ids for all of them.";
    } else {
      h264_header.nalus[h264_header.nalus_length++] = nalu;
    }
  }

  return true;
}

bool RtpDepacketizerH264::ParseFuaNalu(
    RtpDepacketizer::ParsedPayload* parsed_payload,
    const uint8_t* payload_data) {
  if (length_ < kFuAHeaderSize) {
    RTC_LOG(LS_ERROR) << "FU-A NAL units truncated.";
    return false;
  }
  uint8_t fnri = payload_data[0] & (kFBit | kNriMask);
  uint8_t original_nal_type = payload_data[1] & kTypeMask;
  bool first_fragment = (payload_data[1] & kSBit) > 0;
  NaluInfo nalu;
  nalu.type = original_nal_type;
  nalu.sps_id = -1;
  nalu.pps_id = -1;
  if (first_fragment) {
    offset_ = 0;
    length_ -= kNalHeaderSize;
    absl::optional<uint32_t> pps_id = PpsParser::ParsePpsIdFromSlice(
        payload_data + 2 * kNalHeaderSize, length_ - kNalHeaderSize);
    if (pps_id) {
      nalu.pps_id = *pps_id;
    } else {
      RTC_LOG(LS_WARNING)
          << "Failed to parse PPS from first fragment of FU-A NAL "
             "unit with original type: "
          << static_cast<int>(nalu.type);
    }
    uint8_t original_nal_header = fnri | original_nal_type;
    modified_buffer_.reset(new rtc::Buffer());
    modified_buffer_->AppendData(payload_data + kNalHeaderSize, length_);
    (*modified_buffer_)[0] = original_nal_header;
  } else {
    offset_ = kFuAHeaderSize;
    length_ -= kFuAHeaderSize;
  }

  if (original_nal_type == H264::NaluType::kIdr) {
    parsed_payload->frame_type = kVideoFrameKey;
  } else {
    parsed_payload->frame_type = kVideoFrameDelta;
  }
  parsed_payload->video_header().width = 0;
  parsed_payload->video_header().height = 0;
  parsed_payload->video_header().codec = kVideoCodecH264;
  parsed_payload->video_header().simulcastIdx = 0;
  parsed_payload->video_header().is_first_packet_in_frame = first_fragment;
  auto& h264_header = absl::get<RTPVideoHeaderH264>(
      parsed_payload->video_header().video_type_header);
  h264_header.packetization_type = kH264FuA;
  h264_header.nalu_type = original_nal_type;
  if (first_fragment) {
    h264_header.nalus[h264_header.nalus_length] = nalu;
    h264_header.nalus_length = 1;
  }
  return true;
}

}  // namespace webrtc
