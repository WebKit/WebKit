/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/bitrateallocationstrategy.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>

#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
AudioPriorityConfig::AudioPriorityConfig()
    : min_rate("min"), max_rate("max"), target_rate("target") {
  std::string trial_string;
// TODO(bugs.webrtc.org/9889): Remove this when Chromium build has been fixed.
#if !defined(WEBRTC_CHROMIUM_BUILD)
  trial_string = field_trial::FindFullName("WebRTC-Bwe-AudioPriority");
#endif
  ParseFieldTrial({&min_rate, &max_rate, &target_rate}, trial_string);
}
AudioPriorityConfig::AudioPriorityConfig(const AudioPriorityConfig&) = default;
AudioPriorityConfig::~AudioPriorityConfig() = default;

}  // namespace webrtc

namespace rtc {

// The purpose of this is to allow video streams to use extra bandwidth for FEC.
// TODO(bugs.webrtc.org/8541): May be worth to refactor to keep this logic in
// video send stream. Similar logic is implemented in BitrateAllocator.

const int kTransmissionMaxBitrateMultiplier = 2;

std::vector<uint32_t> BitrateAllocationStrategy::SetAllBitratesToMinimum(
    const std::vector<BitrateAllocationStrategy::TrackConfig>& track_configs) {
  std::vector<uint32_t> track_allocations;
  for (const auto& track_config : track_configs) {
    track_allocations.push_back(track_config.min_bitrate_bps);
  }
  return track_allocations;
}

std::vector<uint32_t> BitrateAllocationStrategy::DistributeBitratesEvenly(
    const std::vector<BitrateAllocationStrategy::TrackConfig>& track_configs,
    uint32_t available_bitrate) {
  std::vector<uint32_t> track_allocations =
      SetAllBitratesToMinimum(track_configs);
  uint32_t sum_min_bitrates = 0;
  uint32_t sum_max_bitrates = 0;
  for (const auto& track_config : track_configs) {
    sum_min_bitrates += track_config.min_bitrate_bps;
    sum_max_bitrates += track_config.max_bitrate_bps;
  }
  if (sum_min_bitrates >= available_bitrate) {
    return track_allocations;
  } else if (available_bitrate >= sum_max_bitrates) {
    auto track_allocations_it = track_allocations.begin();
    for (const auto& track_config : track_configs) {
      *track_allocations_it++ = track_config.max_bitrate_bps;
    }
    return track_allocations;
  } else {
    // If sum_min_bitrates < available_bitrate < sum_max_bitrates allocate
    // bitrates evenly up to max_bitrate_bps starting from the track with the
    // lowest max_bitrate_bps. Remainder of available bitrate split evenly among
    // remaining tracks.
    std::multimap<uint32_t, size_t> max_bitrate_sorted_configs;
    for (const auto& track_config : track_configs) {
      max_bitrate_sorted_configs.insert(
          std::make_pair(track_config.max_bitrate_bps,
                         &track_config - &track_configs.front()));
    }
    uint32_t total_available_increase = available_bitrate - sum_min_bitrates;
    int processed_configs = 0;
    for (const auto& track_config_pair : max_bitrate_sorted_configs) {
      uint32_t available_increase =
          total_available_increase /
          (static_cast<uint32_t>(track_configs.size() - processed_configs));
      uint32_t consumed_increase =
          std::min(track_configs[track_config_pair.second].max_bitrate_bps -
                       track_configs[track_config_pair.second].min_bitrate_bps,
                   available_increase);
      track_allocations[track_config_pair.second] += consumed_increase;
      total_available_increase -= consumed_increase;
      ++processed_configs;
    }
    return track_allocations;
  }
}
AudioPriorityBitrateAllocationStrategy::AudioPriorityBitrateAllocationStrategy(
    std::string audio_track_id,
    uint32_t sufficient_audio_bitrate)
    : audio_track_id_(audio_track_id),
      sufficient_audio_bitrate_(sufficient_audio_bitrate) {
  if (config_.target_rate) {
    sufficient_audio_bitrate_ = config_.target_rate->bps();
  }
}

std::vector<uint32_t> AudioPriorityBitrateAllocationStrategy::AllocateBitrates(
    uint32_t available_bitrate,
    std::vector<BitrateAllocationStrategy::TrackConfig> track_configs) {
  TrackConfig* audio_track_config = nullptr;
  size_t audio_config_index = 0;
  uint32_t sum_min_bitrates = 0;
  uint32_t sum_max_bitrates = 0;

  for (auto& track_config : track_configs) {
    if (track_config.track_id == audio_track_id_) {
      audio_config_index = &track_config - &track_configs[0];
      audio_track_config = &track_config;
      if (config_.min_rate)
        audio_track_config->min_bitrate_bps = config_.min_rate->bps();
      if (config_.max_rate)
        audio_track_config->max_bitrate_bps = config_.max_rate->bps();
    }
    sum_min_bitrates += track_config.min_bitrate_bps;
    sum_max_bitrates += track_config.max_bitrate_bps;
  }
  if (sum_max_bitrates < available_bitrate) {
    // Allow non audio streams to go above max upto
    // kTransmissionMaxBitrateMultiplier * max_bitrate_bps
    for (auto& track_config : track_configs) {
      if (&track_config != audio_track_config)
        track_config.max_bitrate_bps *= kTransmissionMaxBitrateMultiplier;
    }
    return DistributeBitratesEvenly(track_configs, available_bitrate);
  }
  if (!audio_track_config) {
    return DistributeBitratesEvenly(track_configs, available_bitrate);
  }
  auto safe_sufficient_audio_bitrate = rtc::SafeClamp(
      sufficient_audio_bitrate_, audio_track_config->min_bitrate_bps,
      audio_track_config->max_bitrate_bps);
  if (available_bitrate <= sum_min_bitrates) {
    return SetAllBitratesToMinimum(track_configs);
  } else {
    if (available_bitrate <= sum_min_bitrates + safe_sufficient_audio_bitrate -
                                 audio_track_config->min_bitrate_bps) {
      std::vector<uint32_t> track_allocations =
          SetAllBitratesToMinimum(track_configs);
      track_allocations[audio_config_index] +=
          available_bitrate - sum_min_bitrates;
      return track_allocations;
    } else {
      // Setting audio track minimum to safe_sufficient_audio_bitrate will
      // allow using DistributeBitratesEvenly to allocate at least sufficient
      // bitrate for audio and the rest evenly.
      audio_track_config->min_bitrate_bps = safe_sufficient_audio_bitrate;
      std::vector<uint32_t> track_allocations =
          DistributeBitratesEvenly(track_configs, available_bitrate);
      return track_allocations;
    }
  }
}

}  // namespace rtc
