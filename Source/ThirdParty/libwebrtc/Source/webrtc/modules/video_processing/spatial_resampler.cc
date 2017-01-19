/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/spatial_resampler.h"

namespace webrtc {

VPMSimpleSpatialResampler::VPMSimpleSpatialResampler()
    : resampling_mode_(kFastRescaling), target_width_(0), target_height_(0) {}

VPMSimpleSpatialResampler::~VPMSimpleSpatialResampler() {}

int32_t VPMSimpleSpatialResampler::SetTargetFrameSize(int32_t width,
                                                      int32_t height) {
  if (resampling_mode_ == kNoRescaling)
    return VPM_OK;

  if (width < 1 || height < 1)
    return VPM_PARAMETER_ERROR;

  target_width_ = width;
  target_height_ = height;

  return VPM_OK;
}

void VPMSimpleSpatialResampler::SetInputFrameResampleMode(
    VideoFrameResampling resampling_mode) {
  resampling_mode_ = resampling_mode;
}

void VPMSimpleSpatialResampler::Reset() {
  resampling_mode_ = kFastRescaling;
  target_width_ = 0;
  target_height_ = 0;
}

int32_t VPMSimpleSpatialResampler::ResampleFrame(const VideoFrame& inFrame,
                                                 VideoFrame* outFrame) {
  // Don't copy if frame remains as is.
  if (resampling_mode_ == kNoRescaling) {
    return VPM_OK;
  // Check if re-sampling is needed
  } else if ((inFrame.width() == target_width_) &&
             (inFrame.height() == target_height_)) {
    return VPM_OK;
  }

  rtc::scoped_refptr<I420Buffer> scaled_buffer(
      buffer_pool_.CreateBuffer(target_width_, target_height_));

  scaled_buffer->CropAndScaleFrom(*inFrame.video_frame_buffer());

  *outFrame = VideoFrame(scaled_buffer,
                         inFrame.timestamp(),
                         inFrame.render_time_ms(),
                         inFrame.rotation());

  return VPM_OK;
}

int32_t VPMSimpleSpatialResampler::TargetHeight() {
  return target_height_;
}

int32_t VPMSimpleSpatialResampler::TargetWidth() {
  return target_width_;
}

bool VPMSimpleSpatialResampler::ApplyResample(int32_t width, int32_t height) {
  if ((width == target_width_ && height == target_height_) ||
      resampling_mode_ == kNoRescaling)
    return false;
  else
    return true;
}

}  // namespace webrtc
