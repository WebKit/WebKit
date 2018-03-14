/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_INCLUDE_FRAME_CALLBACK_H_
#define COMMON_VIDEO_INCLUDE_FRAME_CALLBACK_H_

#include <stddef.h>
#include <stdint.h>

#include "common_types.h"  // NOLINT(build/include)

namespace webrtc {

class VideoFrame;

struct EncodedFrame {
 public:
  EncodedFrame()
      : data_(nullptr),
        length_(0),
        frame_type_(kEmptyFrame),
        stream_id_(0),
        timestamp_(0) {}
  EncodedFrame(const uint8_t* data,
               size_t length,
               FrameType frame_type,
               size_t stream_id,
               uint32_t timestamp)
      : data_(data),
        length_(length),
        frame_type_(frame_type),
        stream_id_(stream_id),
        timestamp_(timestamp) {}

  const uint8_t* data_;
  const size_t length_;
  const FrameType frame_type_;
  const size_t stream_id_;
  const uint32_t timestamp_;
};

class EncodedFrameObserver {
 public:
  virtual void EncodedFrameCallback(const EncodedFrame& encoded_frame) = 0;

 protected:
  virtual ~EncodedFrameObserver() {}
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_INCLUDE_FRAME_CALLBACK_H_
