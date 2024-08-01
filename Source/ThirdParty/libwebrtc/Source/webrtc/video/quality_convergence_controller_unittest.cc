
/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "video/quality_convergence_controller.h"

#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {
namespace {
constexpr int kStaticQpThreshold = 15;

TEST(QualityConvergenceController, Singlecast) {
  test::ScopedKeyValueConfig field_trials;
  QualityConvergenceController controller;
  controller.Initialize(1, kStaticQpThreshold, kVideoCodecVP8, field_trials);

  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, kStaticQpThreshold + 1, /*is_refresh_frame=*/false));
  EXPECT_TRUE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, kStaticQpThreshold, /*is_refresh_frame=*/false));
}

TEST(QualityConvergenceController, Simulcast) {
  test::ScopedKeyValueConfig field_trials;
  QualityConvergenceController controller;
  controller.Initialize(2, kStaticQpThreshold, kVideoCodecVP8, field_trials);

  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, kStaticQpThreshold + 1, /*is_refresh_frame=*/false));
  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/1, kStaticQpThreshold + 1, /*is_refresh_frame=*/false));

  // Layer 0 reaches target quality.
  EXPECT_TRUE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, kStaticQpThreshold, /*is_refresh_frame=*/false));
  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/1, kStaticQpThreshold + 1, /*is_refresh_frame=*/false));

  // Frames are repeated for both layers. Layer 0 still at target quality.
  EXPECT_TRUE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, kStaticQpThreshold, /*is_refresh_frame=*/true));
  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/1, kStaticQpThreshold + 1, /*is_refresh_frame=*/true));
}

TEST(QualityConvergenceController, InvalidLayerIndex) {
  test::ScopedKeyValueConfig field_trials;
  QualityConvergenceController controller;
  controller.Initialize(2, kStaticQpThreshold, kVideoCodecVP8, field_trials);

  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/-1, kStaticQpThreshold, /*is_refresh_frame=*/false));
  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/3, kStaticQpThreshold, /*is_refresh_frame=*/false));
}

}  // namespace
}  // namespace webrtc
