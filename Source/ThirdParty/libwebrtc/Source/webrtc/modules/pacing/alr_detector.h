/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_PACING_ALR_DETECTOR_H_
#define WEBRTC_MODULES_PACING_ALR_DETECTOR_H_

#include "webrtc/common_types.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Application limited region detector is a class that utilizes signals of
// elapsed time and bytes sent to estimate whether network traffic is
// currently limited by the application's ability to generate traffic.
//
// AlrDetector provides a signal that can be utilized to adjust
// estimate bandwidth.
// Note: This class is not thread-safe.
class AlrDetector {
 public:
  AlrDetector();
  ~AlrDetector();
  void OnBytesSent(size_t bytes_sent, int64_t elapsed_time_ms);
  // Set current estimated bandwidth.
  void SetEstimatedBitrate(int bitrate_bps);
  // Returns true if currently in application-limited region.
  bool InApplicationLimitedRegion();

 private:
  size_t measurement_interval_bytes_sent_;
  int64_t measurement_interval_elapsed_time_ms_;
  int estimated_bitrate_bps_;
  // Number of consecutive periods over which we observe traffic is application
  // limited.
  int application_limited_count_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_PACING_ALR_DETECTOR_H_
