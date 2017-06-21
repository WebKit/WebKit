/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_VIDEO_INCLUDE_VIDEO_FRAME_H_
#define WEBRTC_COMMON_VIDEO_INCLUDE_VIDEO_FRAME_H_

// TODO(nisse): This header file should eventually be deleted. The
// EncodedImage class stays in this file until we have figured out how
// to refactor and clean up related interfaces, at which point it
// should be moved to somewhere under api/.

#include "webrtc/common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// TODO(pbos): Rename EncodedFrame and reformat this class' members.
class EncodedImage {
 public:
  static const size_t kBufferPaddingBytesH264;

  // Some decoders require encoded image buffers to be padded with a small
  // number of additional bytes (due to over-reading byte readers).
  static size_t GetBufferPaddingBytes(VideoCodecType codec_type);

  EncodedImage() : EncodedImage(nullptr, 0, 0) {}

  EncodedImage(uint8_t* buffer, size_t length, size_t size)
      : _buffer(buffer), _length(length), _size(size) {}

  void SetEncodeTime(int64_t encode_start_ms, int64_t encode_finish_ms) const {
    timing_.is_timing_frame = true;
    timing_.encode_start_ms = encode_start_ms;
    timing_.encode_finish_ms = encode_finish_ms;
  }

  // TODO(kthelgason): get rid of this struct as it only has a single member
  // remaining.
  struct AdaptReason {
    AdaptReason() : bw_resolutions_disabled(-1) {}
    int bw_resolutions_disabled;  // Number of resolutions that are not sent
                                  // due to bandwidth for this frame.
                                  // Or -1 if information is not provided.
  };
  uint32_t _encodedWidth = 0;
  uint32_t _encodedHeight = 0;
  uint32_t _timeStamp = 0;
  // NTP time of the capture time in local timebase in milliseconds.
  int64_t ntp_time_ms_ = 0;
  int64_t capture_time_ms_ = 0;
  FrameType _frameType = kVideoFrameDelta;
  uint8_t* _buffer;
  size_t _length;
  size_t _size;
  VideoRotation rotation_ = kVideoRotation_0;
  VideoContentType content_type_ = VideoContentType::UNSPECIFIED;
  bool _completeFrame = false;
  AdaptReason adapt_reason_;
  int qp_ = -1;  // Quantizer value.

  // When an application indicates non-zero values here, it is taken as an
  // indication that all future frames will be constrained with those limits
  // until the application indicates a change again.
  PlayoutDelay playout_delay_ = {-1, -1};

  // Timing information should be updatable on const instances.
  mutable struct Timing {
    bool is_timing_frame = false;
    int64_t encode_start_ms = 0;
    int64_t encode_finish_ms = 0;
    int64_t packetization_finish_ms = 0;
    int64_t pacer_exit_ms = 0;
    int64_t network_timestamp_ms = 0;
    int64_t network2_timestamp_ms = 0;
    int64_t receive_start_ms = 0;
    int64_t receive_finish_ms = 0;
  } timing_;
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_VIDEO_INCLUDE_VIDEO_FRAME_H_
