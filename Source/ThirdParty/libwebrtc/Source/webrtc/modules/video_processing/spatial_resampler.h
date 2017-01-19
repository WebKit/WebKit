/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_SPATIAL_RESAMPLER_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_SPATIAL_RESAMPLER_H_

#include "webrtc/typedefs.h"

#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_processing/include/video_processing_defines.h"

#include "webrtc/common_video/include/i420_buffer_pool.h"
#include "webrtc/video_frame.h"

namespace webrtc {

class VPMSpatialResampler {
 public:
  virtual ~VPMSpatialResampler() {}
  virtual int32_t SetTargetFrameSize(int32_t width, int32_t height) = 0;
  virtual void SetInputFrameResampleMode(
      VideoFrameResampling resampling_mode) = 0;
  virtual void Reset() = 0;
  virtual int32_t ResampleFrame(const VideoFrame& inFrame,
                                VideoFrame* outFrame) = 0;
  virtual int32_t TargetWidth() = 0;
  virtual int32_t TargetHeight() = 0;
  virtual bool ApplyResample(int32_t width, int32_t height) = 0;
};

class VPMSimpleSpatialResampler : public VPMSpatialResampler {
 public:
  VPMSimpleSpatialResampler();
  ~VPMSimpleSpatialResampler();
  virtual int32_t SetTargetFrameSize(int32_t width, int32_t height);
  virtual void SetInputFrameResampleMode(VideoFrameResampling resampling_mode);
  virtual void Reset();
  virtual int32_t ResampleFrame(const VideoFrame& inFrame,
                                VideoFrame* outFrame);
  virtual int32_t TargetWidth();
  virtual int32_t TargetHeight();
  virtual bool ApplyResample(int32_t width, int32_t height);

 private:
  VideoFrameResampling resampling_mode_;
  int32_t target_width_;
  int32_t target_height_;
  I420BufferPool buffer_pool_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_SPATIAL_RESAMPLER_H_
