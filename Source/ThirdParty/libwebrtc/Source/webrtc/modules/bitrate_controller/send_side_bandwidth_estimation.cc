/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/bitrate_controller/send_side_bandwidth_estimation.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <string>

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/events/rtc_event.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {
constexpr TimeDelta kBweIncreaseInterval = TimeDelta::Millis<1000>();
constexpr TimeDelta kBweDecreaseInterval = TimeDelta::Millis<300>();
constexpr TimeDelta kStartPhase = TimeDelta::Millis<2000>();
constexpr TimeDelta kBweConverganceTime = TimeDelta::Millis<20000>();
constexpr int kLimitNumPackets = 20;
constexpr DataRate kDefaultMaxBitrate = DataRate::BitsPerSec<1000000000>();
constexpr TimeDelta kLowBitrateLogPeriod = TimeDelta::Millis<10000>();
constexpr TimeDelta kRtcEventLogPeriod = TimeDelta::Millis<5000>();
// Expecting that RTCP feedback is sent uniformly within [0.5, 1.5]s intervals.
constexpr TimeDelta kMaxRtcpFeedbackInterval = TimeDelta::Millis<5000>();
constexpr int kFeedbackTimeoutIntervals = 3;
constexpr TimeDelta kTimeoutInterval = TimeDelta::Millis<1000>();

constexpr float kDefaultLowLossThreshold = 0.02f;
constexpr float kDefaultHighLossThreshold = 0.1f;
constexpr DataRate kDefaultBitrateThreshold = DataRate::Zero();

struct UmaRampUpMetric {
  const char* metric_name;
  int bitrate_kbps;
};

const UmaRampUpMetric kUmaRampupMetrics[] = {
    {"WebRTC.BWE.RampUpTimeTo500kbpsInMs", 500},
    {"WebRTC.BWE.RampUpTimeTo1000kbpsInMs", 1000},
    {"WebRTC.BWE.RampUpTimeTo2000kbpsInMs", 2000}};
const size_t kNumUmaRampupMetrics =
    sizeof(kUmaRampupMetrics) / sizeof(kUmaRampupMetrics[0]);

const char kBweLosExperiment[] = "WebRTC-BweLossExperiment";

bool BweLossExperimentIsEnabled() {
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kBweLosExperiment);
  // The experiment is enabled iff the field trial string begins with "Enabled".
  return experiment_string.find("Enabled") == 0;
}

bool ReadBweLossExperimentParameters(float* low_loss_threshold,
                                     float* high_loss_threshold,
                                     uint32_t* bitrate_threshold_kbps) {
  RTC_DCHECK(low_loss_threshold);
  RTC_DCHECK(high_loss_threshold);
  RTC_DCHECK(bitrate_threshold_kbps);
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kBweLosExperiment);
  int parsed_values =
      sscanf(experiment_string.c_str(), "Enabled-%f,%f,%u", low_loss_threshold,
             high_loss_threshold, bitrate_threshold_kbps);
  if (parsed_values == 3) {
    RTC_CHECK_GT(*low_loss_threshold, 0.0f)
        << "Loss threshold must be greater than 0.";
    RTC_CHECK_LE(*low_loss_threshold, 1.0f)
        << "Loss threshold must be less than or equal to 1.";
    RTC_CHECK_GT(*high_loss_threshold, 0.0f)
        << "Loss threshold must be greater than 0.";
    RTC_CHECK_LE(*high_loss_threshold, 1.0f)
        << "Loss threshold must be less than or equal to 1.";
    RTC_CHECK_LE(*low_loss_threshold, *high_loss_threshold)
        << "The low loss threshold must be less than or equal to the high loss "
           "threshold.";
    RTC_CHECK_GE(*bitrate_threshold_kbps, 0)
        << "Bitrate threshold can't be negative.";
    RTC_CHECK_LT(*bitrate_threshold_kbps,
                 std::numeric_limits<int>::max() / 1000)
        << "Bitrate must be smaller enough to avoid overflows.";
    return true;
  }
  RTC_LOG(LS_WARNING) << "Failed to parse parameters for BweLossExperiment "
                         "experiment from field trial string. Using default.";
  *low_loss_threshold = kDefaultLowLossThreshold;
  *high_loss_threshold = kDefaultHighLossThreshold;
  *bitrate_threshold_kbps = kDefaultBitrateThreshold.kbps();
  return false;
}
}  // namespace

LinkCapacityTracker::LinkCapacityTracker()
    : tracking_rate("rate", TimeDelta::seconds(10)) {
  ParseFieldTrial({&tracking_rate},
                  field_trial::FindFullName("WebRTC-Bwe-LinkCapacity"));
}

LinkCapacityTracker::~LinkCapacityTracker() {}

void LinkCapacityTracker::OnOveruse(DataRate acknowledged_rate,
                                    Timestamp at_time) {
  capacity_estimate_bps_ =
      std::min(capacity_estimate_bps_, acknowledged_rate.bps<double>());
  last_link_capacity_update_ = at_time;
}

void LinkCapacityTracker::OnStartingRate(DataRate start_rate) {
  if (last_link_capacity_update_.IsInfinite())
    capacity_estimate_bps_ = start_rate.bps<double>();
}

void LinkCapacityTracker::OnRateUpdate(DataRate acknowledged,
                                       Timestamp at_time) {
  if (acknowledged.bps() > capacity_estimate_bps_) {
    TimeDelta delta = at_time - last_link_capacity_update_;
    double alpha = delta.IsFinite() ? exp(-(delta / tracking_rate.Get())) : 0;
    capacity_estimate_bps_ = alpha * capacity_estimate_bps_ +
                             (1 - alpha) * acknowledged.bps<double>();
  }
  last_link_capacity_update_ = at_time;
}

void LinkCapacityTracker::OnRttBackoff(DataRate backoff_rate,
                                       Timestamp at_time) {
  capacity_estimate_bps_ =
      std::min(capacity_estimate_bps_, backoff_rate.bps<double>());
  last_link_capacity_update_ = at_time;
}

DataRate LinkCapacityTracker::estimate() const {
  return DataRate::bps(capacity_estimate_bps_);
}

RttBasedBackoff::RttBasedBackoff()
    : rtt_limit_("limit", TimeDelta::PlusInfinity()),
      drop_fraction_("fraction", 0.5),
      drop_interval_("interval", TimeDelta::ms(300)),
      persist_on_route_change_("persist"),
      // By initializing this to plus infinity, we make sure that we never
      // trigger rtt backoff unless packet feedback is enabled.
      last_propagation_rtt_update_(Timestamp::PlusInfinity()),
      last_propagation_rtt_(TimeDelta::Zero()) {
  ParseFieldTrial({&rtt_limit_, &drop_fraction_, &drop_interval_,
                   &persist_on_route_change_},
                  field_trial::FindFullName("WebRTC-Bwe-MaxRttLimit"));
}

void RttBasedBackoff::OnRouteChange() {
  if (!persist_on_route_change_) {
    last_propagation_rtt_update_ = Timestamp::PlusInfinity();
    last_propagation_rtt_ = TimeDelta::Zero();
  }
}

void RttBasedBackoff::UpdatePropagationRtt(Timestamp at_time,
                                           TimeDelta propagation_rtt) {
  last_propagation_rtt_update_ = at_time;
  last_propagation_rtt_ = propagation_rtt;
}

TimeDelta RttBasedBackoff::RttLowerBound(Timestamp at_time) const {
  // TODO(srte): Use time since last unacknowledged packet for this.
  TimeDelta time_since_rtt = at_time - last_propagation_rtt_update_;
  return time_since_rtt + last_propagation_rtt_;
}

RttBasedBackoff::~RttBasedBackoff() = default;

SendSideBandwidthEstimation::SendSideBandwidthEstimation(RtcEventLog* event_log)
    : lost_packets_since_last_loss_update_(0),
      expected_packets_since_last_loss_update_(0),
      current_bitrate_(DataRate::Zero()),
      min_bitrate_configured_(
          DataRate::bps(congestion_controller::GetMinBitrateBps())),
      max_bitrate_configured_(kDefaultMaxBitrate),
      last_low_bitrate_log_(Timestamp::MinusInfinity()),
      has_decreased_since_last_fraction_loss_(false),
      last_loss_feedback_(Timestamp::MinusInfinity()),
      last_loss_packet_report_(Timestamp::MinusInfinity()),
      last_timeout_(Timestamp::MinusInfinity()),
      last_fraction_loss_(0),
      last_logged_fraction_loss_(0),
      last_round_trip_time_(TimeDelta::Zero()),
      bwe_incoming_(DataRate::Zero()),
      delay_based_bitrate_(DataRate::Zero()),
      time_last_decrease_(Timestamp::MinusInfinity()),
      first_report_time_(Timestamp::MinusInfinity()),
      initially_lost_packets_(0),
      bitrate_at_2_seconds_(DataRate::Zero()),
      uma_update_state_(kNoUpdate),
      uma_rtt_state_(kNoUpdate),
      rampup_uma_stats_updated_(kNumUmaRampupMetrics, false),
      event_log_(event_log),
      last_rtc_event_log_(Timestamp::MinusInfinity()),
      in_timeout_experiment_(
          webrtc::field_trial::IsEnabled("WebRTC-FeedbackTimeout")),
      low_loss_threshold_(kDefaultLowLossThreshold),
      high_loss_threshold_(kDefaultHighLossThreshold),
      bitrate_threshold_(kDefaultBitrateThreshold) {
  RTC_DCHECK(event_log);
  if (BweLossExperimentIsEnabled()) {
    uint32_t bitrate_threshold_kbps;
    if (ReadBweLossExperimentParameters(&low_loss_threshold_,
                                        &high_loss_threshold_,
                                        &bitrate_threshold_kbps)) {
      RTC_LOG(LS_INFO) << "Enabled BweLossExperiment with parameters "
                       << low_loss_threshold_ << ", " << high_loss_threshold_
                       << ", " << bitrate_threshold_kbps;
      bitrate_threshold_ = DataRate::kbps(bitrate_threshold_kbps);
    }
  }
}

SendSideBandwidthEstimation::~SendSideBandwidthEstimation() {}

void SendSideBandwidthEstimation::OnRouteChange() {
  lost_packets_since_last_loss_update_ = 0;
  expected_packets_since_last_loss_update_ = 0;
  current_bitrate_ = DataRate::Zero();
  min_bitrate_configured_ =
      DataRate::bps(congestion_controller::GetMinBitrateBps());
  max_bitrate_configured_ = kDefaultMaxBitrate;
  last_low_bitrate_log_ = Timestamp::MinusInfinity();
  has_decreased_since_last_fraction_loss_ = false;
  last_loss_feedback_ = Timestamp::MinusInfinity();
  last_loss_packet_report_ = Timestamp::MinusInfinity();
  last_timeout_ = Timestamp::MinusInfinity();
  last_fraction_loss_ = 0;
  last_logged_fraction_loss_ = 0;
  last_round_trip_time_ = TimeDelta::Zero();
  bwe_incoming_ = DataRate::Zero();
  delay_based_bitrate_ = DataRate::Zero();
  time_last_decrease_ = Timestamp::MinusInfinity();
  first_report_time_ = Timestamp::MinusInfinity();
  initially_lost_packets_ = 0;
  bitrate_at_2_seconds_ = DataRate::Zero();
  uma_update_state_ = kNoUpdate;
  uma_rtt_state_ = kNoUpdate;
  last_rtc_event_log_ = Timestamp::MinusInfinity();

  rtt_backoff_.OnRouteChange();
}

void SendSideBandwidthEstimation::SetBitrates(
    absl::optional<DataRate> send_bitrate,
    DataRate min_bitrate,
    DataRate max_bitrate,
    Timestamp at_time) {
  SetMinMaxBitrate(min_bitrate, max_bitrate);
  if (send_bitrate) {
    link_capacity_.OnStartingRate(*send_bitrate);
    SetSendBitrate(*send_bitrate, at_time);
  }
}

void SendSideBandwidthEstimation::SetSendBitrate(DataRate bitrate,
                                                 Timestamp at_time) {
  RTC_DCHECK(bitrate > DataRate::Zero());
  // Reset to avoid being capped by the estimate.
  delay_based_bitrate_ = DataRate::Zero();
  if (loss_based_bandwidth_estimation_.Enabled()) {
    loss_based_bandwidth_estimation_.MaybeReset(bitrate);
  }
  CapBitrateToThresholds(at_time, bitrate);
  // Clear last sent bitrate history so the new value can be used directly
  // and not capped.
  min_bitrate_history_.clear();
}

void SendSideBandwidthEstimation::SetMinMaxBitrate(DataRate min_bitrate,
                                                   DataRate max_bitrate) {
  min_bitrate_configured_ =
      std::max(min_bitrate, congestion_controller::GetMinBitrate());
  if (max_bitrate > DataRate::Zero() && max_bitrate.IsFinite()) {
    max_bitrate_configured_ = std::max(min_bitrate_configured_, max_bitrate);
  } else {
    max_bitrate_configured_ = kDefaultMaxBitrate;
  }
}

int SendSideBandwidthEstimation::GetMinBitrate() const {
  return min_bitrate_configured_.bps<int>();
}

void SendSideBandwidthEstimation::CurrentEstimate(int* bitrate,
                                                  uint8_t* loss,
                                                  int64_t* rtt) const {
  *bitrate = current_bitrate_.bps<int>();
  *loss = last_fraction_loss_;
  *rtt = last_round_trip_time_.ms<int64_t>();
}

DataRate SendSideBandwidthEstimation::GetEstimatedLinkCapacity() const {
  return link_capacity_.estimate();
}

void SendSideBandwidthEstimation::UpdateReceiverEstimate(Timestamp at_time,
                                                         DataRate bandwidth) {
  bwe_incoming_ = bandwidth;
  CapBitrateToThresholds(at_time, current_bitrate_);
}

void SendSideBandwidthEstimation::UpdateDelayBasedEstimate(Timestamp at_time,
                                                           DataRate bitrate) {
  if (acknowledged_rate_) {
    if (bitrate < delay_based_bitrate_) {
      link_capacity_.OnOveruse(*acknowledged_rate_, at_time);
    }
  }
  delay_based_bitrate_ = bitrate;
  CapBitrateToThresholds(at_time, current_bitrate_);
}

void SendSideBandwidthEstimation::SetAcknowledgedRate(
    absl::optional<DataRate> acknowledged_rate,
    Timestamp at_time) {
  acknowledged_rate_ = acknowledged_rate;
  if (acknowledged_rate && loss_based_bandwidth_estimation_.Enabled()) {
    loss_based_bandwidth_estimation_.UpdateAcknowledgedBitrate(
        *acknowledged_rate, at_time);
  }
}

void SendSideBandwidthEstimation::IncomingPacketFeedbackVector(
    const TransportPacketsFeedback& report) {
  if (loss_based_bandwidth_estimation_.Enabled()) {
    loss_based_bandwidth_estimation_.UpdateLossStatistics(
        report.packet_feedbacks, report.feedback_time);
  }
}

void SendSideBandwidthEstimation::UpdateReceiverBlock(uint8_t fraction_loss,
                                                      TimeDelta rtt,
                                                      int number_of_packets,
                                                      Timestamp at_time) {
  const int kRoundingConstant = 128;
  int packets_lost = (static_cast<int>(fraction_loss) * number_of_packets +
                      kRoundingConstant) >>
                     8;
  UpdatePacketsLost(packets_lost, number_of_packets, at_time);
  UpdateRtt(rtt, at_time);
}

void SendSideBandwidthEstimation::UpdatePacketsLost(int packets_lost,
                                                    int number_of_packets,
                                                    Timestamp at_time) {
  last_loss_feedback_ = at_time;
  if (first_report_time_.IsInfinite())
    first_report_time_ = at_time;

  // Check sequence number diff and weight loss report
  if (number_of_packets > 0) {
    // Accumulate reports.
    lost_packets_since_last_loss_update_ += packets_lost;
    expected_packets_since_last_loss_update_ += number_of_packets;

    // Don't generate a loss rate until it can be based on enough packets.
    if (expected_packets_since_last_loss_update_ < kLimitNumPackets)
      return;

    has_decreased_since_last_fraction_loss_ = false;
    int64_t lost_q8 = lost_packets_since_last_loss_update_ << 8;
    int64_t expected = expected_packets_since_last_loss_update_;
    last_fraction_loss_ = std::min<int>(lost_q8 / expected, 255);

    // Reset accumulators.

    lost_packets_since_last_loss_update_ = 0;
    expected_packets_since_last_loss_update_ = 0;
    last_loss_packet_report_ = at_time;
    UpdateEstimate(at_time);
  }
  UpdateUmaStatsPacketsLost(at_time, packets_lost);
}

void SendSideBandwidthEstimation::UpdateUmaStatsPacketsLost(Timestamp at_time,
                                                            int packets_lost) {
  DataRate bitrate_kbps = DataRate::kbps((current_bitrate_.bps() + 500) / 1000);
  for (size_t i = 0; i < kNumUmaRampupMetrics; ++i) {
    if (!rampup_uma_stats_updated_[i] &&
        bitrate_kbps.kbps() >= kUmaRampupMetrics[i].bitrate_kbps) {
      RTC_HISTOGRAMS_COUNTS_100000(i, kUmaRampupMetrics[i].metric_name,
                                   (at_time - first_report_time_).ms());
      rampup_uma_stats_updated_[i] = true;
    }
  }
  if (IsInStartPhase(at_time)) {
    initially_lost_packets_ += packets_lost;
  } else if (uma_update_state_ == kNoUpdate) {
    uma_update_state_ = kFirstDone;
    bitrate_at_2_seconds_ = bitrate_kbps;
    RTC_HISTOGRAM_COUNTS("WebRTC.BWE.InitiallyLostPackets",
                         initially_lost_packets_, 0, 100, 50);
    RTC_HISTOGRAM_COUNTS("WebRTC.BWE.InitialBandwidthEstimate",
                         bitrate_at_2_seconds_.kbps(), 0, 2000, 50);
  } else if (uma_update_state_ == kFirstDone &&
             at_time - first_report_time_ >= kBweConverganceTime) {
    uma_update_state_ = kDone;
    int bitrate_diff_kbps = std::max(
        bitrate_at_2_seconds_.kbps<int>() - bitrate_kbps.kbps<int>(), 0);
    RTC_HISTOGRAM_COUNTS("WebRTC.BWE.InitialVsConvergedDiff", bitrate_diff_kbps,
                         0, 2000, 50);
  }
}

void SendSideBandwidthEstimation::UpdateRtt(TimeDelta rtt, Timestamp at_time) {
  // Update RTT if we were able to compute an RTT based on this RTCP.
  // FlexFEC doesn't send RTCP SR, which means we won't be able to compute RTT.
  if (rtt > TimeDelta::Zero())
    last_round_trip_time_ = rtt;

  if (!IsInStartPhase(at_time) && uma_rtt_state_ == kNoUpdate) {
    uma_rtt_state_ = kDone;
    RTC_HISTOGRAM_COUNTS("WebRTC.BWE.InitialRtt", rtt.ms<int>(), 0, 2000, 50);
  }
}

void SendSideBandwidthEstimation::UpdateEstimate(Timestamp at_time) {
  DataRate new_bitrate = current_bitrate_;
  if (rtt_backoff_.RttLowerBound(at_time) > rtt_backoff_.rtt_limit_) {
    if (at_time - time_last_decrease_ >= rtt_backoff_.drop_interval_) {
      time_last_decrease_ = at_time;
      new_bitrate = current_bitrate_ * rtt_backoff_.drop_fraction_;
      link_capacity_.OnRttBackoff(new_bitrate, at_time);
    }
    CapBitrateToThresholds(at_time, new_bitrate);
    return;
  }

  // We trust the REMB and/or delay-based estimate during the first 2 seconds if
  // we haven't had any packet loss reported, to allow startup bitrate probing.
  if (last_fraction_loss_ == 0 && IsInStartPhase(at_time)) {
    new_bitrate = std::max(bwe_incoming_, new_bitrate);
    new_bitrate = std::max(delay_based_bitrate_, new_bitrate);
    if (loss_based_bandwidth_estimation_.Enabled()) {
      loss_based_bandwidth_estimation_.SetInitialBitrate(new_bitrate);
    }

    if (new_bitrate != current_bitrate_) {
      min_bitrate_history_.clear();
      min_bitrate_history_.push_back(std::make_pair(at_time, current_bitrate_));
      CapBitrateToThresholds(at_time, new_bitrate);
      return;
    }
  }
  UpdateMinHistory(at_time);
  if (last_loss_packet_report_.IsInfinite()) {
    // No feedback received.
    CapBitrateToThresholds(at_time, current_bitrate_);
    return;
  }

  if (loss_based_bandwidth_estimation_.Enabled()) {
    loss_based_bandwidth_estimation_.Update(
        at_time, min_bitrate_history_.front().second, last_round_trip_time_);
    new_bitrate = MaybeRampupOrBackoff(new_bitrate, at_time);
    CapBitrateToThresholds(at_time, new_bitrate);
    return;
  }

  TimeDelta time_since_loss_packet_report = at_time - last_loss_packet_report_;
  TimeDelta time_since_loss_feedback = at_time - last_loss_feedback_;
  if (time_since_loss_packet_report < 1.2 * kMaxRtcpFeedbackInterval) {
    // We only care about loss above a given bitrate threshold.
    float loss = last_fraction_loss_ / 256.0f;
    // We only make decisions based on loss when the bitrate is above a
    // threshold. This is a crude way of handling loss which is uncorrelated
    // to congestion.
    if (current_bitrate_ < bitrate_threshold_ || loss <= low_loss_threshold_) {
      // Loss < 2%: Increase rate by 8% of the min bitrate in the last
      // kBweIncreaseInterval.
      // Note that by remembering the bitrate over the last second one can
      // rampup up one second faster than if only allowed to start ramping
      // at 8% per second rate now. E.g.:
      //   If sending a constant 100kbps it can rampup immediately to 108kbps
      //   whenever a receiver report is received with lower packet loss.
      //   If instead one would do: current_bitrate_ *= 1.08^(delta time),
      //   it would take over one second since the lower packet loss to achieve
      //   108kbps.
      new_bitrate =
          DataRate::bps(min_bitrate_history_.front().second.bps() * 1.08 + 0.5);

      // Add 1 kbps extra, just to make sure that we do not get stuck
      // (gives a little extra increase at low rates, negligible at higher
      // rates).
      new_bitrate += DataRate::bps(1000);
    } else if (current_bitrate_ > bitrate_threshold_) {
      if (loss <= high_loss_threshold_) {
        // Loss between 2% - 10%: Do nothing.
      } else {
        // Loss > 10%: Limit the rate decreases to once a kBweDecreaseInterval
        // + rtt.
        if (!has_decreased_since_last_fraction_loss_ &&
            (at_time - time_last_decrease_) >=
                (kBweDecreaseInterval + last_round_trip_time_)) {
          time_last_decrease_ = at_time;

          // Reduce rate:
          //   newRate = rate * (1 - 0.5*lossRate);
          //   where packetLoss = 256*lossRate;
          new_bitrate =
              DataRate::bps((current_bitrate_.bps() *
                             static_cast<double>(512 - last_fraction_loss_)) /
                            512.0);
          has_decreased_since_last_fraction_loss_ = true;
        }
      }
    }
  } else if (time_since_loss_feedback >
                 kFeedbackTimeoutIntervals * kMaxRtcpFeedbackInterval &&
             (last_timeout_.IsInfinite() ||
              at_time - last_timeout_ > kTimeoutInterval)) {
    if (in_timeout_experiment_) {
      RTC_LOG(LS_WARNING) << "Feedback timed out ("
                          << ToString(time_since_loss_feedback)
                          << "), reducing bitrate.";
      new_bitrate = new_bitrate * 0.8;
      // Reset accumulators since we've already acted on missing feedback and
      // shouldn't to act again on these old lost packets.
      lost_packets_since_last_loss_update_ = 0;
      expected_packets_since_last_loss_update_ = 0;
      last_timeout_ = at_time;
    }
  }

  CapBitrateToThresholds(at_time, new_bitrate);
}

void SendSideBandwidthEstimation::UpdatePropagationRtt(
    Timestamp at_time,
    TimeDelta propagation_rtt) {
  rtt_backoff_.UpdatePropagationRtt(at_time, propagation_rtt);
}

bool SendSideBandwidthEstimation::IsInStartPhase(Timestamp at_time) const {
  return first_report_time_.IsInfinite() ||
         at_time - first_report_time_ < kStartPhase;
}

void SendSideBandwidthEstimation::UpdateMinHistory(Timestamp at_time) {
  // Remove old data points from history.
  // Since history precision is in ms, add one so it is able to increase
  // bitrate if it is off by as little as 0.5ms.
  while (!min_bitrate_history_.empty() &&
         at_time - min_bitrate_history_.front().first + TimeDelta::ms(1) >
             kBweIncreaseInterval) {
    min_bitrate_history_.pop_front();
  }

  // Typical minimum sliding-window algorithm: Pop values higher than current
  // bitrate before pushing it.
  while (!min_bitrate_history_.empty() &&
         current_bitrate_ <= min_bitrate_history_.back().second) {
    min_bitrate_history_.pop_back();
  }

  min_bitrate_history_.push_back(std::make_pair(at_time, current_bitrate_));
}

DataRate SendSideBandwidthEstimation::MaybeRampupOrBackoff(DataRate new_bitrate,
                                                           Timestamp at_time) {
  // TODO(crodbro): reuse this code in UpdateEstimate instead of current
  // inlining of very similar functionality.
  const TimeDelta time_since_loss_packet_report =
      at_time - last_loss_packet_report_;
  const TimeDelta time_since_loss_feedback = at_time - last_loss_feedback_;
  if (time_since_loss_packet_report < 1.2 * kMaxRtcpFeedbackInterval) {
    new_bitrate = min_bitrate_history_.front().second * 1.08;
    new_bitrate += DataRate::bps(1000);
  } else if (time_since_loss_feedback >
                 kFeedbackTimeoutIntervals * kMaxRtcpFeedbackInterval &&
             (last_timeout_.IsInfinite() ||
              at_time - last_timeout_ > kTimeoutInterval)) {
    if (in_timeout_experiment_) {
      RTC_LOG(LS_WARNING) << "Feedback timed out ("
                          << ToString(time_since_loss_feedback)
                          << "), reducing bitrate.";
      new_bitrate = new_bitrate * 0.8;
      // Reset accumulators since we've already acted on missing feedback and
      // shouldn't to act again on these old lost packets.
      lost_packets_since_last_loss_update_ = 0;
      expected_packets_since_last_loss_update_ = 0;
      last_timeout_ = at_time;
    }
  }
  return new_bitrate;
}

void SendSideBandwidthEstimation::CapBitrateToThresholds(Timestamp at_time,
                                                         DataRate bitrate) {
  if (bwe_incoming_ > DataRate::Zero() && bitrate > bwe_incoming_) {
    bitrate = bwe_incoming_;
  }
  if (delay_based_bitrate_ > DataRate::Zero() &&
      bitrate > delay_based_bitrate_) {
    bitrate = delay_based_bitrate_;
  }
  if (loss_based_bandwidth_estimation_.Enabled() &&
      loss_based_bandwidth_estimation_.GetEstimate() > DataRate::Zero()) {
    bitrate = std::min(bitrate, loss_based_bandwidth_estimation_.GetEstimate());
  }
  if (bitrate > max_bitrate_configured_) {
    bitrate = max_bitrate_configured_;
  }
  if (bitrate < min_bitrate_configured_) {
    if (last_low_bitrate_log_.IsInfinite() ||
        at_time - last_low_bitrate_log_ > kLowBitrateLogPeriod) {
      RTC_LOG(LS_WARNING) << "Estimated available bandwidth "
                          << ToString(bitrate)
                          << " is below configured min bitrate "
                          << ToString(min_bitrate_configured_) << ".";
      last_low_bitrate_log_ = at_time;
    }
    bitrate = min_bitrate_configured_;
  }

  if (bitrate != current_bitrate_ ||
      last_fraction_loss_ != last_logged_fraction_loss_ ||
      at_time - last_rtc_event_log_ > kRtcEventLogPeriod) {
    event_log_->Log(absl::make_unique<RtcEventBweUpdateLossBased>(
        bitrate.bps(), last_fraction_loss_,
        expected_packets_since_last_loss_update_));
    last_logged_fraction_loss_ = last_fraction_loss_;
    last_rtc_event_log_ = at_time;
  }
  current_bitrate_ = bitrate;

  if (acknowledged_rate_) {
    link_capacity_.OnRateUpdate(std::min(current_bitrate_, *acknowledged_rate_),
                                at_time);
  }
}
}  // namespace webrtc
