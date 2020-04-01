/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_VIDEO_QUALITY_METRICS_REPORTER_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_VIDEO_QUALITY_METRICS_REPORTER_H_

#include <map>
#include <string>

#include "api/test/peerconnection_quality_test_fixture.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/numerics/samples_stats_counter.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace webrtc_pc_e2e {

struct VideoBweStats {
  SamplesStatsCounter available_send_bandwidth;
  SamplesStatsCounter transmission_bitrate;
  SamplesStatsCounter retransmission_bitrate;
  SamplesStatsCounter actual_encode_bitrate;
  SamplesStatsCounter target_encode_bitrate;
};

class VideoQualityMetricsReporter
    : public PeerConnectionE2EQualityTestFixture::QualityMetricsReporter {
 public:
  VideoQualityMetricsReporter() = default;
  ~VideoQualityMetricsReporter() override = default;

  void Start(absl::string_view test_case_name) override;
  void OnStatsReports(const std::string& pc_label,
                      const StatsReports& reports) override;
  void StopAndReportResults() override;

 private:
  std::string GetTestCaseName(const std::string& stream_label) const;
  static void ReportVideoBweResults(const std::string& test_case_name,
                                    const VideoBweStats& video_bwe_stats);
  // Report result for single metric for specified stream.
  static void ReportResult(const std::string& metric_name,
                           const std::string& test_case_name,
                           const SamplesStatsCounter& counter,
                           const std::string& unit,
                           webrtc::test::ImproveDirection improve_direction =
                               webrtc::test::ImproveDirection::kNone);

  std::string test_case_name_;

  rtc::CriticalSection video_bwe_stats_lock_;
  // Map between a peer connection label (provided by the framework) and
  // its video BWE stats.
  std::map<std::string, VideoBweStats> video_bwe_stats_
      RTC_GUARDED_BY(video_bwe_stats_lock_);
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_VIDEO_QUALITY_METRICS_REPORTER_H_
