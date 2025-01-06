/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/h264_sps_pps_tracker.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/types/variant.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/pps_parser.h"
#include "common_video/h264/sps_parser.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace video_coding {

namespace {
const uint8_t start_code_h264[] = {0, 0, 0, 1};
}  // namespace

H264SpsPpsTracker::FixedBitstream H264SpsPpsTracker::CopyAndFixBitstream(
    rtc::ArrayView<const uint8_t> bitstream,
    RTPVideoHeader* video_header) {
  RTC_DCHECK(video_header);
  RTC_DCHECK(video_header->codec == kVideoCodecH264);
  RTC_DCHECK_GT(bitstream.size(), 0);

  auto& h264_header =
      absl::get<RTPVideoHeaderH264>(video_header->video_type_header);

  bool append_sps_pps = false;
  auto sps = sps_data_.end();
  auto pps = pps_data_.end();

  for (const NaluInfo& nalu : h264_header.nalus) {
    switch (nalu.type) {
      case H264::NaluType::kSps: {
        SpsInfo& sps_info = sps_data_[nalu.sps_id];
        sps_info.width = video_header->width;
        sps_info.height = video_header->height;
        break;
      }
      case H264::NaluType::kPps: {
        pps_data_[nalu.pps_id].sps_id = nalu.sps_id;
        break;
      }
      case H264::NaluType::kIdr: {
        // If this is the first packet of an IDR, make sure we have the required
        // SPS/PPS and also calculate how much extra space we need in the buffer
        // to prepend the SPS/PPS to the bitstream with start codes.
        if (video_header->is_first_packet_in_frame) {
          if (nalu.pps_id == -1) {
            RTC_LOG(LS_WARNING) << "No PPS id in IDR nalu.";
            return {kRequestKeyframe};
          }

          pps = pps_data_.find(nalu.pps_id);
          if (pps == pps_data_.end()) {
            RTC_LOG(LS_WARNING)
                << "No PPS with id << " << nalu.pps_id << " received";
            return {kRequestKeyframe};
          }

          sps = sps_data_.find(pps->second.sps_id);
          if (sps == sps_data_.end()) {
            RTC_LOG(LS_WARNING)
                << "No SPS with id << " << pps->second.sps_id << " received";
            return {kRequestKeyframe};
          }

          // Since the first packet of every keyframe should have its width and
          // height set we set it here in the case of it being supplied out of
          // band.
          video_header->width = sps->second.width;
          video_header->height = sps->second.height;

          // If the SPS/PPS was supplied out of band then we will have saved
          // the actual bitstream in `data`.
          if (!sps->second.data.empty() && !pps->second.data.empty()) {
            append_sps_pps = true;
          }
        }
        break;
      }
      default:
        break;
    }
  }

  RTC_CHECK(!append_sps_pps ||
            (sps != sps_data_.end() && pps != pps_data_.end()));

  // Calculate how much space we need for the rest of the bitstream.
  size_t required_size = 0;

  if (append_sps_pps) {
    required_size += sps->second.data.size() + sizeof(start_code_h264);
    required_size += pps->second.data.size() + sizeof(start_code_h264);
  }

  if (h264_header.packetization_type == kH264StapA) {
    rtc::ByteBufferReader nalu(bitstream.subview(1));
    while (nalu.Length() > 0) {
      required_size += sizeof(start_code_h264);

      // The first two bytes describe the length of a segment.
      uint16_t segment_length;
      if (!nalu.ReadUInt16(&segment_length))
        return {kDrop};
      if (segment_length == 0 || segment_length > nalu.Length()) {
        return {kDrop};
      }
      required_size += segment_length;
      nalu.Consume(segment_length);
    }
  } else {
    if (!h264_header.nalus.empty()) {
      required_size += sizeof(start_code_h264);
    }
    required_size += bitstream.size();
  }

  // Then we copy to the new buffer.
  H264SpsPpsTracker::FixedBitstream fixed;
  fixed.bitstream.EnsureCapacity(required_size);

  if (append_sps_pps) {
    // Insert SPS.
    fixed.bitstream.AppendData(start_code_h264);
    fixed.bitstream.AppendData(sps->second.data);

    // Insert PPS.
    fixed.bitstream.AppendData(start_code_h264);
    fixed.bitstream.AppendData(pps->second.data);

    // Update codec header to reflect the newly added SPS and PPS.
    h264_header.nalus.push_back(
        {.type = H264::NaluType::kSps, .sps_id = sps->first, .pps_id = -1});
    h264_header.nalus.push_back({.type = H264::NaluType::kPps,
                                 .sps_id = sps->first,
                                 .pps_id = pps->first});
  }

  // Copy the rest of the bitstream and insert start codes.
  if (h264_header.packetization_type == kH264StapA) {
    rtc::ByteBufferReader nalu(bitstream.subview(1));
    while (nalu.Length() > 0) {
      fixed.bitstream.AppendData(start_code_h264);

      // The first two bytes describe the length of a segment.
      uint16_t segment_length;
      if (!nalu.ReadUInt16(&segment_length))
        return {kDrop};
      if (segment_length == 0 || segment_length > nalu.Length()) {
        return {kDrop};
      }
      fixed.bitstream.AppendData(nalu.Data(), segment_length);
      nalu.Consume(segment_length);
    }
  } else {
    if (!h264_header.nalus.empty()) {
      fixed.bitstream.AppendData(start_code_h264);
    }
    fixed.bitstream.AppendData(bitstream.data(), bitstream.size());
  }

  fixed.action = kInsert;
  return fixed;
}

void H264SpsPpsTracker::InsertSpsPpsNalus(const std::vector<uint8_t>& sps,
                                          const std::vector<uint8_t>& pps) {
  constexpr size_t kNaluHeaderOffset = 1;
  if (sps.size() < kNaluHeaderOffset) {
    RTC_LOG(LS_WARNING) << "SPS size  " << sps.size() << " is smaller than "
                        << kNaluHeaderOffset;
    return;
  }
  if ((sps[0] & 0x1f) != H264::NaluType::kSps) {
    RTC_LOG(LS_WARNING) << "SPS Nalu header missing";
    return;
  }
  if (pps.size() < kNaluHeaderOffset) {
    RTC_LOG(LS_WARNING) << "PPS size  " << pps.size() << " is smaller than "
                        << kNaluHeaderOffset;
    return;
  }
  if ((pps[0] & 0x1f) != H264::NaluType::kPps) {
    RTC_LOG(LS_WARNING) << "SPS Nalu header missing";
    return;
  }
  std::optional<SpsParser::SpsState> parsed_sps = SpsParser::ParseSps(
      rtc::ArrayView<const uint8_t>(sps).subview(kNaluHeaderOffset));
  std::optional<PpsParser::PpsState> parsed_pps = PpsParser::ParsePps(
      rtc::ArrayView<const uint8_t>(pps).subview(kNaluHeaderOffset));

  if (!parsed_sps) {
    RTC_LOG(LS_WARNING) << "Failed to parse SPS.";
  }

  if (!parsed_pps) {
    RTC_LOG(LS_WARNING) << "Failed to parse PPS.";
  }

  if (!parsed_pps || !parsed_sps) {
    return;
  }

  SpsInfo sps_info;
  sps_info.width = parsed_sps->width;
  sps_info.height = parsed_sps->height;
  sps_info.data.SetData(sps);
  sps_data_[parsed_sps->id] = std::move(sps_info);

  PpsInfo pps_info;
  pps_info.sps_id = parsed_pps->sps_id;
  pps_info.data.SetData(pps);
  pps_data_[parsed_pps->id] = std::move(pps_info);

  RTC_LOG(LS_INFO) << "Inserted SPS id " << parsed_sps->id << " and PPS id "
                   << parsed_pps->id << " (referencing SPS "
                   << parsed_pps->sps_id << ")";
}

}  // namespace video_coding
}  // namespace webrtc
