/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_MOVING_AVERAGE_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_MOVING_AVERAGE_H_

#include <vector>

#include "webrtc/base/optional.h"

namespace webrtc {
class MovingAverage {
 public:
  explicit MovingAverage(size_t s);
  void AddSample(int sample);
  rtc::Optional<int> GetAverage() const;
  rtc::Optional<int> GetAverage(size_t num_samples) const;
  void Reset();
  size_t size() const;

 private:
  size_t count_ = 0;
  int sum_ = 0;
  std::vector<int> sum_history_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_MOVING_AVERAGE_H_
