/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SYSTEM_WRAPPERS_INCLUDE_RTP_TO_NTP_ESTIMATOR_H_
#define SYSTEM_WRAPPERS_INCLUDE_RTP_TO_NTP_ESTIMATOR_H_

#include <list>

#include "absl/types/optional.h"
#include "modules/include/module_common_types_public.h"
#include "rtc_base/numerics/moving_median_filter.h"
#include "system_wrappers/include/ntp_time.h"

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
    RtcpMeasurement(uint32_t ntp_secs,
                    uint32_t ntp_frac,
                    int64_t unwrapped_timestamp);
    bool IsEqual(const RtcpMeasurement& other) const;

    NtpTime ntp_time;
    int64_t unwrapped_rtp_timestamp;
  };

  // Estimated parameters from RTP and NTP timestamp pairs in |measurements_|.
  struct Parameters {
    // Implicit conversion from int because MovingMedianFilter returns 0
    // internally if no samples are present. However, it should never happen as
    // we don't ask smoothing_filter_ to return anything if there were no
    // samples.
    Parameters(const int& value) {  // NOLINT
      RTC_NOTREACHED();
    }
    Parameters() : frequency_khz(0.0), offset_ms(0.0) {}

    double frequency_khz;
    double offset_ms;

    // Needed to make it work inside MovingMedianFilter
    bool operator<(const Parameters& other) const;
    bool operator==(const Parameters& other) const;
    bool operator<=(const Parameters& other) const;
    bool operator!=(const Parameters& other) const;
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

  // Returns estimated rtp to ntp linear transform parameters.
  const absl::optional<Parameters> params() const;

  static const int kMaxInvalidSamples = 3;

 private:
  void UpdateParameters();

  int consecutive_invalid_samples_;
  std::list<RtcpMeasurement> measurements_;
  MovingMedianFilter<Parameters> smoothing_filter_;
  bool params_calculated_;
  mutable TimestampUnwrapper unwrapper_;
};
}  // namespace webrtc

#endif  // SYSTEM_WRAPPERS_INCLUDE_RTP_TO_NTP_ESTIMATOR_H_
