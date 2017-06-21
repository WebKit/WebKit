/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_processing/aec3/decimator_by_4.h"

#include "webrtc/base/checks.h"

namespace webrtc {
namespace {

// [B,A] = butter(2,1500/16000) which are the same as [B,A] =
// butter(2,750/8000).
const CascadedBiQuadFilter::BiQuadCoefficients kLowPassFilterCoefficients = {
    {0.0179f, 0.0357f, 0.0179f},
    {-1.5879f, 0.6594f}};

}  // namespace

DecimatorBy4::DecimatorBy4()
    : low_pass_filter_(kLowPassFilterCoefficients, 3) {}

void DecimatorBy4::Decimate(rtc::ArrayView<const float> in,
                            rtc::ArrayView<float> out) {
  RTC_DCHECK_EQ(kBlockSize, in.size());
  RTC_DCHECK_EQ(kSubBlockSize, out.size());
  std::array<float, kBlockSize> x;

  // Limit the frequency content of the signal to avoid aliasing.
  low_pass_filter_.Process(in, x);

  // Downsample the signal.
  for (size_t j = 0, k = 0; j < out.size(); ++j, k += 4) {
    RTC_DCHECK_GT(kBlockSize, k);
    out[j] = x[k];
  }
}

}  // namespace webrtc
