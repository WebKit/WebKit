/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/jitter/delay_variation_calculator.h"

#include <string>

#include "absl/types/optional.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

namespace {
constexpr Frequency k90000Hz = Frequency::Hertz(90000);
}  // namespace

void DelayVariationCalculator::Insert(
    uint32_t rtp_timestamp,
    Timestamp arrival_time,
    DataSize size,
    absl::optional<int> spatial_layer,
    absl::optional<int> temporal_layer,
    absl::optional<VideoFrameType> frame_type) {
  Frame frame{.rtp_timestamp = rtp_timestamp,
              .unwrapped_rtp_timestamp = unwrapper_.Unwrap(rtp_timestamp),
              .arrival_time = arrival_time,
              .size = size,
              .spatial_layer = spatial_layer,
              .temporal_layer = temporal_layer,
              .frame_type = frame_type};
  // Using RTP timestamp as the time series sample identifier allows for
  // cross-correlating time series logged by different objects at different
  // arrival timestamps.
  Timestamp sample_time =
      Timestamp::Millis((frame.unwrapped_rtp_timestamp / k90000Hz).ms());
  MetadataT sample_metadata = BuildMetadata(frame);
  if (!prev_frame_) {
    InsertFirstFrame(frame, sample_time, sample_metadata);
  } else {
    InsertFrame(frame, sample_time, sample_metadata);
  }
  prev_frame_ = frame;
}

void DelayVariationCalculator::InsertFirstFrame(const Frame& frame,
                                                Timestamp sample_time,
                                                MetadataT sample_metadata) {
  const auto s = [=](double sample_value) {
    return SamplesStatsCounter::StatsSample{.value = sample_value,
                                            .time = sample_time,
                                            .metadata = sample_metadata};
  };
  time_series_.rtp_timestamps.AddSample(
      s(static_cast<double>(frame.rtp_timestamp)));
  time_series_.arrival_times_ms.AddSample(s(frame.arrival_time.ms<double>()));
  time_series_.sizes_bytes.AddSample(s(frame.size.bytes<double>()));
  time_series_.inter_departure_times_ms.AddSample(s(0.0));
  time_series_.inter_arrival_times_ms.AddSample(s(0.0));
  time_series_.inter_delay_variations_ms.AddSample(s(0.0));
  time_series_.inter_size_variations_bytes.AddSample(s(0.0));
}

void DelayVariationCalculator::InsertFrame(const Frame& frame,
                                           Timestamp sample_time,
                                           MetadataT sample_metadata) {
  int64_t inter_rtp_time =
      frame.unwrapped_rtp_timestamp - prev_frame_->unwrapped_rtp_timestamp;
  TimeDelta inter_departure_time = inter_rtp_time / k90000Hz;
  TimeDelta inter_arrival_time = frame.arrival_time - prev_frame_->arrival_time;
  TimeDelta inter_delay_variation = inter_arrival_time - inter_departure_time;
  double inter_size_variation_bytes =
      frame.size.bytes<double>() - prev_frame_->size.bytes<double>();
  const auto s = [=](double sample_value) {
    return SamplesStatsCounter::StatsSample{.value = sample_value,
                                            .time = sample_time,
                                            .metadata = sample_metadata};
  };
  const auto ms = [](const TimeDelta& td) { return td.ms<double>(); };
  time_series_.rtp_timestamps.AddSample(
      s(static_cast<double>(frame.rtp_timestamp)));
  time_series_.arrival_times_ms.AddSample(s(frame.arrival_time.ms<double>()));
  time_series_.sizes_bytes.AddSample(s(frame.size.bytes<double>()));
  time_series_.inter_departure_times_ms.AddSample(s(ms(inter_departure_time)));
  time_series_.inter_arrival_times_ms.AddSample(s(ms(inter_arrival_time)));
  time_series_.inter_delay_variations_ms.AddSample(
      s(ms(inter_delay_variation)));
  time_series_.inter_size_variations_bytes.AddSample(
      s(inter_size_variation_bytes));
}

DelayVariationCalculator::MetadataT DelayVariationCalculator::BuildMetadata(
    const Frame& frame) {
  MetadataT metadata;
  if (prev_frame_) {
    if (prev_frame_->spatial_layer) {
      metadata["sl_prev"] = std::to_string(*prev_frame_->spatial_layer);
    }
    if (prev_frame_->temporal_layer) {
      metadata["tl_prev"] = std::to_string(*prev_frame_->temporal_layer);
    }
    if (prev_frame_->frame_type) {
      metadata["frame_type_prev"] =
          VideoFrameTypeToString(*prev_frame_->frame_type);
    }
  }
  if (frame.spatial_layer) {
    metadata["sl"] = std::to_string(*frame.spatial_layer);
  }
  if (frame.temporal_layer) {
    metadata["tl"] = std::to_string(*frame.temporal_layer);
  }
  if (frame.frame_type) {
    metadata["frame_type"] = VideoFrameTypeToString(*frame.frame_type);
  }
  return metadata;
}

}  // namespace test
}  // namespace webrtc
