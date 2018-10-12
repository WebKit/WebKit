/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <vector>

#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "modules/video_coding/codecs/vp9/svc_config.h"
#include "test/gtest.h"

namespace webrtc {
TEST(SvcConfig, NumSpatialLayers) {
  const size_t max_num_spatial_layers = 6;
  const size_t num_spatial_layers = 2;

  std::vector<SpatialLayer> spatial_layers =
      GetSvcConfig(kMinVp9SpatialLayerWidth << (num_spatial_layers - 1),
                   kMinVp9SpatialLayerHeight << (num_spatial_layers - 1), 30,
                   max_num_spatial_layers, 1, false);

  EXPECT_EQ(spatial_layers.size(), num_spatial_layers);
}

TEST(SvcConfig, BitrateThresholds) {
  const size_t num_spatial_layers = 3;
  std::vector<SpatialLayer> spatial_layers =
      GetSvcConfig(kMinVp9SpatialLayerWidth << (num_spatial_layers - 1),
                   kMinVp9SpatialLayerHeight << (num_spatial_layers - 1), 30,
                   num_spatial_layers, 1, false);

  EXPECT_EQ(spatial_layers.size(), num_spatial_layers);

  for (const SpatialLayer& layer : spatial_layers) {
    EXPECT_LE(layer.minBitrate, layer.maxBitrate);
    EXPECT_LE(layer.minBitrate, layer.targetBitrate);
    EXPECT_LE(layer.targetBitrate, layer.maxBitrate);
  }
}

TEST(SvcConfig, ScreenSharing) {
  std::vector<SpatialLayer> spatial_layers =
      GetSvcConfig(1920, 1080, 30, 3, 3, true);

  EXPECT_EQ(spatial_layers.size(), 2UL);

  for (const SpatialLayer& layer : spatial_layers) {
    EXPECT_EQ(layer.width, 1920);
    EXPECT_EQ(layer.height, 1080);
    EXPECT_EQ(layer.maxFramerate, 5);
    EXPECT_EQ(layer.numberOfTemporalLayers, 1);
    EXPECT_LE(layer.minBitrate, layer.maxBitrate);
    EXPECT_LE(layer.minBitrate, layer.targetBitrate);
    EXPECT_LE(layer.targetBitrate, layer.maxBitrate);
  }
}
}  // namespace webrtc
