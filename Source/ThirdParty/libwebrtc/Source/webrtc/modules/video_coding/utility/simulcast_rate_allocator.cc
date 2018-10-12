/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/simulcast_rate_allocator.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/checks.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
// Ratio allocation between temporal streams:
// Values as required for the VP8 codec (accumulating).
static const float
    kLayerRateAllocation[kMaxTemporalStreams][kMaxTemporalStreams] = {
        {1.0f, 1.0f, 1.0f, 1.0f},  // 1 layer
        {0.6f, 1.0f, 1.0f, 1.0f},  // 2 layers {60%, 40%}
        {0.4f, 0.6f, 1.0f, 1.0f},  // 3 layers {40%, 20%, 40%}
        {0.25f, 0.4f, 0.6f, 1.0f}  // 4 layers {25%, 15%, 20%, 40%}
};

static const float kBaseHeavy3TlRateAllocation[kMaxTemporalStreams] = {
    0.6f, 0.8f, 1.0f, 1.0f  // 3 layers {60%, 20%, 20%}
};

const uint32_t kLegacyScreenshareTl0BitrateKbps = 200;
const uint32_t kLegacyScreenshareTl1BitrateKbps = 1000;

double GetHysteresisFactor(const VideoCodec& codec) {
  double factor = 1.0;

  std::string field_trial_name;
  switch (codec.mode) {
    case VideoCodecMode::kRealtimeVideo:
      field_trial_name = "WebRTC-SimulcastUpswitchHysteresisPercent";
      // Default to no hysteresis for simulcast video.
      factor = 1.0;
      break;
    case VideoCodecMode::kScreensharing:
      field_trial_name = "WebRTC-SimulcastScreenshareUpswitchHysteresisPercent";
      // Default to 35% hysteresis for simulcast screenshare.
      factor = 1.35;
      break;
  }

  std::string group_name = webrtc::field_trial::FindFullName(field_trial_name);
  int percent = 0;
  if (!group_name.empty() && sscanf(group_name.c_str(), "%d", &percent) == 1 &&
      percent >= 0) {
    factor = 1.0 + (percent / 100.0);
  }

  return factor;
}
}  // namespace

float SimulcastRateAllocator::GetTemporalRateAllocation(int num_layers,
                                                        int temporal_id) {
  RTC_CHECK_GT(num_layers, 0);
  RTC_CHECK_LE(num_layers, kMaxTemporalStreams);
  RTC_CHECK_GE(temporal_id, 0);
  RTC_CHECK_LT(temporal_id, num_layers);
  if (num_layers == 3 &&
      field_trial::IsEnabled("WebRTC-UseBaseHeavyVP8TL3RateAllocation")) {
    return kBaseHeavy3TlRateAllocation[temporal_id];
  }
  return kLayerRateAllocation[num_layers - 1][temporal_id];
}

SimulcastRateAllocator::SimulcastRateAllocator(const VideoCodec& codec)
    : codec_(codec), hysteresis_factor_(GetHysteresisFactor(codec)) {}

SimulcastRateAllocator::~SimulcastRateAllocator() = default;

VideoBitrateAllocation SimulcastRateAllocator::GetAllocation(
    uint32_t total_bitrate_bps,
    uint32_t framerate) {
  VideoBitrateAllocation allocated_bitrates_bps;
  DistributeAllocationToSimulcastLayers(total_bitrate_bps,
                                        &allocated_bitrates_bps);
  DistributeAllocationToTemporalLayers(framerate, &allocated_bitrates_bps);
  return allocated_bitrates_bps;
}

void SimulcastRateAllocator::DistributeAllocationToSimulcastLayers(
    uint32_t total_bitrate_bps,
    VideoBitrateAllocation* allocated_bitrates_bps) {
  uint32_t left_to_allocate = total_bitrate_bps;
  if (codec_.maxBitrate && codec_.maxBitrate * 1000 < left_to_allocate)
    left_to_allocate = codec_.maxBitrate * 1000;

  if (codec_.numberOfSimulcastStreams == 0) {
    // No simulcast, just set the target as this has been capped already.
    if (codec_.active) {
      allocated_bitrates_bps->SetBitrate(
          0, 0, std::max(codec_.minBitrate * 1000, left_to_allocate));
    }
    return;
  }
  // Find the first active layer. We don't allocate to inactive layers.
  size_t active_layer = 0;
  for (; active_layer < codec_.numberOfSimulcastStreams; ++active_layer) {
    if (codec_.simulcastStream[active_layer].active) {
      // Found the first active layer.
      break;
    }
  }
  // All streams could be inactive, and nothing more to do.
  if (active_layer == codec_.numberOfSimulcastStreams) {
    return;
  }

  // Always allocate enough bitrate for the minimum bitrate of the first
  // active layer. Suspending below min bitrate is controlled outside the
  // codec implementation and is not overridden by this.
  left_to_allocate = std::max(
      codec_.simulcastStream[active_layer].minBitrate * 1000, left_to_allocate);

  // Begin by allocating bitrate to simulcast streams, putting all bitrate in
  // temporal layer 0. We'll then distribute this bitrate, across potential
  // temporal layers, when stream allocation is done.

  bool first_allocation = false;
  if (stream_enabled_.empty()) {
    // First time allocating, this means we should not include hysteresis in
    // case this is a reconfiguration of an existing enabled stream.
    first_allocation = true;
    stream_enabled_.resize(codec_.numberOfSimulcastStreams, false);
  }

  size_t top_active_layer = active_layer;
  // Allocate up to the target bitrate for each active simulcast layer.
  for (; active_layer < codec_.numberOfSimulcastStreams; ++active_layer) {
    const SimulcastStream& stream = codec_.simulcastStream[active_layer];
    if (!stream.active) {
      stream_enabled_[active_layer] = false;
      continue;
    }
    // If we can't allocate to the current layer we can't allocate to higher
    // layers because they require a higher minimum bitrate.
    uint32_t min_bitrate = stream.minBitrate * 1000;
    if (!first_allocation && !stream_enabled_[active_layer]) {
      min_bitrate = std::min(
          static_cast<uint32_t>(hysteresis_factor_ * min_bitrate + 0.5),
          stream.targetBitrate * 1000);
    }
    if (left_to_allocate < min_bitrate) {
      break;
    }

    // We are allocating to this layer so it is the current active allocation.
    top_active_layer = active_layer;
    stream_enabled_[active_layer] = true;
    uint32_t allocation =
        std::min(left_to_allocate, stream.targetBitrate * 1000);
    allocated_bitrates_bps->SetBitrate(active_layer, 0, allocation);
    RTC_DCHECK_LE(allocation, left_to_allocate);
    left_to_allocate -= allocation;
  }

  // All layers above this one are not active.
  for (; active_layer < codec_.numberOfSimulcastStreams; ++active_layer) {
    stream_enabled_[active_layer] = false;
  }

  // Next, try allocate remaining bitrate, up to max bitrate, in top active
  // stream.
  // TODO(sprang): Allocate up to max bitrate for all layers once we have a
  //               better idea of possible performance implications.
  if (left_to_allocate > 0) {
    const SimulcastStream& stream = codec_.simulcastStream[top_active_layer];
    uint32_t bitrate_bps =
        allocated_bitrates_bps->GetSpatialLayerSum(top_active_layer);
    uint32_t allocation =
        std::min(left_to_allocate, stream.maxBitrate * 1000 - bitrate_bps);
    bitrate_bps += allocation;
    RTC_DCHECK_LE(allocation, left_to_allocate);
    left_to_allocate -= allocation;
    allocated_bitrates_bps->SetBitrate(top_active_layer, 0, bitrate_bps);
  }
}

void SimulcastRateAllocator::DistributeAllocationToTemporalLayers(
    uint32_t framerate,
    VideoBitrateAllocation* allocated_bitrates_bps) const {
  const int num_spatial_streams =
      std::max(1, static_cast<int>(codec_.numberOfSimulcastStreams));

  // Finally, distribute the bitrate for the simulcast streams across the
  // available temporal layers.
  for (int simulcast_id = 0; simulcast_id < num_spatial_streams;
       ++simulcast_id) {
    uint32_t target_bitrate_kbps =
        allocated_bitrates_bps->GetBitrate(simulcast_id, 0) / 1000;
    if (target_bitrate_kbps == 0) {
      continue;
    }

    const uint32_t expected_allocated_bitrate_kbps = target_bitrate_kbps;
    RTC_DCHECK_EQ(
        target_bitrate_kbps,
        allocated_bitrates_bps->GetSpatialLayerSum(simulcast_id) / 1000);
    const int num_temporal_streams = NumTemporalStreams(simulcast_id);
    uint32_t max_bitrate_kbps;
    // Legacy temporal-layered only screenshare, or simulcast screenshare
    // with legacy mode for simulcast stream 0.
    const bool conference_screenshare_mode =
        codec_.mode == VideoCodecMode::kScreensharing &&
        ((num_spatial_streams == 1 && num_temporal_streams == 2) ||  // Legacy.
         (num_spatial_streams > 1 && simulcast_id == 0));  // Simulcast.
    if (conference_screenshare_mode) {
      // TODO(holmer): This is a "temporary" hack for screensharing, where we
      // interpret the startBitrate as the encoder target bitrate. This is
      // to allow for a different max bitrate, so if the codec can't meet
      // the target we still allow it to overshoot up to the max before dropping
      // frames. This hack should be improved.
      max_bitrate_kbps =
          std::min(kLegacyScreenshareTl1BitrateKbps, target_bitrate_kbps);
      target_bitrate_kbps =
          std::min(kLegacyScreenshareTl0BitrateKbps, target_bitrate_kbps);
    } else if (num_spatial_streams == 1) {
      max_bitrate_kbps = codec_.maxBitrate;
    } else {
      max_bitrate_kbps = codec_.simulcastStream[simulcast_id].maxBitrate;
    }

    std::vector<uint32_t> tl_allocation;
    if (num_temporal_streams == 1) {
      tl_allocation.push_back(target_bitrate_kbps);
    } else {
      if (conference_screenshare_mode) {
        tl_allocation = ScreenshareTemporalLayerAllocation(
            target_bitrate_kbps, max_bitrate_kbps, framerate, simulcast_id);
      } else {
        tl_allocation = DefaultTemporalLayerAllocation(
            target_bitrate_kbps, max_bitrate_kbps, framerate, simulcast_id);
      }
    }
    RTC_DCHECK_GT(tl_allocation.size(), 0);
    RTC_DCHECK_LE(tl_allocation.size(), num_temporal_streams);

    uint64_t tl_allocation_sum_kbps = 0;
    for (size_t tl_index = 0; tl_index < tl_allocation.size(); ++tl_index) {
      uint32_t layer_rate_kbps = tl_allocation[tl_index];
      if (layer_rate_kbps > 0) {
        allocated_bitrates_bps->SetBitrate(simulcast_id, tl_index,
                                           layer_rate_kbps * 1000);
      }
      tl_allocation_sum_kbps += layer_rate_kbps;
    }
    RTC_DCHECK_LE(tl_allocation_sum_kbps, expected_allocated_bitrate_kbps);
  }
}

std::vector<uint32_t> SimulcastRateAllocator::DefaultTemporalLayerAllocation(
    int bitrate_kbps,
    int max_bitrate_kbps,
    int framerate,
    int simulcast_id) const {
  const size_t num_temporal_layers = NumTemporalStreams(simulcast_id);
  std::vector<uint32_t> bitrates;
  for (size_t i = 0; i < num_temporal_layers; ++i) {
    float layer_bitrate =
        bitrate_kbps * GetTemporalRateAllocation(num_temporal_layers, i);
    bitrates.push_back(static_cast<uint32_t>(layer_bitrate + 0.5));
  }

  // Allocation table is of aggregates, transform to individual rates.
  uint32_t sum = 0;
  for (size_t i = 0; i < num_temporal_layers; ++i) {
    uint32_t layer_bitrate = bitrates[i];
    RTC_DCHECK_LE(sum, bitrates[i]);
    bitrates[i] -= sum;
    sum = layer_bitrate;

    if (sum >= static_cast<uint32_t>(bitrate_kbps)) {
      // Sum adds up; any subsequent layers will be 0.
      bitrates.resize(i + 1);
      break;
    }
  }

  return bitrates;
}

std::vector<uint32_t>
SimulcastRateAllocator::ScreenshareTemporalLayerAllocation(
    int bitrate_kbps,
    int max_bitrate_kbps,
    int framerate,
    int simulcast_id) const {
  if (simulcast_id > 0) {
    return DefaultTemporalLayerAllocation(bitrate_kbps, max_bitrate_kbps,
                                          framerate, simulcast_id);
  }
  std::vector<uint32_t> allocation;
  allocation.push_back(bitrate_kbps);
  if (max_bitrate_kbps > bitrate_kbps)
    allocation.push_back(max_bitrate_kbps - bitrate_kbps);
  return allocation;
}

const VideoCodec& webrtc::SimulcastRateAllocator::GetCodec() const {
  return codec_;
}

int SimulcastRateAllocator::NumTemporalStreams(size_t simulcast_id) const {
  return std::max<uint8_t>(
      1,
      codec_.codecType == kVideoCodecVP8 && codec_.numberOfSimulcastStreams == 0
          ? codec_.VP8().numberOfTemporalLayers
          : codec_.simulcastStream[simulcast_id].numberOfTemporalLayers);
}

}  // namespace webrtc
