/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/vector_float_frame.h"

namespace webrtc {

VectorFloatFrame::VectorFloatFrame(int num_channels,
                                   int samples_per_channel,
                                   float start_value)
    : channels_(num_channels * samples_per_channel, start_value),
      view_(channels_.data(), samples_per_channel, num_channels) {}

VectorFloatFrame::~VectorFloatFrame() = default;

AudioFrameView<float> VectorFloatFrame::float_frame_view() {
  return AudioFrameView<float>(view_);
}

AudioFrameView<const float> VectorFloatFrame::float_frame_view() const {
  return AudioFrameView<const float>(view_);
}

}  // namespace webrtc
