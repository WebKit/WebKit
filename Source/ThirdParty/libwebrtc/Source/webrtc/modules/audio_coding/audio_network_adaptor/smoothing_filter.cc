/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>

#include "webrtc/modules/audio_coding/audio_network_adaptor/smoothing_filter.h"

namespace webrtc {

SmoothingFilterImpl::SmoothingFilterImpl(int time_constant_ms,
                                         const Clock* clock)
    : time_constant_ms_(time_constant_ms),
      clock_(clock),
      first_sample_received_(false),
      initialized_(false),
      first_sample_time_ms_(0),
      last_sample_time_ms_(0),
      filter_(0.0) {}

void SmoothingFilterImpl::AddSample(float sample) {
  if (!first_sample_received_) {
    last_sample_time_ms_ = first_sample_time_ms_ = clock_->TimeInMilliseconds();
    first_sample_received_ = true;
    RTC_DCHECK_EQ(rtc::ExpFilter::kValueUndefined, filter_.filtered());

    // Since this is first sample, any value for argument 1 should work.
    filter_.Apply(0.0f, sample);
    return;
  }

  int64_t now_ms = clock_->TimeInMilliseconds();
  if (!initialized_) {
    float duration = now_ms - first_sample_time_ms_;
    if (duration < static_cast<int64_t>(time_constant_ms_)) {
      filter_.UpdateBase(exp(1.0f / duration));
    } else {
      initialized_ = true;
      filter_.UpdateBase(exp(1.0f / time_constant_ms_));
    }
  }

  // The filter will do the following:
  //   float alpha = pow(base, last_update_time_ms_ - now_ms);
  //   filtered_ = alpha * filtered_ + (1 - alpha) * sample;
  filter_.Apply(static_cast<float>(last_sample_time_ms_ - now_ms), sample);
  last_sample_time_ms_ = now_ms;
}

rtc::Optional<float> SmoothingFilterImpl::GetAverage() const {
  float value = filter_.filtered();
  return value == rtc::ExpFilter::kValueUndefined ? rtc::Optional<float>()
                                                  : rtc::Optional<float>(value);
}

}  // namespace webrtc
