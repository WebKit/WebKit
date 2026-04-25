/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_PEER_SCENARIO_BWE_INTEGRATION_TESTS_STATS_UTILITIES_H_
#define TEST_PEER_SCENARIO_BWE_INTEGRATION_TESTS_STATS_UTILITIES_H_

#include <cstdint>
#include <optional>
#include <string_view>

#include "api/array_view.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats_report.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/peer_scenario/peer_scenario.h"
#include "test/peer_scenario/peer_scenario_client.h"

namespace webrtc {
namespace test {

scoped_refptr<const RTCStatsReport> GetStatsAndProcess(
    PeerScenario& s,
    PeerScenarioClient* client);

std::optional<scoped_refptr<const RTCStatsReport>> GetFirstReportAtOrAfter(
    Timestamp time,
    ArrayView<const scoped_refptr<const RTCStatsReport>> reports);

DataRate GetAvailableSendBitrate(
    const scoped_refptr<const RTCStatsReport>& report);

TimeDelta GetAverageRoundTripTime(
    const scoped_refptr<const RTCStatsReport>& report);

int64_t GetPacketsSentWithEct1(
    const scoped_refptr<const RTCStatsReport>& report);

int64_t GetPacketsReceivedWithEct1(
    const scoped_refptr<const RTCStatsReport>& report);

int64_t GetPacketsReceivedWithCe(
    const scoped_refptr<const RTCStatsReport>& report);

int64_t GetPacketsSent(const scoped_refptr<const RTCStatsReport>& report,
                       std::string_view kind = "");

int64_t GetPacketsReceived(const scoped_refptr<const RTCStatsReport>& report);

int64_t GetPacketsLost(const scoped_refptr<const RTCStatsReport>& report);

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PEER_SCENARIO_BWE_INTEGRATION_TESTS_STATS_UTILITIES_H_
