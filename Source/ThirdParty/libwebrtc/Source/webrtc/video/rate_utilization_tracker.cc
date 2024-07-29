/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/rate_utilization_tracker.h"

#include <algorithm>

namespace webrtc {

RateUtilizationTracker::RateUtilizationTracker(
    size_t max_num_encoded_data_points,
    TimeDelta max_duration)
    : max_data_points_(max_num_encoded_data_points),
      max_duration_(max_duration),
      current_rate_(DataRate::Zero()) {
  RTC_CHECK_GE(max_num_encoded_data_points, 0);
  RTC_CHECK_GT(max_duration, TimeDelta::Zero());
}

void RateUtilizationTracker::OnDataRateChanged(DataRate rate, Timestamp time) {
  current_rate_ = rate;
  if (data_points_.empty()) {
    // First entry should be contain first produced data, so just return after
    // setting `current_rate_`.
    return;
  } else {
    RateUsageUpdate& last_data_point = data_points_.back();
    RTC_CHECK_GE(time, last_data_point.time);
    if (last_data_point.time == time) {
      last_data_point.target_rate = rate;
    } else {
      data_points_.push_back({.time = time,
                              .target_rate = rate,
                              .produced_data = DataSize::Zero()});
    }
  }

  CullOldData(time);
}

void RateUtilizationTracker::OnDataProduced(DataSize size, Timestamp time) {
  if (data_points_.empty()) {
    data_points_.push_back(
        {.time = time, .target_rate = current_rate_, .produced_data = size});
  } else {
    RateUsageUpdate& last_data_point = data_points_.back();
    RTC_CHECK_GE(time, last_data_point.time);
    if (last_data_point.time == time) {
      last_data_point.produced_data += size;
    } else {
      data_points_.push_back(
          {.time = time, .target_rate = current_rate_, .produced_data = size});
    }
  }

  CullOldData(time);
}

absl::optional<double> RateUtilizationTracker::GetRateUtilizationFactor(
    Timestamp time) const {
  if (data_points_.empty()) {
    return absl::nullopt;
  }

  RTC_CHECK_GE(time, data_points_.back().time);
  DataSize allocated_send_data_size = DataSize::Zero();
  DataSize total_produced_data = DataSize::Zero();

  // Keep track of the last time data was produced - how much it was and how
  // much rate budget has been allocated since then.
  DataSize data_allocated_for_last_data = DataSize::Zero();
  DataSize size_of_last_data = DataSize::Zero();

  RTC_DCHECK(!data_points_.front().produced_data.IsZero());
  for (size_t i = 0; i < data_points_.size(); ++i) {
    const RateUsageUpdate& update = data_points_[i];
    total_produced_data += update.produced_data;

    DataSize allocated_since_previous_data_point =
        i == 0 ? DataSize::Zero()
               : (update.time - data_points_[i - 1].time) *
                     data_points_[i - 1].target_rate;
    allocated_send_data_size += allocated_since_previous_data_point;

    if (update.produced_data.IsZero()) {
      // Just a rate update past the last seen produced data.
      data_allocated_for_last_data =
          std::min(size_of_last_data, data_allocated_for_last_data +
                                          allocated_since_previous_data_point);
    } else {
      // A newer data point with produced data, reset accumulator for rate
      // allocated past the last data point.
      size_of_last_data = update.produced_data;
      data_allocated_for_last_data = DataSize::Zero();
    }
  }

  if (allocated_send_data_size.IsZero() && current_rate_.IsZero()) {
    // No allocated rate across all of the data points, ignore.
    return absl::nullopt;
  }

  // Calculate the rate past the very last data point until the polling time.
  const RateUsageUpdate& last_update = data_points_.back();
  DataSize allocated_since_last_data_point =
      (time - last_update.time) * last_update.target_rate;

  // If the last produced data packet is larger than the accumulated rate
  // allocation window since then, use that data point size instead (minus any
  // data rate accumulated in rate updates after that data point was produced).
  allocated_send_data_size +=
      std::max(allocated_since_last_data_point,
               size_of_last_data - data_allocated_for_last_data);

  return total_produced_data.bytes<double>() / allocated_send_data_size.bytes();
}

void RateUtilizationTracker::CullOldData(Timestamp time) {
  // Remove data points that are either too old, exceed the limit of number of
  // data points - and make sure the first entry in the list contains actual
  // data produced since we calculate send usage since that time.

  // We don't allow negative times so always start window at absolute time >= 0.
  const Timestamp oldest_included_time =
      time.ms() > max_duration_.ms() ? time - max_duration_ : Timestamp::Zero();

  while (!data_points_.empty() &&
         (data_points_.front().time < oldest_included_time ||
          data_points_.size() > max_data_points_ ||
          data_points_.front().produced_data.IsZero())) {
    data_points_.pop_front();
  }
}

}  // namespace webrtc
