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
#include "rtc_base/logging.h"

namespace webrtc {
namespace webrtc_pc_e2e {

void DefaultAudioQualityAnalyzer::Start(std::string test_case_name,
                                        TrackIdStreamInfoMap* analyzer_helper) {
  test_case_name_ = std::move(test_case_name);
  analyzer_helper_ = analyzer_helper;
}

void DefaultAudioQualityAnalyzer::OnStatsReports(
    absl::string_view pc_label,
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  // TODO(https://crbug.com/webrtc/11789): use "inbound-rtp" instead of "track"
  // stats when required audio metrics moved there
  auto stats = report->GetStatsOfType<RTCMediaStreamTrackStats>();

  for (auto& stat : stats) {
    if (!stat->kind.is_defined() ||
        !(*stat->kind == RTCMediaStreamTrackKind::kAudio) ||
        !*stat->remote_source) {
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

    const std::string stream_label = std::string(
        analyzer_helper_->GetStreamLabelFromTrackId(*stat->track_identifier));

    MutexLock lock(&lock_);
    StatsSample prev_sample = last_stats_sample_[stream_label];
    RTC_CHECK_GE(sample.total_samples_received,
                 prev_sample.total_samples_received);
    double total_samples_diff = static_cast<double>(
        sample.total_samples_received - prev_sample.total_samples_received);
    if (total_samples_diff == 0) {
      return;
    }

    AudioStreamStats& audio_stream_stats = streams_stats_[stream_label];
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

    last_stats_sample_[stream_label] = sample;
  }
}

std::string DefaultAudioQualityAnalyzer::GetTestCaseName(
    const std::string& stream_label) const {
  return test_case_name_ + "/" + stream_label;
}

void DefaultAudioQualityAnalyzer::Stop() {
  using ::webrtc::test::ImproveDirection;
  MutexLock lock(&lock_);
  for (auto& item : streams_stats_) {
    ReportResult("expand_rate", item.first, item.second.expand_rate, "unitless",
                 ImproveDirection::kSmallerIsBetter);
    ReportResult("accelerate_rate", item.first, item.second.accelerate_rate,
                 "unitless", ImproveDirection::kSmallerIsBetter);
    ReportResult("preemptive_rate", item.first, item.second.preemptive_rate,
                 "unitless", ImproveDirection::kSmallerIsBetter);
    ReportResult("speech_expand_rate", item.first,
                 item.second.speech_expand_rate, "unitless",
                 ImproveDirection::kSmallerIsBetter);
    ReportResult("average_jitter_buffer_delay_ms", item.first,
                 item.second.average_jitter_buffer_delay_ms, "ms",
                 ImproveDirection::kNone);
    ReportResult("preferred_buffer_size_ms", item.first,
                 item.second.preferred_buffer_size_ms, "ms",
                 ImproveDirection::kNone);
  }
}

std::map<std::string, AudioStreamStats>
DefaultAudioQualityAnalyzer::GetAudioStreamsStats() const {
  MutexLock lock(&lock_);
  return streams_stats_;
}

void DefaultAudioQualityAnalyzer::ReportResult(
    const std::string& metric_name,
    const std::string& stream_label,
    const SamplesStatsCounter& counter,
    const std::string& unit,
    webrtc::test::ImproveDirection improve_direction) const {
  test::PrintResultMeanAndError(
      metric_name, /*modifier=*/"", GetTestCaseName(stream_label),
      counter.IsEmpty() ? 0 : counter.GetAverage(),
      counter.IsEmpty() ? 0 : counter.GetStandardDeviation(), unit,
      /*important=*/false, improve_direction);
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
