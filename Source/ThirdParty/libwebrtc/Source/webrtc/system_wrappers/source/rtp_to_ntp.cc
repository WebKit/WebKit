/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/include/rtp_to_ntp.h"

#include "webrtc/base/logging.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {
namespace {
// Calculates the RTP timestamp frequency from two pairs of NTP/RTP timestamps.
bool CalculateFrequency(int64_t rtcp_ntp_ms1,
                        uint32_t rtp_timestamp1,
                        int64_t rtcp_ntp_ms2,
                        uint32_t rtp_timestamp2,
                        double* frequency_khz) {
  if (rtcp_ntp_ms1 <= rtcp_ntp_ms2) {
    return false;
  }
  *frequency_khz = static_cast<double>(rtp_timestamp1 - rtp_timestamp2) /
      static_cast<double>(rtcp_ntp_ms1 - rtcp_ntp_ms2);
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
}  // namespace

// Class holding RTP and NTP timestamp from a RTCP SR report.
RtcpMeasurement::RtcpMeasurement()
    : ntp_secs(0), ntp_frac(0), rtp_timestamp(0) {}

RtcpMeasurement::RtcpMeasurement(uint32_t ntp_secs,
                                 uint32_t ntp_frac,
                                 uint32_t timestamp)
    : ntp_secs(ntp_secs), ntp_frac(ntp_frac), rtp_timestamp(timestamp) {}

bool RtcpMeasurement::IsEqual(const RtcpMeasurement& other) const {
  // Use || since two equal timestamps will result in zero frequency and in
  // RtpToNtpMs, |rtp_timestamp_ms| is estimated by dividing by the frequency.
  return (ntp_secs == other.ntp_secs && ntp_frac == other.ntp_frac) ||
         (rtp_timestamp == other.rtp_timestamp);
}

// Class holding list of RTP and NTP timestamp pairs.
RtcpMeasurements::RtcpMeasurements() {}
RtcpMeasurements::~RtcpMeasurements() {}

bool RtcpMeasurements::Contains(const RtcpMeasurement& other) const {
  for (const auto& it : list) {
    if (it.IsEqual(other))
      return true;
  }
  return false;
}

bool RtcpMeasurements::IsValid(const RtcpMeasurement& other) const {
  if (other.ntp_secs == 0 && other.ntp_frac == 0) {
    // Invalid or not defined.
    return false;
  }
  int64_t ntp_ms_new = Clock::NtpToMs(other.ntp_secs, other.ntp_frac);
  for (const auto& it : list) {
    if (ntp_ms_new <= Clock::NtpToMs(it.ntp_secs, it.ntp_frac)) {
      // Old report.
      return false;
    }
    int64_t timestamp_new = other.rtp_timestamp;
    if (!CompensateForWrapAround(timestamp_new, it.rtp_timestamp,
                                 &timestamp_new)) {
      return false;
    }
    if (timestamp_new <= it.rtp_timestamp) {
      LOG(LS_WARNING) << "Newer RTCP SR report with older RTP timestamp.";
      return false;
    }
  }
  return true;
}

void RtcpMeasurements::UpdateParameters() {
  if (list.size() != 2)
    return;

  int64_t timestamp_new = list.front().rtp_timestamp;
  int64_t timestamp_old = list.back().rtp_timestamp;
  if (!CompensateForWrapAround(timestamp_new, timestamp_old, &timestamp_new))
    return;

  int64_t ntp_ms_new =
      Clock::NtpToMs(list.front().ntp_secs, list.front().ntp_frac);
  int64_t ntp_ms_old =
      Clock::NtpToMs(list.back().ntp_secs, list.back().ntp_frac);

  if (!CalculateFrequency(ntp_ms_new, timestamp_new, ntp_ms_old, timestamp_old,
                          &params.frequency_khz)) {
    return;
  }
  params.offset_ms = timestamp_new - params.frequency_khz * ntp_ms_new;
  params.calculated = true;
}

// Updates list holding NTP and RTP timestamp pairs.
bool UpdateRtcpList(uint32_t ntp_secs,
                    uint32_t ntp_frac,
                    uint32_t rtp_timestamp,
                    RtcpMeasurements* rtcp_measurements,
                    bool* new_rtcp_sr) {
  *new_rtcp_sr = false;

  RtcpMeasurement measurement(ntp_secs, ntp_frac, rtp_timestamp);
  if (rtcp_measurements->Contains(measurement)) {
    // RTCP SR report already added.
    return true;
  }

  if (!rtcp_measurements->IsValid(measurement)) {
    // Old report or invalid parameters.
    return false;
  }

  // Two RTCP SR reports are needed to map between RTP and NTP.
  // More than two will not improve the mapping.
  if (rtcp_measurements->list.size() == 2)
    rtcp_measurements->list.pop_back();

  rtcp_measurements->list.push_front(measurement);
  *new_rtcp_sr = true;

  // List updated, calculate new parameters.
  rtcp_measurements->UpdateParameters();
  return true;
}

// Converts |rtp_timestamp| to the NTP time base using the NTP and RTP timestamp
// pairs in |rtcp|. The converted timestamp is returned in
// |rtp_timestamp_in_ms|. This function compensates for wrap arounds in RTP
// timestamps and returns false if it can't do the conversion due to reordering.
bool RtpToNtpMs(int64_t rtp_timestamp,
                const RtcpMeasurements& rtcp,
                int64_t* rtp_timestamp_in_ms) {
  if (!rtcp.params.calculated || rtcp.list.empty())
    return false;

  uint32_t rtcp_timestamp_old = rtcp.list.back().rtp_timestamp;
  int64_t rtp_timestamp_unwrapped;
  if (!CompensateForWrapAround(rtp_timestamp, rtcp_timestamp_old,
                               &rtp_timestamp_unwrapped)) {
    return false;
  }

  double rtp_timestamp_ms =
      (static_cast<double>(rtp_timestamp_unwrapped) - rtcp.params.offset_ms) /
          rtcp.params.frequency_khz +
      0.5f;
  if (rtp_timestamp_ms < 0) {
    return false;
  }
  *rtp_timestamp_in_ms = rtp_timestamp_ms;
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
