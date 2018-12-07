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

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "modules/congestion_controller/goog_cc/delay_increase_detector_interface.h"
#include "modules/congestion_controller/goog_cc/probe_bitrate_estimator.h"
#include "modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/remote_bitrate_estimator/inter_arrival.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"  // For PacketFeedback
#include "rtc_base/constructormagic.h"
#include "rtc_base/race_checker.h"

namespace webrtc {
class RtcEventLog;

class DelayBasedBwe {
 public:
  struct Result {
    Result();
    Result(bool probe, DataRate target_bitrate);
    ~Result();
    bool updated;
    bool probe;
    DataRate target_bitrate = DataRate::Zero();
    bool recovered_from_overuse;
  };

  explicit DelayBasedBwe(RtcEventLog* event_log);
  virtual ~DelayBasedBwe();

  Result IncomingPacketFeedbackVector(
      const std::vector<PacketFeedback>& packet_feedback_vector,
      absl::optional<DataRate> acked_bitrate,
      absl::optional<DataRate> probe_bitrate,
      Timestamp at_time);
  Result OnDelayedFeedback(Timestamp receive_time);
  void OnRttUpdate(TimeDelta avg_rtt);
  bool LatestEstimate(std::vector<uint32_t>* ssrcs, DataRate* bitrate) const;
  void SetStartBitrate(DataRate start_bitrate);
  void SetMinBitrate(DataRate min_bitrate);
  TimeDelta GetExpectedBwePeriod() const;

 private:
  friend class GoogCcStatePrinter;
  void IncomingPacketFeedback(const PacketFeedback& packet_feedback,
                              Timestamp at_time);
  Result OnLongFeedbackDelay(Timestamp arrival_time);
  Result MaybeUpdateEstimate(absl::optional<DataRate> acked_bitrate,
                             absl::optional<DataRate> probe_bitrate,
                             bool request_probe,
                             Timestamp at_time);
  // Updates the current remote rate estimate and returns true if a valid
  // estimate exists.
  bool UpdateEstimate(Timestamp now,
                      absl::optional<DataRate> acked_bitrate,
                      DataRate* target_bitrate);

  rtc::RaceChecker network_race_;
  RtcEventLog* const event_log_;
  std::unique_ptr<InterArrival> inter_arrival_;
  std::unique_ptr<DelayIncreaseDetectorInterface> delay_detector_;
  Timestamp last_seen_packet_;
  bool uma_recorded_;
  AimdRateControl rate_control_;
  size_t trendline_window_size_;
  double trendline_smoothing_coeff_;
  double trendline_threshold_gain_;
  int consecutive_delayed_feedbacks_;
  DataRate prev_bitrate_;
  BandwidthUsage prev_state_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(DelayBasedBwe);
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_BWE_H_
