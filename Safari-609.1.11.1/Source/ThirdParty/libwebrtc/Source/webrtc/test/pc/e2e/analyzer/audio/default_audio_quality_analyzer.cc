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

#include "api/stats_types.h"
#include "rtc_base/logging.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

static const char kStatsAudioMediaType[] = "audio";

}  // namespace

void DefaultAudioQualityAnalyzer::Start(
    std::string test_case_name,
    TrackIdStreamLabelMap* analyzer_helper) {
  test_case_name_ = std::move(test_case_name);
  analyzer_helper_ = analyzer_helper;
}

void DefaultAudioQualityAnalyzer::OnStatsReports(
    const std::string& pc_label,
    const StatsReports& stats_reports) {
  for (const StatsReport* stats_report : stats_reports) {
    // NetEq stats are only present in kStatsReportTypeSsrc reports, so all
    // other reports are just ignored.
    if (stats_report->type() != StatsReport::StatsType::kStatsReportTypeSsrc) {
      continue;
    }
    // Ignoring stats reports of "video" SSRC.
    const webrtc::StatsReport::Value* media_type = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNameMediaType);
    RTC_CHECK(media_type);
    if (strcmp(media_type->static_string_val(), kStatsAudioMediaType) != 0) {
      continue;
    }
    if (stats_report->FindValue(
            webrtc::StatsReport::kStatsValueNameBytesSent)) {
      // If kStatsValueNameBytesSent is present, it means it's a send stream,
      // but we need audio metrics for receive stream, so skip it.
      continue;
    }

    const webrtc::StatsReport::Value* expand_rate = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNameExpandRate);
    const webrtc::StatsReport::Value* accelerate_rate = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNameAccelerateRate);
    const webrtc::StatsReport::Value* preemptive_rate = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNamePreemptiveExpandRate);
    const webrtc::StatsReport::Value* speech_expand_rate =
        stats_report->FindValue(
            StatsReport::StatsValueName::kStatsValueNameSpeechExpandRate);
    const webrtc::StatsReport::Value* preferred_buffer_size_ms =
        stats_report->FindValue(StatsReport::StatsValueName::
                                    kStatsValueNamePreferredJitterBufferMs);
    RTC_CHECK(expand_rate);
    RTC_CHECK(accelerate_rate);
    RTC_CHECK(preemptive_rate);
    RTC_CHECK(speech_expand_rate);
    RTC_CHECK(preferred_buffer_size_ms);

    const std::string& stream_label =
        GetStreamLabelFromStatsReport(stats_report);

    rtc::CritScope crit(&lock_);
    AudioStreamStats& audio_stream_stats = streams_stats_[stream_label];
    audio_stream_stats.expand_rate.AddSample(expand_rate->float_val());
    audio_stream_stats.accelerate_rate.AddSample(accelerate_rate->float_val());
    audio_stream_stats.preemptive_rate.AddSample(preemptive_rate->float_val());
    audio_stream_stats.speech_expand_rate.AddSample(
        speech_expand_rate->float_val());
    audio_stream_stats.preferred_buffer_size_ms.AddSample(
        preferred_buffer_size_ms->int_val());
  }
}

const std::string& DefaultAudioQualityAnalyzer::GetStreamLabelFromStatsReport(
    const StatsReport* stats_report) const {
  const webrtc::StatsReport::Value* report_track_id = stats_report->FindValue(
      StatsReport::StatsValueName::kStatsValueNameTrackId);
  RTC_CHECK(report_track_id);
  return analyzer_helper_->GetStreamLabelFromTrackId(
      report_track_id->string_val());
}

std::string DefaultAudioQualityAnalyzer::GetTestCaseName(
    const std::string& stream_label) const {
  return test_case_name_ + "/" + stream_label;
}

void DefaultAudioQualityAnalyzer::Stop() {
  rtc::CritScope crit(&lock_);
  for (auto& item : streams_stats_) {
    ReportResult("expand_rate", item.first, item.second.expand_rate,
                 "unitless");
    ReportResult("accelerate_rate", item.first, item.second.accelerate_rate,
                 "unitless");
    ReportResult("preemptive_rate", item.first, item.second.preemptive_rate,
                 "unitless");
    ReportResult("speech_expand_rate", item.first,
                 item.second.speech_expand_rate, "unitless");
    ReportResult("preferred_buffer_size_ms", item.first,
                 item.second.preferred_buffer_size_ms, "ms");
  }
}

std::map<std::string, AudioStreamStats>
DefaultAudioQualityAnalyzer::GetAudioStreamsStats() const {
  rtc::CritScope crit(&lock_);
  return streams_stats_;
}

void DefaultAudioQualityAnalyzer::ReportResult(
    const std::string& metric_name,
    const std::string& stream_label,
    const SamplesStatsCounter& counter,
    const std::string& unit) const {
  test::PrintResultMeanAndError(
      metric_name, /*modifier=*/"", GetTestCaseName(stream_label),
      counter.IsEmpty() ? 0 : counter.GetAverage(),
      counter.IsEmpty() ? 0 : counter.GetStandardDeviation(), unit,
      /*important=*/false);
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
