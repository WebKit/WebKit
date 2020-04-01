/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/transformable_encoded_frame.h"

namespace webrtc {

TransformableEncodedFrame::TransformableEncodedFrame(
    rtc::scoped_refptr<EncodedImageBufferInterface> encoded_data,
    const RTPVideoHeader& video_header,
    int payload_type,
    absl::optional<VideoCodecType> codec_type,
    uint32_t rtp_timestamp,
    int64_t capture_time_ms,
    const RTPFragmentationHeader* fragmentation,
    absl::optional<int64_t> expected_retransmission_time_ms)
    : video_header_(video_header),
      codec_type_(codec_type),
      expected_retransmission_time_ms_(expected_retransmission_time_ms) {
  SetEncodedData(encoded_data);
  _payloadType = payload_type;
  SetTimestamp(rtp_timestamp);
  capture_time_ms_ = capture_time_ms;
  if (fragmentation) {
    fragmentation_header_ = std::make_unique<RTPFragmentationHeader>();
    fragmentation_header_->CopyFrom(*fragmentation);
  }
}

TransformableEncodedFrame::~TransformableEncodedFrame() = default;

const RTPVideoHeader& TransformableEncodedFrame::video_header() const {
  return video_header_;
}

absl::optional<VideoCodecType> TransformableEncodedFrame::codec_type() const {
  return codec_type_;
}

int64_t TransformableEncodedFrame::ReceivedTime() const {
  return 0;
}

int64_t TransformableEncodedFrame::RenderTime() const {
  return _renderTimeMs;
}

}  // namespace webrtc
