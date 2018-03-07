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
#include <utility>

namespace rtc {

// The purpose of this is to allow video streams to use extra bandwidth for FEC.
// TODO(bugs.webrtc.org/8541): May be worth to refactor to keep this logic in
// video send stream. Similar logic is implemented in BitrateAllocator.

const int kTransmissionMaxBitrateMultiplier = 2;

std::vector<uint32_t> BitrateAllocationStrategy::SetAllBitratesToMinimum(
    const ArrayView<const TrackConfig*> track_configs) {
  std::vector<uint32_t> track_allocations;
  for (const auto* track_config : track_configs) {
    track_allocations.push_back(track_config->min_bitrate_bps);
  }
  return track_allocations;
}

std::vector<uint32_t> BitrateAllocationStrategy::DistributeBitratesEvenly(
    const ArrayView<const TrackConfig*> track_configs,
    uint32_t available_bitrate) {
  std::vector<uint32_t> track_allocations =
      SetAllBitratesToMinimum(track_configs);
  uint32_t sum_min_bitrates = 0;
  uint32_t sum_max_bitrates = 0;
  for (const auto* track_config : track_configs) {
    sum_min_bitrates += track_config->min_bitrate_bps;
    sum_max_bitrates += track_config->max_bitrate_bps;
  }
  if (sum_min_bitrates >= available_bitrate) {
    return track_allocations;
  } else if (available_bitrate >= sum_max_bitrates) {
    auto track_allocations_it = track_allocations.begin();
    for (const auto* track_config : track_configs) {
      *track_allocations_it++ = track_config->max_bitrate_bps;
    }
    return track_allocations;
  } else {
    // If sum_min_bitrates < available_bitrate < sum_max_bitrates allocate
    // bitrates evenly up to max_bitrate_bps starting from the track with the
    // lowest max_bitrate_bps. Remainder of available bitrate split evenly among
    // remaining tracks.
    std::multimap<uint32_t, size_t> max_bitrate_sorted_configs;
    for (const TrackConfig** track_configs_it = track_configs.begin();
         track_configs_it != track_configs.end(); ++track_configs_it) {
      max_bitrate_sorted_configs.insert(
          std::make_pair((*track_configs_it)->max_bitrate_bps,
                         track_configs_it - track_configs.begin()));
    }
    uint32_t total_available_increase = available_bitrate - sum_min_bitrates;
    int processed_configs = 0;
    for (const auto& track_config_pair : max_bitrate_sorted_configs) {
      uint32_t available_increase =
          total_available_increase /
          (static_cast<uint32_t>(track_configs.size() - processed_configs));
      uint32_t consumed_increase =
          std::min(track_configs[track_config_pair.second]->max_bitrate_bps -
                       track_configs[track_config_pair.second]->min_bitrate_bps,
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
      sufficient_audio_bitrate_(sufficient_audio_bitrate) {}

std::vector<uint32_t> AudioPriorityBitrateAllocationStrategy::AllocateBitrates(
    uint32_t available_bitrate,
    const ArrayView<const TrackConfig*> track_configs) {
  const TrackConfig* audio_track_config = NULL;
  size_t audio_config_index = 0;
  uint32_t sum_min_bitrates = 0;
  uint32_t sum_max_bitrates = 0;

  for (const auto*& track_config : track_configs) {
    sum_min_bitrates += track_config->min_bitrate_bps;
    sum_max_bitrates += track_config->max_bitrate_bps;
    if (track_config->track_id == audio_track_id_) {
      audio_track_config = track_config;
      audio_config_index = &track_config - &track_configs[0];
    }
  }
  if (sum_max_bitrates < available_bitrate) {
    // Allow non audio streams to go above max upto
    // kTransmissionMaxBitrateMultiplier * max_bitrate_bps
    size_t track_configs_size = track_configs.size();
    std::vector<TrackConfig> increased_track_configs(track_configs_size);
    std::vector<const TrackConfig*> increased_track_configs_ptr(
        track_configs_size);
    for (unsigned long i = 0; i < track_configs_size; i++) {
      increased_track_configs[i] = (*track_configs[i]);
      increased_track_configs_ptr[i] = &increased_track_configs[i];
      if (track_configs[i]->track_id != audio_track_id_) {
        increased_track_configs[i].max_bitrate_bps =
            track_configs[i]->max_bitrate_bps *
            kTransmissionMaxBitrateMultiplier;
      }
    }
    return DistributeBitratesEvenly(increased_track_configs_ptr,
                                    available_bitrate);
  }
  if (audio_track_config == nullptr) {
    return DistributeBitratesEvenly(track_configs, available_bitrate);
  }
  auto safe_sufficient_audio_bitrate = std::min(
      std::max(audio_track_config->min_bitrate_bps, sufficient_audio_bitrate_),
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
      TrackConfig sufficient_track_config(*track_configs[audio_config_index]);
      sufficient_track_config.min_bitrate_bps = safe_sufficient_audio_bitrate;
      track_configs[audio_config_index] = &sufficient_track_config;
      std::vector<uint32_t> track_allocations =
          DistributeBitratesEvenly(track_configs, available_bitrate);
      return track_allocations;
    }
  }
}

}  // namespace rtc
