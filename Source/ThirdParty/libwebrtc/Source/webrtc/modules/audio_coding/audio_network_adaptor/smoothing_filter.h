/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_SMOOTHING_FILTER_H_
#define WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_SMOOTHING_FILTER_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/exp_filter.h"
#include "webrtc/base/optional.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

class SmoothingFilter {
 public:
  virtual ~SmoothingFilter() = default;
  virtual void AddSample(float sample) = 0;
  virtual rtc::Optional<float> GetAverage() const = 0;
};

// SmoothingFilterImpl applies an exponential filter
//   alpha = exp(-sample_interval / time_constant);
//   y[t] = alpha * y[t-1] + (1 - alpha) * sample;
class SmoothingFilterImpl final : public SmoothingFilter {
 public:
  // |time_constant_ms| is the time constant for the exponential filter.
  SmoothingFilterImpl(int time_constant_ms, const Clock* clock);

  void AddSample(float sample) override;
  rtc::Optional<float> GetAverage() const override;

 private:
  const int time_constant_ms_;
  const Clock* const clock_;

  bool first_sample_received_;
  bool initialized_;
  int64_t first_sample_time_ms_;
  int64_t last_sample_time_ms_;
  rtc::ExpFilter filter_;

  RTC_DISALLOW_COPY_AND_ASSIGN(SmoothingFilterImpl);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_SMOOTHING_FILTER_H_
