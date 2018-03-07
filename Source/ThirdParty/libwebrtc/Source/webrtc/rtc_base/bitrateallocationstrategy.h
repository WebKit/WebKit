/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_BITRATEALLOCATIONSTRATEGY_H_
#define RTC_BASE_BITRATEALLOCATIONSTRATEGY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "api/array_view.h"
#include "rtc_base/checks.h"

namespace rtc {

// Pluggable strategy allows configuration of bitrate allocation per media
// track.
//
// The strategy should provide allocation for every track passed with
// track_configs in AllocateBitrates. The allocations are constrained by
// max_bitrate_bps, min_bitrate_bps defining the track supported range and
// enforce_min_bitrate indicating if the track my be paused by allocating 0
// bitrate.
class BitrateAllocationStrategy {
 public:
  struct TrackConfig {
    TrackConfig(uint32_t min_bitrate_bps,
                uint32_t max_bitrate_bps,
                bool enforce_min_bitrate,
                std::string track_id)
        : min_bitrate_bps(min_bitrate_bps),
          max_bitrate_bps(max_bitrate_bps),
          enforce_min_bitrate(enforce_min_bitrate),
          track_id(track_id) {}
    TrackConfig(const TrackConfig& track_config) = default;
    virtual ~TrackConfig() = default;
    TrackConfig() {}

    // Minimum bitrate supported by track.
    uint32_t min_bitrate_bps;

    // Maximum bitrate supported by track.
    uint32_t max_bitrate_bps;

    // True means track may not be paused by allocating 0 bitrate.
    bool enforce_min_bitrate;

    // MediaStreamTrack ID as defined by application. May be empty.
    std::string track_id;
  };

  static std::vector<uint32_t> SetAllBitratesToMinimum(
      const ArrayView<const TrackConfig*> track_configs);
  static std::vector<uint32_t> DistributeBitratesEvenly(
      const ArrayView<const TrackConfig*> track_configs,
      uint32_t available_bitrate);

  // Strategy is expected to allocate all available_bitrate up to the sum of
  // max_bitrate_bps of all tracks. If available_bitrate is less than the sum of
  // min_bitrate_bps of all tracks, tracks having enforce_min_bitrate set to
  // false may get 0 allocation and are suppoused to pause, tracks with
  // enforce_min_bitrate set to true are expecting to get min_bitrate_bps.
  //
  // If the strategy will allocate more than available_bitrate it may cause
  // overuse of the currently available network capacity and may cause increase
  // in RTT and packet loss. Allocating less than available bitrate may cause
  // available_bitrate decrease.
  virtual std::vector<uint32_t> AllocateBitrates(
      uint32_t available_bitrate,
      const ArrayView<const TrackConfig*> track_configs) = 0;

  virtual ~BitrateAllocationStrategy() = default;
};

// Simple allocation strategy giving priority to audio until
// sufficient_audio_bitrate is reached. Bitrate is distributed evenly between
// the tracks after sufficient_audio_bitrate is reached. This implementation
// does not pause tracks even if enforce_min_bitrate is false.
class AudioPriorityBitrateAllocationStrategy
    : public BitrateAllocationStrategy {
 public:
  AudioPriorityBitrateAllocationStrategy(std::string audio_track_id,
                                         uint32_t sufficient_audio_bitrate);
  std::vector<uint32_t> AllocateBitrates(
      uint32_t available_bitrate,
      const ArrayView<const TrackConfig*> track_configs) override;

 private:
  std::string audio_track_id_;
  uint32_t sufficient_audio_bitrate_;
};
}  // namespace rtc

#endif  // RTC_BASE_BITRATEALLOCATIONSTRATEGY_H_
