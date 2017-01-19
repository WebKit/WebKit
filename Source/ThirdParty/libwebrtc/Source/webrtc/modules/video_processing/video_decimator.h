/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_DECIMATOR_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_DECIMATOR_H_

#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class VPMVideoDecimator {
 public:
  VPMVideoDecimator();
  ~VPMVideoDecimator();

  void Reset();

  void EnableTemporalDecimation(bool enable);

  void SetTargetFramerate(int frame_rate);

  bool DropFrame();

  void UpdateIncomingframe_rate();

  // Get Decimated Frame Rate/Dimensions.
  uint32_t GetDecimatedFrameRate();

  // Get input frame rate.
  uint32_t Inputframe_rate();

 private:
  void ProcessIncomingframe_rate(int64_t now);

  enum { kFrameCountHistory_size = 90 };
  enum { kFrameHistoryWindowMs = 2000 };

  // Temporal decimation.
  int32_t overshoot_modifier_;
  uint32_t drop_count_;
  uint32_t keep_count_;
  uint32_t target_frame_rate_;
  float incoming_frame_rate_;
  int64_t incoming_frame_times_[kFrameCountHistory_size];
  bool enable_temporal_decimation_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_DECIMATOR_H_
