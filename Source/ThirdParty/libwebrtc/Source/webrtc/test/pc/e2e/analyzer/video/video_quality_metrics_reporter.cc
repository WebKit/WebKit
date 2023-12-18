/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/video_quality_metrics_reporter.h"

#include <map>
#include <string>

#include "api/stats/rtc_stats.h"
#include "api/stats/rtcstats_objects.h"
#include "api/test/metrics/metric.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "test/pc/e2e/metric_metadata_keys.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using ::webrtc::test::ImprovementDirection;
using ::webrtc::test::Unit;
using ::webrtc::webrtc_pc_e2e::MetricMetadataKey;

SamplesStatsCounter BytesPerSecondToKbps(const SamplesStatsCounter& counter) {
  return counter * 0.008;
}

}  // namespace

VideoQualityMetricsReporter::VideoQualityMetricsReporter(
    Clock* const clock,
    test::MetricsLogger* const metrics_logger)
    : clock_(clock), metrics_logger_(metrics_logger) {
  RTC_CHECK(metrics_logger_);
}

void VideoQualityMetricsReporter::Start(
    absl::string_view test_case_name,
    const TrackIdStreamInfoMap* /*reporter_helper*/) {
  test_case_name_ = std::string(test_case_name);
  start_time_ = Now();
}

void VideoQualityMetricsReporter::OnStatsReports(
    absl::string_view pc_label,
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  RTC_CHECK(start_time_)
      << "Please invoke Start(...) method before calling OnStatsReports(...)";

  auto transport_stats = report->GetStatsOfType<RTCTransportStats>();
  if (transport_stats.size() == 0u ||
      !transport_stats[0]->selected_candidate_pair_id.is_defined()) {
    return;
  }
  RTC_DCHECK_EQ(transport_stats.size(), 1);
  std::string selected_ice_id =
      transport_stats[0]->selected_candidate_pair_id.ValueToString();
  // Use the selected ICE candidate pair ID to get the appropriate ICE stats.
  const RTCIceCandidatePairStats ice_candidate_pair_stats =
      report->Get(selected_ice_id)->cast_to<const RTCIceCandidatePairStats>();

  auto outbound_rtp_stats = report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  StatsSample sample;
  for (auto& s : outbound_rtp_stats) {
    if (!s->kind.is_defined()) {
      continue;
    }
    if (!(*s->kind == "video")) {
      continue;
    }
    if (s->timestamp() > sample.sample_time) {
      sample.sample_time = s->timestamp();
    }
    sample.retransmitted_bytes_sent +=
        DataSize::Bytes(s->retransmitted_bytes_sent.ValueOrDefault(0ul));
    sample.bytes_sent += DataSize::Bytes(s->bytes_sent.ValueOrDefault(0ul));
    sample.header_bytes_sent +=
        DataSize::Bytes(s->header_bytes_sent.ValueOrDefault(0ul));
  }

  MutexLock lock(&video_bwe_stats_lock_);
  VideoBweStats& video_bwe_stats = video_bwe_stats_[std::string(pc_label)];
  if (ice_candidate_pair_stats.available_outgoing_bitrate.is_defined()) {
    video_bwe_stats.available_send_bandwidth.AddSample(
        DataRate::BitsPerSec(
            *ice_candidate_pair_stats.available_outgoing_bitrate)
            .bytes_per_sec());
  }

  StatsSample prev_sample = last_stats_sample_[std::string(pc_label)];
  if (prev_sample.sample_time.IsZero()) {
    prev_sample.sample_time = start_time_.value();
  }
  last_stats_sample_[std::string(pc_label)] = sample;

  TimeDelta time_between_samples = sample.sample_time - prev_sample.sample_time;
  if (time_between_samples.IsZero()) {
    return;
  }

  DataRate retransmission_bitrate =
      (sample.retransmitted_bytes_sent - prev_sample.retransmitted_bytes_sent) /
      time_between_samples;
  video_bwe_stats.retransmission_bitrate.AddSample(
      retransmission_bitrate.bytes_per_sec());
  DataRate transmission_bitrate =
      (sample.bytes_sent + sample.header_bytes_sent - prev_sample.bytes_sent -
       prev_sample.header_bytes_sent) /
      time_between_samples;
  video_bwe_stats.transmission_bitrate.AddSample(
      transmission_bitrate.bytes_per_sec());
}

void VideoQualityMetricsReporter::StopAndReportResults() {
  MutexLock video_bwemutex_(&video_bwe_stats_lock_);
  for (const auto& item : video_bwe_stats_) {
    ReportVideoBweResults(item.first, item.second);
  }
}

std::string VideoQualityMetricsReporter::GetTestCaseName(
    const std::string& peer_name) const {
  return test_case_name_ + "/" + peer_name;
}

void VideoQualityMetricsReporter::ReportVideoBweResults(
    const std::string& peer_name,
    const VideoBweStats& video_bwe_stats) {
  std::string test_case_name = GetTestCaseName(peer_name);
  // TODO(bugs.webrtc.org/14757): Remove kExperimentalTestNameMetadataKey.
  std::map<std::string, std::string> metric_metadata{
      {MetricMetadataKey::kPeerMetadataKey, peer_name},
      {MetricMetadataKey::kExperimentalTestNameMetadataKey, test_case_name_}};

  metrics_logger_->LogMetric(
      "available_send_bandwidth", test_case_name,
      BytesPerSecondToKbps(video_bwe_stats.available_send_bandwidth),
      Unit::kKilobitsPerSecond, ImprovementDirection::kNeitherIsBetter,
      metric_metadata);
  metrics_logger_->LogMetric(
      "transmission_bitrate", test_case_name,
      BytesPerSecondToKbps(video_bwe_stats.transmission_bitrate),
      Unit::kKilobitsPerSecond, ImprovementDirection::kNeitherIsBetter,
      metric_metadata);
  metrics_logger_->LogMetric(
      "retransmission_bitrate", test_case_name,
      BytesPerSecondToKbps(video_bwe_stats.retransmission_bitrate),
      Unit::kKilobitsPerSecond, ImprovementDirection::kNeitherIsBetter,
      metric_metadata);
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
