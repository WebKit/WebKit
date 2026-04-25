/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/peer_scenario/bwe_integration_tests/stats_utilities.h"

#include <cstdint>
#include <optional>
#include <string_view>

#include "api/array_view.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats_report.h"
#include "api/stats/rtcstats_objects.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/checks.h"
#include "test/peer_scenario/peer_scenario.h"
#include "test/peer_scenario/peer_scenario_client.h"

namespace webrtc {
namespace test {

scoped_refptr<const RTCStatsReport> GetStatsAndProcess(
    PeerScenario& s,
    PeerScenarioClient* client) {
  auto stats_collector = make_ref_counted<MockRTCStatsCollectorCallback>();
  client->pc()->GetStats(stats_collector.get());
  s.ProcessMessages(TimeDelta::Millis(0));
  RTC_CHECK(stats_collector->called());
  return stats_collector->report();
}

std::optional<scoped_refptr<const RTCStatsReport>> GetFirstReportAtOrAfter(
    Timestamp time,
    ArrayView<const scoped_refptr<const RTCStatsReport>> reports) {
  if (reports.empty()) {
    return std::nullopt;
  }
  for (const scoped_refptr<const RTCStatsReport>& report : reports) {
    if (report->timestamp() >= time) {
      return report;
    }
  }
  return std::nullopt;
}

DataRate GetAvailableSendBitrate(
    const scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCIceCandidatePairStats>();
  if (stats.empty()) {
    return DataRate::Zero();
  }
  return DataRate::BitsPerSec(
      stats[0]->available_outgoing_bitrate.value_or(0.0));
}

TimeDelta GetAverageRoundTripTime(
    const scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCIceCandidatePairStats>();
  if (stats.empty() || (stats[0]->responses_received.value_or(0) == 0)) {
    return TimeDelta::Zero();
  }

  return TimeDelta::Seconds(*stats[0]->total_round_trip_time /
                            *stats[0]->responses_received);
}

int64_t GetPacketsSentWithEct1(
    const scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  if (stats.empty()) {
    return 0;
  }
  int64_t number_of_packets = 0;
  for (const RTCOutboundRtpStreamStats* stream_stats : stats) {
    number_of_packets += stream_stats->packets_sent_with_ect1.value_or(0);
  }
  return number_of_packets;
}

int64_t GetPacketsReceivedWithEct1(
    const scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCInboundRtpStreamStats>();
  if (stats.empty()) {
    return 0;
  }
  int64_t number_of_packets = 0;
  for (const RTCInboundRtpStreamStats* stream_stats : stats) {
    number_of_packets += stream_stats->packets_received_with_ect1.value_or(0);
  }
  return number_of_packets;
}

int64_t GetPacketsReceivedWithCe(
    const scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCInboundRtpStreamStats>();
  if (stats.empty()) {
    return 0;
  }
  int64_t number_of_packets = 0;
  for (const RTCInboundRtpStreamStats* stream_stats : stats) {
    number_of_packets += stream_stats->packets_received_with_ce.value_or(0);
  }
  return number_of_packets;
}

int64_t GetPacketsSent(const scoped_refptr<const RTCStatsReport>& report,
                       std::string_view kind) {
  auto stats = report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  if (stats.empty()) {
    return 0;
  }
  int64_t number_of_packets = 0;
  for (const RTCOutboundRtpStreamStats* stream_stats : stats) {
    if (kind.empty() || kind == stream_stats->kind) {
      number_of_packets += stream_stats->packets_sent.value_or(0);
    }
  }
  return number_of_packets;
}

int64_t GetPacketsReceived(const scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCInboundRtpStreamStats>();
  if (stats.empty()) {
    return 0;
  }
  int64_t number_of_packets = 0;
  for (const RTCInboundRtpStreamStats* stream_stats : stats) {
    number_of_packets += stream_stats->packets_received.value_or(0);
  }
  return number_of_packets;
}

int64_t GetPacketsLost(const scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCInboundRtpStreamStats>();
  if (stats.empty()) {
    return 0;
  }
  int64_t number_of_packets = 0;
  for (const RTCInboundRtpStreamStats* stream_stats : stats) {
    number_of_packets += stream_stats->packets_lost.value_or(0);
  }
  return number_of_packets;
}

}  // namespace test
}  // namespace webrtc
