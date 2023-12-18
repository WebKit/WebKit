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
#include "api/test/metrics/metric.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "system_wrappers/include/field_trial.h"
#include "test/pc/e2e/metric_metadata_keys.h"

namespace webrtc {
namespace webrtc_pc_e2e {

using ::webrtc::test::ImprovementDirection;
using ::webrtc::test::Unit;

CrossMediaMetricsReporter::CrossMediaMetricsReporter(
    test::MetricsLogger* metrics_logger)
    : metrics_logger_(metrics_logger) {
  RTC_CHECK(metrics_logger_);
}

void CrossMediaMetricsReporter::Start(
    absl::string_view test_case_name,
    const TrackIdStreamInfoMap* reporter_helper) {
  test_case_name_ = std::string(test_case_name);
  reporter_helper_ = reporter_helper;
}

void CrossMediaMetricsReporter::OnStatsReports(
    absl::string_view pc_label,
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  auto inbound_stats = report->GetStatsOfType<RTCInboundRtpStreamStats>();
  std::map<std::string, std::vector<const RTCInboundRtpStreamStats*>>
      sync_group_stats;
  for (const auto& stat : inbound_stats) {
    if (stat->estimated_playout_timestamp.ValueOrDefault(0.) > 0 &&
        stat->track_identifier.is_defined()) {
      sync_group_stats[reporter_helper_
                           ->GetStreamInfoFromTrackId(*stat->track_identifier)
                           .sync_group]
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
    const RTCInboundRtpStreamStats* audio_stat = pair.second[0];
    const RTCInboundRtpStreamStats* video_stat = pair.second[1];

    RTC_CHECK(pair.second.size() == 2 && audio_stat->kind.is_defined() &&
              video_stat->kind.is_defined() &&
              *audio_stat->kind != *video_stat->kind)
        << "Sync group should consist of one audio and one video stream.";

    if (*audio_stat->kind == "video") {
      std::swap(audio_stat, video_stat);
    }
    // Stream labels of a sync group are same for all polls, so we need it add
    // it only once.
    if (stats_info_.find(sync_group) == stats_info_.end()) {
      RTC_CHECK(audio_stat->track_identifier.is_defined());
      RTC_CHECK(video_stat->track_identifier.is_defined());
      stats_info_[sync_group].audio_stream_info =
          reporter_helper_->GetStreamInfoFromTrackId(
              *audio_stat->track_identifier);
      stats_info_[sync_group].video_stream_info =
          reporter_helper_->GetStreamInfoFromTrackId(
              *video_stat->track_identifier);
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
    // TODO(bugs.webrtc.org/14757): Remove kExperimentalTestNameMetadataKey.
    std::map<std::string, std::string> audio_metric_metadata{
        {MetricMetadataKey::kPeerSyncGroupMetadataKey, sync_group},
        {MetricMetadataKey::kAudioStreamMetadataKey,
         pair.second.audio_stream_info.stream_label},
        {MetricMetadataKey::kPeerMetadataKey,
         pair.second.audio_stream_info.receiver_peer},
        {MetricMetadataKey::kReceiverMetadataKey,
         pair.second.audio_stream_info.receiver_peer},
        {MetricMetadataKey::kExperimentalTestNameMetadataKey, test_case_name_}};
    metrics_logger_->LogMetric(
        "audio_ahead_ms",
        GetTestCaseName(pair.second.audio_stream_info.stream_label, sync_group),
        pair.second.audio_ahead_ms, Unit::kMilliseconds,
        webrtc::test::ImprovementDirection::kSmallerIsBetter,
        std::move(audio_metric_metadata));

    // TODO(bugs.webrtc.org/14757): Remove kExperimentalTestNameMetadataKey.
    std::map<std::string, std::string> video_metric_metadata{
        {MetricMetadataKey::kPeerSyncGroupMetadataKey, sync_group},
        {MetricMetadataKey::kAudioStreamMetadataKey,
         pair.second.video_stream_info.stream_label},
        {MetricMetadataKey::kPeerMetadataKey,
         pair.second.video_stream_info.receiver_peer},
        {MetricMetadataKey::kReceiverMetadataKey,
         pair.second.video_stream_info.receiver_peer},
        {MetricMetadataKey::kExperimentalTestNameMetadataKey, test_case_name_}};
    metrics_logger_->LogMetric(
        "video_ahead_ms",
        GetTestCaseName(pair.second.video_stream_info.stream_label, sync_group),
        pair.second.video_ahead_ms, Unit::kMilliseconds,
        webrtc::test::ImprovementDirection::kSmallerIsBetter,
        std::move(video_metric_metadata));
  }
}

std::string CrossMediaMetricsReporter::GetTestCaseName(
    const std::string& stream_label,
    const std::string& sync_group) const {
  return test_case_name_ + "/" + sync_group + "_" + stream_label;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
