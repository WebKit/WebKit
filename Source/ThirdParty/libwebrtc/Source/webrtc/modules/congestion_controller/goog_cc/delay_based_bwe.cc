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
#include <cmath>
#include <cstdio>
#include <string>

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/congestion_controller/goog_cc/trendline_estimator.h"
#include "modules/pacing/paced_sender.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

namespace {
static const int64_t kStreamTimeOutMs = 2000;
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

constexpr int kMaxConsecutiveFailedLookups = 5;

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
  RTC_LOG(LS_WARNING) << "Failed to parse parameters for BweTrendlineFilter "
                         "experiment from field trial string. Using default.";
  return kDefaultTrendlineWindowSize;
}
}  // namespace

namespace webrtc {

DelayBasedBwe::Result::Result()
    : updated(false),
      probe(false),
      target_bitrate_bps(0),
      recovered_from_overuse(false) {}

DelayBasedBwe::Result::Result(bool probe, uint32_t target_bitrate_bps)
    : updated(true),
      probe(probe),
      target_bitrate_bps(target_bitrate_bps),
      recovered_from_overuse(false) {}

DelayBasedBwe::Result::~Result() {}

DelayBasedBwe::DelayBasedBwe(RtcEventLog* event_log)
    : event_log_(event_log),
      inter_arrival_(),
      delay_detector_(),
      last_seen_packet_ms_(-1),
      uma_recorded_(false),
      probe_bitrate_estimator_(event_log),
      trendline_window_size_(
          webrtc::field_trial::IsEnabled(kBweWindowSizeInPacketsExperiment)
              ? ReadTrendlineFilterWindowSize()
              : kDefaultTrendlineWindowSize),
      trendline_smoothing_coeff_(kDefaultTrendlineSmoothingCoeff),
      trendline_threshold_gain_(kDefaultTrendlineThresholdGain),
      consecutive_delayed_feedbacks_(0),
      prev_bitrate_(0),
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
    absl::optional<uint32_t> acked_bitrate_bps,
    int64_t at_time_ms) {
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
    IncomingPacketFeedback(packet_feedback, at_time_ms);
    if (prev_detector_state == BandwidthUsage::kBwUnderusing &&
        delay_detector_->State() == BandwidthUsage::kBwNormal) {
      recovered_from_overuse = true;
    }
    prev_detector_state = delay_detector_->State();
  }

  if (delayed_feedback) {
    ++consecutive_delayed_feedbacks_;
    if (consecutive_delayed_feedbacks_ >= kMaxConsecutiveFailedLookups) {
      consecutive_delayed_feedbacks_ = 0;
      return OnLongFeedbackDelay(packet_feedback_vector.back().arrival_time_ms);
    }
  } else {
    consecutive_delayed_feedbacks_ = 0;
    return MaybeUpdateEstimate(acked_bitrate_bps, recovered_from_overuse,
                               at_time_ms);
  }
  return Result();
}

DelayBasedBwe::Result DelayBasedBwe::OnLongFeedbackDelay(
    int64_t arrival_time_ms) {
  // Estimate should always be valid since a start bitrate always is set in the
  // Call constructor. An alternative would be to return an empty Result here,
  // or to estimate the throughput based on the feedback we received.
  RTC_DCHECK(rate_control_.ValidEstimate());
  rate_control_.SetEstimate(rate_control_.LatestEstimate() / 2,
                            arrival_time_ms);
  Result result;
  result.updated = true;
  result.probe = false;
  result.target_bitrate_bps = rate_control_.LatestEstimate();
  RTC_LOG(LS_WARNING) << "Long feedback delay detected, reducing BWE to "
                      << result.target_bitrate_bps;
  return result;
}

void DelayBasedBwe::IncomingPacketFeedback(
    const PacketFeedback& packet_feedback,
    int64_t at_time_ms) {
  int64_t now_ms = at_time_ms;
  // Reset if the stream has timed out.
  if (last_seen_packet_ms_ == -1 ||
      now_ms - last_seen_packet_ms_ > kStreamTimeOutMs) {
    inter_arrival_.reset(
        new InterArrival((kTimestampGroupLengthMs << kInterArrivalShift) / 1000,
                         kTimestampToMs, true));
    delay_detector_.reset(new TrendlineEstimator(trendline_window_size_,
                                                 trendline_smoothing_coeff_,
                                                 trendline_threshold_gain_));
  }
  last_seen_packet_ms_ = now_ms;

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
                                    now_ms, packet_feedback.payload_size,
                                    &ts_delta, &t_delta, &size_delta)) {
    double ts_delta_ms = (1000.0 * ts_delta) / (1 << kInterArrivalShift);
    delay_detector_->Update(t_delta, ts_delta_ms,
                            packet_feedback.arrival_time_ms);
  }
  if (packet_feedback.pacing_info.probe_cluster_id !=
      PacedPacketInfo::kNotAProbe) {
    probe_bitrate_estimator_.HandleProbeAndEstimateBitrate(packet_feedback);
  }
}

DelayBasedBwe::Result DelayBasedBwe::MaybeUpdateEstimate(
    absl::optional<uint32_t> acked_bitrate_bps,
    bool recovered_from_overuse,
    int64_t at_time_ms) {
  Result result;
  int64_t now_ms = at_time_ms;

  absl::optional<int> probe_bitrate_bps =
      probe_bitrate_estimator_.FetchAndResetLastEstimatedBitrateBps();
  // Currently overusing the bandwidth.
  if (delay_detector_->State() == BandwidthUsage::kBwOverusing) {
    if (acked_bitrate_bps &&
        rate_control_.TimeToReduceFurther(now_ms, *acked_bitrate_bps)) {
      result.updated =
          UpdateEstimate(now_ms, acked_bitrate_bps, &result.target_bitrate_bps);
    } else if (!acked_bitrate_bps && rate_control_.ValidEstimate() &&
               rate_control_.InitialTimeToReduceFurther(now_ms)) {
      // Overusing before we have a measured acknowledged bitrate. Reduce send
      // rate by 50% every 200 ms.
      // TODO(tschumim): Improve this and/or the acknowledged bitrate estimator
      // so that we (almost) always have a bitrate estimate.
      rate_control_.SetEstimate(rate_control_.LatestEstimate() / 2, now_ms);
      result.updated = true;
      result.probe = false;
      result.target_bitrate_bps = rate_control_.LatestEstimate();
    }
  } else {
    if (probe_bitrate_bps) {
      result.probe = true;
      result.updated = true;
      result.target_bitrate_bps = *probe_bitrate_bps;
      rate_control_.SetEstimate(*probe_bitrate_bps, now_ms);
    } else {
      result.updated =
          UpdateEstimate(now_ms, acked_bitrate_bps, &result.target_bitrate_bps);
      result.recovered_from_overuse = recovered_from_overuse;
    }
  }
  BandwidthUsage detector_state = delay_detector_->State();
  if ((result.updated && prev_bitrate_ != result.target_bitrate_bps) ||
      detector_state != prev_state_) {
    uint32_t bitrate_bps =
        result.updated ? result.target_bitrate_bps : prev_bitrate_;

    BWE_TEST_LOGGING_PLOT(1, "target_bitrate_bps", now_ms, bitrate_bps);

    if (event_log_) {
      event_log_->Log(absl::make_unique<RtcEventBweUpdateDelayBased>(
          bitrate_bps, detector_state));
    }

    prev_bitrate_ = bitrate_bps;
    prev_state_ = detector_state;
  }
  return result;
}

bool DelayBasedBwe::UpdateEstimate(int64_t now_ms,
                                   absl::optional<uint32_t> acked_bitrate_bps,
                                   uint32_t* target_bitrate_bps) {
  const RateControlInput input(delay_detector_->State(), acked_bitrate_bps);
  *target_bitrate_bps = rate_control_.Update(&input, now_ms);
  return rate_control_.ValidEstimate();
}

void DelayBasedBwe::OnRttUpdate(int64_t avg_rtt_ms) {
  rate_control_.SetRtt(avg_rtt_ms);
}

bool DelayBasedBwe::LatestEstimate(std::vector<uint32_t>* ssrcs,
                                   uint32_t* bitrate_bps) const {
  // Currently accessed from both the process thread (see
  // ModuleRtpRtcpImpl::Process()) and the configuration thread (see
  // Call::GetStats()). Should in the future only be accessed from a single
  // thread.
  RTC_DCHECK(ssrcs);
  RTC_DCHECK(bitrate_bps);
  if (!rate_control_.ValidEstimate())
    return false;

  *ssrcs = {kFixedSsrc};
  *bitrate_bps = rate_control_.LatestEstimate();
  return true;
}

void DelayBasedBwe::SetStartBitrate(int start_bitrate_bps) {
  RTC_LOG(LS_INFO) << "BWE Setting start bitrate to: " << start_bitrate_bps;
  rate_control_.SetStartBitrate(start_bitrate_bps);
}

void DelayBasedBwe::SetMinBitrate(int min_bitrate_bps) {
  // Called from both the configuration thread and the network thread. Shouldn't
  // be called from the network thread in the future.
  rate_control_.SetMinBitrate(min_bitrate_bps);
}

int64_t DelayBasedBwe::GetExpectedBwePeriodMs() const {
  return rate_control_.GetExpectedBandwidthPeriodMs();
}
}  // namespace webrtc
