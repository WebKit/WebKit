
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

#include <optional>

#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {
namespace {
constexpr int kVp8DefaultStaticQpThreshold = 15;

TEST(QualityConvergenceController, Singlecast) {
  test::ScopedKeyValueConfig field_trials;
  QualityConvergenceController controller;
  controller.Initialize(1, /*encoder_min_qp=*/std::nullopt, kVideoCodecVP8,
                        field_trials);

  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, kVp8DefaultStaticQpThreshold + 1,
      /*is_refresh_frame=*/false));
  EXPECT_TRUE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, kVp8DefaultStaticQpThreshold,
      /*is_refresh_frame=*/false));
}

TEST(QualityConvergenceController, Simulcast) {
  test::ScopedKeyValueConfig field_trials;
  QualityConvergenceController controller;
  controller.Initialize(2, /*encoder_min_qp=*/std::nullopt, kVideoCodecVP8,
                        field_trials);

  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, kVp8DefaultStaticQpThreshold + 1,
      /*is_refresh_frame=*/false));
  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/1, kVp8DefaultStaticQpThreshold + 1,
      /*is_refresh_frame=*/false));

  // Layer 0 reaches target quality.
  EXPECT_TRUE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, kVp8DefaultStaticQpThreshold,
      /*is_refresh_frame=*/false));
  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/1, kVp8DefaultStaticQpThreshold + 1,
      /*is_refresh_frame=*/false));

  // Frames are repeated for both layers. Layer 0 still at target quality.
  EXPECT_TRUE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, kVp8DefaultStaticQpThreshold,
      /*is_refresh_frame=*/true));
  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/1, kVp8DefaultStaticQpThreshold + 1,
      /*is_refresh_frame=*/true));
}

TEST(QualityConvergenceController, InvalidLayerIndex) {
  test::ScopedKeyValueConfig field_trials;
  QualityConvergenceController controller;
  controller.Initialize(2, /*encoder_min_qp=*/std::nullopt, kVideoCodecVP8,
                        field_trials);

  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/-1, kVp8DefaultStaticQpThreshold,
      /*is_refresh_frame=*/false));
  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/3, kVp8DefaultStaticQpThreshold,
      /*is_refresh_frame=*/false));
}

TEST(QualityConvergenceController, UseMaxOfEncoderMinAndDefaultQpThresholds) {
  test::ScopedKeyValueConfig field_trials;
  QualityConvergenceController controller;
  controller.Initialize(1, kVp8DefaultStaticQpThreshold + 1, kVideoCodecVP8,
                        field_trials);

  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, kVp8DefaultStaticQpThreshold + 2,
      /*is_refresh_frame=*/false));
  EXPECT_TRUE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, kVp8DefaultStaticQpThreshold + 1,
      /*is_refresh_frame=*/false));
}

TEST(QualityConvergenceController, OverrideVp8StaticThreshold) {
  test::ScopedKeyValueConfig field_trials(
      "WebRTC-QCM-Static-VP8/static_qp_threshold:22/");
  QualityConvergenceController controller;
  controller.Initialize(1, /*encoder_min_qp=*/std::nullopt, kVideoCodecVP8,
                        field_trials);

  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, /*qp=*/23, /*is_refresh_frame=*/false));
  EXPECT_TRUE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, /*qp=*/22, /*is_refresh_frame=*/false));
}

TEST(QualityConvergenceMonitorSetup, OverrideVp9StaticThreshold) {
  test::ScopedKeyValueConfig field_trials(
      "WebRTC-QCM-Static-VP9/static_qp_threshold:44/");
  QualityConvergenceController controller;
  controller.Initialize(1, /*encoder_min_qp=*/std::nullopt, kVideoCodecVP9,
                        field_trials);

  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, /*qp=*/45, /*is_refresh_frame=*/false));
  EXPECT_TRUE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, /*qp=*/44, /*is_refresh_frame=*/false));
}

TEST(QualityConvergenceMonitorSetup, OverrideAv1StaticThreshold) {
  test::ScopedKeyValueConfig field_trials(
      "WebRTC-QCM-Static-AV1/static_qp_threshold:46/");
  QualityConvergenceController controller;
  controller.Initialize(1, /*encoder_min_qp=*/std::nullopt, kVideoCodecAV1,
                        field_trials);

  EXPECT_FALSE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, /*qp=*/47, /*is_refresh_frame=*/false));
  EXPECT_TRUE(controller.AddSampleAndCheckTargetQuality(
      /*layer_index=*/0, /*qp=*/46, /*is_refresh_frame=*/false));
}

}  // namespace
}  // namespace webrtc
