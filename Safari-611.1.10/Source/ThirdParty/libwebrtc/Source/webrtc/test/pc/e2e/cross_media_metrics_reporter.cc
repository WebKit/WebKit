/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/cross_media_metrics_reporter.h"

#include <utility>
#include <vector>

#include "api/stats/rtc_stats.h"
#include "api/stats/rtcstats_objects.h"
#include "api/units/timestamp.h"
#include "rtc_base/event.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace webrtc_pc_e2e {

void CrossMediaMetricsReporter::Start(
    absl::string_view test_case_name,
    const TrackIdStreamInfoMap* reporter_helper) {
  test_case_name_ = std::string(test_case_name);
  reporter_helper_ = reporter_helper;
}

void CrossMediaMetricsReporter::OnStatsReports(
    absl::string_view pc_label,
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  auto inbound_stats = report->GetStatsOfType<RTCInboundRTPStreamStats>();
  std::map<absl::string_view, std::vector<const RTCInboundRTPStreamStats*>>
      sync_group_stats;
  for (const auto& stat : inbound_stats) {
    auto media_source_stat =
        report->GetAs<RTCMediaStreamTrackStats>(*stat->track_id);
    if (stat->estimated_playout_timestamp.ValueOrDefault(0.) > 0 &&
        media_source_stat->track_identifier.is_defined()) {
      sync_group_stats[reporter_helper_->GetSyncGroupLabelFromTrackId(
                           *media_source_stat->track_identifier)]
          .push_back(stat);
    }
  }

  MutexLock lock(&mutex_);
  for (const auto& pair : sync_group_stats) {
    // If there is less than two streams, it is not a sync group.
    if (pair.second.size() < 2) {
      continue;
    }
    auto sync_group = std::string(pair.first);
    const RTCInboundRTPStreamStats* audio_stat = pair.second[0];
    const RTCInboundRTPStreamStats* video_stat = pair.second[1];

    RTC_CHECK(pair.second.size() == 2 && audio_stat->kind.is_defined() &&
              video_stat->kind.is_defined() &&
              *audio_stat->kind != *video_stat->kind)
        << "Sync group should consist of one audio and one video stream.";

    if (*audio_stat->kind == RTCMediaStreamTrackKind::kVideo) {
      std::swap(audio_stat, video_stat);
    }
    // Stream labels of a sync group are same for all polls, so we need it add
    // it only once.
    if (stats_info_.find(sync_group) == stats_info_.end()) {
      auto audio_source_stat =
          report->GetAs<RTCMediaStreamTrackStats>(*audio_stat->track_id);
      auto video_source_stat =
          report->GetAs<RTCMediaStreamTrackStats>(*video_stat->track_id);
      // *_source_stat->track_identifier is always defined here because we
      // checked it while grouping stats.
      stats_info_[sync_group].audio_stream_label =
          std::string(reporter_helper_->GetStreamLabelFromTrackId(
              *audio_source_stat->track_identifier));
      stats_info_[sync_group].video_stream_label =
          std::string(reporter_helper_->GetStreamLabelFromTrackId(
              *video_source_stat->track_identifier));
    }

    double audio_video_playout_diff = *audio_stat->estimated_playout_timestamp -
                                      *video_stat->estimated_playout_timestamp;
    if (audio_video_playout_diff > 0) {
      stats_info_[sync_group].audio_ahead_ms.AddSample(
          audio_video_playout_diff);
      stats_info_[sync_group].video_ahead_ms.AddSample(0);
    } else {
      stats_info_[sync_group].audio_ahead_ms.AddSample(0);
      stats_info_[sync_group].video_ahead_ms.AddSample(
          std::abs(audio_video_playout_diff));
    }
  }
}

void CrossMediaMetricsReporter::StopAndReportResults() {
  MutexLock lock(&mutex_);
  for (const auto& pair : stats_info_) {
    const std::string& sync_group = pair.first;
    ReportResult("audio_ahead_ms",
                 GetTestCaseName(pair.second.audio_stream_label, sync_group),
                 pair.second.audio_ahead_ms, "ms",
                 webrtc::test::ImproveDirection::kSmallerIsBetter);
    ReportResult("video_ahead_ms",
                 GetTestCaseName(pair.second.video_stream_label, sync_group),
                 pair.second.video_ahead_ms, "ms",
                 webrtc::test::ImproveDirection::kSmallerIsBetter);
  }
}

void CrossMediaMetricsReporter::ReportResult(
    const std::string& metric_name,
    const std::string& test_case_name,
    const SamplesStatsCounter& counter,
    const std::string& unit,
    webrtc::test::ImproveDirection improve_direction) {
  test::PrintResult(metric_name, /*modifier=*/"", test_case_name, counter, unit,
                    /*important=*/false, improve_direction);
}

std::string CrossMediaMetricsReporter::GetTestCaseName(
    const std::string& stream_label,
    const std::string& sync_group) const {
  return test_case_name_ + "/" + sync_group + "_" + stream_label;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
