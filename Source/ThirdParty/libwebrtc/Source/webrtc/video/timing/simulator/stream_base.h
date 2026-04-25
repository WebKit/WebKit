/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/moving_percentile_filter.h"

#ifndef VIDEO_TIMING_SIMULATOR_STREAM_BASE_H_
#define VIDEO_TIMING_SIMULATOR_STREAM_BASE_H_

namespace webrtc::video_timing_simulator {

// CRTP base struct for code reuse.
template <typename StreamT>
struct StreamBase {
  // Data members are defined in derived struct.

  // -- CRTP accessors --
  const StreamT& self() const { return static_cast<const StreamT&>(*this); }
  StreamT& self() { return static_cast<StreamT&>(*this); }

  // -- Helpers --
  bool IsEmpty() const { return self().frames.empty(); }

  // -- Metric population --
  void PopulateFrameDelayVariations(float baseline_percentile = 0.0,
                                    size_t baseline_window_size = 300) {
    auto& frames = self().frames;
    if (frames.empty()) {
      return;
    }

    // The baseline filter measures the minimum (by default) one-way delay
    // seen over a window. The corresponding value is then used to anchor all
    // other one-way delay measurements, creating the frame delay variation.
    MovingPercentileFilter<TimeDelta> baseline_filter(baseline_percentile,
                                                      baseline_window_size);

    // One-way delay measurement offsets.
    Timestamp arrival_offset = Timestamp::PlusInfinity();
    Timestamp departure_offset = Timestamp::PlusInfinity();
    for (const auto& frame : frames) {
      Timestamp arrival = frame.ArrivalTimestamp();
      Timestamp departure = frame.DepartureTimestamp();
      if (arrival.IsFinite() && departure.IsFinite()) {
        arrival_offset = arrival;
        departure_offset = departure;
        break;
      }
    }
    if (!arrival_offset.IsFinite() || !departure_offset.IsFinite()) {
      RTC_LOG(LS_WARNING)
          << "Did not find valid arrival and/or departure offsets";
      return;
    }

    // Calculate frame delay variations relative the moving baseline.
    for (auto& frame : frames) {
      TimeDelta one_way_delay =
          frame.OneWayDelay(arrival_offset, departure_offset);
      baseline_filter.Insert(one_way_delay);
      frame.frame_delay_variation =
          one_way_delay - baseline_filter.GetFilteredValue();
    }
  }
};

// -- Comparators and sorting --
template <typename StreamT>
bool StreamOrder(const StreamT& a, const StreamT& b) {
  if (a.creation_timestamp != b.creation_timestamp) {
    return a.creation_timestamp < b.creation_timestamp;
  }
  return a.ssrc < b.ssrc;
}
template <typename StreamT>
void SortByStreamOrder(std::vector<StreamT>& streams) {
  absl::c_stable_sort(streams, StreamOrder<StreamT>);
}

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_STREAM_BASE_H_
