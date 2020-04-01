/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_TRANSFORMABLE_ENCODED_FRAME_H_
#define MODULES_RTP_RTCP_SOURCE_TRANSFORMABLE_ENCODED_FRAME_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/video/encoded_frame.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"

namespace webrtc {

class TransformableEncodedFrame : public video_coding::EncodedFrame {
 public:
  TransformableEncodedFrame(
      rtc::scoped_refptr<EncodedImageBufferInterface> encoded_data,
      const RTPVideoHeader& video_header,
      int payload_type,
      absl::optional<VideoCodecType> codec_type,
      uint32_t rtp_timestamp,
      int64_t capture_time_ms,
      const RTPFragmentationHeader* fragmentation,
      absl::optional<int64_t> expected_retransmission_time_ms);
  ~TransformableEncodedFrame() override;

  const RTPVideoHeader& video_header() const;
  absl::optional<VideoCodecType> codec_type() const;
  int64_t capture_time_ms() const { return capture_time_ms_; }
  RTPFragmentationHeader* fragmentation_header() const {
    return fragmentation_header_.get();
  }
  const absl::optional<int64_t>& expected_retransmission_time_ms() const {
    return expected_retransmission_time_ms_;
  }

  // Implements EncodedFrame.
  int64_t ReceivedTime() const override;
  int64_t RenderTime() const override;

 private:
  RTPVideoHeader video_header_;
  absl::optional<VideoCodecType> codec_type_ = absl::nullopt;
  std::unique_ptr<RTPFragmentationHeader> fragmentation_header_;
  absl::optional<int64_t> expected_retransmission_time_ms_ = absl::nullopt;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_TRANSFORMABLE_ENCODED_FRAME_H_
