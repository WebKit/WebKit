/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_RTT_SIMULATOR_H_
#define VIDEO_TIMING_SIMULATOR_RTT_SIMULATOR_H_

#include <cstdint>
#include <vector>

#include "absl/base/nullability.h"
#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/thread_annotations.h"
#include "video/call_stats2.h"
#include "video/timing/simulator/rtcp_rtt_calculator.h"

namespace webrtc::video_timing_simulator {

class SimulatedRttCallback {
 public:
  virtual ~SimulatedRttCallback() = default;
  virtual void OnMaxRttUpdate(TimeDelta max_rtt) = 0;
};

// Simulates session-level Round Trip Time (RTT) estimation by processing logged
// RTCP packets. It uses `RtcpRttCalculator` to calculate RTT samples from
// Sender Reports (SR), Receiver Reports (RR), and Extended Reports (XR),
// feeds them into `CallStats` to calculate moving averages/maxima, and
// propagates the updates back to the driver via `SimulatedRttCallback`.
class RttSimulator : public CallStatsObserver {
 public:
  RttSimulator(const Environment& env,
               TaskQueueBase* absl_nonnull simulator_queue,
               SimulatedRttCallback* absl_nonnull callback);
  ~RttSimulator() override;

  RttSimulator(const RttSimulator&) = delete;
  RttSimulator& operator=(const RttSimulator&) = delete;

  // Handlers for logged RTCP packets.
  void OnOutgoingSenderReport(const LoggedRtcpPacketSenderReport& packet);
  void OnOutgoingExtendedReports(const LoggedRtcpPacketExtendedReports& packet);
  void OnIncomingSenderReport(const LoggedRtcpPacketSenderReport& packet);
  void OnIncomingReceiverReport(const LoggedRtcpPacketReceiverReport& packet);
  void OnIncomingExtendedReports(const LoggedRtcpPacketExtendedReports& packet);

  // Implements `CallStatsObserver`.
  void OnRttUpdate(int64_t /*avg_rtt_ms*/, int64_t max_rtt_ms) override;

 private:
  void UpdateCallStats(const std::vector<TimeDelta>& rtts, Timestamp now);

  // Environment.
  SequenceChecker sequence_checker_;
  const Environment env_;

  // Worker objects.
  RtcpRttCalculator rtt_calculator_ RTC_GUARDED_BY(sequence_checker_);
  internal::CallStats call_stats_ RTC_GUARDED_BY(sequence_checker_);

  // Outputs.
  SimulatedRttCallback& simulated_rtt_cb_;
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_RTT_SIMULATOR_H_
