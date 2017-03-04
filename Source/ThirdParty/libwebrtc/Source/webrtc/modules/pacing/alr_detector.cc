/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/pacing/alr_detector.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace {

// Time period over which outgoing traffic is measured.
constexpr int kMeasurementPeriodMs = 500;

// Sent traffic percentage as a function of network capacity used to determine
// application-limited region. ALR region start when bandwidth usage drops below
// kAlrStartUsagePercent and ends when it raises above kAlrEndUsagePercent.
// NOTE: This is intentionally conservative at the moment until BW adjustments
// of application limited region is fine tuned.
constexpr int kAlrStartUsagePercent = 60;
constexpr int kAlrEndUsagePercent = 70;

}  // namespace

namespace webrtc {

AlrDetector::AlrDetector()
    : rate_(kMeasurementPeriodMs, RateStatistics::kBpsScale) {}

AlrDetector::~AlrDetector() {}

void AlrDetector::OnBytesSent(size_t bytes_sent, int64_t now_ms) {
  RTC_DCHECK(estimated_bitrate_bps_);

  rate_.Update(bytes_sent, now_ms);
  rtc::Optional<uint32_t> rate = rate_.Rate(now_ms);
  if (!rate)
    return;

  int percentage = static_cast<int>(*rate) * 100 / estimated_bitrate_bps_;
  if (percentage < kAlrStartUsagePercent && !alr_started_time_ms_) {
    alr_started_time_ms_ = rtc::Optional<int64_t>(now_ms);
  } else if (percentage > kAlrEndUsagePercent && alr_started_time_ms_) {
    alr_started_time_ms_ = rtc::Optional<int64_t>();
  }
}

void AlrDetector::SetEstimatedBitrate(int bitrate_bps) {
  RTC_DCHECK(bitrate_bps);
  estimated_bitrate_bps_ = bitrate_bps;
}

rtc::Optional<int64_t> AlrDetector::GetApplicationLimitedRegionStartTime()
    const {
  return alr_started_time_ms_;
}

}  // namespace webrtc
