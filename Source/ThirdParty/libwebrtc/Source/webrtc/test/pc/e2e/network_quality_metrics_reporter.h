/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_NETWORK_QUALITY_METRICS_REPORTER_H_
#define TEST_PC_E2E_NETWORK_QUALITY_METRICS_REPORTER_H_

#include <string>

#include "api/test/network_emulation_manager.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "rtc_base/critical_section.h"

namespace webrtc {
namespace webrtc_pc_e2e {

class NetworkQualityMetricsReporter
    : public PeerConnectionE2EQualityTestFixture::QualityMetricsReporter {
 public:
  NetworkQualityMetricsReporter(EmulatedNetworkManagerInterface* alice_network,
                                EmulatedNetworkManagerInterface* bob_network)
      : alice_network_(alice_network), bob_network_(bob_network) {}
  ~NetworkQualityMetricsReporter() override = default;

  // Network stats must be empty when this method will be invoked.
  void Start(absl::string_view test_case_name) override;
  void OnStatsReports(const std::string& pc_label,
                      const StatsReports& reports) override;
  void StopAndReportResults() override;

 private:
  struct PCStats {
    // TODO(nisse): Separate audio and video counters. Depends on standard stat
    // counters, enabled by field trial "WebRTC-UseStandardBytesStats".
    int64_t payload_bytes_received = 0;
    int64_t payload_bytes_sent = 0;
  };

  static EmulatedNetworkStats PopulateStats(
      EmulatedNetworkManagerInterface* network);
  void ReportStats(const std::string& network_label,
                   const EmulatedNetworkStats& stats,
                   int64_t packet_loss);
  void ReportPCStats(const std::string& pc_label, const PCStats& stats);
  void ReportResult(const std::string& metric_name,
                    const std::string& network_label,
                    const double value,
                    const std::string& unit) const;
  std::string GetTestCaseName(const std::string& network_label) const;

  std::string test_case_name_;

  EmulatedNetworkManagerInterface* alice_network_;
  EmulatedNetworkManagerInterface* bob_network_;
  rtc::CriticalSection lock_;
  std::map<std::string, PCStats> pc_stats_ RTC_GUARDED_BY(lock_);
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_NETWORK_QUALITY_METRICS_REPORTER_H_
