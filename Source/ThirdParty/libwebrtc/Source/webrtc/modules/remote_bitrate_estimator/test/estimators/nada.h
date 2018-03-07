/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
*/

//  Implementation of Network-Assisted Dynamic Adaptation's (NADA's) proposal
//  Version according to Draft Document (mentioned in references)
//  http://tools.ietf.org/html/draft-zhu-rmcat-nada-06
//  From March 26, 2015.

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_NADA_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_NADA_H_

#include <list>
#include <map>
#include <memory>

#include "modules/include/module_common_types.h"
#include "modules/remote_bitrate_estimator/test/bwe.h"
#include "rtc_base/constructormagic.h"
#include "voice_engine/channel.h"

namespace webrtc {

class ReceiveStatistics;

namespace testing {
namespace bwe {

class NadaBweReceiver : public BweReceiver {
 public:
  explicit NadaBweReceiver(int flow_id);
  virtual ~NadaBweReceiver();

  void ReceivePacket(int64_t arrival_time_ms,
                     const MediaPacket& media_packet) override;
  FeedbackPacket* GetFeedback(int64_t now_ms) override;

  static int64_t MedianFilter(int64_t* v, int size);
  static int64_t ExponentialSmoothingFilter(int64_t new_value,
                                            int64_t last_smoothed_value,
                                            float alpha);

  static const int64_t kReceivingRateTimeWindowMs;

 private:
  SimulatedClock clock_;
  int64_t last_feedback_ms_;
  std::unique_ptr<ReceiveStatistics> recv_stats_;
  int64_t baseline_delay_ms_;  // Referred as d_f.
  int64_t delay_signal_ms_;    // Referred as d_n.
  int64_t last_congestion_signal_ms_;
  int last_delays_index_;
  int64_t exp_smoothed_delay_ms_;        // Referred as d_hat_n.
  int64_t est_queuing_delay_signal_ms_;  // Referred as d_tilde_n.
  int64_t last_delays_ms_[5];            // Used for Median Filter.
};

class NadaBweSender : public BweSender {
 public:
  static const int kMinNadaBitrateKbps;

  NadaBweSender(int kbps, BitrateObserver* observer, Clock* clock);
  NadaBweSender(BitrateObserver* observer, Clock* clock);
  virtual ~NadaBweSender();

  int GetFeedbackIntervalMs() const override;
  // Updates the min_feedback_delay_ms_ and the min_round_trip_time_ms_.
  void GiveFeedback(const FeedbackPacket& feedback) override;
  void OnPacketsSent(const Packets& packets) override {}
  int64_t TimeUntilNextProcess() override;
  void Process() override;
  void AcceleratedRampUp(const NadaFeedback& fb);
  void AcceleratedRampDown(const NadaFeedback& fb);
  void GradualRateUpdate(const NadaFeedback& fb,
                         float delta_s,
                         double smoothing_factor);

  int bitrate_kbps() const { return bitrate_kbps_; }
  void set_bitrate_kbps(int bitrate_kbps) { bitrate_kbps_ = bitrate_kbps; }
  bool original_operating_mode() const { return original_operating_mode_; }
  void set_original_operating_mode(bool original_operating_mode) {
    original_operating_mode_ = original_operating_mode;
  }
  int64_t NowMs() const { return clock_->TimeInMilliseconds(); }

 private:
  Clock* const clock_;
  BitrateObserver* const observer_;
  // Referred as R_min, default initialization for bitrate R_n.
  int64_t last_feedback_ms_ = 0;
  // Referred as delta_0, initialized as an upper bound.
  int64_t min_feedback_delay_ms_ = 200;
  // Referred as RTT_0, initialized as an upper bound.
  int64_t min_round_trip_time_ms_ = 100;
  bool original_operating_mode_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(NadaBweSender);
};

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_NADA_H_
