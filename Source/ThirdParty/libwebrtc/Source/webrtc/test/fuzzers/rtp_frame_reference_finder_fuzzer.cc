/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <utility>

#include "api/rtp_packet_infos.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame_type.h"
#include "api/video/video_rotation.h"
#include "api/video/video_timing.h"
#include "modules/rtp_rtcp/source/frame_object.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/codecs/vp8/include/vp8_globals.h"
#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "modules/video_coding/rtp_frame_reference_finder.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

namespace {
class DataReader {
 public:
  DataReader(FuzzDataHelper fuzz_data) : data_(fuzz_data) {}

  template <typename T>
  void CopyTo(T& object) {
    return data_.CopyTo(object);
  }

  template <typename T>
  T GetNum() {
    return data_.Read<T>();
  }
  bool MoreToRead() { return data_.BytesLeft() > 0; }

 private:
  FuzzDataHelper data_;
};

RTPVideoHeaderH264 GenerateRTPVideoHeaderH264(DataReader* reader) {
  RTPVideoHeaderH264 result;
  result.nalu_type = reader->GetNum<uint8_t>();
  result.packetization_type = reader->GetNum<H264PacketizationTypes>();
  int nalus_length = reader->GetNum<uint8_t>();
  for (int i = 0; i < nalus_length; ++i) {
    reader->CopyTo(result.nalus.emplace_back());
  }
  result.packetization_mode = reader->GetNum<H264PacketizationMode>();
  return result;
}

std::optional<RTPVideoHeader::GenericDescriptorInfo>
GenerateGenericFrameDependencies(DataReader* reader) {
  std::optional<RTPVideoHeader::GenericDescriptorInfo> result;
  uint8_t flags = reader->GetNum<uint8_t>();
  if (flags & 0b1000'0000) {
    // i.e. with 50% chance there are no generic dependencies.
    // in such case codec-specfic code path of the RtpFrameReferenceFinder will
    // be validated.
    return result;
  }

  result.emplace();
  result->frame_id = reader->GetNum<int32_t>();
  result->spatial_index = (flags & 0b0111'0000) >> 4;
  result->temporal_index = (flags & 0b0000'1110) >> 1;

  // Larger than supported by the RtpFrameReferenceFinder.
  int num_diffs = (reader->GetNum<uint8_t>() % 16);
  for (int i = 0; i < num_diffs; ++i) {
    result->dependencies.push_back(result->frame_id -
                                   (reader->GetNum<uint16_t>() % (1 << 14)));
  }

  return result;
}
}  // namespace

void FuzzOneInput(FuzzDataHelper fuzz_data) {
  DataReader reader(fuzz_data);
  RtpFrameReferenceFinder reference_finder;

  auto codec = static_cast<VideoCodecType>(reader.GetNum<uint8_t>() % 5);

  while (reader.MoreToRead()) {
    uint16_t first_seq_num = reader.GetNum<uint16_t>();
    uint16_t last_seq_num = reader.GetNum<uint16_t>();
    bool marker_bit = reader.GetNum<uint8_t>();

    RTPVideoHeader video_header;
    switch (reader.GetNum<uint8_t>() % 3) {
      case 0:
        video_header.frame_type = VideoFrameType::kEmptyFrame;
        break;
      case 1:
        video_header.frame_type = VideoFrameType::kVideoFrameKey;
        break;
      case 2:
        video_header.frame_type = VideoFrameType::kVideoFrameDelta;
        break;
    }

    switch (codec) {
      case kVideoCodecVP8:
        reader.CopyTo(
            video_header.video_type_header.emplace<RTPVideoHeaderVP8>());
        break;
      case kVideoCodecVP9:
        reader.CopyTo(
            video_header.video_type_header.emplace<RTPVideoHeaderVP9>());
        break;
      case kVideoCodecH264:
        video_header.video_type_header = GenerateRTPVideoHeaderH264(&reader);
        break;
      case kVideoCodecH265:
        // TODO(bugs.webrtc.org/13485)
        break;
      default:
        break;
    }

    video_header.generic = GenerateGenericFrameDependencies(&reader);

    // clang-format off
    auto frame = std::make_unique<RtpFrameObject>(
        first_seq_num,
        last_seq_num,
        marker_bit,
        /*times_nacked=*/0,
        /*first_packet_received_time=*/std::nullopt,
        /*last_packet_received_time=*/std::nullopt,
        /*rtp_timestamp=*/0,
        /*ntp_time_ms=*/0,
        VideoSendTiming(),
        /*payload_type=*/0,
        codec,
        kVideoRotation_0,
        VideoContentType::UNSPECIFIED,
        video_header,
        /*color_space=*/std::nullopt,
        /*frame_instrumentation_data=*/std::nullopt,
        RtpPacketInfos(),
        EncodedImageBuffer::Create(/*size=*/0));
    // clang-format on

    reference_finder.ManageFrame(std::move(frame));
  }
}

}  // namespace webrtc
