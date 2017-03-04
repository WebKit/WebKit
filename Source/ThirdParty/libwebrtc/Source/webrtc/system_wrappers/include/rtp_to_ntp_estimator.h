/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INCLUDE_RTP_TO_NTP_ESTIMATOR_H_
#define WEBRTC_SYSTEM_WRAPPERS_INCLUDE_RTP_TO_NTP_ESTIMATOR_H_

#include <list>

#include "webrtc/system_wrappers/include/ntp_time.h"
#include "webrtc/typedefs.h"

namespace webrtc {
// Class for converting an RTP timestamp to the NTP domain in milliseconds.
// The class needs to be trained with (at least 2) RTP/NTP timestamp pairs from
// RTCP sender reports before the convertion can be done.
class RtpToNtpEstimator {
 public:
  RtpToNtpEstimator();
  ~RtpToNtpEstimator();

  // RTP and NTP timestamp pair from a RTCP SR report.
  struct RtcpMeasurement {
    RtcpMeasurement(uint32_t ntp_secs, uint32_t ntp_frac, uint32_t timestamp);
    bool IsEqual(const RtcpMeasurement& other) const;

    NtpTime ntp_time;
    uint32_t rtp_timestamp;
  };

  // Estimated parameters from RTP and NTP timestamp pairs in |measurements_|.
  struct Parameters {
    double frequency_khz = 0.0;
    double offset_ms = 0.0;
    bool calculated = false;
  };

  // Updates measurements with RTP/NTP timestamp pair from a RTCP sender report.
  // |new_rtcp_sr| is set to true if a new report is added.
  bool UpdateMeasurements(uint32_t ntp_secs,
                          uint32_t ntp_frac,
                          uint32_t rtp_timestamp,
                          bool* new_rtcp_sr);

  // Converts an RTP timestamp to the NTP domain in milliseconds.
  // Returns true on success, false otherwise.
  bool Estimate(int64_t rtp_timestamp, int64_t* rtp_timestamp_ms) const;

  const Parameters& params() const { return params_; }

 private:
  void UpdateParameters();

  std::list<RtcpMeasurement> measurements_;
  Parameters params_;
};

// Returns:
//  1: forward wrap around.
//  0: no wrap around.
// -1: backwards wrap around (i.e. reordering).
int CheckForWrapArounds(uint32_t new_timestamp, uint32_t old_timestamp);

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_INCLUDE_RTP_TO_NTP_ESTIMATOR_H_
