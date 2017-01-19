/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_FRAME_PREPROCESSOR_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_FRAME_PREPROCESSOR_H_

#include <memory>

#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/modules/video_processing/spatial_resampler.h"
#include "webrtc/modules/video_processing/video_decimator.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_frame.h"

namespace webrtc {

class VideoDenoiser;

// All pointers/members in this class are assumed to be protected by the class
// owner.
class VPMFramePreprocessor {
 public:
  VPMFramePreprocessor();
  ~VPMFramePreprocessor();

  void Reset();

  // Enable temporal decimation.
  void EnableTemporalDecimation(bool enable);

  void SetInputFrameResampleMode(VideoFrameResampling resampling_mode);

  // Set target resolution: frame rate and dimension.
  int32_t SetTargetResolution(uint32_t width,
                              uint32_t height,
                              uint32_t frame_rate);

  // Update incoming frame rate/dimension.
  void UpdateIncomingframe_rate();

  int32_t updateIncomingFrameSize(uint32_t width, uint32_t height);

  // Set decimated values: frame rate/dimension.
  uint32_t GetDecimatedFrameRate();
  uint32_t GetDecimatedWidth() const;
  uint32_t GetDecimatedHeight() const;

  // Preprocess output:
  void EnableDenoising(bool enable);
  const VideoFrame* PreprocessFrame(const VideoFrame& frame);

 private:
  // The content does not change so much every frame, so to reduce complexity
  // we can compute new content metrics every |kSkipFrameCA| frames.
  enum { kSkipFrameCA = 2 };

  VideoFrame denoised_frame_;
  VideoFrame resampled_frame_;
  VPMSpatialResampler* spatial_resampler_;
  VPMVideoDecimator* vd_;
  std::unique_ptr<VideoDenoiser> denoiser_;
  uint32_t frame_cnt_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_FRAME_PREPROCESSOR_H_
