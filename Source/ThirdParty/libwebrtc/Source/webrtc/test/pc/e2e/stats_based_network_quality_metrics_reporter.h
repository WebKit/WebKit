/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_STATS_BASED_NETWORK_QUALITY_METRICS_REPORTER_H_
#define TEST_PC_E2E_STATS_BASED_NETWORK_QUALITY_METRICS_REPORTER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/test/network_emulation/network_emulation_interfaces.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {
namespace webrtc_pc_e2e {

class StatsBasedNetworkQualityMetricsReporter
    : public PeerConnectionE2EQualityTestFixture::QualityMetricsReporter {
 public:
  // |networks| map peer name to network to report network layer stability stats
  // and to log network layer metrics.
  StatsBasedNetworkQualityMetricsReporter(
      std::map<std::string, std::vector<EmulatedEndpoint*>> peer_endpoints,
      NetworkEmulationManager* network_emulation)
      : collector_(std::move(peer_endpoints), network_emulation),
        clock_(network_emulation->time_controller()->GetClock()) {}
  ~StatsBasedNetworkQualityMetricsReporter() override = default;

  // Network stats must be empty when this method will be invoked.
  void Start(absl::string_view test_case_name,
             const TrackIdStreamInfoMap* reporter_helper) override;
  void OnStatsReports(
      absl::string_view pc_label,
      const rtc::scoped_refptr<const RTCStatsReport>& report) override;
  void StopAndReportResults() override;

 private:
  struct PCStats {
    // TODO(nisse): Separate audio and video counters. Depends on standard stat
    // counters, enabled by field trial "WebRTC-UseStandardBytesStats".
    DataSize payload_received = DataSize::Zero();
    DataSize payload_sent = DataSize::Zero();

    // Total bytes/packets sent/received in all RTCTransport's.
    DataSize total_received = DataSize::Zero();
    DataSize total_sent = DataSize::Zero();
    int64_t packets_received = 0;
    int64_t packets_sent = 0;
  };

  struct NetworkLayerStats {
    std::unique_ptr<EmulatedNetworkStats> stats;
    std::set<std::string> receivers;
  };

  class NetworkLayerStatsCollector {
   public:
    NetworkLayerStatsCollector(
        std::map<std::string, std::vector<EmulatedEndpoint*>> peer_endpoints,
        NetworkEmulationManager* network_emulation);

    void Start();

    std::map<std::string, NetworkLayerStats> GetStats();

   private:
    const std::map<std::string, std::vector<EmulatedEndpoint*>> peer_endpoints_;
    const std::map<rtc::IPAddress, std::string> ip_to_peer_;
    NetworkEmulationManager* const network_emulation_;
  };

  void ReportStats(const std::string& pc_label,
                   const PCStats& pc_stats,
                   const NetworkLayerStats& network_layer_stats,
                   int64_t packet_loss,
                   const Timestamp& end_time);
  void ReportResult(const std::string& metric_name,
                    const std::string& network_label,
                    const double value,
                    const std::string& unit) const;
  void ReportResult(const std::string& metric_name,
                    const std::string& network_label,
                    const SamplesStatsCounter& value,
                    const std::string& unit) const;
  std::string GetTestCaseName(absl::string_view network_label) const;
  void LogNetworkLayerStats(const std::string& peer_name,
                            const NetworkLayerStats& stats) const;

  NetworkLayerStatsCollector collector_;
  Clock* const clock_;

  std::string test_case_name_;
  Timestamp start_time_ = Timestamp::MinusInfinity();

  Mutex mutex_;
  std::map<std::string, PCStats> pc_stats_ RTC_GUARDED_BY(mutex_);
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_STATS_BASED_NETWORK_QUALITY_METRICS_REPORTER_H_
