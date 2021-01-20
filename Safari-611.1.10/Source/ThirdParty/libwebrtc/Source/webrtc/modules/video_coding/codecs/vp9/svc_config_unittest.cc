/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp9/svc_config.h"

#include <cstddef>
#include <vector>

#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "test/gtest.h"

namespace webrtc {
TEST(SvcConfig, NumSpatialLayers) {
  const size_t max_num_spatial_layers = 6;
  const size_t first_active_layer = 0;
  const size_t num_spatial_layers = 2;

  std::vector<SpatialLayer> spatial_layers =
      GetSvcConfig(kMinVp9SpatialLayerWidth << (num_spatial_layers - 1),
                   kMinVp9SpatialLayerHeight << (num_spatial_layers - 1), 30,
                   first_active_layer, max_num_spatial_layers, 1, false);

  EXPECT_EQ(spatial_layers.size(), num_spatial_layers);
}

TEST(SvcConfig, AlwaysSendsAtLeastOneLayer) {
  const size_t max_num_spatial_layers = 6;
  const size_t first_active_layer = 5;

  std::vector<SpatialLayer> spatial_layers =
      GetSvcConfig(kMinVp9SpatialLayerWidth, kMinVp9SpatialLayerHeight, 30,
                   first_active_layer, max_num_spatial_layers, 1, false);
  EXPECT_EQ(spatial_layers.size(), 1u);
  EXPECT_EQ(spatial_layers.back().width, kMinVp9SpatialLayerWidth);
}

TEST(SvcConfig, EnforcesMinimalRequiredParity) {
  const size_t max_num_spatial_layers = 3;
  const size_t kOddSize = 1023;

  std::vector<SpatialLayer> spatial_layers =
      GetSvcConfig(kOddSize, kOddSize, 30,
                   /*first_active_layer=*/1, max_num_spatial_layers, 1, false);
  // Since there are 2 layers total (1, 2), divisiblity by 2 is required.
  EXPECT_EQ(spatial_layers.back().width, kOddSize - 1);
  EXPECT_EQ(spatial_layers.back().width, kOddSize - 1);

  spatial_layers =
      GetSvcConfig(kOddSize, kOddSize, 30,
                   /*first_active_layer=*/0, max_num_spatial_layers, 1, false);
  // Since there are 3 layers total (0, 1, 2), divisiblity by 4 is required.
  EXPECT_EQ(spatial_layers.back().width, kOddSize - 3);
  EXPECT_EQ(spatial_layers.back().width, kOddSize - 3);

  spatial_layers =
      GetSvcConfig(kOddSize, kOddSize, 30,
                   /*first_active_layer=*/2, max_num_spatial_layers, 1, false);
  // Since there is only 1 layer active (2), divisiblity by 1 is required.
  EXPECT_EQ(spatial_layers.back().width, kOddSize);
  EXPECT_EQ(spatial_layers.back().width, kOddSize);
}

TEST(SvcConfig, SkipsInactiveLayers) {
  const size_t num_spatial_layers = 4;
  const size_t first_active_layer = 2;

  std::vector<SpatialLayer> spatial_layers =
      GetSvcConfig(kMinVp9SpatialLayerWidth << (num_spatial_layers - 1),
                   kMinVp9SpatialLayerHeight << (num_spatial_layers - 1), 30,
                   first_active_layer, num_spatial_layers, 1, false);
  EXPECT_EQ(spatial_layers.size(), 2u);
  EXPECT_EQ(spatial_layers.back().width,
            kMinVp9SpatialLayerWidth << (num_spatial_layers - 1));
}

TEST(SvcConfig, BitrateThresholds) {
  const size_t first_active_layer = 0;
  const size_t num_spatial_layers = 3;
  std::vector<SpatialLayer> spatial_layers =
      GetSvcConfig(kMinVp9SpatialLayerWidth << (num_spatial_layers - 1),
                   kMinVp9SpatialLayerHeight << (num_spatial_layers - 1), 30,
                   first_active_layer, num_spatial_layers, 1, false);

  EXPECT_EQ(spatial_layers.size(), num_spatial_layers);

  for (const SpatialLayer& layer : spatial_layers) {
    EXPECT_LE(layer.minBitrate, layer.maxBitrate);
    EXPECT_LE(layer.minBitrate, layer.targetBitrate);
    EXPECT_LE(layer.targetBitrate, layer.maxBitrate);
  }
}

TEST(SvcConfig, ScreenSharing) {
  std::vector<SpatialLayer> spatial_layers =
      GetSvcConfig(1920, 1080, 30, 1, 3, 3, true);

  EXPECT_EQ(spatial_layers.size(), 3UL);

  for (size_t i = 0; i < 3; ++i) {
    const SpatialLayer& layer = spatial_layers[i];
    EXPECT_EQ(layer.width, 1920);
    EXPECT_EQ(layer.height, 1080);
    EXPECT_EQ(layer.maxFramerate, (i < 1) ? 5 : (i < 2 ? 10 : 30));
    EXPECT_EQ(layer.numberOfTemporalLayers, 1);
    EXPECT_LE(layer.minBitrate, layer.maxBitrate);
    EXPECT_LE(layer.minBitrate, layer.targetBitrate);
    EXPECT_LE(layer.targetBitrate, layer.maxBitrate);
  }
}
}  // namespace webrtc
