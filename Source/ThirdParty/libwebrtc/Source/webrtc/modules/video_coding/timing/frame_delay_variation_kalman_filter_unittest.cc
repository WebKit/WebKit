/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/frame_delay_variation_kalman_filter.h"

#include "test/gtest.h"

namespace webrtc {
namespace {

// This test verifies that the initial filter state (link bandwidth, link
// propagation delay) is such that a frame of size zero would take no time to
// propagate.
TEST(FrameDelayVariationKalmanFilterTest,
     InitializedFilterWithZeroSizeFrameTakesNoTimeToPropagate) {
  FrameDelayVariationKalmanFilter filter;

  // A zero-sized frame...
  double frame_size_variation_bytes = 0.0;

  // ...should take no time to propagate due to it's size...
  EXPECT_EQ(filter.GetFrameDelayVariationEstimateSizeBased(
                frame_size_variation_bytes),
            0.0);

  // ...and no time due to the initial link propagation delay being zero.
  EXPECT_EQ(
      filter.GetFrameDelayVariationEstimateTotal(frame_size_variation_bytes),
      0.0);
}

// TODO(brandtr): Look into if there is a factor 1000 missing here? It seems
// unreasonable to have an initial link bandwidth of 512 _mega_bits per second?
TEST(FrameDelayVariationKalmanFilterTest,
     InitializedFilterWithSmallSizeFrameTakesFixedTimeToPropagate) {
  FrameDelayVariationKalmanFilter filter;

  // A 1000-byte frame...
  double frame_size_variation_bytes = 1000.0;
  // ...should take around `1000.0 / (512e3 / 8.0) = 0.015625 ms` to transmit.
  double expected_frame_delay_variation_estimate_ms = 1000.0 / (512e3 / 8.0);

  EXPECT_EQ(filter.GetFrameDelayVariationEstimateSizeBased(
                frame_size_variation_bytes),
            expected_frame_delay_variation_estimate_ms);
  EXPECT_EQ(
      filter.GetFrameDelayVariationEstimateTotal(frame_size_variation_bytes),
      expected_frame_delay_variation_estimate_ms);
}

TEST(FrameDelayVariationKalmanFilterTest,
     NegativeNoiseVarianceDoesNotUpdateFilter) {
  FrameDelayVariationKalmanFilter filter;

  // Negative variance...
  double var_noise = -0.1;
  filter.PredictAndUpdate(/*frame_delay_variation_ms=*/3,
                          /*frame_size_variation_bytes=*/200.0,
                          /*max_frame_size_bytes=*/2000, var_noise);

  // ...does _not_ update the filter.
  EXPECT_EQ(filter.GetFrameDelayVariationEstimateTotal(
                /*frame_size_variation_bytes=*/0.0),
            0.0);

  // Positive variance...
  var_noise = 0.1;
  filter.PredictAndUpdate(/*frame_delay_variation_ms=*/3,
                          /*frame_size_variation_bytes=*/200.0,
                          /*max_frame_size_bytes=*/2000, var_noise);

  // ...does update the filter.
  EXPECT_GT(filter.GetFrameDelayVariationEstimateTotal(
                /*frame_size_variation_bytes=*/0.0),
            0.0);
}

TEST(FrameDelayVariationKalmanFilterTest,
     VerifyConvergenceWithAlternatingDeviations) {
  FrameDelayVariationKalmanFilter filter;

  // One frame every 33 ms.
  int framerate_fps = 30;
  // Let's assume approximately 10% delay variation.
  double frame_delay_variation_ms = 3;
  // With a bitrate of 512 kbps, each frame will be around 2000 bytes.
  double max_frame_size_bytes = 2000;
  // And again, let's assume 10% size deviation.
  double frame_size_variation_bytes = 200;
  double var_noise = 0.1;
  int test_duration_s = 60;

  for (int i = 0; i < test_duration_s * framerate_fps; ++i) {
    // For simplicity, assume alternating variations.
    double sign = (i % 2 == 0) ? 1.0 : -1.0;
    filter.PredictAndUpdate(sign * frame_delay_variation_ms,
                            sign * frame_size_variation_bytes,
                            max_frame_size_bytes, var_noise);
  }

  // Verify that the filter has converged within a margin of 0.1 ms.
  EXPECT_NEAR(
      filter.GetFrameDelayVariationEstimateTotal(frame_size_variation_bytes),
      frame_delay_variation_ms, 0.1);
}

}  // namespace
}  // namespace webrtc
