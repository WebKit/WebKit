/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_

#include "webrtc/common_types.h"
#include "webrtc/common_video/include/i420_buffer_pool.h"
#include "webrtc/modules/video_coding/utility/moving_average.h"

namespace webrtc {
class QualityScaler {
 public:
  struct Resolution {
    int width;
    int height;
  };

  QualityScaler();
  void Init(VideoCodecType codec_type,
            int initial_bitrate_kbps,
            int width,
            int height,
            int fps);
  void Init(int low_qp_threshold,
            int high_qp_threshold,
            int initial_bitrate_kbps,
            int width,
            int height,
            int fps);
  void ReportFramerate(int framerate);
  void ReportQP(int qp);
  void ReportDroppedFrame();
  void OnEncodeFrame(int width, int height);
  Resolution GetScaledResolution() const;
  rtc::scoped_refptr<VideoFrameBuffer> GetScaledBuffer(
      const rtc::scoped_refptr<VideoFrameBuffer>& frame);
  int downscale_shift() const { return downscale_shift_; }

 private:
  void ClearSamples();
  void ScaleUp();
  void ScaleDown();
  void UpdateTargetResolution(int width, int height);

  I420BufferPool pool_;

  size_t num_samples_downscale_;
  size_t num_samples_upscale_;
  bool fast_rampup_;
  MovingAverage average_qp_;
  MovingAverage framedrop_percent_;

  int low_qp_threshold_;
  int high_qp_threshold_;
  Resolution target_res_;

  int downscale_shift_;
  int maximum_shift_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_
