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

#include "api/stats_types.h"
#include "rtc_base/event.h"
#include "system_wrappers/include/field_trial.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr int kStatsWaitTimeoutMs = 1000;

// Field trial which controls whether to report standard-compliant bytes
// sent/received per stream.  If enabled, padding and headers are not included
// in bytes sent or received.
constexpr char kUseStandardBytesStats[] = "WebRTC-UseStandardBytesStats";
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

void NetworkQualityMetricsReporter::OnStatsReports(
    const std::string& pc_label,
    const StatsReports& reports) {
  rtc::CritScope cs(&lock_);
  int64_t payload_bytes_received = 0;
  int64_t payload_bytes_sent = 0;
  for (const StatsReport* report : reports) {
    if (report->type() == StatsReport::kStatsReportTypeSsrc) {
      const auto* received =
          report->FindValue(StatsReport::kStatsValueNameBytesReceived);
      if (received) {
        payload_bytes_received += received->int64_val();
      }
      const auto* sent =
          report->FindValue(StatsReport::kStatsValueNameBytesSent);
      if (sent) {
        payload_bytes_sent += sent->int64_val();
      }
    }
  }
  PCStats& stats = pc_stats_[pc_label];
  stats.payload_bytes_received = payload_bytes_received;
  stats.payload_bytes_sent = payload_bytes_sent;
}

void NetworkQualityMetricsReporter::StopAndReportResults() {
  EmulatedNetworkStats alice_stats = PopulateStats(alice_network_);
  EmulatedNetworkStats bob_stats = PopulateStats(bob_network_);
  ReportStats("alice", alice_stats,
              alice_stats.packets_sent - bob_stats.packets_received);
  ReportStats("bob", bob_stats,
              bob_stats.packets_sent - alice_stats.packets_received);

  if (!webrtc::field_trial::IsEnabled(kUseStandardBytesStats)) {
    RTC_LOG(LS_ERROR)
        << "Non-standard GetStats; \"payload\" counts include RTP headers";
  }

  rtc::CritScope cs(&lock_);
  for (const auto& pair : pc_stats_) {
    ReportPCStats(pair.first, pair.second);
  }
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

void NetworkQualityMetricsReporter::ReportPCStats(const std::string& pc_label,
                                                  const PCStats& stats) {
  ReportResult("payload_bytes_received", pc_label, stats.payload_bytes_received,
               "sizeInBytes");
  ReportResult("payload_bytes_sent", pc_label, stats.payload_bytes_sent,
               "sizeInBytes");
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
