/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/test/echo_canceller_test_tools.h"

#include "webrtc/base/checks.h"

namespace webrtc {

void RandomizeSampleVector(Random* random_generator, rtc::ArrayView<float> v) {
  for (auto& v_k : v) {
    v_k = 2 * 32767.f * random_generator->Rand<float>() - 32767.f;
  }
}

template <typename T>
void DelayBuffer<T>::Delay(rtc::ArrayView<const T> x,
                           rtc::ArrayView<T> x_delayed) {
  RTC_DCHECK_EQ(x.size(), x_delayed.size());
  if (buffer_.size() == 0) {
    std::copy(x.begin(), x.end(), x_delayed.begin());
  } else {
    for (size_t k = 0; k < x.size(); ++k) {
      x_delayed[k] = buffer_[next_insert_index_];
      buffer_[next_insert_index_] = x[k];
      next_insert_index_ = (next_insert_index_ + 1) % buffer_.size();
    }
  }
}

template class DelayBuffer<float>;
template class DelayBuffer<int>;
}  // namespace webrtc
