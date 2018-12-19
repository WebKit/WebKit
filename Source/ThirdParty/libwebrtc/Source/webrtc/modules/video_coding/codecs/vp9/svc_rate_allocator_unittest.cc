/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "modules/video_coding/codecs/vp9/svc_config.h"
#include "modules/video_coding/codecs/vp9/svc_rate_allocator.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
static VideoCodec Configure(size_t width,
                            size_t height,
                            size_t num_spatial_layers,
                            size_t num_temporal_layers,
                            bool is_screen_sharing) {
  VideoCodec codec;
  codec.width = width;
  codec.height = height;
  codec.codecType = kVideoCodecVP9;
  codec.mode = is_screen_sharing ? VideoCodecMode::kScreensharing
                                 : VideoCodecMode::kRealtimeVideo;

  std::vector<SpatialLayer> spatial_layers =
      GetSvcConfig(width, height, 30, num_spatial_layers, num_temporal_layers,
                   is_screen_sharing);
  RTC_CHECK_LE(spatial_layers.size(), kMaxSpatialLayers);

  codec.VP9()->numberOfSpatialLayers =
      std::min<unsigned char>(num_spatial_layers, spatial_layers.size());
  codec.VP9()->numberOfTemporalLayers = std::min<unsigned char>(
      num_temporal_layers, spatial_layers.back().numberOfTemporalLayers);

  for (size_t sl_idx = 0; sl_idx < spatial_layers.size(); ++sl_idx) {
    codec.spatialLayers[sl_idx] = spatial_layers[sl_idx];
  }

  return codec;
}
}  // namespace

TEST(SvcRateAllocatorTest, SingleLayerFor320x180Input) {
  VideoCodec codec = Configure(320, 180, 3, 3, false);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  VideoBitrateAllocation allocation = allocator.GetAllocation(1000 * 1000, 30);

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1), 0u);
}

TEST(SvcRateAllocatorTest, TwoLayersFor640x360Input) {
  VideoCodec codec = Configure(640, 360, 3, 3, false);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  VideoBitrateAllocation allocation = allocator.GetAllocation(1000 * 1000, 30);

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0u);
  EXPECT_EQ(allocation.GetSpatialLayerSum(2), 0u);
}

TEST(SvcRateAllocatorTest, ThreeLayersFor1280x720Input) {
  VideoCodec codec = Configure(1280, 720, 3, 3, false);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  VideoBitrateAllocation allocation = allocator.GetAllocation(1000 * 1000, 30);

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(2), 0u);
}

TEST(SvcRateAllocatorTest,
     BaseLayerNonZeroBitrateEvenIfTotalIfLessThanMinimum) {
  VideoCodec codec = Configure(1280, 720, 3, 3, false);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  const SpatialLayer* layers = codec.spatialLayers;

  VideoBitrateAllocation allocation =
      allocator.GetAllocation(layers[0].minBitrate * 1000 / 2, 30);

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_LT(allocation.GetSpatialLayerSum(0), layers[0].minBitrate * 1000);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1), 0u);
}

TEST(SvcRateAllocatorTest, Disable640x360Layer) {
  VideoCodec codec = Configure(1280, 720, 3, 3, false);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  const SpatialLayer* layers = codec.spatialLayers;

  size_t min_bitrate_for_640x360_layer_kbps =
      layers[0].minBitrate + layers[1].minBitrate;

  VideoBitrateAllocation allocation = allocator.GetAllocation(
      min_bitrate_for_640x360_layer_kbps * 1000 - 1, 30);

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1), 0u);
}

TEST(SvcRateAllocatorTest, Disable1280x720Layer) {
  VideoCodec codec = Configure(1280, 720, 3, 3, false);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  const SpatialLayer* layers = codec.spatialLayers;

  size_t min_bitrate_for_1280x720_layer_kbps =
      layers[0].minBitrate + layers[1].minBitrate + layers[2].minBitrate;

  VideoBitrateAllocation allocation = allocator.GetAllocation(
      min_bitrate_for_1280x720_layer_kbps * 1000 - 1, 30);

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0u);
  EXPECT_EQ(allocation.GetSpatialLayerSum(2), 0u);
}

TEST(SvcRateAllocatorTest, BitrateIsCapped) {
  VideoCodec codec = Configure(1280, 720, 3, 3, false);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  const SpatialLayer* layers = codec.spatialLayers;

  const uint32_t link_mbps = 100;
  VideoBitrateAllocation allocation =
      allocator.GetAllocation(link_mbps * 1000000, 30);

  EXPECT_EQ(allocation.get_sum_kbps(),
            layers[0].maxBitrate + layers[1].maxBitrate + layers[2].maxBitrate);
  EXPECT_EQ(allocation.GetSpatialLayerSum(0) / 1000, layers[0].maxBitrate);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1) / 1000, layers[1].maxBitrate);
  EXPECT_EQ(allocation.GetSpatialLayerSum(2) / 1000, layers[2].maxBitrate);
}

TEST(SvcRateAllocatorTest, MinBitrateToGetQualityLayer) {
  VideoCodec codec = Configure(1280, 720, 3, 1, true);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  const SpatialLayer* layers = codec.spatialLayers;

  EXPECT_LE(codec.VP9()->numberOfSpatialLayers, 3U);

  VideoBitrateAllocation allocation =
      allocator.GetAllocation(layers[0].minBitrate * 1000, 30);
  EXPECT_EQ(allocation.GetSpatialLayerSum(0) / 1000, layers[0].minBitrate);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1), 0UL);

  allocation = allocator.GetAllocation(
      (layers[0].maxBitrate + layers[1].minBitrate) * 1000, 30);
  EXPECT_EQ(allocation.GetSpatialLayerSum(0) / 1000, layers[0].maxBitrate);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1) / 1000, layers[1].minBitrate);
}

TEST(SvcRateAllocatorTest, DeativateLayers) {
  for (int deactivated_idx = 2; deactivated_idx >= 0; --deactivated_idx) {
    VideoCodec codec = Configure(1280, 720, 3, 1, false);
    EXPECT_LE(codec.VP9()->numberOfSpatialLayers, 3U);

    codec.spatialLayers[deactivated_idx].active = false;

    SvcRateAllocator allocator = SvcRateAllocator(codec);

    VideoBitrateAllocation allocation =
        allocator.GetAllocation(10 * 1000 * 1000, 30);

    // Ensure layers spatial_idx < deactivated_idx are activated.
    for (int spatial_idx = 0; spatial_idx < deactivated_idx; ++spatial_idx) {
      EXPECT_GT(allocation.GetSpatialLayerSum(spatial_idx), 0UL);
    }

    // Ensure layers spatial_idx >= deactivated_idx are deactivated.
    for (int spatial_idx = deactivated_idx; spatial_idx < 3; ++spatial_idx) {
      EXPECT_EQ(allocation.GetSpatialLayerSum(spatial_idx), 0UL);
    }
  }
}

class SvcRateAllocatorTestParametrizedContentType
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  SvcRateAllocatorTestParametrizedContentType()
      : is_screen_sharing_(GetParam()) {}

  const bool is_screen_sharing_;
};

TEST_P(SvcRateAllocatorTestParametrizedContentType, MaxBitrate) {
  VideoCodec codec = Configure(1280, 720, 3, 1, is_screen_sharing_);
  EXPECT_EQ(
      SvcRateAllocator::GetMaxBitrateBps(codec),
      (codec.spatialLayers[0].maxBitrate + codec.spatialLayers[1].maxBitrate +
       codec.spatialLayers[2].maxBitrate) *
          1000);

  // Deactivate middle layer. This causes deactivation of top layer as well.
  codec.spatialLayers[1].active = false;
  EXPECT_EQ(SvcRateAllocator::GetMaxBitrateBps(codec),
            codec.spatialLayers[0].maxBitrate * 1000);
}

TEST_P(SvcRateAllocatorTestParametrizedContentType, PaddingBitrate) {
  VideoCodec codec = Configure(1280, 720, 3, 1, is_screen_sharing_);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  uint32_t padding_bitrate_bps = SvcRateAllocator::GetPaddingBitrateBps(codec);

  VideoBitrateAllocation allocation =
      allocator.GetAllocation(padding_bitrate_bps, 30);
  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0UL);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0UL);
  EXPECT_GT(allocation.GetSpatialLayerSum(2), 0UL);

  // Allocate 90% of padding bitrate. Top layer should be disabled.
  allocation = allocator.GetAllocation(9 * padding_bitrate_bps / 10, 30);
  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0UL);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0UL);
  EXPECT_EQ(allocation.GetSpatialLayerSum(2), 0UL);

  // Deactivate top layer.
  codec.spatialLayers[2].active = false;

  padding_bitrate_bps = SvcRateAllocator::GetPaddingBitrateBps(codec);
  allocation = allocator.GetAllocation(padding_bitrate_bps, 30);
  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0UL);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0UL);
  EXPECT_EQ(allocation.GetSpatialLayerSum(2), 0UL);

  allocation = allocator.GetAllocation(9 * padding_bitrate_bps / 10, 30);
  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0UL);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1), 0UL);
  EXPECT_EQ(allocation.GetSpatialLayerSum(2), 0UL);
}

INSTANTIATE_TEST_CASE_P(_,
                        SvcRateAllocatorTestParametrizedContentType,
                        ::testing::Bool());

}  // namespace webrtc
