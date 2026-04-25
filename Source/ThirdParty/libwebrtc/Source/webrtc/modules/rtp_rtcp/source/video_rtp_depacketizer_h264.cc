/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/video_rtp_depacketizer_h264.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame_type.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/pps_parser.h"
#include "common_video/h264/sps_parser.h"
#include "common_video/h264/sps_vui_rewriter.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_format_h264.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "rtc_base/buffer.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr size_t kNalHeaderSize = 1;
constexpr size_t kFuAHeaderSize = 2;
constexpr size_t kLengthFieldSize = 2;

std::vector<ArrayView<const uint8_t>> ParseStapA(
    ArrayView<const uint8_t> data) {
  std::vector<ArrayView<const uint8_t>> nal_units;
  ByteBufferReader reader(data);
  if (!reader.Consume(kNalHeaderSize)) {
    return nal_units;
  }

  while (reader.Length() > 0) {
    uint16_t nalu_size;
    if (!reader.ReadUInt16(&nalu_size)) {
      return {};
    }
    if (nalu_size == 0 || nalu_size > reader.Length()) {
      return {};
    }
    nal_units.emplace_back(reader.Data(), nalu_size);
    reader.Consume(nalu_size);
  }
  return nal_units;
}

std::optional<VideoRtpDepacketizer::ParsedRtpPayload> ProcessStapAOrSingleNalu(
    CopyOnWriteBuffer rtp_payload) {
  ArrayView<const uint8_t> payload_data(rtp_payload);
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload(
      std::in_place);
  bool modified_buffer = false;
  Buffer output_buffer;
  parsed_payload->video_payload = rtp_payload;
  parsed_payload->video_header.width = 0;
  parsed_payload->video_header.height = 0;
  parsed_payload->video_header.codec = kVideoCodecH264;
  parsed_payload->video_header.simulcastIdx = 0;
  parsed_payload->video_header.is_first_packet_in_frame = false;
  auto& h264_header = parsed_payload->video_header.video_type_header
                          .emplace<RTPVideoHeaderH264>();

  uint8_t nal_type = payload_data[0] & kH264TypeMask;
  std::vector<ArrayView<const uint8_t>> nal_units;
  if (nal_type == H264::NaluType::kStapA) {
    nal_units = ParseStapA(payload_data);
    if (nal_units.empty()) {
      RTC_LOG(LS_ERROR) << "Incorrect StapA packet.";
      return std::nullopt;
    }
    h264_header.packetization_type = kH264StapA;
    h264_header.nalu_type = nal_units[0][0] & kH264TypeMask;
  } else {
    h264_header.packetization_type = kH264SingleNalu;
    h264_header.nalu_type = nal_type;
    nal_units.push_back(payload_data);
  }

  parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameDelta;

  for (const ArrayView<const uint8_t>& nal_unit : nal_units) {
    NaluInfo nalu;
    nalu.type = nal_unit[0] & kH264TypeMask;
    nalu.sps_id = -1;
    nalu.pps_id = -1;
    ArrayView<const uint8_t> nalu_data = nal_unit.subspan(H264::kNaluTypeSize);

    if (nalu_data.empty()) {
      RTC_LOG(LS_WARNING) << "Skipping empty NAL unit.";
      continue;
    }

    switch (nalu.type) {
      case H264::NaluType::kSps: {
        // Check if VUI is present in SPS and if it needs to be modified to
        // avoid excessive decoder latency.

        // Copy any previous data first (likely just the first header).
        output_buffer.Clear();
        size_t start_offset = nalu_data.data() - payload_data.data();
        size_t end_offset = start_offset + nalu_data.size();
        if (start_offset) {
          output_buffer.AppendData(payload_data.data(), start_offset);
        }

        std::optional<SpsParser::SpsState> sps;

        SpsVuiRewriter::ParseResult result = SpsVuiRewriter::ParseAndRewriteSps(
            nalu_data, &sps, nullptr, &output_buffer,
            SpsVuiRewriter::Direction::kIncoming);
        switch (result) {
          case SpsVuiRewriter::ParseResult::kFailure:
            RTC_LOG(LS_WARNING) << "Failed to parse SPS NAL unit.";
            return std::nullopt;
          case SpsVuiRewriter::ParseResult::kVuiRewritten:
            if (modified_buffer) {
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
                  output_buffer.size() - start_offset + H264::kNaluTypeSize;
              ByteWriter<uint16_t>::WriteBigEndian(
                  &output_buffer[length_field_offset], rewritten_size);
            }

            // Append rest of packet.
            output_buffer.AppendData(payload_data.subspan(end_offset));

            modified_buffer = true;
            [[fallthrough]];
          case SpsVuiRewriter::ParseResult::kVuiOk:
            RTC_DCHECK(sps);
            nalu.sps_id = sps->id;
            parsed_payload->video_header.width = sps->width;
            parsed_payload->video_header.height = sps->height;
            parsed_payload->video_header.frame_type =
                VideoFrameType::kVideoFrameKey;
            break;
        }
        parsed_payload->video_header.is_first_packet_in_frame = true;
        break;
      }
      case H264::NaluType::kPps: {
        uint32_t pps_id;
        uint32_t sps_id;
        if (PpsParser::ParsePpsIds(nalu_data, &pps_id, &sps_id)) {
          nalu.pps_id = pps_id;
          nalu.sps_id = sps_id;
        } else {
          RTC_LOG(LS_WARNING)
              << "Failed to parse PPS id and SPS id from PPS slice.";
          return std::nullopt;
        }
        parsed_payload->video_header.is_first_packet_in_frame = true;
        break;
      }
      case H264::NaluType::kIdr:
        parsed_payload->video_header.frame_type =
            VideoFrameType::kVideoFrameKey;
        [[fallthrough]];
      case H264::NaluType::kSlice: {
        std::optional<PpsParser::SliceHeader> slice_header =
            PpsParser::ParseSliceHeader(nalu_data);
        if (slice_header) {
          nalu.pps_id = slice_header->pic_parameter_set_id;
          if (slice_header->first_mb_in_slice == 0) {
            parsed_payload->video_header.is_first_packet_in_frame = true;
          }
        } else {
          RTC_LOG(LS_WARNING) << "Failed to parse PPS id from slice of type: "
                              << static_cast<int>(nalu.type);
          return std::nullopt;
        }
        break;
      }
      case H264::NaluType::kAud:
        parsed_payload->video_header.is_first_packet_in_frame = true;
        break;
      case H264::NaluType::kSei:
        parsed_payload->video_header.is_first_packet_in_frame = true;
        break;
      // Slices below don't contain SPS or PPS ids.
      case H264::NaluType::kEndOfSequence:
      case H264::NaluType::kEndOfStream:
      case H264::NaluType::kFiller:
        break;
      case H264::NaluType::kStapA:
      case H264::NaluType::kFuA:
        RTC_LOG(LS_WARNING) << "Unexpected STAP-A or FU-A received.";
        return std::nullopt;
    }

    h264_header.nalus.push_back(nalu);
  }

  if (modified_buffer) {
    parsed_payload->video_payload.SetData(output_buffer.data(),
                                          output_buffer.size());
  }
  return parsed_payload;
}

std::optional<VideoRtpDepacketizer::ParsedRtpPayload> ParseFuaNalu(
    CopyOnWriteBuffer rtp_payload) {
  if (rtp_payload.size() < kFuAHeaderSize) {
    RTC_LOG(LS_ERROR) << "FU-A NAL units truncated.";
    return std::nullopt;
  }
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload(
      std::in_place);
  uint8_t fnri = rtp_payload.cdata()[0] & (kH264FBit | kH264NriMask);
  uint8_t original_nal_type = rtp_payload.cdata()[1] & kH264TypeMask;
  bool first_fragment = (rtp_payload.cdata()[1] & kH264SBit) > 0;
  bool is_first_packet_in_frame = false;
  NaluInfo nalu;
  nalu.type = original_nal_type;
  nalu.sps_id = -1;
  nalu.pps_id = -1;
  if (first_fragment) {
    if (original_nal_type == H264::NaluType::kIdr ||
        original_nal_type == H264::NaluType::kSlice) {
      std::optional<PpsParser::SliceHeader> slice_header =
          PpsParser::ParseSliceHeader(ArrayView<const uint8_t>(rtp_payload)
                                          .subspan(2 * kNalHeaderSize));
      if (slice_header) {
        nalu.pps_id = slice_header->pic_parameter_set_id;
        is_first_packet_in_frame = slice_header->first_mb_in_slice == 0;
      } else {
        RTC_LOG(LS_WARNING)
            << "Failed to parse PPS from first fragment of FU-A NAL "
               "unit with original type: "
            << static_cast<int>(nalu.type);
      }
    }
    uint8_t original_nal_header = fnri | original_nal_type;
    rtp_payload =
        rtp_payload.Slice(kNalHeaderSize, rtp_payload.size() - kNalHeaderSize);
    rtp_payload.MutableData()[0] = original_nal_header;
    parsed_payload->video_payload = std::move(rtp_payload);
  } else {
    parsed_payload->video_payload =
        rtp_payload.Slice(kFuAHeaderSize, rtp_payload.size() - kFuAHeaderSize);
  }

  if (original_nal_type == H264::NaluType::kIdr) {
    parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameKey;
  } else {
    parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  }
  parsed_payload->video_header.width = 0;
  parsed_payload->video_header.height = 0;
  parsed_payload->video_header.codec = kVideoCodecH264;
  parsed_payload->video_header.simulcastIdx = 0;
  parsed_payload->video_header.is_first_packet_in_frame =
      is_first_packet_in_frame;
  auto& h264_header = parsed_payload->video_header.video_type_header
                          .emplace<RTPVideoHeaderH264>();
  h264_header.packetization_type = kH264FuA;
  h264_header.nalu_type = original_nal_type;
  if (first_fragment) {
    h264_header.nalus = {nalu};
  }
  return parsed_payload;
}

}  // namespace

std::optional<VideoRtpDepacketizer::ParsedRtpPayload>
VideoRtpDepacketizerH264::Parse(CopyOnWriteBuffer rtp_payload) {
  if (rtp_payload.empty()) {
    RTC_LOG(LS_ERROR) << "Empty payload.";
    return std::nullopt;
  }

  uint8_t nal_type = rtp_payload.cdata()[0] & kH264TypeMask;

  if (nal_type == H264::NaluType::kFuA) {
    // Fragmented NAL units (FU-A).
    return ParseFuaNalu(std::move(rtp_payload));
  } else {
    // We handle STAP-A and single NALU's the same way here. The jitter buffer
    // will depacketize the STAP-A into NAL units later.
    return ProcessStapAOrSingleNalu(std::move(rtp_payload));
  }
}

}  // namespace webrtc
