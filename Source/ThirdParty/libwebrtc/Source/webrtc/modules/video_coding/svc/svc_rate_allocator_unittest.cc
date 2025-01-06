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
#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "api/units/data_rate.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_bitrate_allocator.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/spatial_layer.h"
#include "api/video_codecs/video_codec.h"
#include "modules/video_coding/codecs/av1/av1_svc_config.h"
#include "modules/video_coding/codecs/vp9/svc_config.h"
#include "rtc_base/checks.h"
#include "test/explicit_key_value_config.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {
using ::testing::Bool;
using ::testing::TestWithParam;

static VideoCodec Configure(VideoCodecType codecType,
                            size_t width,
                            size_t height,
                            size_t num_spatial_layers,
                            size_t num_temporal_layers,
                            bool is_screen_sharing) {
  VideoCodec codec;
  codec.width = width;
  codec.height = height;
  codec.codecType = codecType;
  codec.mode = is_screen_sharing ? VideoCodecMode::kScreensharing
                                 : VideoCodecMode::kRealtimeVideo;

  std::vector<SpatialLayer> spatial_layers;
  if (codecType == kVideoCodecVP9) {
    spatial_layers = GetSvcConfig(width, height, 30, /*first_active_layer=*/0,
                                  num_spatial_layers, num_temporal_layers,
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

  RTC_DCHECK_EQ(codecType, kVideoCodecAV1);

  if (num_spatial_layers == 1) {
    // SetAv1SvcConfig expects bitrate limits for be set when single spatial
    // layer is requested.
    codec.minBitrate = 30;
    codec.maxBitrate = 5000;
  }

  SetAv1SvcConfig(codec, num_temporal_layers, num_spatial_layers);

  return codec;
}

}  // namespace

TEST(SvcRateAllocatorTest, SingleLayerFor320x180Input) {
  VideoCodec codec = Configure(kVideoCodecVP9, 320, 180, 3, 3, false);
  ExplicitKeyValueConfig field_trials("");
  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(1000 * 1000, 30));

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1), 0u);
}

TEST(SvcRateAllocatorTest, TwoLayersFor640x360Input) {
  VideoCodec codec = Configure(kVideoCodecVP9, 640, 360, 3, 3, false);
  ExplicitKeyValueConfig field_trials("");
  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(1000 * 1000, 30));

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0u);
  EXPECT_EQ(allocation.GetSpatialLayerSum(2), 0u);
}

TEST(SvcRateAllocatorTest, ThreeLayersFor1280x720Input) {
  VideoCodec codec = Configure(kVideoCodecVP9, 1280, 720, 3, 3, false);
  ExplicitKeyValueConfig field_trials("");
  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(1000 * 1000, 30));

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(1), 0u);
  EXPECT_GT(allocation.GetSpatialLayerSum(2), 0u);
}

TEST(SvcRateAllocatorTest,
     BaseLayerNonZeroBitrateEvenIfTotalIfLessThanMinimum) {
  VideoCodec codec = Configure(kVideoCodecVP9, 1280, 720, 3, 3, false);
  ExplicitKeyValueConfig field_trials("");
  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

  const SpatialLayer* layers = codec.spatialLayers;

  VideoBitrateAllocation allocation = allocator.Allocate(
      VideoBitrateAllocationParameters(layers[0].minBitrate * 1000 / 2, 30));

  EXPECT_GT(allocation.GetSpatialLayerSum(0), 0u);
  EXPECT_LT(allocation.GetSpatialLayerSum(0), layers[0].minBitrate * 1000);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1), 0u);
}

TEST(SvcRateAllocatorTest, Disable640x360Layer) {
  VideoCodec codec = Configure(kVideoCodecVP9, 1280, 720, 3, 3, false);
  ExplicitKeyValueConfig field_trials("");
  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

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
  VideoCodec codec = Configure(kVideoCodecVP9, 1280, 720, 3, 3, false);
  ExplicitKeyValueConfig field_trials("");
  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

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
  VideoCodec codec = Configure(kVideoCodecVP9, 1280, 720, 3, 3, false);
  ExplicitKeyValueConfig field_trials("");
  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

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
  VideoCodec codec = Configure(kVideoCodecVP9, 1280, 720, 3, 1, true);
  ExplicitKeyValueConfig field_trials("");
  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

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
  ExplicitKeyValueConfig field_trials("");
  for (int deactivated_idx = 2; deactivated_idx >= 0; --deactivated_idx) {
    VideoCodec codec = Configure(kVideoCodecVP9, 1280, 720, 3, 1, false);
    EXPECT_LE(codec.VP9()->numberOfSpatialLayers, 3U);

    for (int i = deactivated_idx; i < 3; ++i)
      codec.spatialLayers[i].active = false;

    SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

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
  ExplicitKeyValueConfig field_trials("");
  for (int deactivated_idx = 0; deactivated_idx < 3; ++deactivated_idx) {
    VideoCodec codec = Configure(kVideoCodecVP9, 1280, 720, 3, 1, false);
    EXPECT_LE(codec.VP9()->numberOfSpatialLayers, 3U);

    for (int i = deactivated_idx; i >= 0; --i)
      codec.spatialLayers[i].active = false;

    SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

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
  VideoCodec codec = Configure(kVideoCodecVP9, 1280, 720, 3, 1, false);
  ExplicitKeyValueConfig field_trials("");
  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

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
  VideoCodec codec = Configure(kVideoCodecVP9, 1280, 720, 3, 1, false);
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

  VideoCodec codec = Configure(kVideoCodecVP9, 1280, 720, 3, 1, false);
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
  codec.SetScalabilityMode(ScalabilityMode::kL3T3);
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
  ExplicitKeyValueConfig field_trials("");

  SvcRateAllocator allocator(codec, field_trials);

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
  codec.SetScalabilityMode(ScalabilityMode::kL3T3);
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
  ExplicitKeyValueConfig field_trials("");

  SvcRateAllocator allocator(codec, field_trials);

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
  codec.SetScalabilityMode(ScalabilityMode::kL2T2);
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
  ExplicitKeyValueConfig field_trials("");

  SvcRateAllocator allocator(codec, field_trials);
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

TEST(SvcRateAllocatorTest, CapsAllocationToMaxBitrate) {
  VideoCodec codec = Configure(kVideoCodecVP9, 1280, 720, 3, 3, false);
  codec.maxBitrate = 70;  // Cap the overall max bitrate to 70kbps.
  ExplicitKeyValueConfig field_trials("");

  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

  // Allocate 3Mbps which should be enough for all layers.
  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(3'000'000, 30));

  // The 3Mbps should be capped to 70kbps, so only first layer is active.
  EXPECT_EQ(allocation.GetSpatialLayerSum(0), 70'000u);
  EXPECT_EQ(allocation.GetSpatialLayerSum(1), 0u);
  EXPECT_EQ(allocation.GetSpatialLayerSum(2), 0u);
}

class SvcRateAllocatorTestParametrizedContentType : public TestWithParam<bool> {
 public:
  SvcRateAllocatorTestParametrizedContentType()
      : is_screen_sharing_(GetParam()) {}

  const bool is_screen_sharing_;
};

TEST_P(SvcRateAllocatorTestParametrizedContentType, MaxBitrate) {
  VideoCodec codec =
      Configure(kVideoCodecVP9, 1280, 720, 3, 1, is_screen_sharing_);
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
  VideoCodec codec =
      Configure(kVideoCodecVP9, 1280, 720, 3, 1, is_screen_sharing_);
  ExplicitKeyValueConfig field_trials("");
  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

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
  ExplicitKeyValueConfig field_trials(
      "WebRTC-StableTargetRate/enabled:true,video_hysteresis_factor:1.0,"
      "screenshare_hysteresis_factor:1.0/");

  const VideoCodec codec =
      Configure(kVideoCodecVP9, 1280, 720, 3, 1, is_screen_sharing_);
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

  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);

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
  const VideoCodec codec =
      Configure(kVideoCodecVP9, 1280, 720, 3, 1, is_screen_sharing_);
  const auto start_rates = SvcRateAllocator::GetLayerStartBitrates(codec);
  const DataRate min_rate_single_layer = start_rates[0];
  const DataRate min_rate_two_layers = start_rates[1];
  const DataRate min_rate_three_layers = start_rates[2];

  ExplicitKeyValueConfig field_trials(
      "WebRTC-StableTargetRate/enabled:true,video_hysteresis_factor:1.1,"
      "screenshare_hysteresis_factor:1.1/");
  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);
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

TEST_P(SvcRateAllocatorTestParametrizedContentType, TwoTemporalLayersAv1) {
  VideoCodec codec =
      Configure(kVideoCodecAV1, 1280, 720, 1, 2, is_screen_sharing_);
  ExplicitKeyValueConfig field_trials("");
  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);
  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(
          /*total_bitrate_bps=*/1024'000, /*framerate=*/30));

  EXPECT_EQ(allocation.GetBitrate(/*spatial_index=*/0, /*temporal_index=*/0),
            660645u);
  EXPECT_EQ(allocation.GetBitrate(/*spatial_index=*/0, /*temporal_index=*/1),
            363355u);
}

TEST_P(SvcRateAllocatorTestParametrizedContentType, ThreeTemporalLayersAv1) {
  VideoCodec codec =
      Configure(kVideoCodecAV1, 1280, 720, 1, 3, is_screen_sharing_);
  ExplicitKeyValueConfig field_trials("");
  SvcRateAllocator allocator = SvcRateAllocator(codec, field_trials);
  VideoBitrateAllocation allocation =
      allocator.Allocate(VideoBitrateAllocationParameters(
          /*total_bitrate_bps=*/1024'000, /*framerate=*/30));

  EXPECT_EQ(allocation.GetBitrate(/*spatial_index=*/0, /*temporal_index=*/0),
            552766u);
  EXPECT_EQ(allocation.GetBitrate(/*spatial_index=*/0, /*temporal_index=*/1),
            167212u);
  EXPECT_EQ(allocation.GetBitrate(/*spatial_index=*/0, /*temporal_index=*/2),
            304022u);
}

INSTANTIATE_TEST_SUITE_P(_,
                         SvcRateAllocatorTestParametrizedContentType,
                         Bool());

}  // namespace test
}  // namespace webrtc
