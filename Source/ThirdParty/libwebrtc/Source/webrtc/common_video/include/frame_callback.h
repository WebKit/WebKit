/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_VIDEO_INCLUDE_FRAME_CALLBACK_H_
#define WEBRTC_COMMON_VIDEO_INCLUDE_FRAME_CALLBACK_H_

#include <stddef.h>
#include <stdint.h>

#include "webrtc/common_types.h"

namespace webrtc {

class VideoFrame;

struct EncodedFrame {
 public:
  EncodedFrame()
      : data_(nullptr),
        length_(0),
        frame_type_(kEmptyFrame),
        encoded_width_(0),
        encoded_height_(0),
        timestamp_(0) {}
  EncodedFrame(const uint8_t* data,
               size_t length,
               FrameType frame_type,
               uint32_t encoded_width,
               uint32_t encoded_height,
               uint32_t timestamp)
      : data_(data),
        length_(length),
        frame_type_(frame_type),
        encoded_width_(encoded_width),
        encoded_height_(encoded_height),
        timestamp_(timestamp) {}

  const uint8_t* data_;
  const size_t length_;
  const FrameType frame_type_;
  const uint32_t encoded_width_;
  const uint32_t encoded_height_;
  const uint32_t timestamp_;
};

class EncodedFrameObserver {
 public:
  virtual void EncodedFrameCallback(const EncodedFrame& encoded_frame) = 0;
  virtual void OnEncodeTiming(int64_t capture_ntp_ms, int encode_duration_ms) {}

 protected:
  virtual ~EncodedFrameObserver() {}
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_VIDEO_INCLUDE_FRAME_CALLBACK_H_
