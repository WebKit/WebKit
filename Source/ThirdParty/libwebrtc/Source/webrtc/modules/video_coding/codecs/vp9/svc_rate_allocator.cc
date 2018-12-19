/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp9/svc_rate_allocator.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {

const float kSpatialLayeringRateScalingFactor = 0.55f;
const float kTemporalLayeringRateScalingFactor = 0.55f;

static size_t GetNumActiveSpatialLayers(const VideoCodec& codec) {
  RTC_DCHECK_EQ(codec.codecType, kVideoCodecVP9);
  RTC_DCHECK_GT(codec.VP9().numberOfSpatialLayers, 0u);

  size_t num_spatial_layers = 0;
  for (; num_spatial_layers < codec.VP9().numberOfSpatialLayers;
       ++num_spatial_layers) {
    if (!codec.spatialLayers[num_spatial_layers].active) {
      // TODO(bugs.webrtc.org/9350): Deactivation of middle layer is not
      // implemented. For now deactivation of a VP9 layer deactivates all
      // layers above the deactivated one.
      break;
    }
  }

  return num_spatial_layers;
}

static bool AdjustAndVerify(const VideoCodec& codec,
                            std::vector<size_t>* spatial_layer_bitrate_bps) {
  bool enough_bitrate = true;
  size_t excess_rate = 0;
  for (size_t sl_idx = 0;
       sl_idx < spatial_layer_bitrate_bps->size() && enough_bitrate; ++sl_idx) {
    RTC_DCHECK_GT(codec.spatialLayers[sl_idx].maxBitrate, 0);
    RTC_DCHECK_GE(codec.spatialLayers[sl_idx].maxBitrate,
                  codec.spatialLayers[sl_idx].minBitrate);

    const size_t min_bitrate_bps =
        codec.spatialLayers[sl_idx].minBitrate * 1000;
    const size_t max_bitrate_bps =
        codec.spatialLayers[sl_idx].maxBitrate * 1000;

    spatial_layer_bitrate_bps->at(sl_idx) += excess_rate;
    if (spatial_layer_bitrate_bps->at(sl_idx) < max_bitrate_bps) {
      excess_rate = 0;
    } else {
      excess_rate = spatial_layer_bitrate_bps->at(sl_idx) - max_bitrate_bps;
      spatial_layer_bitrate_bps->at(sl_idx) = max_bitrate_bps;
    }

    size_t bitrate_bps = spatial_layer_bitrate_bps->at(sl_idx);
    enough_bitrate = (bitrate_bps >= min_bitrate_bps);
  }

  return enough_bitrate;
}

static std::vector<size_t> SplitBitrate(size_t num_layers,
                                        size_t total_bitrate,
                                        float rate_scaling_factor) {
  std::vector<size_t> bitrates;

  double denominator = 0.0;
  for (size_t layer_idx = 0; layer_idx < num_layers; ++layer_idx) {
    denominator += std::pow(rate_scaling_factor, layer_idx);
  }

  double numerator = std::pow(rate_scaling_factor, num_layers - 1);
  for (size_t layer_idx = 0; layer_idx < num_layers; ++layer_idx) {
    bitrates.push_back(numerator * total_bitrate / denominator);
    numerator /= rate_scaling_factor;
  }

  const size_t sum = std::accumulate(bitrates.begin(), bitrates.end(), 0);
  // Ensure the sum of split bitrates doesn't exceed the total bitrate.
  RTC_DCHECK_LE(sum, total_bitrate);

  // Keep the sum of split bitrates equal to the total bitrate by adding bits,
  // which were lost due to rounding, to the latest layer.
  bitrates.back() += total_bitrate - sum;

  return bitrates;
}

SvcRateAllocator::SvcRateAllocator(const VideoCodec& codec) : codec_(codec) {
  RTC_DCHECK_EQ(codec.codecType, kVideoCodecVP9);
  RTC_DCHECK_GT(codec.VP9().numberOfSpatialLayers, 0u);
  RTC_DCHECK_GT(codec.VP9().numberOfTemporalLayers, 0u);
}

VideoBitrateAllocation SvcRateAllocator::GetAllocation(
    uint32_t total_bitrate_bps,
    uint32_t framerate_fps) {
  if (codec_.maxBitrate != 0) {
    total_bitrate_bps = std::min(total_bitrate_bps, codec_.maxBitrate * 1000);
  }

  if (codec_.spatialLayers[0].targetBitrate == 0) {
    // Delegate rate distribution to VP9 encoder wrapper if bitrate thresholds
    // are not set.
    VideoBitrateAllocation bitrate_allocation;
    bitrate_allocation.SetBitrate(0, 0, total_bitrate_bps);
    return bitrate_allocation;
  }

  size_t num_spatial_layers = GetNumActiveSpatialLayers(codec_);
  if (num_spatial_layers == 0) {
    return VideoBitrateAllocation();  // All layers are deactivated.
  }

  if (codec_.mode == VideoCodecMode::kRealtimeVideo) {
    return GetAllocationNormalVideo(total_bitrate_bps, num_spatial_layers);
  } else {
    return GetAllocationScreenSharing(total_bitrate_bps, num_spatial_layers);
  }
}

VideoBitrateAllocation SvcRateAllocator::GetAllocationNormalVideo(
    uint32_t total_bitrate_bps,
    size_t num_spatial_layers) const {
  std::vector<size_t> spatial_layer_bitrate_bps;

  // Distribute total bitrate across spatial layers. If there is not enough
  // bitrate to provide all layers with at least minimum required bitrate
  // then number of layers is reduced by one and distribution is repeated
  // until that condition is met or if number of layers is reduced to one.
  for (;; --num_spatial_layers) {
    spatial_layer_bitrate_bps =
        SplitBitrate(num_spatial_layers, total_bitrate_bps,
                     kSpatialLayeringRateScalingFactor);

    const bool enough_bitrate =
        AdjustAndVerify(codec_, &spatial_layer_bitrate_bps);
    if (enough_bitrate || num_spatial_layers == 1) {
      break;
    }
  }

  VideoBitrateAllocation bitrate_allocation;

  const size_t num_temporal_layers = codec_.VP9().numberOfTemporalLayers;
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    std::vector<size_t> temporal_layer_bitrate_bps =
        SplitBitrate(num_temporal_layers, spatial_layer_bitrate_bps[sl_idx],
                     kTemporalLayeringRateScalingFactor);

    // Distribute rate across temporal layers. Allocate more bits to lower
    // layers since they are used for prediction of higher layers and their
    // references are far apart.
    if (num_temporal_layers == 1) {
      bitrate_allocation.SetBitrate(sl_idx, 0, temporal_layer_bitrate_bps[0]);
    } else if (num_temporal_layers == 2) {
      bitrate_allocation.SetBitrate(sl_idx, 0, temporal_layer_bitrate_bps[1]);
      bitrate_allocation.SetBitrate(sl_idx, 1, temporal_layer_bitrate_bps[0]);
    } else {
      RTC_CHECK_EQ(num_temporal_layers, 3);
      // In case of three temporal layers the high layer has two frames and the
      // middle layer has one frame within GOP (in between two consecutive low
      // layer frames). Thus high layer requires more bits (comparing pure
      // bitrate of layer, excluding bitrate of base layers) to keep quality on
      // par with lower layers.
      bitrate_allocation.SetBitrate(sl_idx, 0, temporal_layer_bitrate_bps[2]);
      bitrate_allocation.SetBitrate(sl_idx, 1, temporal_layer_bitrate_bps[0]);
      bitrate_allocation.SetBitrate(sl_idx, 2, temporal_layer_bitrate_bps[1]);
    }
  }

  return bitrate_allocation;
}

VideoBitrateAllocation SvcRateAllocator::GetAllocationScreenSharing(
    uint32_t total_bitrate_bps,
    size_t num_spatial_layers) const {
  VideoBitrateAllocation bitrate_allocation;

  // Add next layer after bitrate of previous layer has reached its maximum.
  size_t left_bitrate_bps = total_bitrate_bps;
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    const size_t min_bitrate_bps =
        codec_.spatialLayers[sl_idx].minBitrate * 1000;
    const size_t max_bitrate_bps =
        codec_.spatialLayers[sl_idx].maxBitrate * 1000;

    const size_t bitrate_bps = std::min(left_bitrate_bps, max_bitrate_bps);
    if (bitrate_bps >= min_bitrate_bps) {
      bitrate_allocation.SetBitrate(sl_idx, 0, bitrate_bps);
    } else {
      break;
    }

    left_bitrate_bps -= bitrate_bps;
  }

  return bitrate_allocation;
}

uint32_t SvcRateAllocator::GetMaxBitrateBps(const VideoCodec& codec) {
  const size_t num_spatial_layers = GetNumActiveSpatialLayers(codec);

  uint32_t max_bitrate_kbps = 0;
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    max_bitrate_kbps += codec.spatialLayers[sl_idx].maxBitrate;
  }

  if (codec.maxBitrate != 0) {
    max_bitrate_kbps = std::min(max_bitrate_kbps, codec.maxBitrate);
  }

  return max_bitrate_kbps * 1000;
}

uint32_t SvcRateAllocator::GetPaddingBitrateBps(const VideoCodec& codec) {
  const size_t num_spatial_layers = GetNumActiveSpatialLayers(codec);

  if (codec.mode == VideoCodecMode::kRealtimeVideo) {
    float scale_factor = 0.0;
    for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
      scale_factor += std::pow(kSpatialLayeringRateScalingFactor, sl_idx);
    }
    uint32_t min_bitrate_bps =
        codec.spatialLayers[num_spatial_layers - 1].minBitrate * 1000;
    return static_cast<uint32_t>(min_bitrate_bps * scale_factor);
  }

  RTC_DCHECK(codec.mode == VideoCodecMode::kScreensharing);

  uint32_t min_bitrate_kbps = 0;
  for (size_t sl_idx = 0; sl_idx < num_spatial_layers - 1; ++sl_idx) {
    min_bitrate_kbps += codec.spatialLayers[sl_idx].maxBitrate;
  }
  min_bitrate_kbps += codec.spatialLayers[num_spatial_layers - 1].minBitrate;

  return min_bitrate_kbps * 1000;
}

}  // namespace webrtc
