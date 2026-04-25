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

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/string_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats.h"
#include "api/stats/rtc_stats_report.h"
#include "api/stats/rtcstats_objects.h"
#include "api/test/metrics/metric.h"
#include "api/test/metrics/metrics_logger.h"
#include "api/test/track_id_stream_info_map.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/synchronization/mutex.h"
#include "system_wrappers/include/clock.h"
#include "test/pc/e2e/metric_metadata_keys.h"
#include "test/test_flags.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using test::ImprovementDirection;
using test::Unit;

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
  start_time_ = clock_->CurrentTime();
}

void VideoQualityMetricsReporter::OnStatsReports(
    absl::string_view pc_label,
    const scoped_refptr<const RTCStatsReport>& report) {
  RTC_CHECK(start_time_)
      << "Please invoke Start(...) method before calling OnStatsReports(...)";

  std::vector<const RTCTransportStats*> transport_stats =
      report->GetStatsOfType<RTCTransportStats>();
  if (transport_stats.empty() ||
      !transport_stats[0]->selected_candidate_pair_id.has_value()) {
    return;
  }
  RTC_DCHECK_EQ(transport_stats.size(), 1);
  std::string selected_ice_id =
      transport_stats[0]
          ->GetAttribute(transport_stats[0]->selected_candidate_pair_id)
          .ToString();
  // Use the selected ICE candidate pair ID to get the appropriate ICE stats.
  const RTCIceCandidatePairStats ice_candidate_pair_stats =
      report->Get(selected_ice_id)->cast_to<const RTCIceCandidatePairStats>();

  StatsSample sample = {.timestamp = *start_time_};
  for (const RTCOutboundRtpStreamStats* s :
       report->GetStatsOfType<RTCOutboundRtpStreamStats>()) {
    if (!s->kind.has_value() || *s->kind != "video") {
      continue;
    }
    sample.timestamp = std::max(*sample.timestamp, s->timestamp());
    sample.retransmitted_bytes_sent +=
        DataSize::Bytes(s->retransmitted_bytes_sent.value_or(0ul));
    sample.bytes_sent += DataSize::Bytes(s->bytes_sent.value_or(0ul));
    sample.header_bytes_sent +=
        DataSize::Bytes(s->header_bytes_sent.value_or(0ul));
  }

  MutexLock lock(&stats_lock_);
  VideoBweStats& video_bwe_stats = video_bwe_stats_[std::string(pc_label)];
  if (ice_candidate_pair_stats.available_outgoing_bitrate.has_value()) {
    video_bwe_stats.available_send_bandwidth.AddSample({
        .value = DataRate::BitsPerSec(
                     *ice_candidate_pair_stats.available_outgoing_bitrate)
                     .bytes_per_sec<double>(),
        .time = clock_->CurrentTime(),
    });
  }

  StatsSample prev_sample = last_stats_sample_[std::string(pc_label)];
  last_stats_sample_[std::string(pc_label)] = sample;

  TimeDelta time_between_samples =
      *sample.timestamp - prev_sample.timestamp.value_or(*start_time_);
  if (time_between_samples.IsZero()) {
    return;
  }

  DataRate retransmission_bitrate =
      (sample.retransmitted_bytes_sent - prev_sample.retransmitted_bytes_sent) /
      time_between_samples;
  video_bwe_stats.retransmission_bitrate.AddSample({
      .value = retransmission_bitrate.bytes_per_sec<double>(),
      .time = clock_->CurrentTime(),
  });
  DataRate transmission_bitrate =
      (sample.bytes_sent + sample.header_bytes_sent - prev_sample.bytes_sent -
       prev_sample.header_bytes_sent) /
      time_between_samples;
  video_bwe_stats.transmission_bitrate.AddSample({
      .value = transmission_bitrate.bytes_per_sec<double>(),
      .time = clock_->CurrentTime(),
  });
}

void VideoQualityMetricsReporter::StopAndReportResults() {
  MutexLock lock(&stats_lock_);
  for (const auto& item : video_bwe_stats_) {
    ReportVideoBweResults(item.first, item.second);
  }
}

void VideoQualityMetricsReporter::ReportVideoBweResults(
    const std::string& peer_name,
    const VideoBweStats& video_bwe_stats) {
  std::string test_case_name =
      !absl::GetFlag(FLAGS_isolated_script_test_perf_output).empty()
          ? test_case_name_ + "/" + peer_name
          : test_case_name_;
  std::map<std::string, std::string> metric_metadata{
      {MetricMetadataKey::kPeerMetadataKey, peer_name}};

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
