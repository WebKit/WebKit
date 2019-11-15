/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/network_quality_metrics_reporter.h"

#include <utility>

#include "rtc_base/event.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr int kStatsWaitTimeoutMs = 1000;

}

void NetworkQualityMetricsReporter::Start(absl::string_view test_case_name) {
  test_case_name_ = std::string(test_case_name);
  // Check that network stats are clean before test execution.
  EmulatedNetworkStats alice_stats = PopulateStats(alice_network_);
  RTC_CHECK_EQ(alice_stats.packets_sent, 0);
  RTC_CHECK_EQ(alice_stats.packets_received, 0);
  EmulatedNetworkStats bob_stats = PopulateStats(bob_network_);
  RTC_CHECK_EQ(bob_stats.packets_sent, 0);
  RTC_CHECK_EQ(bob_stats.packets_received, 0);
}

void NetworkQualityMetricsReporter::StopAndReportResults() {
  EmulatedNetworkStats alice_stats = PopulateStats(alice_network_);
  EmulatedNetworkStats bob_stats = PopulateStats(bob_network_);
  ReportStats("alice", alice_stats,
              alice_stats.packets_sent - bob_stats.packets_received);
  ReportStats("bob", bob_stats,
              bob_stats.packets_sent - alice_stats.packets_received);
}

EmulatedNetworkStats NetworkQualityMetricsReporter::PopulateStats(
    EmulatedNetworkManagerInterface* network) {
  rtc::Event wait;
  EmulatedNetworkStats stats;
  network->GetStats([&](const EmulatedNetworkStats& s) {
    stats = s;
    wait.Set();
  });
  bool stats_received = wait.Wait(kStatsWaitTimeoutMs);
  RTC_CHECK(stats_received);
  return stats;
}

void NetworkQualityMetricsReporter::ReportStats(
    const std::string& network_label,
    const EmulatedNetworkStats& stats,
    int64_t packet_loss) {
  ReportResult("bytes_sent", network_label, stats.bytes_sent.bytes(),
               "sizeInBytes");
  ReportResult("packets_sent", network_label, stats.packets_sent, "unitless");
  ReportResult(
      "average_send_rate", network_label,
      stats.packets_sent >= 2 ? stats.AverageSendRate().bytes_per_sec() : 0,
      "bytesPerSecond");
  ReportResult("bytes_dropped", network_label, stats.bytes_dropped.bytes(),
               "sizeInBytes");
  ReportResult("packets_dropped", network_label, stats.packets_dropped,
               "unitless");
  ReportResult("bytes_received", network_label, stats.bytes_received.bytes(),
               "sizeInBytes");
  ReportResult("packets_received", network_label, stats.packets_received,
               "unitless");
  ReportResult("average_receive_rate", network_label,
               stats.packets_received >= 2
                   ? stats.AverageReceiveRate().bytes_per_sec()
                   : 0,
               "bytesPerSecond");
  ReportResult("sent_packets_loss", network_label, packet_loss, "unitless");
}

void NetworkQualityMetricsReporter::ReportResult(
    const std::string& metric_name,
    const std::string& network_label,
    const double value,
    const std::string& unit) const {
  test::PrintResult(metric_name, /*modifier=*/"",
                    GetTestCaseName(network_label), value, unit,
                    /*important=*/false);
}

std::string NetworkQualityMetricsReporter::GetTestCaseName(
    const std::string& network_label) const {
  return test_case_name_ + "/" + network_label;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
