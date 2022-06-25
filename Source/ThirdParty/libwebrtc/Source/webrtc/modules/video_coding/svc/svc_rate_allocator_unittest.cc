/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/svc/svc_rate_allocator.h"

#include <algorithm>
#include <vector>

#include "modules/video_coding/codecs/vp9/svc_config.h"
#include "rtc_base/checks.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
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
      GetSvcConfig(width, height, 30, /*first_active_layer=*/0,
                   num_spatial_layers, num_temporal_layers, is_screen_sharing);
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

  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(1000 * 1000, 30));

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1), 0u);
}

TEST(SvcRateAllocatorTest, TwoLayersFor640x360Input) {
  VideoCodec codec = Configure(640, 360, 3, 3, false);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(1000 * 1000, 30));

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0u);
  EXPECT_EQ(allocation.GetSpatialLayerSum(2), 0u);
}

TEST(SvcRateAllocatorTest, ThreeLayersFor1280x720Input) {
  VideoCodec codec = Configure(1280, 720, 3, 3, false);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(1000 * 1000, 30));

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(2), 0u);
}

TEST(SvcRateAllocatorTest,
     BaseLayerNonZeroBitrateEvenIfTotalIfLessThanMinimum) {
  VideoCodec codec = Configure(1280, 720, 3, 3, false);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  const SpatialLayer* layers = codec.spatialLayers;

  VideoBitrateAllocation allocation = allocator.Allocate(
      VideoBitrateAllocationParameters(layers[0].minBitrate * 1000 / 2, 30));

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

  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(
          min_bitrate_for_640x360_layer_kbps * 1000 - 1, 30));

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1), 0u);
}

TEST(SvcRateAllocatorTest, Disable1280x720Layer) {
  VideoCodec codec = Configure(1280, 720, 3, 3, false);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  const SpatialLayer* layers = codec.spatialLayers;

  size_t min_bitrate_for_1280x720_layer_kbps =
      layers[0].minBitrate + layers[1].minBitrate + layers[2].minBitrate;

  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(
          min_bitrate_for_1280x720_layer_kbps * 1000 - 1, 30));

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0u);
  EXPECT_EQ(allocation.GetSpatialLayerSum(2), 0u);
}

TEST(SvcRateAllocatorTest, BitrateIsCapped) {
  VideoCodec codec = Configure(1280, 720, 3, 3, false);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  const SpatialLayer* layers = codec.spatialLayers;

  const uint32_t link_mbps = 100;
  VideoBitrateAllocation allocation = allocator.Allocate(
      VideoBitrateAllocationParameters(link_mbps * 1000000, 30));

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

  VideoBitrateAllocation allocation = allocator.Allocate(
      VideoBitrateAllocationParameters(layers[0].minBitrate * 1000, 30));
  EXPECT_EQ(allocation.GetSpatialLayerSum(0) / 1000, layers[0].minBitrate);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1), 0UL);

  allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      (layers[0].targetBitrate + layers[1].minBitrate) * 1000, 30));
  EXPECT_EQ(allocation.GetSpatialLayerSum(0) / 1000, layers[0].targetBitrate);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1) / 1000, layers[1].minBitrate);
}

TEST(SvcRateAllocatorTest, DeactivateHigherLayers) {
  for (int deactivated_idx = 2; deactivated_idx >= 0; --deactivated_idx) {
    VideoCodec codec = Configure(1280, 720, 3, 1, false);
    EXPECT_LE(codec.VP9()->numberOfSpatialLayers, 3U);

    for (int i = deactivated_idx; i < 3; ++i)
      codec.spatialLayers[i].active = false;

    SvcRateAllocator allocator = SvcRateAllocator(codec);

    VideoBitrateAllocation allocation = allocator.Allocate(
        VideoBitrateAllocationParameters(10 * 1000 * 1000, 30));

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

TEST(SvcRateAllocatorTest, DeactivateLowerLayers) {
  for (int deactivated_idx = 0; deactivated_idx < 3; ++deactivated_idx) {
    VideoCodec codec = Configure(1280, 720, 3, 1, false);
    EXPECT_LE(codec.VP9()->numberOfSpatialLayers, 3U);

    for (int i = deactivated_idx; i >= 0; --i)
      codec.spatialLayers[i].active = false;

    SvcRateAllocator allocator = SvcRateAllocator(codec);

    VideoBitrateAllocation allocation = allocator.Allocate(
        VideoBitrateAllocationParameters(10 * 1000 * 1000, 30));

    // Ensure layers spatial_idx <= deactivated_idx are deactivated.
    for (int spatial_idx = 0; spatial_idx <= deactivated_idx; ++spatial_idx) {
      EXPECT_EQ(allocation.GetSpatialLayerSum(spatial_idx), 0UL);
    }

    // Ensure layers spatial_idx > deactivated_idx are activated.
    for (int spatial_idx = deactivated_idx + 1; spatial_idx < 3;
         ++spatial_idx) {
      EXPECT_GT(allocation.GetSpatialLayerSum(spatial_idx), 0UL);
    }
  }
}

TEST(SvcRateAllocatorTest, SignalsBwLimited) {
  VideoCodec codec = Configure(1280, 720, 3, 1, false);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  // Rough estimate calculated by hand.
  uint32_t min_to_enable_all = 900000;

  EXPECT_TRUE(
      allocator
          .Allocate(VideoBitrateAllocationParameters(min_to_enable_all / 2, 30))
          .is_bw_limited());

  EXPECT_FALSE(
      allocator
          .Allocate(VideoBitrateAllocationParameters(min_to_enable_all, 30))
          .is_bw_limited());
}

TEST(SvcRateAllocatorTest, NoPaddingIfAllLayersAreDeactivated) {
  VideoCodec codec = Configure(1280, 720, 3, 1, false);
  EXPECT_EQ(codec.VP9()->numberOfSpatialLayers, 3U);
  // Deactivation of base layer deactivates all layers.
  codec.spatialLayers[0].active = false;
  codec.spatialLayers[1].active = false;
  codec.spatialLayers[2].active = false;
  DataRate padding_rate = SvcRateAllocator::GetPaddingBitrate(codec);
  EXPECT_EQ(padding_rate, DataRate::Zero());
}

TEST(SvcRateAllocatorTest, FindLayerTogglingThreshold) {
  // Let's unit test a utility method of the unit test...

  // Predetermined constants indicating the min bitrate needed for two and three
  // layers to be enabled respectively, using the config from Configure() with
  // 1280x720 resolution and three spatial layers.
  const DataRate kTwoLayerMinRate = DataRate::BitsPerSec(299150);
  const DataRate kThreeLayerMinRate = DataRate::BitsPerSec(891052);

  VideoCodec codec = Configure(1280, 720, 3, 1, false);
  absl::InlinedVector<DataRate, kMaxSpatialLayers> layer_start_bitrates =
      SvcRateAllocator::GetLayerStartBitrates(codec);
  ASSERT_EQ(layer_start_bitrates.size(), 3u);
  EXPECT_EQ(layer_start_bitrates[1], kTwoLayerMinRate);
  EXPECT_EQ(layer_start_bitrates[2], kThreeLayerMinRate);
}

TEST(SvcRateAllocatorTest, SupportsAv1) {
  VideoCodec codec;
  codec.width = 640;
  codec.height = 360;
  codec.codecType = kVideoCodecAV1;
  codec.SetScalabilityMode("L3T3");
  codec.spatialLayers[0].active = true;
  codec.spatialLayers[0].minBitrate = 30;
  codec.spatialLayers[0].targetBitrate = 51;
  codec.spatialLayers[0].maxBitrate = 73;
  codec.spatialLayers[1].active = true;
  codec.spatialLayers[1].minBitrate = 49;
  codec.spatialLayers[1].targetBitrate = 64;
  codec.spatialLayers[1].maxBitrate = 97;
  codec.spatialLayers[2].active = true;
  codec.spatialLayers[2].minBitrate = 193;
  codec.spatialLayers[2].targetBitrate = 305;
  codec.spatialLayers[2].maxBitrate = 418;

  SvcRateAllocator allocator(codec);

  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(1'000'000, 30));

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(2), 0u);
}

TEST(SvcRateAllocatorTest, SupportsAv1WithSkippedLayer) {
  VideoCodec codec;
  codec.width = 640;
  codec.height = 360;
  codec.codecType = kVideoCodecAV1;
  codec.SetScalabilityMode("L3T3");
  codec.spatialLayers[0].active = false;
  codec.spatialLayers[0].minBitrate = 30;
  codec.spatialLayers[0].targetBitrate = 51;
  codec.spatialLayers[0].maxBitrate = 73;
  codec.spatialLayers[1].active = true;
  codec.spatialLayers[1].minBitrate = 49;
  codec.spatialLayers[1].targetBitrate = 64;
  codec.spatialLayers[1].maxBitrate = 97;
  codec.spatialLayers[2].active = true;
  codec.spatialLayers[2].minBitrate = 193;
  codec.spatialLayers[2].targetBitrate = 305;
  codec.spatialLayers[2].maxBitrate = 418;

  SvcRateAllocator allocator(codec);

  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(1'000'000, 30));

  EXPECT_EQ(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(2), 0u);
}

TEST(SvcRateAllocatorTest, UsesScalabilityModeToGetNumberOfLayers) {
  VideoCodec codec;
  codec.width = 640;
  codec.height = 360;
  codec.codecType = kVideoCodecAV1;
  codec.SetScalabilityMode("L2T2");
  codec.spatialLayers[0].active = true;
  codec.spatialLayers[0].minBitrate = 30;
  codec.spatialLayers[0].targetBitrate = 51;
  codec.spatialLayers[0].maxBitrate = 73;
  codec.spatialLayers[1].active = true;
  codec.spatialLayers[1].minBitrate = 49;
  codec.spatialLayers[1].targetBitrate = 64;
  codec.spatialLayers[1].maxBitrate = 97;
  codec.spatialLayers[2].active = true;
  codec.spatialLayers[2].minBitrate = 193;
  codec.spatialLayers[2].targetBitrate = 305;
  codec.spatialLayers[2].maxBitrate = 418;

  SvcRateAllocator allocator(codec);
  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(1'000'000, 30));

  // Expect bitrates for 2 temporal layers.
  EXPECT_TRUE(allocation.HasBitrate(1, /*temporal_index=*/0));
  EXPECT_TRUE(allocation.HasBitrate(1, /*temporal_index=*/1));
  EXPECT_FALSE(allocation.HasBitrate(1, /*temporal_index=*/2));

  // expect codec.spatialLayers[2].active is ignored because scability mode uses
  // just 2 spatial layers.
  EXPECT_EQ(allocation.GetSpatialLayerSum(2), 0u);
}

class SvcRateAllocatorTestParametrizedContentType
    : public ::testing::Test,
      public ::testing::WithParamInterface<bool> {
 public:
  SvcRateAllocatorTestParametrizedContentType()
      : is_screen_sharing_(GetParam()) {}

  const bool is_screen_sharing_;
};

TEST_P(SvcRateAllocatorTestParametrizedContentType, MaxBitrate) {
  VideoCodec codec = Configure(1280, 720, 3, 1, is_screen_sharing_);
  EXPECT_EQ(SvcRateAllocator::GetMaxBitrate(codec),
            DataRate::KilobitsPerSec(codec.spatialLayers[0].maxBitrate +
                                     codec.spatialLayers[1].maxBitrate +
                                     codec.spatialLayers[2].maxBitrate));

  // Deactivate middle layer. This causes deactivation of top layer as well.
  codec.spatialLayers[1].active = false;
  EXPECT_EQ(SvcRateAllocator::GetMaxBitrate(codec),
            DataRate::KilobitsPerSec(codec.spatialLayers[0].maxBitrate));
}

TEST_P(SvcRateAllocatorTestParametrizedContentType, PaddingBitrate) {
  VideoCodec codec = Configure(1280, 720, 3, 1, is_screen_sharing_);
  SvcRateAllocator allocator = SvcRateAllocator(codec);

  DataRate padding_bitrate = SvcRateAllocator::GetPaddingBitrate(codec);

  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(padding_bitrate, 30));
  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0UL);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0UL);
  EXPECT_GT(allocation.GetSpatialLayerSum(2), 0UL);

  // Allocate 90% of padding bitrate. Top layer should be disabled.
  allocation = allocator.Allocate(
      VideoBitrateAllocationParameters(9 * padding_bitrate / 10, 30));
  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0UL);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0UL);
  EXPECT_EQ(allocation.GetSpatialLayerSum(2), 0UL);

  // Deactivate top layer.
  codec.spatialLayers[2].active = false;

  padding_bitrate = SvcRateAllocator::GetPaddingBitrate(codec);
  allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(padding_bitrate, 30));
  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0UL);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0UL);
  EXPECT_EQ(allocation.GetSpatialLayerSum(2), 0UL);

  allocation = allocator.Allocate(
      VideoBitrateAllocationParameters(9 * padding_bitrate / 10, 30));
  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0UL);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1), 0UL);
  EXPECT_EQ(allocation.GetSpatialLayerSum(2), 0UL);

  // Deactivate all layers.
  codec.spatialLayers[0].active = false;
  codec.spatialLayers[1].active = false;
  codec.spatialLayers[2].active = false;

  padding_bitrate = SvcRateAllocator::GetPaddingBitrate(codec);
  // No padding expected.
  EXPECT_EQ(DataRate::Zero(), padding_bitrate);
}

TEST_P(SvcRateAllocatorTestParametrizedContentType, StableBitrate) {
  ScopedFieldTrials field_trial(
      "WebRTC-StableTargetRate/enabled:true,video_hysteresis_factor:1.0,"
      "screenshare_hysteresis_factor:1.0/");

  const VideoCodec codec = Configure(1280, 720, 3, 1, is_screen_sharing_);
  const auto start_rates = SvcRateAllocator::GetLayerStartBitrates(codec);
  const DataRate min_rate_two_layers = start_rates[1];
  const DataRate min_rate_three_layers = start_rates[2];

  const DataRate max_rate_one_layer =
      DataRate::KilobitsPerSec(codec.spatialLayers[0].maxBitrate);
  const DataRate max_rate_two_layers =
      is_screen_sharing_
          ? DataRate::KilobitsPerSec(codec.spatialLayers[0].targetBitrate +
                                     codec.spatialLayers[1].maxBitrate)
          : DataRate::KilobitsPerSec(codec.spatialLayers[0].maxBitrate +
                                     codec.spatialLayers[1].maxBitrate);

  SvcRateAllocator allocator = SvcRateAllocator(codec);

  // Two layers, stable and target equal.
  auto allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      /*total_bitrate=*/min_rate_two_layers,
      /*stable_bitrate=*/min_rate_two_layers, /*fps=*/30.0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(1));
  EXPECT_EQ(allocation.get_sum_bps(), min_rate_two_layers.bps());

  // Two layers, stable bitrate too low for two layers.
  allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      /*total_bitrate=*/min_rate_two_layers,
      /*stable_bitrate=*/min_rate_two_layers - DataRate::BitsPerSec(1),
      /*fps=*/30.0));
  EXPECT_FALSE(allocation.IsSpatialLayerUsed(1));
  EXPECT_EQ(DataRate::BitsPerSec(allocation.get_sum_bps()),
            std::min(min_rate_two_layers - DataRate::BitsPerSec(1),
                     max_rate_one_layer));

  // Three layers, stable and target equal.
  allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      /*total_bitrate=*/min_rate_three_layers,
      /*stable_bitrate=*/min_rate_three_layers, /*fps=*/30.0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(2));
  EXPECT_EQ(allocation.get_sum_bps(), min_rate_three_layers.bps());

  // Three layers, stable bitrate too low for three layers.
  allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      /*total_bitrate=*/min_rate_three_layers,
      /*stable_bitrate=*/min_rate_three_layers - DataRate::BitsPerSec(1),
      /*fps=*/30.0));
  EXPECT_FALSE(allocation.IsSpatialLayerUsed(2));
  EXPECT_EQ(DataRate::BitsPerSec(allocation.get_sum_bps()),
            std::min(min_rate_three_layers - DataRate::BitsPerSec(1),
                     max_rate_two_layers));
}

TEST_P(SvcRateAllocatorTestParametrizedContentType,
       StableBitrateWithHysteresis) {
  const VideoCodec codec = Configure(1280, 720, 3, 1, is_screen_sharing_);
  const auto start_rates = SvcRateAllocator::GetLayerStartBitrates(codec);
  const DataRate min_rate_single_layer = start_rates[0];
  const DataRate min_rate_two_layers = start_rates[1];
  const DataRate min_rate_three_layers = start_rates[2];

  ScopedFieldTrials field_trial(
      "WebRTC-StableTargetRate/enabled:true,video_hysteresis_factor:1.1,"
      "screenshare_hysteresis_factor:1.1/");
  SvcRateAllocator allocator = SvcRateAllocator(codec);
  // Always use max bitrate as target, verify only stable is used for layer
  // count selection.
  const DataRate max_bitrate = allocator.GetMaxBitrate(codec);

  // Start with a single layer.
  auto allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      /*total_bitrate=*/max_bitrate,
      /*stable_bitrate=*/min_rate_single_layer, /*fps=*/30.0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(0));
  EXPECT_FALSE(allocation.IsSpatialLayerUsed(1));
  EXPECT_FALSE(allocation.IsSpatialLayerUsed(2));

  // Min bitrate not enough to enable second layer due to 10% hysteresis.
  allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      /*total_bitrate=*/max_bitrate,
      /*stable_bitrate=*/min_rate_two_layers, /*fps=*/30.0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(0));
  EXPECT_FALSE(allocation.IsSpatialLayerUsed(1));
  EXPECT_FALSE(allocation.IsSpatialLayerUsed(2));

  // Add hysteresis, second layer should turn on.
  allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      /*total_bitrate=*/max_bitrate,
      /*stable_bitrate=*/min_rate_two_layers * 1.1, /*fps=*/30.0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(1));
  EXPECT_FALSE(allocation.IsSpatialLayerUsed(2));

  // Remove hysteresis, second layer should stay on.
  allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      /*total_bitrate=*/max_bitrate,
      /*stable_bitrate=*/min_rate_two_layers, /*fps=*/30.0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(1));
  EXPECT_FALSE(allocation.IsSpatialLayerUsed(2));

  // Going below min for two layers, second layer should turn off again.
  allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      /*total_bitrate=*/max_bitrate,
      /*stable_bitrate=*/min_rate_two_layers - DataRate::BitsPerSec(1),
      /*fps=*/30.0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(0));
  EXPECT_FALSE(allocation.IsSpatialLayerUsed(1));
  EXPECT_FALSE(allocation.IsSpatialLayerUsed(2));

  // Min bitrate not enough to enable third layer due to 10% hysteresis.
  allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      /*total_bitrate=*/max_bitrate,
      /*stable_bitrate=*/min_rate_three_layers, /*fps=*/30.0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(1));
  EXPECT_FALSE(allocation.IsSpatialLayerUsed(2));

  // Add hysteresis, third layer should turn on.
  allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      /*total_bitrate=*/max_bitrate,
      /*stable_bitrate=*/min_rate_three_layers * 1.1, /*fps=*/30.0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(1));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(2));

  // Remove hysteresis, third layer should stay on.
  allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      /*total_bitrate=*/max_bitrate,
      /*stable_bitrate=*/min_rate_three_layers, /*fps=*/30.0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(1));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(2));

  // Going below min for three layers, third layer should turn off again.
  allocation = allocator.Allocate(VideoBitrateAllocationParameters(
      /*total_bitrate=*/max_bitrate,
      /*stable_bitrate=*/min_rate_three_layers - DataRate::BitsPerSec(1),
      /*fps=*/30.0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(0));
  EXPECT_TRUE(allocation.IsSpatialLayerUsed(1));
  EXPECT_FALSE(allocation.IsSpatialLayerUsed(2));
}

INSTANTIATE_TEST_SUITE_P(_,
                         SvcRateAllocatorTestParametrizedContentType,
                         ::testing::Bool());

}  // namespace test
}  // namespace webrtc
