/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/fft_buffer.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for that the provided numbers of Ffts to include in
// the spectral sum is equal to the one supported works.
TEST(FftBuffer, TooLargeNumberOfSpectralSums) {
  EXPECT_DEATH(FftBuffer(Aec3Optimization::kNone, 1, std::vector<size_t>(2, 1)),
               "");
}

TEST(FftBuffer, TooSmallNumberOfSpectralSums) {
  EXPECT_DEATH(FftBuffer(Aec3Optimization::kNone, 1, std::vector<size_t>()),
               "");
}

// Verifies that the check for that the provided number of Ffts to to include in
// the spectral is feasible works.
TEST(FftBuffer, FeasibleNumberOfFftsInSum) {
  EXPECT_DEATH(FftBuffer(Aec3Optimization::kNone, 1, std::vector<size_t>(1, 2)),
               "");
}

#endif

// Verify the basic usage of the FftBuffer.
TEST(FftBuffer, NormalUsage) {
  constexpr int kBufferSize = 10;
  FftBuffer buffer(Aec3Optimization::kNone, kBufferSize,
                   std::vector<size_t>(1, kBufferSize));
  FftData X;
  std::vector<std::array<float, kFftLengthBy2Plus1>> buffer_ref(kBufferSize);

  for (int k = 0; k < 30; ++k) {
    std::array<float, kFftLengthBy2Plus1> X2_sum_ref;
    X2_sum_ref.fill(0.f);
    for (size_t j = 0; j < buffer.Buffer().size(); ++j) {
      const std::array<float, kFftLengthBy2Plus1>& X2 = buffer.Spectrum(j);
      const std::array<float, kFftLengthBy2Plus1>& X2_ref = buffer_ref[j];
      EXPECT_EQ(X2_ref, X2);

      std::transform(X2_ref.begin(), X2_ref.end(), X2_sum_ref.begin(),
                     X2_sum_ref.begin(), std::plus<float>());
    }
    EXPECT_EQ(X2_sum_ref, buffer.SpectralSum(kBufferSize));

    std::array<float, kFftLengthBy2Plus1> X2;
    X.re.fill(k);
    X.im.fill(k);
    X.Spectrum(Aec3Optimization::kNone, &X2);
    buffer.Insert(X);
    buffer_ref.pop_back();
    buffer_ref.insert(buffer_ref.begin(), X2);
  }
}

}  // namespace webrtc
