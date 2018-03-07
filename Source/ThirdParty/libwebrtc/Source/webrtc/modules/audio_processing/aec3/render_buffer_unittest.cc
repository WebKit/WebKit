/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_buffer.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "test/gtest.h"

namespace webrtc {

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for the provided numbers of Ffts to include in the
// spectral sum.
TEST(RenderBuffer, TooLargeNumberOfSpectralSums) {
  EXPECT_DEATH(
      RenderBuffer(Aec3Optimization::kNone, 3, 1, std::vector<size_t>(2, 1)),
      "");
}

TEST(RenderBuffer, TooSmallNumberOfSpectralSums) {
  EXPECT_DEATH(
      RenderBuffer(Aec3Optimization::kNone, 3, 1, std::vector<size_t>()), "");
}

// Verifies the feasibility check for the provided number of Ffts to include in
// the spectral.
TEST(RenderBuffer, FeasibleNumberOfFftsInSum) {
  EXPECT_DEATH(
      RenderBuffer(Aec3Optimization::kNone, 3, 1, std::vector<size_t>(1, 2)),
      "");
}

#endif

}  // namespace webrtc
