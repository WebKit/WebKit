/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/skew_estimator.h"

#include "test/gtest.h"

namespace webrtc {
namespace aec3 {

// Tests that the skew ends up as it should after a skew change.
TEST(SkewEstimator, SkewChangeAdaptation) {
  constexpr int kNumSkewsLog2 = 7;
  constexpr int kNumSkews = 1 << kNumSkewsLog2;

  SkewEstimator estimator(kNumSkewsLog2);

  for (int k = 0; k < kNumSkews - 1; ++k) {
    estimator.LogRenderCall();
    auto skew = estimator.GetSkewFromCapture();
    EXPECT_FALSE(skew);
  }

  estimator.LogRenderCall();

  absl::optional<int> skew;
  for (int k = 0; k < kNumSkews; ++k) {
    estimator.LogRenderCall();
    skew = estimator.GetSkewFromCapture();
    EXPECT_TRUE(skew);
  }
  EXPECT_EQ(1, *skew);

  estimator.LogRenderCall();

  for (int k = 0; k < kNumSkews; ++k) {
    estimator.LogRenderCall();
    skew = estimator.GetSkewFromCapture();
    EXPECT_TRUE(skew);
  }
  EXPECT_EQ(2, *skew);
}

// Tests that the skew ends up as it should for a surplus of render calls.
TEST(SkewEstimator, SkewForSurplusRender) {
  constexpr int kNumSkewsLog2 = 7;
  constexpr int kNumSkews = 1 << kNumSkewsLog2;

  SkewEstimator estimator(kNumSkewsLog2);

  for (int k = 0; k < kNumSkews - 1; ++k) {
    estimator.LogRenderCall();
    auto skew = estimator.GetSkewFromCapture();
    EXPECT_FALSE(skew);
  }

  estimator.LogRenderCall();

  absl::optional<int> skew;
  for (int k = 0; k < kNumSkews; ++k) {
    estimator.LogRenderCall();
    skew = estimator.GetSkewFromCapture();
    EXPECT_TRUE(skew);
  }
  EXPECT_EQ(1, *skew);
}

// Tests that the skew ends up as it should for a surplus of capture calls.
TEST(SkewEstimator, SkewForSurplusCapture) {
  constexpr int kNumSkewsLog2 = 7;
  constexpr int kNumSkews = 1 << kNumSkewsLog2;

  SkewEstimator estimator(kNumSkewsLog2);

  for (int k = 0; k < kNumSkews - 1; ++k) {
    estimator.LogRenderCall();
    auto skew = estimator.GetSkewFromCapture();
    EXPECT_FALSE(skew);
  }

  absl::optional<int> skew;
  skew = estimator.GetSkewFromCapture();

  for (int k = 0; k < kNumSkews; ++k) {
    estimator.LogRenderCall();
    skew = estimator.GetSkewFromCapture();
    EXPECT_TRUE(skew);
  }
  EXPECT_EQ(-1, *skew);
}

// Tests that the skew estimator returns a null optional when it should.
TEST(SkewEstimator, NullEstimate) {
  constexpr int kNumSkewsLog2 = 4;
  constexpr int kNumSkews = 1 << kNumSkewsLog2;

  SkewEstimator estimator(kNumSkewsLog2);

  for (int k = 0; k < kNumSkews - 1; ++k) {
    estimator.LogRenderCall();
    auto skew = estimator.GetSkewFromCapture();
    EXPECT_FALSE(skew);
  }

  estimator.LogRenderCall();
  auto skew = estimator.GetSkewFromCapture();
  EXPECT_TRUE(skew);

  estimator.Reset();
  for (int k = 0; k < kNumSkews - 1; ++k) {
    estimator.LogRenderCall();
    auto skew = estimator.GetSkewFromCapture();
    EXPECT_FALSE(skew);
  }
}

// Tests that the skew estimator properly rounds the average skew.
TEST(SkewEstimator, SkewRounding) {
  constexpr int kNumSkewsLog2 = 4;
  constexpr int kNumSkews = 1 << kNumSkewsLog2;

  SkewEstimator estimator(kNumSkewsLog2);

  absl::optional<int> skew;
  for (int k = 0; k < kNumSkews; ++k) {
    if (k == kNumSkews - 1) {
      // Reverse call order once.
      skew = estimator.GetSkewFromCapture();
      estimator.LogRenderCall();
    } else {
      // Normal call order.
      estimator.LogRenderCall();
      skew = estimator.GetSkewFromCapture();
    }
  }
  EXPECT_EQ(*skew, 0);

  estimator.Reset();
  for (int k = 0; k < kNumSkews; ++k) {
    estimator.LogRenderCall();
    estimator.LogRenderCall();
    estimator.LogRenderCall();
    estimator.GetSkewFromCapture();
    estimator.GetSkewFromCapture();
    skew = estimator.GetSkewFromCapture();
  }
  EXPECT_EQ(*skew, 1);
}
}  // namespace aec3
}  // namespace webrtc
