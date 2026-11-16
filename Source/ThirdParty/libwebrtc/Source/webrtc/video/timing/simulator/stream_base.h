/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/functional/any_invocable.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/moving_percentile_filter.h"
#include "video/timing/simulator/frame_base.h"

#ifndef VIDEO_TIMING_SIMULATOR_STREAM_BASE_H_
#define VIDEO_TIMING_SIMULATOR_STREAM_BASE_H_

namespace webrtc::video_timing_simulator {

// CRTP base struct for code reuse.
template <typename StreamT, typename FrameT>
struct StreamBase {
  // Data members are defined in derived struct.

  // -- CRTP accessors --
  const StreamT& self() const { return static_cast<const StreamT&>(*this); }
  StreamT& self() { return static_cast<StreamT&>(*this); }

  // -- Helpers --
  bool IsEmpty() const { return self().frames.empty(); }

  // -- Per-frame metric population --
  void PopulateFrameDelayVariations(float baseline_percentile = 0.0,
                                    size_t baseline_window_size = 300) {
    if (IsEmpty()) {
      return;
    }

    SortByArrivalOrder(self().frames);

    // One-way delay measurement offsets.
    Timestamp arrival_offset = Timestamp::PlusInfinity();
    Timestamp departure_offset = Timestamp::PlusInfinity();
    for (const auto& frame : self().frames) {
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

    // The baseline filter measures the minimum (by default) one-way delay
    // seen over a window. The corresponding value is then used to anchor all
    // other one-way delay measurements, creating the frame delay variation.
    MovingPercentileFilter<TimeDelta> baseline_filter(baseline_percentile,
                                                      baseline_window_size);

    // Calculate frame delay variations relative the moving baseline.
    for (auto& frame : self().frames) {
      TimeDelta one_way_delay =
          frame.OneWayDelay(arrival_offset, departure_offset);
      baseline_filter.Insert(one_way_delay);
      frame.frame_delay_variation =
          one_way_delay - baseline_filter.GetFilteredValue();
    }
  }

  // -- Per stream-metric aggregation --

  // Count number of set and true booleans accessed through `accessor`.
  int CountSetAndTrue(absl::AnyInvocable<std::optional<bool>(const FrameT&)
                                             const> accessor) const {
    return absl::c_count_if(
        self().frames, [accessor = std::move(accessor)](const auto& frame) {
          std::optional<bool> value = accessor(frame);
          return value.has_value() && *value;
        });
  }

  // Count number of finite timestamps accessed through `accessor`.
  int CountFiniteTimestamps(
      absl::AnyInvocable<Timestamp(const FrameT&) const> accessor) const {
    return absl::c_count_if(
        self().frames, [accessor = std::move(accessor)](const auto& frame) {
          return accessor(frame).IsFinite();
        });
  }

  // Sum non-negative int field values accessed through `accessor`.
  int SumNonNegativeIntField(
      absl::AnyInvocable<int(const FrameT&) const> accessor) const {
    return absl::c_accumulate(
        self().frames, 0,
        [accessor = std::move(accessor)](int sum, const auto& frame) {
          int value = accessor(frame);
          RTC_DCHECK_GE(value, 0);
          return sum + value;
        });
  }

  // Build samples of positive int field values accessed through `accessor`.
  SamplesStatsCounter BuildSamplesPositiveInt(
      absl::AnyInvocable<int(const FrameT&) const> accessor) const {
    SamplesStatsCounter stats(self().frames.size());
    for (const auto& frame : self().frames) {
      int value = accessor(frame);
      RTC_DCHECK_GT(value, 0);
      stats.AddSample({.value = static_cast<double>(value),
                       .time = Timestamp::PlusInfinity()});
    }
    return stats;
  }

  // Build samples of all set and finite TimeDelta field values accessed
  // through `accessor`.
  SamplesStatsCounter BuildSamplesMs(
      absl::AnyInvocable<std::optional<TimeDelta>(const FrameT&) const>
          accessor) const {
    SamplesStatsCounter stats(self().frames.size());
    for (const auto& frame : self().frames) {
      std::optional<TimeDelta> value = accessor(frame);
      if (!value.has_value() || !value->IsFinite()) {
        continue;
      }
      stats.AddSample(
          {.value = value->ms<double>(), .time = Timestamp::PlusInfinity()});
    }
    return stats;
  }

  // Build samples of all TimeDelta inter-calculated metrics provided by
  // `calculator`.
  SamplesStatsCounter BuildSamplesMs(
      absl::AnyInvocable<TimeDelta(const FrameT&, const FrameT&) const>
          calculator) const {
    SamplesStatsCounter stats(self().frames.size());
    for (size_t i = 1; i < self().frames.size(); ++i) {
      const auto& cur = self().frames[i];
      const auto& prev = self().frames[i - 1];
      TimeDelta inter = calculator(cur, prev);
      if (!inter.IsFinite()) {
        continue;
      }
      stats.AddSample(
          {.value = inter.ms<double>(), .time = Timestamp::PlusInfinity()});
    }
    return stats;
  }

  // Duration between min and max finite timestamps accessed through
  // `accessor`.
  TimeDelta MinMaxDuration(
      absl::AnyInvocable<Timestamp(const FrameT&) const> accessor) const {
    if (IsEmpty()) {
      return TimeDelta::PlusInfinity();
    }
    Timestamp min_value = Timestamp::PlusInfinity();
    Timestamp max_value = Timestamp::MinusInfinity();
    for (const auto& frame : self().frames) {
      Timestamp time = accessor(frame);
      if (!time.IsFinite()) {
        continue;
      }
      min_value = std::min(min_value, time);
      max_value = std::max(time, max_value);
    }
    if (!min_value.IsFinite() || !max_value.IsFinite()) {
      return TimeDelta::PlusInfinity();
    }
    RTC_DCHECK_LE(min_value, max_value);
    return max_value - min_value;
  }

  // -- Per-stream metrics --

  // Duration between first and last departed frames.
  TimeDelta DepartureDuration() const {
    return MinMaxDuration(
        [](const auto& frame) { return frame.DepartureTimestamp(); });
  }

  // Duration between first and last arrived frames.
  TimeDelta ArrivalDuration() const {
    return MinMaxDuration(
        [](const auto& frame) { return frame.ArrivalTimestamp(); });
  }

  // Total number of assembled frames.
  int NumAssembledFrames() const {
    int num_finite_timestamps =
        CountFiniteTimestamps(&FrameT::assembled_timestamp);
    RTC_DCHECK_EQ(num_finite_timestamps, self().frames.size());
    return num_finite_timestamps;
  }

  // Samples of per-frame sizes.
  SamplesStatsCounter NumPackets() const {
    return BuildSamplesPositiveInt(&FrameT::num_packets);
  }
  SamplesStatsCounter SizeBytes() const {
    return BuildSamplesPositiveInt(
        [](const auto& frame) { return frame.size.bytes(); });
  }

  // Samples of per-frame delay variation metrics.
  SamplesStatsCounter FrameDelayVariationMs() const {
    return BuildSamplesMs(&FrameT::frame_delay_variation);
  }
  SamplesStatsCounter InterDepartureTimeMs() {
    SortByDepartureOrder(self().frames);
    return BuildSamplesMs(&InterDepartureTime<FrameT>);
  }
  SamplesStatsCounter InterArrivalTimeMs() {
    SortByArrivalOrder(self().frames);
    return BuildSamplesMs(&InterArrivalTime<FrameT>);
  }
  SamplesStatsCounter InterFrameDelayVariationMs() {
    SortByArrivalOrder(self().frames);
    return BuildSamplesMs(&InterFrameDelayVariation<FrameT>);
  }
  SamplesStatsCounter InterAssembledTimeMs() {
    SortByAssembledOrder(self().frames);
    return BuildSamplesMs(&InterAssembledTime<FrameT>);
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
