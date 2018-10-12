/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_BWE_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_BWE_H_

#include <memory>
#include <utility>
#include <vector>

#include "modules/congestion_controller/goog_cc/delay_increase_detector_interface.h"
#include "modules/congestion_controller/goog_cc/probe_bitrate_estimator.h"
#include "modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/remote_bitrate_estimator/inter_arrival.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/race_checker.h"

namespace webrtc {
class RtcEventLog;

class DelayBasedBwe {
 public:
  struct Result {
    Result();
    Result(bool probe, uint32_t target_bitrate_bps);
    ~Result();
    bool updated;
    bool probe;
    uint32_t target_bitrate_bps;
    bool recovered_from_overuse;
  };

  explicit DelayBasedBwe(RtcEventLog* event_log);
  virtual ~DelayBasedBwe();

  Result IncomingPacketFeedbackVector(
      const std::vector<PacketFeedback>& packet_feedback_vector,
      absl::optional<uint32_t> acked_bitrate_bps,
      int64_t at_time_ms);
  void OnRttUpdate(int64_t avg_rtt_ms);
  bool LatestEstimate(std::vector<uint32_t>* ssrcs,
                      uint32_t* bitrate_bps) const;
  void SetStartBitrate(int start_bitrate_bps);
  void SetMinBitrate(int min_bitrate_bps);
  int64_t GetExpectedBwePeriodMs() const;

 private:
  friend class GoogCcStatePrinter;
  void IncomingPacketFeedback(const PacketFeedback& packet_feedback,
                              int64_t at_time_ms);
  Result OnLongFeedbackDelay(int64_t arrival_time_ms);
  Result MaybeUpdateEstimate(absl::optional<uint32_t> acked_bitrate_bps,
                             bool request_probe,
                             int64_t at_time_ms);
  // Updates the current remote rate estimate and returns true if a valid
  // estimate exists.
  bool UpdateEstimate(int64_t now_ms,
                      absl::optional<uint32_t> acked_bitrate_bps,
                      uint32_t* target_bitrate_bps);

  rtc::RaceChecker network_race_;
  RtcEventLog* const event_log_;
  std::unique_ptr<InterArrival> inter_arrival_;
  std::unique_ptr<DelayIncreaseDetectorInterface> delay_detector_;
  int64_t last_seen_packet_ms_;
  bool uma_recorded_;
  AimdRateControl rate_control_;
  ProbeBitrateEstimator probe_bitrate_estimator_;
  size_t trendline_window_size_;
  double trendline_smoothing_coeff_;
  double trendline_threshold_gain_;
  int consecutive_delayed_feedbacks_;
  uint32_t prev_bitrate_;
  BandwidthUsage prev_state_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(DelayBasedBwe);
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_BWE_H_
