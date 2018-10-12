/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_HEADER_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_HEADER_H_

#include "absl/container/inlined_vector.h"
#include "absl/types/variant.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame_marking.h"
#include "api/video/video_rotation.h"
#include "api/video/video_timing.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/codecs/vp8/include/vp8_globals.h"
#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"

namespace webrtc {
using RTPVideoTypeHeader = absl::variant<absl::monostate,
                                         RTPVideoHeaderVP8,
                                         RTPVideoHeaderVP9,
                                         RTPVideoHeaderH264>;

struct RTPVideoHeader {
  struct GenericDescriptorInfo {
    GenericDescriptorInfo();
    GenericDescriptorInfo(const GenericDescriptorInfo& other);
    ~GenericDescriptorInfo();

    int64_t frame_id = 0;
    int spatial_index = 0;
    int temporal_index = 0;
    absl::InlinedVector<int64_t, 5> dependencies;
    absl::InlinedVector<int, 5> higher_spatial_layers;
  };

  RTPVideoHeader();
  RTPVideoHeader(const RTPVideoHeader& other);

  ~RTPVideoHeader();

  absl::optional<GenericDescriptorInfo> generic;

  uint16_t width = 0;
  uint16_t height = 0;
  VideoRotation rotation = VideoRotation::kVideoRotation_0;
  VideoContentType content_type = VideoContentType::UNSPECIFIED;
  bool is_first_packet_in_frame = false;
  bool is_last_packet_in_frame = false;
  uint8_t simulcastIdx = 0;
  VideoCodecType codec = VideoCodecType::kVideoCodecGeneric;

  PlayoutDelay playout_delay;
  VideoSendTiming video_timing;
  FrameMarking frame_marking;
  RTPVideoTypeHeader video_type_header;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_HEADER_H_
