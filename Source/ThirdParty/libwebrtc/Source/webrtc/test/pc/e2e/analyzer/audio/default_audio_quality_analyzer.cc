/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/audio/default_audio_quality_analyzer.h"

#include "api/stats/rtc_stats.h"
#include "api/stats/rtcstats_objects.h"
#include "api/test/metrics/metric.h"
#include "api/test/track_id_stream_info_map.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/pc/e2e/metric_metadata_keys.h"

namespace webrtc {
namespace webrtc_pc_e2e {

using ::webrtc::test::ImprovementDirection;
using ::webrtc::test::Unit;

DefaultAudioQualityAnalyzer::DefaultAudioQualityAnalyzer(
    test::MetricsLogger* const metrics_logger)
    : metrics_logger_(metrics_logger) {
  RTC_CHECK(metrics_logger_);
}

void DefaultAudioQualityAnalyzer::Start(std::string test_case_name,
                                        TrackIdStreamInfoMap* analyzer_helper) {
  test_case_name_ = std::move(test_case_name);
  analyzer_helper_ = analyzer_helper;
}

void DefaultAudioQualityAnalyzer::OnStatsReports(
    absl::string_view pc_label,
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCInboundRtpStreamStats>();

  for (auto& stat : stats) {
    if (!stat->kind.is_defined() || !(*stat->kind == "audio")) {
      continue;
    }

    StatsSample sample;
    sample.total_samples_received =
        stat->total_samples_received.ValueOrDefault(0ul);
    sample.concealed_samples = stat->concealed_samples.ValueOrDefault(0ul);
    sample.removed_samples_for_acceleration =
        stat->removed_samples_for_acceleration.ValueOrDefault(0ul);
    sample.inserted_samples_for_deceleration =
        stat->inserted_samples_for_deceleration.ValueOrDefault(0ul);
    sample.silent_concealed_samples =
        stat->silent_concealed_samples.ValueOrDefault(0ul);
    sample.jitter_buffer_delay =
        TimeDelta::Seconds(stat->jitter_buffer_delay.ValueOrDefault(0.));
    sample.jitter_buffer_target_delay =
        TimeDelta::Seconds(stat->jitter_buffer_target_delay.ValueOrDefault(0.));
    sample.jitter_buffer_emitted_count =
        stat->jitter_buffer_emitted_count.ValueOrDefault(0ul);
    sample.total_samples_duration =
        stat->total_samples_duration.ValueOrDefault(0.);
    sample.total_audio_energy = stat->total_audio_energy.ValueOrDefault(0.);

    TrackIdStreamInfoMap::StreamInfo stream_info =
        analyzer_helper_->GetStreamInfoFromTrackId(*stat->track_identifier);

    MutexLock lock(&lock_);
    stream_info_.emplace(stream_info.stream_label, stream_info);
    StatsSample prev_sample = last_stats_sample_[stream_info.stream_label];
    RTC_CHECK_GE(sample.total_samples_received,
                 prev_sample.total_samples_received);
    double total_samples_diff = static_cast<double>(
        sample.total_samples_received - prev_sample.total_samples_received);
    if (total_samples_diff == 0) {
      return;
    }

    AudioStreamStats& audio_stream_stats =
        streams_stats_[stream_info.stream_label];
    audio_stream_stats.expand_rate.AddSample(
        (sample.concealed_samples - prev_sample.concealed_samples) /
        total_samples_diff);
    audio_stream_stats.accelerate_rate.AddSample(
        (sample.removed_samples_for_acceleration -
         prev_sample.removed_samples_for_acceleration) /
        total_samples_diff);
    audio_stream_stats.preemptive_rate.AddSample(
        (sample.inserted_samples_for_deceleration -
         prev_sample.inserted_samples_for_deceleration) /
        total_samples_diff);

    int64_t speech_concealed_samples =
        sample.concealed_samples - sample.silent_concealed_samples;
    int64_t prev_speech_concealed_samples =
        prev_sample.concealed_samples - prev_sample.silent_concealed_samples;
    audio_stream_stats.speech_expand_rate.AddSample(
        (speech_concealed_samples - prev_speech_concealed_samples) /
        total_samples_diff);

    int64_t jitter_buffer_emitted_count_diff =
        sample.jitter_buffer_emitted_count -
        prev_sample.jitter_buffer_emitted_count;
    if (jitter_buffer_emitted_count_diff > 0) {
      TimeDelta jitter_buffer_delay_diff =
          sample.jitter_buffer_delay - prev_sample.jitter_buffer_delay;
      TimeDelta jitter_buffer_target_delay_diff =
          sample.jitter_buffer_target_delay -
          prev_sample.jitter_buffer_target_delay;
      audio_stream_stats.average_jitter_buffer_delay_ms.AddSample(
          jitter_buffer_delay_diff.ms<double>() /
          jitter_buffer_emitted_count_diff);
      audio_stream_stats.preferred_buffer_size_ms.AddSample(
          jitter_buffer_target_delay_diff.ms<double>() /
          jitter_buffer_emitted_count_diff);
    }
    audio_stream_stats.energy.AddSample(sqrt(
        (sample.total_audio_energy - prev_sample.total_audio_energy) /
        (sample.total_samples_duration - prev_sample.total_samples_duration)));

    last_stats_sample_[stream_info.stream_label] = sample;
  }
}

std::string DefaultAudioQualityAnalyzer::GetTestCaseName(
    const std::string& stream_label) const {
  return test_case_name_ + "/" + stream_label;
}

void DefaultAudioQualityAnalyzer::Stop() {
  MutexLock lock(&lock_);
  for (auto& item : streams_stats_) {
    const TrackIdStreamInfoMap::StreamInfo& stream_info =
        stream_info_[item.first];
    // TODO(bugs.webrtc.org/14757): Remove kExperimentalTestNameMetadataKey.
    std::map<std::string, std::string> metric_metadata{
        {MetricMetadataKey::kAudioStreamMetadataKey, item.first},
        {MetricMetadataKey::kPeerMetadataKey, stream_info.receiver_peer},
        {MetricMetadataKey::kReceiverMetadataKey, stream_info.receiver_peer},
        {MetricMetadataKey::kExperimentalTestNameMetadataKey, test_case_name_}};

    metrics_logger_->LogMetric("expand_rate", GetTestCaseName(item.first),
                               item.second.expand_rate, Unit::kUnitless,
                               ImprovementDirection::kSmallerIsBetter,
                               metric_metadata);
    metrics_logger_->LogMetric("accelerate_rate", GetTestCaseName(item.first),
                               item.second.accelerate_rate, Unit::kUnitless,
                               ImprovementDirection::kSmallerIsBetter,
                               metric_metadata);
    metrics_logger_->LogMetric("preemptive_rate", GetTestCaseName(item.first),
                               item.second.preemptive_rate, Unit::kUnitless,
                               ImprovementDirection::kSmallerIsBetter,
                               metric_metadata);
    metrics_logger_->LogMetric(
        "speech_expand_rate", GetTestCaseName(item.first),
        item.second.speech_expand_rate, Unit::kUnitless,
        ImprovementDirection::kSmallerIsBetter, metric_metadata);
    metrics_logger_->LogMetric(
        "average_jitter_buffer_delay_ms", GetTestCaseName(item.first),
        item.second.average_jitter_buffer_delay_ms, Unit::kMilliseconds,
        ImprovementDirection::kNeitherIsBetter, metric_metadata);
    metrics_logger_->LogMetric(
        "preferred_buffer_size_ms", GetTestCaseName(item.first),
        item.second.preferred_buffer_size_ms, Unit::kMilliseconds,
        ImprovementDirection::kNeitherIsBetter, metric_metadata);
    metrics_logger_->LogMetric("energy", GetTestCaseName(item.first),
                               item.second.energy, Unit::kUnitless,
                               ImprovementDirection::kNeitherIsBetter,
                               metric_metadata);
  }
}

std::map<std::string, AudioStreamStats>
DefaultAudioQualityAnalyzer::GetAudioStreamsStats() const {
  MutexLock lock(&lock_);
  return streams_stats_;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
