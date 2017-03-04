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

//  Implementation of Network-Assisted Dynamic Adaptation's (NADA's) proposal.
//  Version according to Draft Document (mentioned in references)
//  http://tools.ietf.org/html/draft-zhu-rmcat-nada-06
//  From March 26, 2015.

#include <math.h>
#include <algorithm>
#include <vector>

#include "webrtc/base/arraysize.h"
#include "webrtc/modules/remote_bitrate_estimator/test/estimators/nada.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "webrtc/modules/rtp_rtcp/include/receive_statistics.h"

namespace webrtc {
namespace testing {
namespace bwe {

namespace {
// Used as an upper bound for calling AcceleratedRampDown.
const float kMaxCongestionSignalMs =
    40.0f + NadaBweSender::kMinNadaBitrateKbps / 15;
}  // namespace

const int NadaBweSender::kMinNadaBitrateKbps = 50;
const int64_t NadaBweReceiver::kReceivingRateTimeWindowMs = 500;

NadaBweReceiver::NadaBweReceiver(int flow_id)
    : BweReceiver(flow_id, kReceivingRateTimeWindowMs),
      clock_(0),
      last_feedback_ms_(0),
      recv_stats_(ReceiveStatistics::Create(&clock_)),
      baseline_delay_ms_(10000),  // Initialized as an upper bound.
      delay_signal_ms_(0),
      last_congestion_signal_ms_(0),
      last_delays_index_(0),
      exp_smoothed_delay_ms_(-1),
      est_queuing_delay_signal_ms_(0) {
}

NadaBweReceiver::~NadaBweReceiver() {
}

void NadaBweReceiver::ReceivePacket(int64_t arrival_time_ms,
                                    const MediaPacket& media_packet) {
  const float kAlpha = 0.1f;                 // Used for exponential smoothing.
  const int64_t kDelayLowThresholdMs = 50;   // Referred as d_th.
  const int64_t kDelayMaxThresholdMs = 400;  // Referred as d_max.

  clock_.AdvanceTimeMilliseconds(arrival_time_ms - clock_.TimeInMilliseconds());
  recv_stats_->IncomingPacket(media_packet.header(),
                              media_packet.payload_size(), false);
  // Refered as x_n.
  int64_t delay_ms = arrival_time_ms - media_packet.sender_timestamp_ms();

  // The min should be updated within the first 10 minutes.
  if (clock_.TimeInMilliseconds() < 10 * 60 * 1000) {
    baseline_delay_ms_ = std::min(baseline_delay_ms_, delay_ms);
  }

  delay_signal_ms_ = delay_ms - baseline_delay_ms_;  // Refered as d_n.
  const int kMedian = arraysize(last_delays_ms_);
  last_delays_ms_[(last_delays_index_++) % kMedian] = delay_signal_ms_;
  int size = std::min(last_delays_index_, kMedian);

  int64_t median_filtered_delay_ms_ = MedianFilter(last_delays_ms_, size);
  exp_smoothed_delay_ms_ = ExponentialSmoothingFilter(
      median_filtered_delay_ms_, exp_smoothed_delay_ms_, kAlpha);

  if (exp_smoothed_delay_ms_ < kDelayLowThresholdMs) {
    est_queuing_delay_signal_ms_ = exp_smoothed_delay_ms_;
  } else if (exp_smoothed_delay_ms_ < kDelayMaxThresholdMs) {
    est_queuing_delay_signal_ms_ = static_cast<int64_t>(
        pow((static_cast<double>(kDelayMaxThresholdMs -
                                 exp_smoothed_delay_ms_)) /
                (kDelayMaxThresholdMs - kDelayLowThresholdMs),
            4.0) *
        kDelayLowThresholdMs);
  } else {
    est_queuing_delay_signal_ms_ = 0;
  }

  // Log received packet information.
  BweReceiver::ReceivePacket(arrival_time_ms, media_packet);
}

FeedbackPacket* NadaBweReceiver::GetFeedback(int64_t now_ms) {
  const int64_t kPacketLossPenaltyMs = 1000;  // Referred as d_L.

  if (now_ms - last_feedback_ms_ < 100) {
    return NULL;
  }

  float loss_fraction = RecentPacketLossRatio();

  int64_t loss_signal_ms =
      static_cast<int64_t>(loss_fraction * kPacketLossPenaltyMs + 0.5f);
  int64_t congestion_signal_ms = est_queuing_delay_signal_ms_ + loss_signal_ms;

  float derivative = 0.0f;
  if (last_feedback_ms_ > 0) {
    derivative = (congestion_signal_ms - last_congestion_signal_ms_) /
                 static_cast<float>(now_ms - last_feedback_ms_);
  }
  last_feedback_ms_ = now_ms;
  last_congestion_signal_ms_ = congestion_signal_ms;

  int64_t corrected_send_time_ms = 0L;

  if (!received_packets_.empty()) {
    PacketIdentifierNode* latest = *(received_packets_.begin());
    corrected_send_time_ms =
        latest->send_time_ms + now_ms - latest->arrival_time_ms;
  }

  // Sends a tuple containing latest values of <d_hat_n, d_tilde_n, x_n, x'_n,
  // R_r> and additional information.
  return new NadaFeedback(flow_id_, now_ms * 1000, exp_smoothed_delay_ms_,
                          est_queuing_delay_signal_ms_, congestion_signal_ms,
                          derivative, RecentKbps(), corrected_send_time_ms);
}

// If size is even, the median is the average of the two middlemost numbers.
int64_t NadaBweReceiver::MedianFilter(int64_t* last_delays_ms, int size) {
  std::vector<int64_t> array_copy(last_delays_ms, last_delays_ms + size);
  std::nth_element(array_copy.begin(), array_copy.begin() + size / 2,
                   array_copy.end());
  if (size % 2 == 1) {
    // Typically, size = 5. For odd size values, right and left are equal.
    return array_copy.at(size / 2);
  }
  int64_t right = array_copy.at(size / 2);
  std::nth_element(array_copy.begin(), array_copy.begin() + (size - 1) / 2,
                   array_copy.end());
  int64_t left = array_copy.at((size - 1) / 2);
  return (left + right + 1) / 2;
}

int64_t NadaBweReceiver::ExponentialSmoothingFilter(int64_t new_value,
                                                    int64_t last_smoothed_value,
                                                    float alpha) {
  if (last_smoothed_value < 0) {
    return new_value;  // Handling initial case.
  }
  return static_cast<int64_t>(alpha * new_value +
                              (1.0f - alpha) * last_smoothed_value + 0.5f);
}

// Implementation according to Cisco's proposal by default.
NadaBweSender::NadaBweSender(int kbps, BitrateObserver* observer, Clock* clock)
    : BweSender(kbps),  // Referred as "Reference Rate" = R_n.,
      clock_(clock),
      observer_(observer),
      original_operating_mode_(true) {
}

NadaBweSender::NadaBweSender(BitrateObserver* observer, Clock* clock)
    : BweSender(kMinNadaBitrateKbps),  // Referred as "Reference Rate" = R_n.
      clock_(clock),
      observer_(observer),
      original_operating_mode_(true) {}

NadaBweSender::~NadaBweSender() {
}

int NadaBweSender::GetFeedbackIntervalMs() const {
  return 100;
}

void NadaBweSender::GiveFeedback(const FeedbackPacket& feedback) {
  const NadaFeedback& fb = static_cast<const NadaFeedback&>(feedback);

  // Following parameters might be optimized.
  const int64_t kQueuingDelayUpperBoundMs = 10;
  const float kDerivativeUpperBound =
      10.0f / std::max<int64_t>(1, min_feedback_delay_ms_);
  // In the modified version, a higher kMinUpperBound allows a higher d_hat
  // upper bound for calling AcceleratedRampUp.
  const float kProportionalityDelayBits = 20.0f;

  int64_t now_ms = clock_->TimeInMilliseconds();
  float delta_s = now_ms - last_feedback_ms_;
  last_feedback_ms_ = now_ms;
  // Update delta_0.
  min_feedback_delay_ms_ =
      std::min(min_feedback_delay_ms_, static_cast<int64_t>(delta_s));

  // Update RTT_0.
  int64_t rtt_ms = now_ms - fb.latest_send_time_ms();
  min_round_trip_time_ms_ = std::min(min_round_trip_time_ms_, rtt_ms);

  // Independent limits for AcceleratedRampUp conditions variables:
  // x_n, d_tilde and x'_n in the original implementation, plus
  // d_hat and receiving_rate in the modified one.
  // There should be no packet losses/marking, hence x_n == d_tilde.
  if (original_operating_mode_) {
    // Original if conditions and rate update.
    if (fb.congestion_signal() == fb.est_queuing_delay_signal_ms() &&
        fb.est_queuing_delay_signal_ms() < kQueuingDelayUpperBoundMs &&
        fb.derivative() < kDerivativeUpperBound) {
      AcceleratedRampUp(fb);
    } else {
      GradualRateUpdate(fb, delta_s, 1.0);
    }
  } else {
    // Modified if conditions and rate update; new ramp down mode.
    if (fb.congestion_signal() == fb.est_queuing_delay_signal_ms() &&
        fb.est_queuing_delay_signal_ms() < kQueuingDelayUpperBoundMs &&
        fb.exp_smoothed_delay_ms() <
            kMinNadaBitrateKbps / kProportionalityDelayBits &&
        fb.derivative() < kDerivativeUpperBound &&
        fb.receiving_rate() > kMinNadaBitrateKbps) {
      AcceleratedRampUp(fb);
    } else if (fb.congestion_signal() > kMaxCongestionSignalMs ||
               fb.exp_smoothed_delay_ms() > kMaxCongestionSignalMs) {
      AcceleratedRampDown(fb);
    } else {
      double bitrate_reference =
          (2.0 * bitrate_kbps_) / (kMaxBitrateKbps + kMinNadaBitrateKbps);
      double smoothing_factor = pow(bitrate_reference, 0.75);
      GradualRateUpdate(fb, delta_s, smoothing_factor);
    }
  }

  bitrate_kbps_ = std::min(bitrate_kbps_, kMaxBitrateKbps);
  bitrate_kbps_ = std::max(bitrate_kbps_, kMinNadaBitrateKbps);

  observer_->OnNetworkChanged(1000 * bitrate_kbps_, 0, rtt_ms);
}

int64_t NadaBweSender::TimeUntilNextProcess() {
  return 100;
}

void NadaBweSender::Process() {
}

void NadaBweSender::AcceleratedRampUp(const NadaFeedback& fb) {
  const int kMaxRampUpQueuingDelayMs = 50;  // Referred as T_th.
  const float kGamma0 = 0.5f;               // Referred as gamma_0.

  float gamma =
      std::min(kGamma0, static_cast<float>(kMaxRampUpQueuingDelayMs) /
                            (min_round_trip_time_ms_ + min_feedback_delay_ms_));

  bitrate_kbps_ = static_cast<int>((1.0f + gamma) * fb.receiving_rate() + 0.5f);
}

void NadaBweSender::AcceleratedRampDown(const NadaFeedback& fb) {
  const float kGamma0 = 0.9f;
  float gamma = 3.0f * kMaxCongestionSignalMs /
                (fb.congestion_signal() + fb.exp_smoothed_delay_ms());
  gamma = std::min(gamma, kGamma0);
  bitrate_kbps_ = gamma * fb.receiving_rate() + 0.5f;
}

void NadaBweSender::GradualRateUpdate(const NadaFeedback& fb,
                                      float delta_s,
                                      double smoothing_factor) {
  const float kTauOMs = 500.0f;           // Referred as tau_o.
  const float kEta = 2.0f;                // Referred as eta.
  const float kKappa = 1.0f;              // Referred as kappa.
  const float kReferenceDelayMs = 10.0f;  // Referred as x_ref.
  const float kPriorityWeight = 1.0f;     // Referred as w.

  float x_hat = fb.congestion_signal() + kEta * kTauOMs * fb.derivative();

  float kTheta = kPriorityWeight * (kMaxBitrateKbps - kMinNadaBitrateKbps) *
                 kReferenceDelayMs;

  int original_increase = static_cast<int>(
      (kKappa * delta_s *
       (kTheta - (bitrate_kbps_ - kMinNadaBitrateKbps) * x_hat)) /
          (kTauOMs * kTauOMs) +
      0.5f);

  bitrate_kbps_ = bitrate_kbps_ + smoothing_factor * original_increase;
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
