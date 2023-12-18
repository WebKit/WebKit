/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/jitter/logging_delay_variation_calculator.h"

#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

void LoggingDelayVariationCalculator::Insert(
    uint32_t rtp_timestamp,
    Timestamp arrival_time,
    DataSize size,
    absl::optional<int> spatial_layer,
    absl::optional<int> temporal_layer,
    absl::optional<VideoFrameType> frame_type) {
  calc_.Insert(rtp_timestamp, arrival_time, size, spatial_layer, temporal_layer,
               frame_type);
}

void LoggingDelayVariationCalculator::LogMetrics() const {
  const DelayVariationCalculator::TimeSeries& time_series = calc_.time_series();

  if (!time_series.rtp_timestamps.IsEmpty()) {
    logger_->LogMetric("rtp_timestamp", log_type_, time_series.rtp_timestamps,
                       Unit::kUnitless, ImprovementDirection::kNeitherIsBetter);
  }
  if (!time_series.arrival_times_ms.IsEmpty()) {
    logger_->LogMetric("arrival_time", log_type_, time_series.arrival_times_ms,
                       Unit::kMilliseconds,
                       ImprovementDirection::kNeitherIsBetter);
  }
  if (!time_series.sizes_bytes.IsEmpty()) {
    logger_->LogMetric("size_bytes", log_type_, time_series.sizes_bytes,
                       Unit::kBytes, ImprovementDirection::kNeitherIsBetter);
  }
  if (!time_series.inter_departure_times_ms.IsEmpty()) {
    logger_->LogMetric(
        "inter_departure_time", log_type_, time_series.inter_departure_times_ms,
        Unit::kMilliseconds, ImprovementDirection::kNeitherIsBetter);
  }
  if (!time_series.inter_arrival_times_ms.IsEmpty()) {
    logger_->LogMetric("inter_arrival_time", log_type_,
                       time_series.inter_arrival_times_ms, Unit::kMilliseconds,
                       ImprovementDirection::kNeitherIsBetter);
  }
  if (!time_series.inter_delay_variations_ms.IsEmpty()) {
    logger_->LogMetric("inter_delay_variation", log_type_,
                       time_series.inter_delay_variations_ms,
                       Unit::kMilliseconds,
                       ImprovementDirection::kNeitherIsBetter);
  }
  if (!time_series.inter_size_variations_bytes.IsEmpty()) {
    logger_->LogMetric("inter_size_variation", log_type_,
                       time_series.inter_size_variations_bytes, Unit::kBytes,
                       ImprovementDirection::kNeitherIsBetter);
  }
}

}  // namespace test
}  // namespace webrtc
