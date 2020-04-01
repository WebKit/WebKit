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

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr int kBitsInByte = 8;

}  // namespace

void VideoQualityMetricsReporter::Start(absl::string_view test_case_name) {
  test_case_name_ = std::string(test_case_name);
}

// TODO(bugs.webrtc.org/10430): Migrate to the new GetStats as soon as
// bugs.webrtc.org/10428 is fixed.
void VideoQualityMetricsReporter::OnStatsReports(
    const std::string& pc_label,
    const StatsReports& stats_reports) {
  for (const StatsReport* stats_report : stats_reports) {
    // The only stats collected by this analyzer are present in
    // kStatsReportTypeBwe reports, so all other reports are just ignored.
    if (stats_report->type() != StatsReport::StatsType::kStatsReportTypeBwe) {
      continue;
    }
    const webrtc::StatsReport::Value* available_send_bandwidth =
        stats_report->FindValue(
            StatsReport::StatsValueName::kStatsValueNameAvailableSendBandwidth);
    const webrtc::StatsReport::Value* retransmission_bitrate =
        stats_report->FindValue(
            StatsReport::StatsValueName::kStatsValueNameRetransmitBitrate);
    const webrtc::StatsReport::Value* transmission_bitrate =
        stats_report->FindValue(
            StatsReport::StatsValueName::kStatsValueNameTransmitBitrate);
    const webrtc::StatsReport::Value* actual_encode_bitrate =
        stats_report->FindValue(
            StatsReport::StatsValueName::kStatsValueNameActualEncBitrate);
    const webrtc::StatsReport::Value* target_encode_bitrate =
        stats_report->FindValue(
            StatsReport::StatsValueName::kStatsValueNameTargetEncBitrate);
    RTC_CHECK(available_send_bandwidth);
    RTC_CHECK(retransmission_bitrate);
    RTC_CHECK(transmission_bitrate);
    RTC_CHECK(actual_encode_bitrate);
    RTC_CHECK(target_encode_bitrate);

    rtc::CritScope crit(&video_bwe_stats_lock_);
    VideoBweStats& video_bwe_stats = video_bwe_stats_[pc_label];
    video_bwe_stats.available_send_bandwidth.AddSample(
        available_send_bandwidth->int_val());
    video_bwe_stats.transmission_bitrate.AddSample(
        transmission_bitrate->int_val());
    video_bwe_stats.retransmission_bitrate.AddSample(
        retransmission_bitrate->int_val());
    video_bwe_stats.actual_encode_bitrate.AddSample(
        actual_encode_bitrate->int_val());
    video_bwe_stats.target_encode_bitrate.AddSample(
        target_encode_bitrate->int_val());
  }
}

void VideoQualityMetricsReporter::StopAndReportResults() {
  rtc::CritScope video_bwe_crit(&video_bwe_stats_lock_);
  for (const auto& item : video_bwe_stats_) {
    ReportVideoBweResults(GetTestCaseName(item.first), item.second);
  }
}

std::string VideoQualityMetricsReporter::GetTestCaseName(
    const std::string& stream_label) const {
  return test_case_name_ + "/" + stream_label;
}

void VideoQualityMetricsReporter::ReportVideoBweResults(
    const std::string& test_case_name,
    const VideoBweStats& video_bwe_stats) {
  ReportResult("available_send_bandwidth", test_case_name,
               video_bwe_stats.available_send_bandwidth / kBitsInByte,
               "bytesPerSecond");
  ReportResult("transmission_bitrate", test_case_name,
               video_bwe_stats.transmission_bitrate / kBitsInByte,
               "bytesPerSecond");
  ReportResult("retransmission_bitrate", test_case_name,
               video_bwe_stats.retransmission_bitrate / kBitsInByte,
               "bytesPerSecond");
  ReportResult("actual_encode_bitrate", test_case_name,
               video_bwe_stats.actual_encode_bitrate / kBitsInByte,
               "bytesPerSecond");
  ReportResult("target_encode_bitrate", test_case_name,
               video_bwe_stats.target_encode_bitrate / kBitsInByte,
               "bytesPerSecond");
}

void VideoQualityMetricsReporter::ReportResult(
    const std::string& metric_name,
    const std::string& test_case_name,
    const SamplesStatsCounter& counter,
    const std::string& unit,
    webrtc::test::ImproveDirection improve_direction) {
  test::PrintResult(metric_name, /*modifier=*/"", test_case_name, counter, unit,
                    /*important=*/false, improve_direction);
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
