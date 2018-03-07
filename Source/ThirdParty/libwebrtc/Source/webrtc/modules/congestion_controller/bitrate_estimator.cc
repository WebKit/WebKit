/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/bitrate_estimator.h"

#include <cmath>

#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

namespace {
constexpr int kInitialRateWindowMs = 500;
constexpr int kRateWindowMs = 150;
}  // namespace

BitrateEstimator::BitrateEstimator()
    : sum_(0),
      current_win_ms_(0),
      prev_time_ms_(-1),
      bitrate_estimate_(-1.0f),
      bitrate_estimate_var_(50.0f) {}

BitrateEstimator::~BitrateEstimator() = default;

void BitrateEstimator::Update(int64_t now_ms, int bytes) {
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
  BWE_TEST_LOGGING_PLOT(1, "acknowledged_bitrate", now_ms,
                        bitrate_estimate_ * 1000);
}

float BitrateEstimator::UpdateWindow(int64_t now_ms,
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

rtc::Optional<uint32_t> BitrateEstimator::bitrate_bps() const {
  if (bitrate_estimate_ < 0.f)
    return rtc::nullopt;
  return bitrate_estimate_ * 1000;
}

void BitrateEstimator::ExpectFastRateChange() {
  // By setting the bitrate-estimate variance to a higher value we allow the
  // bitrate to change fast for the next few samples.
  bitrate_estimate_var_ += 200;
}

}  // namespace webrtc
