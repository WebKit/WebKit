/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/rtt_simulator.h"

#include <cstdint>
#include <vector>

#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "rtc_base/checks.h"
#include "video/call_stats2.h"

namespace webrtc::video_timing_simulator {

RttSimulator::RttSimulator(const Environment& env,
                           TaskQueueBase* simulator_queue,
                           SimulatedRttCallback* callback)
    : env_(env),
      call_stats_(&env_.clock(), simulator_queue),
      simulated_rtt_cb_(*callback) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  call_stats_.RegisterStatsObserver(this);
  call_stats_.EnsureStarted();
}

RttSimulator::~RttSimulator() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  call_stats_.DeregisterStatsObserver(this);
}

void RttSimulator::OnOutgoingSenderReport(
    const LoggedRtcpPacketSenderReport& packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  rtt_calculator_.OnOutgoingSenderReport(packet.sr, packet.log_time());
}

void RttSimulator::OnOutgoingExtendedReports(
    const LoggedRtcpPacketExtendedReports& packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  rtt_calculator_.OnOutgoingExtendedReports(packet.xr, packet.log_time());
}

void RttSimulator::OnIncomingSenderReport(
    const LoggedRtcpPacketSenderReport& packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  Timestamp now = packet.log_time();
  UpdateCallStats(rtt_calculator_.OnIncomingSenderReport(packet.sr, now), now);
}

void RttSimulator::OnIncomingReceiverReport(
    const LoggedRtcpPacketReceiverReport& packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  Timestamp now = packet.log_time();
  UpdateCallStats(rtt_calculator_.OnIncomingReceiverReport(packet.rr, now),
                  now);
}

void RttSimulator::OnIncomingExtendedReports(
    const LoggedRtcpPacketExtendedReports& packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  Timestamp now = packet.log_time();
  UpdateCallStats(rtt_calculator_.OnIncomingExtendedReports(packet.xr, now),
                  now);
}

void RttSimulator::OnRttUpdate(int64_t /*avg_rtt_ms*/, int64_t max_rtt_ms) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  simulated_rtt_cb_.OnMaxRttUpdate(TimeDelta::Millis(max_rtt_ms));
}

void RttSimulator::UpdateCallStats(const std::vector<TimeDelta>& rtts,
                                   Timestamp now) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // `call_stats_` uses `clock_->TimeInMilliseconds()` internally, so we
  // should double check that it will return `now`.
  RTC_DCHECK_EQ(env_.clock().TimeInMilliseconds(), now.ms());
  for (TimeDelta rtt : rtts) {
    call_stats_.AsRtcpRttStats()->OnRttUpdate(rtt.ms());
  }
}

}  // namespace webrtc::video_timing_simulator
