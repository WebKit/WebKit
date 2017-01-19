/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/delay_based_bwe.h"

#include <algorithm>
#include <cmath>

#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/system_wrappers/include/field_trial.h"
#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/typedefs.h"

namespace {
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
constexpr int kInitialRateWindowMs = 500;
constexpr int kRateWindowMs = 150;

const char kBitrateEstimateExperiment[] = "WebRTC-ImprovedBitrateEstimate";

bool BitrateEstimateExperimentIsEnabled() {
  return webrtc::field_trial::FindFullName(kBitrateEstimateExperiment) ==
         "Enabled";
}
}  // namespace

namespace webrtc {
DelayBasedBwe::BitrateEstimator::BitrateEstimator()
    : sum_(0),
      current_win_ms_(0),
      prev_time_ms_(-1),
      bitrate_estimate_(-1.0f),
      bitrate_estimate_var_(50.0f),
      old_estimator_(kBitrateWindowMs, 8000),
      in_experiment_(BitrateEstimateExperimentIsEnabled()) {}

void DelayBasedBwe::BitrateEstimator::Update(int64_t now_ms, int bytes) {
  if (!in_experiment_) {
    old_estimator_.Update(bytes, now_ms);
    rtc::Optional<uint32_t> rate = old_estimator_.Rate(now_ms);
    bitrate_estimate_ = -1.0f;
    if (rate)
      bitrate_estimate_ = *rate / 1000.0f;
    return;
  }
  int rate_window_ms = kRateWindowMs;
  // We use a larger window at the beginning to get a more stable sample that
  // we can use to initialize the estimate.
  if (bitrate_estimate_ < 0.f)
    rate_window_ms = kInitialRateWindowMs;
  float bitrate_sample = UpdateWindow(now_ms, bytes, rate_window_ms);
  if (bitrate_sample < 0.0f)
    return;
  if (bitrate_estimate_ < 0.0f) {
    // This is the very first sample we get. Use it to initialize the estimate.
    bitrate_estimate_ = bitrate_sample;
    return;
  }
  // Define the sample uncertainty as a function of how far away it is from the
  // current estimate.
  float sample_uncertainty =
      10.0f * std::abs(bitrate_estimate_ - bitrate_sample) / bitrate_estimate_;
  float sample_var = sample_uncertainty * sample_uncertainty;
  // Update a bayesian estimate of the rate, weighting it lower if the sample
  // uncertainty is large.
  // The bitrate estimate uncertainty is increased with each update to model
  // that the bitrate changes over time.
  float pred_bitrate_estimate_var = bitrate_estimate_var_ + 5.f;
  bitrate_estimate_ = (sample_var * bitrate_estimate_ +
                       pred_bitrate_estimate_var * bitrate_sample) /
                      (sample_var + pred_bitrate_estimate_var);
  bitrate_estimate_var_ = sample_var * pred_bitrate_estimate_var /
                          (sample_var + pred_bitrate_estimate_var);
}

float DelayBasedBwe::BitrateEstimator::UpdateWindow(int64_t now_ms,
                                                    int bytes,
                                                    int rate_window_ms) {
  // Reset if time moves backwards.
  if (now_ms < prev_time_ms_) {
    prev_time_ms_ = -1;
    sum_ = 0;
    current_win_ms_ = 0;
  }
  if (prev_time_ms_ >= 0) {
    current_win_ms_ += now_ms - prev_time_ms_;
    // Reset if nothing has been received for more than a full window.
    if (now_ms - prev_time_ms_ > rate_window_ms) {
      sum_ = 0;
      current_win_ms_ %= rate_window_ms;
    }
  }
  prev_time_ms_ = now_ms;
  float bitrate_sample = -1.0f;
  if (current_win_ms_ >= rate_window_ms) {
    bitrate_sample = 8.0f * sum_ / static_cast<float>(rate_window_ms);
    current_win_ms_ -= rate_window_ms;
    sum_ = 0;
  }
  sum_ += bytes;
  return bitrate_sample;
}

rtc::Optional<uint32_t> DelayBasedBwe::BitrateEstimator::bitrate_bps() const {
  if (bitrate_estimate_ < 0.f)
    return rtc::Optional<uint32_t>();
  return rtc::Optional<uint32_t>(bitrate_estimate_ * 1000);
}

DelayBasedBwe::DelayBasedBwe(Clock* clock)
    : clock_(clock),
      inter_arrival_(),
      estimator_(),
      detector_(OverUseDetectorOptions()),
      receiver_incoming_bitrate_(),
      last_update_ms_(-1),
      last_seen_packet_ms_(-1),
      uma_recorded_(false) {
  network_thread_.DetachFromThread();
}

DelayBasedBwe::Result DelayBasedBwe::IncomingPacketFeedbackVector(
    const std::vector<PacketInfo>& packet_feedback_vector) {
  RTC_DCHECK(network_thread_.CalledOnValidThread());
  if (!uma_recorded_) {
    RTC_HISTOGRAM_ENUMERATION(kBweTypeHistogram,
                              BweNames::kSendSideTransportSeqNum,
                              BweNames::kBweNamesMax);
    uma_recorded_ = true;
  }
  Result aggregated_result;
  for (const auto& packet_info : packet_feedback_vector) {
    Result result = IncomingPacketInfo(packet_info);
    if (result.updated)
      aggregated_result = result;
  }
  return aggregated_result;
}

DelayBasedBwe::Result DelayBasedBwe::IncomingPacketInfo(
    const PacketInfo& info) {
  int64_t now_ms = clock_->TimeInMilliseconds();

  receiver_incoming_bitrate_.Update(info.arrival_time_ms, info.payload_size);
  Result result;
  // Reset if the stream has timed out.
  if (last_seen_packet_ms_ == -1 ||
      now_ms - last_seen_packet_ms_ > kStreamTimeOutMs) {
    inter_arrival_.reset(
        new InterArrival((kTimestampGroupLengthMs << kInterArrivalShift) / 1000,
                         kTimestampToMs, true));
    estimator_.reset(new OveruseEstimator(OverUseDetectorOptions()));
  }
  last_seen_packet_ms_ = now_ms;

  uint32_t send_time_24bits =
      static_cast<uint32_t>(
          ((static_cast<uint64_t>(info.send_time_ms) << kAbsSendTimeFraction) +
           500) /
          1000) &
      0x00FFFFFF;
  // Shift up send time to use the full 32 bits that inter_arrival works with,
  // so wrapping works properly.
  uint32_t timestamp = send_time_24bits << kAbsSendTimeInterArrivalUpshift;

  uint32_t ts_delta = 0;
  int64_t t_delta = 0;
  int size_delta = 0;
  if (inter_arrival_->ComputeDeltas(timestamp, info.arrival_time_ms, now_ms,
                                    info.payload_size, &ts_delta, &t_delta,
                                    &size_delta)) {
    double ts_delta_ms = (1000.0 * ts_delta) / (1 << kInterArrivalShift);
    estimator_->Update(t_delta, ts_delta_ms, size_delta, detector_.State(),
                       info.arrival_time_ms);
    detector_.Detect(estimator_->offset(), ts_delta_ms,
                     estimator_->num_of_deltas(), info.arrival_time_ms);
  }

  int probing_bps = 0;
  if (info.probe_cluster_id != PacketInfo::kNotAProbe) {
    probing_bps = probe_bitrate_estimator_.HandleProbeAndEstimateBitrate(info);
  }
  rtc::Optional<uint32_t> acked_bitrate_bps =
      receiver_incoming_bitrate_.bitrate_bps();
  // Currently overusing the bandwidth.
  if (detector_.State() == kBwOverusing) {
    if (acked_bitrate_bps &&
        rate_control_.TimeToReduceFurther(now_ms, *acked_bitrate_bps)) {
      result.updated =
          UpdateEstimate(info.arrival_time_ms, now_ms, acked_bitrate_bps,
                         &result.target_bitrate_bps);
    }
  } else if (probing_bps > 0) {
    // No overuse, but probing measured a bitrate.
    rate_control_.SetEstimate(probing_bps, info.arrival_time_ms);
    result.probe = true;
    result.updated =
        UpdateEstimate(info.arrival_time_ms, now_ms, acked_bitrate_bps,
                       &result.target_bitrate_bps);
  }
  if (!result.updated &&
      (last_update_ms_ == -1 ||
       now_ms - last_update_ms_ > rate_control_.GetFeedbackInterval())) {
    result.updated =
        UpdateEstimate(info.arrival_time_ms, now_ms, acked_bitrate_bps,
                       &result.target_bitrate_bps);
  }
  if (result.updated)
    last_update_ms_ = now_ms;

  return result;
}

bool DelayBasedBwe::UpdateEstimate(int64_t arrival_time_ms,
                                   int64_t now_ms,
                                   rtc::Optional<uint32_t> acked_bitrate_bps,
                                   uint32_t* target_bitrate_bps) {
  const RateControlInput input(detector_.State(), acked_bitrate_bps,
                               estimator_->var_noise());
  rate_control_.Update(&input, now_ms);
  *target_bitrate_bps = rate_control_.UpdateBandwidthEstimate(now_ms);
  return rate_control_.ValidEstimate();
}

void DelayBasedBwe::OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
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

void DelayBasedBwe::SetMinBitrate(int min_bitrate_bps) {
  // Called from both the configuration thread and the network thread. Shouldn't
  // be called from the network thread in the future.
  rate_control_.SetMinBitrate(min_bitrate_bps);
}
}  // namespace webrtc
