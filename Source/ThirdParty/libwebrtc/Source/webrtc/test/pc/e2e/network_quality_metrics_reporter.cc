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

#include "api/stats/rtc_stats.h"
#include "api/stats/rtcstats_objects.h"
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

void NetworkQualityMetricsReporter::Start(
    absl::string_view test_case_name,
    const TrackIdStreamInfoMap* /*reporter_helper*/) {
  test_case_name_ = std::string(test_case_name);
  // Check that network stats are clean before test execution.
  std::unique_ptr<EmulatedNetworkStats> alice_stats =
      PopulateStats(alice_network_);
  RTC_CHECK_EQ(alice_stats->PacketsSent(), 0);
  RTC_CHECK_EQ(alice_stats->PacketsReceived(), 0);
  std::unique_ptr<EmulatedNetworkStats> bob_stats = PopulateStats(bob_network_);
  RTC_CHECK_EQ(bob_stats->PacketsSent(), 0);
  RTC_CHECK_EQ(bob_stats->PacketsReceived(), 0);
}

void NetworkQualityMetricsReporter::OnStatsReports(
    absl::string_view pc_label,
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  DataSize payload_received = DataSize::Zero();
  DataSize payload_sent = DataSize::Zero();

  auto inbound_stats = report->GetStatsOfType<RTCInboundRTPStreamStats>();
  for (const auto& stat : inbound_stats) {
    payload_received +=
        DataSize::Bytes(stat->bytes_received.ValueOrDefault(0ul) +
                        stat->header_bytes_received.ValueOrDefault(0ul));
  }

  auto outbound_stats = report->GetStatsOfType<RTCOutboundRTPStreamStats>();
  for (const auto& stat : outbound_stats) {
    payload_sent +=
        DataSize::Bytes(stat->bytes_sent.ValueOrDefault(0ul) +
                        stat->header_bytes_sent.ValueOrDefault(0ul));
  }

  MutexLock lock(&lock_);
  PCStats& stats = pc_stats_[std::string(pc_label)];
  stats.payload_received = payload_received;
  stats.payload_sent = payload_sent;
}

void NetworkQualityMetricsReporter::StopAndReportResults() {
  std::unique_ptr<EmulatedNetworkStats> alice_stats =
      PopulateStats(alice_network_);
  std::unique_ptr<EmulatedNetworkStats> bob_stats = PopulateStats(bob_network_);
  int64_t alice_packets_loss =
      alice_stats->PacketsSent() - bob_stats->PacketsReceived();
  int64_t bob_packets_loss =
      bob_stats->PacketsSent() - alice_stats->PacketsReceived();
  ReportStats("alice", std::move(alice_stats), alice_packets_loss);
  ReportStats("bob", std::move(bob_stats), bob_packets_loss);

  if (!webrtc::field_trial::IsEnabled(kUseStandardBytesStats)) {
    RTC_LOG(LS_ERROR)
        << "Non-standard GetStats; \"payload\" counts include RTP headers";
  }

  MutexLock lock(&lock_);
  for (const auto& pair : pc_stats_) {
    ReportPCStats(pair.first, pair.second);
  }
}

std::unique_ptr<EmulatedNetworkStats>
NetworkQualityMetricsReporter::PopulateStats(
    EmulatedNetworkManagerInterface* network) {
  rtc::Event wait;
  std::unique_ptr<EmulatedNetworkStats> stats;
  network->GetStats([&](std::unique_ptr<EmulatedNetworkStats> s) {
    stats = std::move(s);
    wait.Set();
  });
  bool stats_received = wait.Wait(kStatsWaitTimeoutMs);
  RTC_CHECK(stats_received);
  return stats;
}

void NetworkQualityMetricsReporter::ReportStats(
    const std::string& network_label,
    std::unique_ptr<EmulatedNetworkStats> stats,
    int64_t packet_loss) {
  ReportResult("bytes_sent", network_label, stats->BytesSent().bytes(),
               "sizeInBytes");
  ReportResult("packets_sent", network_label, stats->PacketsSent(), "unitless");
  ReportResult(
      "average_send_rate", network_label,
      stats->PacketsSent() >= 2 ? stats->AverageSendRate().bytes_per_sec() : 0,
      "bytesPerSecond");
  ReportResult("bytes_discarded_no_receiver", network_label,
               stats->BytesDropped().bytes(), "sizeInBytes");
  ReportResult("packets_discarded_no_receiver", network_label,
               stats->PacketsDropped(), "unitless");
  ReportResult("bytes_received", network_label, stats->BytesReceived().bytes(),
               "sizeInBytes");
  ReportResult("packets_received", network_label, stats->PacketsReceived(),
               "unitless");
  ReportResult("average_receive_rate", network_label,
               stats->PacketsReceived() >= 2
                   ? stats->AverageReceiveRate().bytes_per_sec()
                   : 0,
               "bytesPerSecond");
  ReportResult("sent_packets_loss", network_label, packet_loss, "unitless");
}

void NetworkQualityMetricsReporter::ReportPCStats(const std::string& pc_label,
                                                  const PCStats& stats) {
  ReportResult("payload_bytes_received", pc_label,
               stats.payload_received.bytes(), "sizeInBytes");
  ReportResult("payload_bytes_sent", pc_label, stats.payload_sent.bytes(),
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
