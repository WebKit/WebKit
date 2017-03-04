/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/include/rtp_to_ntp_estimator.h"

#include "webrtc/base/logging.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {
namespace {
// Number of RTCP SR reports to use to map between RTP and NTP.
const size_t kNumRtcpReportsToUse = 2;

// Calculates the RTP timestamp frequency from two pairs of NTP/RTP timestamps.
bool CalculateFrequency(int64_t ntp_ms1,
                        uint32_t rtp_timestamp1,
                        int64_t ntp_ms2,
                        uint32_t rtp_timestamp2,
                        double* frequency_khz) {
  if (ntp_ms1 <= ntp_ms2)
    return false;

  *frequency_khz = static_cast<double>(rtp_timestamp1 - rtp_timestamp2) /
                   static_cast<double>(ntp_ms1 - ntp_ms2);
  return true;
}

// Detects if there has been a wraparound between |old_timestamp| and
// |new_timestamp|, and compensates by adding 2^32 if that is the case.
bool CompensateForWrapAround(uint32_t new_timestamp,
                             uint32_t old_timestamp,
                             int64_t* compensated_timestamp) {
  int64_t wraps = CheckForWrapArounds(new_timestamp, old_timestamp);
  if (wraps < 0) {
    // Reordering, don't use this packet.
    return false;
  }
  *compensated_timestamp = new_timestamp + (wraps << 32);
  return true;
}

bool Contains(const std::list<RtpToNtpEstimator::RtcpMeasurement>& measurements,
              const RtpToNtpEstimator::RtcpMeasurement& other) {
  for (const auto& measurement : measurements) {
    if (measurement.IsEqual(other))
      return true;
  }
  return false;
}

bool IsValid(const std::list<RtpToNtpEstimator::RtcpMeasurement>& measurements,
             const RtpToNtpEstimator::RtcpMeasurement& other) {
  if (!other.ntp_time.Valid())
    return false;

  int64_t ntp_ms_new = other.ntp_time.ToMs();
  for (const auto& measurement : measurements) {
    if (ntp_ms_new <= measurement.ntp_time.ToMs()) {
      // Old report.
      return false;
    }
    int64_t timestamp_new = other.rtp_timestamp;
    if (!CompensateForWrapAround(timestamp_new, measurement.rtp_timestamp,
                                 &timestamp_new)) {
      return false;
    }
    if (timestamp_new <= measurement.rtp_timestamp) {
      LOG(LS_WARNING) << "Newer RTCP SR report with older RTP timestamp.";
      return false;
    }
  }
  return true;
}
}  // namespace

RtpToNtpEstimator::RtcpMeasurement::RtcpMeasurement(uint32_t ntp_secs,
                                                    uint32_t ntp_frac,
                                                    uint32_t timestamp)
    : ntp_time(ntp_secs, ntp_frac), rtp_timestamp(timestamp) {}

bool RtpToNtpEstimator::RtcpMeasurement::IsEqual(
    const RtcpMeasurement& other) const {
  // Use || since two equal timestamps will result in zero frequency and in
  // RtpToNtpMs, |rtp_timestamp_ms| is estimated by dividing by the frequency.
  return (ntp_time == other.ntp_time) || (rtp_timestamp == other.rtp_timestamp);
}

// Class for converting an RTP timestamp to the NTP domain.
RtpToNtpEstimator::RtpToNtpEstimator() {}
RtpToNtpEstimator::~RtpToNtpEstimator() {}

void RtpToNtpEstimator::UpdateParameters() {
  if (measurements_.size() != kNumRtcpReportsToUse)
    return;

  int64_t timestamp_new = measurements_.front().rtp_timestamp;
  int64_t timestamp_old = measurements_.back().rtp_timestamp;
  if (!CompensateForWrapAround(timestamp_new, timestamp_old, &timestamp_new))
    return;

  int64_t ntp_ms_new = measurements_.front().ntp_time.ToMs();
  int64_t ntp_ms_old = measurements_.back().ntp_time.ToMs();

  if (!CalculateFrequency(ntp_ms_new, timestamp_new, ntp_ms_old, timestamp_old,
                          &params_.frequency_khz)) {
    return;
  }
  params_.offset_ms = timestamp_new - params_.frequency_khz * ntp_ms_new;
  params_.calculated = true;
}

bool RtpToNtpEstimator::UpdateMeasurements(uint32_t ntp_secs,
                                           uint32_t ntp_frac,
                                           uint32_t rtp_timestamp,
                                           bool* new_rtcp_sr) {
  *new_rtcp_sr = false;

  RtcpMeasurement measurement(ntp_secs, ntp_frac, rtp_timestamp);
  if (Contains(measurements_, measurement)) {
    // RTCP SR report already added.
    return true;
  }
  if (!IsValid(measurements_, measurement)) {
    // Old report or invalid parameters.
    return false;
  }

  // Insert new RTCP SR report.
  if (measurements_.size() == kNumRtcpReportsToUse)
    measurements_.pop_back();

  measurements_.push_front(measurement);
  *new_rtcp_sr = true;

  // List updated, calculate new parameters.
  UpdateParameters();
  return true;
}

bool RtpToNtpEstimator::Estimate(int64_t rtp_timestamp,
                                 int64_t* rtp_timestamp_ms) const {
  if (!params_.calculated || measurements_.empty())
    return false;

  uint32_t rtp_timestamp_old = measurements_.back().rtp_timestamp;
  int64_t rtp_timestamp_unwrapped;
  if (!CompensateForWrapAround(rtp_timestamp, rtp_timestamp_old,
                               &rtp_timestamp_unwrapped)) {
    return false;
  }

  double rtp_ms =
      (static_cast<double>(rtp_timestamp_unwrapped) - params_.offset_ms) /
          params_.frequency_khz +
      0.5f;

  if (rtp_ms < 0)
    return false;

  *rtp_timestamp_ms = rtp_ms;
  return true;
}

int CheckForWrapArounds(uint32_t new_timestamp, uint32_t old_timestamp) {
  if (new_timestamp < old_timestamp) {
    // This difference should be less than -2^31 if we have had a wrap around
    // (e.g. |new_timestamp| = 1, |rtcp_rtp_timestamp| = 2^32 - 1). Since it is
    // cast to a int32_t, it should be positive.
    if (static_cast<int32_t>(new_timestamp - old_timestamp) > 0) {
      // Forward wrap around.
      return 1;
    }
  } else if (static_cast<int32_t>(old_timestamp - new_timestamp) > 0) {
    // This difference should be less than -2^31 if we have had a backward wrap
    // around. Since it is cast to a int32_t, it should be positive.
    return -1;
  }
  return 0;
}

}  // namespace webrtc
