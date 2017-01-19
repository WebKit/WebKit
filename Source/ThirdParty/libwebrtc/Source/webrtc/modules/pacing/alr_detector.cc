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

// Time period over which outgoing traffic is measured and considered a single
// data point.
constexpr int kMeasurementPeriodMs = 100;

// Minimum number of consecutive measurements over |kMeasurementPeriodMs| time
// that indicate sending rate is below |kUsagePercent| to consider being
// application limited.
constexpr int kApplicationLimitedThreshold = 5;

// Sent traffic percentage as a function of network capaicty to consider traffic
// as application limited.
// NOTE: This is intentionally conservative at the moment until BW adjustments
// of application limited region is fine tuned.
constexpr int kUsagePercent = 30;

}  // namespace

namespace webrtc {

AlrDetector::AlrDetector()
    : measurement_interval_bytes_sent_(0),
      measurement_interval_elapsed_time_ms_(0),
      estimated_bitrate_bps_(0),
      application_limited_count_(0) {}

AlrDetector::~AlrDetector() {}

void AlrDetector::OnBytesSent(size_t bytes_sent, int64_t elapsed_time_ms) {
  if (measurement_interval_elapsed_time_ms_ > kMeasurementPeriodMs) {
    RTC_DCHECK(estimated_bitrate_bps_);
    int max_bytes =
        (measurement_interval_elapsed_time_ms_ * estimated_bitrate_bps_) /
        (8 * 1000);
    RTC_DCHECK_GT(max_bytes, 0);
    int utilization =
        static_cast<int>((measurement_interval_bytes_sent_ * 100) / max_bytes);
    if (utilization < kUsagePercent) {
      application_limited_count_++;
      if (application_limited_count_ == kApplicationLimitedThreshold)
        LOG(LS_INFO) << "ALR start";
    } else {
      if (application_limited_count_ >= kApplicationLimitedThreshold)
        LOG(LS_INFO) << "ALR stop";
      application_limited_count_ = 0;
    }
    measurement_interval_elapsed_time_ms_ = elapsed_time_ms;
    measurement_interval_bytes_sent_ = bytes_sent;
  } else {
    measurement_interval_elapsed_time_ms_ += elapsed_time_ms;
    measurement_interval_bytes_sent_ += bytes_sent;
  }
}

void AlrDetector::SetEstimatedBitrate(int bitrate_bps) {
  RTC_DCHECK(bitrate_bps);
  estimated_bitrate_bps_ = bitrate_bps;
}

bool AlrDetector::InApplicationLimitedRegion() {
  return application_limited_count_ >= kApplicationLimitedThreshold;
}

}  // namespace webrtc
