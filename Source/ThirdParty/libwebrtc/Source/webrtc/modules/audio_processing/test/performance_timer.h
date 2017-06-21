/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_TEST_PERFORMANCE_TIMER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_TEST_PERFORMANCE_TIMER_H_

#include <vector>

#include "webrtc/base/optional.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

class PerformanceTimer {
 public:
  explicit PerformanceTimer(int num_frames_to_process);
  ~PerformanceTimer();

  void StartTimer();
  void StopTimer();

  double GetDurationAverage() const;
  double GetDurationStandardDeviation() const;

  // These methods are the same as those above, but they ignore the first
  // |number_of_warmup_samples| measurements.
  double GetDurationAverage(size_t number_of_warmup_samples) const;
  double GetDurationStandardDeviation(size_t number_of_warmup_samples) const;

 private:
  webrtc::Clock* clock_;
  rtc::Optional<int64_t> start_timestamp_us_;
  std::vector<int64_t> timestamps_us_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_TEST_PERFORMANCE_TIMER_H_
