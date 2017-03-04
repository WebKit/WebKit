/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_CONGESTION_CONTROLLER_DELAY_BASED_BWE_H_
#define WEBRTC_MODULES_CONGESTION_CONTROLLER_DELAY_BASED_BWE_H_

#include <memory>
#include <utility>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/rate_statistics.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/congestion_controller/median_slope_estimator.h"
#include "webrtc/modules/congestion_controller/probe_bitrate_estimator.h"
#include "webrtc/modules/congestion_controller/probing_interval_estimator.h"
#include "webrtc/modules/congestion_controller/trendline_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/inter_arrival.h"
#include "webrtc/modules/remote_bitrate_estimator/overuse_detector.h"
#include "webrtc/modules/remote_bitrate_estimator/overuse_estimator.h"

namespace webrtc {

class RtcEventLog;

class DelayBasedBwe {
 public:
  static const int64_t kStreamTimeOutMs = 2000;

  struct Result {
    Result() : updated(false), probe(false), target_bitrate_bps(0) {}
    Result(bool probe, uint32_t target_bitrate_bps)
        : updated(true), probe(probe), target_bitrate_bps(target_bitrate_bps) {}
    bool updated;
    bool probe;
    uint32_t target_bitrate_bps;
  };

  DelayBasedBwe(RtcEventLog* event_log, Clock* clock);
  virtual ~DelayBasedBwe() {}

  Result IncomingPacketFeedbackVector(
      const std::vector<PacketInfo>& packet_feedback_vector);
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms);
  bool LatestEstimate(std::vector<uint32_t>* ssrcs,
                      uint32_t* bitrate_bps) const;
  void SetStartBitrate(int start_bitrate_bps);
  void SetMinBitrate(int min_bitrate_bps);
  int64_t GetProbingIntervalMs() const;

 private:
  // Computes a bayesian estimate of the throughput given acks containing
  // the arrival time and payload size. Samples which are far from the current
  // estimate or are based on few packets are given a smaller weight, as they
  // are considered to be more likely to have been caused by, e.g., delay spikes
  // unrelated to congestion.
  class BitrateEstimator {
   public:
    BitrateEstimator();
    void Update(int64_t now_ms, int bytes);
    rtc::Optional<uint32_t> bitrate_bps() const;

   private:
    float UpdateWindow(int64_t now_ms, int bytes, int rate_window_ms);
    int sum_;
    int64_t current_win_ms_;
    int64_t prev_time_ms_;
    float bitrate_estimate_;
    float bitrate_estimate_var_;
    RateStatistics old_estimator_;
    const bool in_experiment_;
  };

  Result IncomingPacketInfo(const PacketInfo& info);
  Result OnLongFeedbackDelay(int64_t arrival_time_ms);
  // Updates the current remote rate estimate and returns true if a valid
  // estimate exists.
  bool UpdateEstimate(int64_t packet_arrival_time_ms,
                      int64_t now_ms,
                      rtc::Optional<uint32_t> acked_bitrate_bps,
                      uint32_t* target_bitrate_bps);
  const bool in_trendline_experiment_;
  const bool in_median_slope_experiment_;

  rtc::ThreadChecker network_thread_;
  RtcEventLog* const event_log_;
  Clock* const clock_;
  std::unique_ptr<InterArrival> inter_arrival_;
  std::unique_ptr<OveruseEstimator> kalman_estimator_;
  std::unique_ptr<TrendlineEstimator> trendline_estimator_;
  std::unique_ptr<MedianSlopeEstimator> median_slope_estimator_;
  OveruseDetector detector_;
  BitrateEstimator receiver_incoming_bitrate_;
  int64_t last_update_ms_;
  int64_t last_seen_packet_ms_;
  bool uma_recorded_;
  AimdRateControl rate_control_;
  ProbeBitrateEstimator probe_bitrate_estimator_;
  size_t trendline_window_size_;
  double trendline_smoothing_coeff_;
  double trendline_threshold_gain_;
  ProbingIntervalEstimator probing_interval_estimator_;
  size_t median_slope_window_size_;
  double median_slope_threshold_gain_;
  int consecutive_delayed_feedbacks_;
  uint32_t last_logged_bitrate_;
  BandwidthUsage last_logged_state_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(DelayBasedBwe);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_DELAY_BASED_BWE_H_
