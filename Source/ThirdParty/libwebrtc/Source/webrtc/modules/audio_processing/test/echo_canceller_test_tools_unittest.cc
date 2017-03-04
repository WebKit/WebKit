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

#include <vector>

#include "webrtc/base/array_view.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/random.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

TEST(EchoCancellerTestTools, FloatDelayBuffer) {
  constexpr size_t kDelay = 10;
  DelayBuffer<float> delay_buffer(kDelay);
  std::vector<float> v(1000, 0.f);
  for (size_t k = 0; k < v.size(); ++k) {
    v[k] = k;
  }
  std::vector<float> v_delayed = v;
  constexpr size_t kBlockSize = 50;
  for (size_t k = 0; k < rtc::CheckedDivExact(v.size(), kBlockSize); ++k) {
    delay_buffer.Delay(
        rtc::ArrayView<const float>(&v[k * kBlockSize], kBlockSize),
        rtc::ArrayView<float>(&v_delayed[k * kBlockSize], kBlockSize));
  }
  for (size_t k = kDelay; k < v.size(); ++k) {
    EXPECT_EQ(v[k - kDelay], v_delayed[k]);
  }
}

TEST(EchoCancellerTestTools, IntDelayBuffer) {
  constexpr size_t kDelay = 10;
  DelayBuffer<int> delay_buffer(kDelay);
  std::vector<int> v(1000, 0);
  for (size_t k = 0; k < v.size(); ++k) {
    v[k] = k;
  }
  std::vector<int> v_delayed = v;
  const size_t kBlockSize = 50;
  for (size_t k = 0; k < rtc::CheckedDivExact(v.size(), kBlockSize); ++k) {
    delay_buffer.Delay(
        rtc::ArrayView<const int>(&v[k * kBlockSize], kBlockSize),
        rtc::ArrayView<int>(&v_delayed[k * kBlockSize], kBlockSize));
  }
  for (size_t k = kDelay; k < v.size(); ++k) {
    EXPECT_EQ(v[k - kDelay], v_delayed[k]);
  }
}

TEST(EchoCancellerTestTools, RandomizeSampleVector) {
  Random random_generator(42U);
  std::vector<float> v(50, 0.f);
  std::vector<float> v_ref = v;
  RandomizeSampleVector(&random_generator, v);
  EXPECT_NE(v, v_ref);
  v_ref = v;
  RandomizeSampleVector(&random_generator, v);
  EXPECT_NE(v, v_ref);
}

}  // namespace webrtc
