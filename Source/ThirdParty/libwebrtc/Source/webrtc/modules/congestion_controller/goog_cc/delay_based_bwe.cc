/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/delay_based_bwe.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

#include "absl/memory/memory.h"
#include "api/transport/network_types.h"  // For PacedPacketInfo
#include "logging/rtc_event_log/events/rtc_event.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/congestion_controller/goog_cc/trendline_estimator.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {
constexpr TimeDelta kStreamTimeOut = TimeDelta::Seconds<2>();
constexpr int kTimestampGroupLengthMs = 5;
constexpr int kAbsSendTimeFraction = 18;
constexpr int kAbsSendTimeInterArrivalUpshift = 8;
constexpr int kInterArrivalShift =
    kAbsSendTimeFraction + kAbsSendTimeInterArrivalUpshift;
constexpr double kTimestampToMs =
    1000.0 / static_cast<double>(1 << kInterArrivalShift);
// This ssrc is used to fulfill the current API but will be removed
// after the API has been changed.
constexpr uint32_t kFixedSsrc = 0;

// Parameters for linear least squares fit of regression line to noisy data.
constexpr size_t kDefaultTrendlineWindowSize = 20;
constexpr double kDefaultTrendlineSmoothingCoeff = 0.9;
constexpr double kDefaultTrendlineThresholdGain = 4.0;

const char kBweWindowSizeInPacketsExperiment[] =
    "WebRTC-BweWindowSizeInPackets";

size_t ReadTrendlineFilterWindowSize() {
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kBweWindowSizeInPacketsExperiment);
  size_t window_size;
  int parsed_values =
      sscanf(experiment_string.c_str(), "Enabled-%zu", &window_size);
  if (parsed_values == 1) {
    if (window_size > 1)
      return window_size;
    RTC_LOG(WARNING) << "Window size must be greater than 1.";
  }
  RTC_LOG(LS_WARNING) << "Failed to parse parameters for BweWindowSizeInPackets"
                         " experiment from field trial string. Using default.";
  return kDefaultTrendlineWindowSize;
}
}  // namespace

DelayBasedBwe::Result::Result()
    : updated(false),
      probe(false),
      target_bitrate(DataRate::Zero()),
      recovered_from_overuse(false) {}

DelayBasedBwe::Result::Result(bool probe, DataRate target_bitrate)
    : updated(true),
      probe(probe),
      target_bitrate(target_bitrate),
      recovered_from_overuse(false) {}

DelayBasedBwe::Result::~Result() {}

DelayBasedBwe::DelayBasedBwe(RtcEventLog* event_log)
    : event_log_(event_log),
      inter_arrival_(),
      delay_detector_(),
      last_seen_packet_(Timestamp::MinusInfinity()),
      uma_recorded_(false),
      trendline_window_size_(
          webrtc::field_trial::IsEnabled(kBweWindowSizeInPacketsExperiment)
              ? ReadTrendlineFilterWindowSize()
              : kDefaultTrendlineWindowSize),
      trendline_smoothing_coeff_(kDefaultTrendlineSmoothingCoeff),
      trendline_threshold_gain_(kDefaultTrendlineThresholdGain),
      prev_bitrate_(DataRate::Zero()),
      prev_state_(BandwidthUsage::kBwNormal) {
  RTC_LOG(LS_INFO)
      << "Using Trendline filter for delay change estimation with window size "
      << trendline_window_size_;
  delay_detector_.reset(new TrendlineEstimator(trendline_window_size_,
                                               trendline_smoothing_coeff_,
                                               trendline_threshold_gain_));
}

DelayBasedBwe::~DelayBasedBwe() {}

DelayBasedBwe::Result DelayBasedBwe::IncomingPacketFeedbackVector(
    const std::vector<PacketFeedback>& packet_feedback_vector,
    absl::optional<DataRate> acked_bitrate,
    absl::optional<DataRate> probe_bitrate,
    Timestamp at_time) {
  RTC_DCHECK(std::is_sorted(packet_feedback_vector.begin(),
                            packet_feedback_vector.end(),
                            PacketFeedbackComparator()));
  RTC_DCHECK_RUNS_SERIALIZED(&network_race_);

  // TOOD(holmer): An empty feedback vector here likely means that
  // all acks were too late and that the send time history had
  // timed out. We should reduce the rate when this occurs.
  if (packet_feedback_vector.empty()) {
    RTC_LOG(LS_WARNING) << "Very late feedback received.";
    return DelayBasedBwe::Result();
  }

  if (!uma_recorded_) {
    RTC_HISTOGRAM_ENUMERATION(kBweTypeHistogram,
                              BweNames::kSendSideTransportSeqNum,
                              BweNames::kBweNamesMax);
    uma_recorded_ = true;
  }
  bool delayed_feedback = true;
  bool recovered_from_overuse = false;
  BandwidthUsage prev_detector_state = delay_detector_->State();
  for (const auto& packet_feedback : packet_feedback_vector) {
    if (packet_feedback.send_time_ms < 0)
      continue;
    delayed_feedback = false;
    IncomingPacketFeedback(packet_feedback, at_time);
    if (prev_detector_state == BandwidthUsage::kBwUnderusing &&
        delay_detector_->State() == BandwidthUsage::kBwNormal) {
      recovered_from_overuse = true;
    }
    prev_detector_state = delay_detector_->State();
  }

  if (delayed_feedback) {
    // TODO(bugs.webrtc.org/10125): Design a better mechanism to safe-guard
    // against building very large network queues.
    return Result();
  }
  return MaybeUpdateEstimate(acked_bitrate, probe_bitrate,
                             recovered_from_overuse, at_time);
}

void DelayBasedBwe::IncomingPacketFeedback(
    const PacketFeedback& packet_feedback,
    Timestamp at_time) {
  // Reset if the stream has timed out.
  if (last_seen_packet_.IsInfinite() ||
      at_time - last_seen_packet_ > kStreamTimeOut) {
    inter_arrival_.reset(
        new InterArrival((kTimestampGroupLengthMs << kInterArrivalShift) / 1000,
                         kTimestampToMs, true));
    delay_detector_.reset(new TrendlineEstimator(trendline_window_size_,
                                                 trendline_smoothing_coeff_,
                                                 trendline_threshold_gain_));
  }
  last_seen_packet_ = at_time;

  uint32_t send_time_24bits =
      static_cast<uint32_t>(
          ((static_cast<uint64_t>(packet_feedback.send_time_ms)
            << kAbsSendTimeFraction) +
           500) /
          1000) &
      0x00FFFFFF;
  // Shift up send time to use the full 32 bits that inter_arrival works with,
  // so wrapping works properly.
  uint32_t timestamp = send_time_24bits << kAbsSendTimeInterArrivalUpshift;

  uint32_t ts_delta = 0;
  int64_t t_delta = 0;
  int size_delta = 0;
  if (inter_arrival_->ComputeDeltas(timestamp, packet_feedback.arrival_time_ms,
                                    at_time.ms(), packet_feedback.payload_size,
                                    &ts_delta, &t_delta, &size_delta)) {
    double ts_delta_ms = (1000.0 * ts_delta) / (1 << kInterArrivalShift);
    delay_detector_->Update(t_delta, ts_delta_ms,
                            packet_feedback.arrival_time_ms);
  }
}

DelayBasedBwe::Result DelayBasedBwe::MaybeUpdateEstimate(
    absl::optional<DataRate> acked_bitrate,
    absl::optional<DataRate> probe_bitrate,
    bool recovered_from_overuse,
    Timestamp at_time) {
  Result result;

  // Currently overusing the bandwidth.
  if (delay_detector_->State() == BandwidthUsage::kBwOverusing) {
    if (acked_bitrate &&
        rate_control_.TimeToReduceFurther(at_time, *acked_bitrate)) {
      result.updated =
          UpdateEstimate(at_time, acked_bitrate, &result.target_bitrate);
    } else if (!acked_bitrate && rate_control_.ValidEstimate() &&
               rate_control_.InitialTimeToReduceFurther(at_time)) {
      // Overusing before we have a measured acknowledged bitrate. Reduce send
      // rate by 50% every 200 ms.
      // TODO(tschumim): Improve this and/or the acknowledged bitrate estimator
      // so that we (almost) always have a bitrate estimate.
      rate_control_.SetEstimate(rate_control_.LatestEstimate() / 2, at_time);
      result.updated = true;
      result.probe = false;
      result.target_bitrate = rate_control_.LatestEstimate();
    }
  } else {
    if (probe_bitrate) {
      result.probe = true;
      result.updated = true;
      result.target_bitrate = *probe_bitrate;
      rate_control_.SetEstimate(*probe_bitrate, at_time);
    } else {
      result.updated =
          UpdateEstimate(at_time, acked_bitrate, &result.target_bitrate);
      result.recovered_from_overuse = recovered_from_overuse;
    }
  }
  BandwidthUsage detector_state = delay_detector_->State();
  if ((result.updated && prev_bitrate_ != result.target_bitrate) ||
      detector_state != prev_state_) {
    DataRate bitrate = result.updated ? result.target_bitrate : prev_bitrate_;

    BWE_TEST_LOGGING_PLOT(1, "target_bitrate_bps", at_time.ms(), bitrate.bps());

    if (event_log_) {
      event_log_->Log(absl::make_unique<RtcEventBweUpdateDelayBased>(
          bitrate.bps(), detector_state));
    }

    prev_bitrate_ = bitrate;
    prev_state_ = detector_state;
  }
  return result;
}

bool DelayBasedBwe::UpdateEstimate(Timestamp at_time,
                                   absl::optional<DataRate> acked_bitrate,
                                   DataRate* target_rate) {
  const RateControlInput input(delay_detector_->State(), acked_bitrate);
  *target_rate = rate_control_.Update(&input, at_time);
  return rate_control_.ValidEstimate();
}

void DelayBasedBwe::OnRttUpdate(TimeDelta avg_rtt) {
  rate_control_.SetRtt(avg_rtt);
}

bool DelayBasedBwe::LatestEstimate(std::vector<uint32_t>* ssrcs,
                                   DataRate* bitrate) const {
  // Currently accessed from both the process thread (see
  // ModuleRtpRtcpImpl::Process()) and the configuration thread (see
  // Call::GetStats()). Should in the future only be accessed from a single
  // thread.
  RTC_DCHECK(ssrcs);
  RTC_DCHECK(bitrate);
  if (!rate_control_.ValidEstimate())
    return false;

  *ssrcs = {kFixedSsrc};
  *bitrate = rate_control_.LatestEstimate();
  return true;
}

void DelayBasedBwe::SetStartBitrate(DataRate start_bitrate) {
  RTC_LOG(LS_INFO) << "BWE Setting start bitrate to: "
                   << ToString(start_bitrate);
  rate_control_.SetStartBitrate(start_bitrate);
}

void DelayBasedBwe::SetMinBitrate(DataRate min_bitrate) {
  // Called from both the configuration thread and the network thread. Shouldn't
  // be called from the network thread in the future.
  rate_control_.SetMinBitrate(min_bitrate);
}

TimeDelta DelayBasedBwe::GetExpectedBwePeriod() const {
  return rate_control_.GetExpectedBandwidthPeriod();
}
}  // namespace webrtc
