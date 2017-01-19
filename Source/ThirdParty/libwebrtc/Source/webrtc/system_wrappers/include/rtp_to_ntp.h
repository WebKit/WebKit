/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SYSTEM_WRAPPERS_INCLUDE_RTP_TO_NTP_H_
#define SYSTEM_WRAPPERS_INCLUDE_RTP_TO_NTP_H_

#include <list>

#include "webrtc/typedefs.h"

namespace webrtc {

struct RtcpMeasurement {
  RtcpMeasurement();
  RtcpMeasurement(uint32_t ntp_secs, uint32_t ntp_frac, uint32_t timestamp);
  bool IsEqual(const RtcpMeasurement& other) const;

  uint32_t ntp_secs;
  uint32_t ntp_frac;
  uint32_t rtp_timestamp;
};

struct RtcpMeasurements {
  RtcpMeasurements();
  ~RtcpMeasurements();
  bool Contains(const RtcpMeasurement& other) const;
  bool IsValid(const RtcpMeasurement& other) const;
  void UpdateParameters();

  // Estimated parameters from RTP and NTP timestamp pairs in |list|.
  struct RtpToNtpParameters {
    double frequency_khz = 0.0;
    double offset_ms = 0.0;
    bool calculated = false;
  };

  std::list<RtcpMeasurement> list;
  RtpToNtpParameters params;
};

// Updates |list| in |rtcp_measurements| with timestamps from the RTCP SR.
// |new_rtcp_sr| will be set to true if these are the timestamps which have
// never be added to |list|.
// |rtcp_measurements.params| are estimated from the RTP and NTP timestamp pairs
// in the |list| when a new RTCP SR is inserted.
bool UpdateRtcpList(uint32_t ntp_secs,
                    uint32_t ntp_frac,
                    uint32_t rtp_timestamp,
                    RtcpMeasurements* rtcp_measurements,
                    bool* new_rtcp_sr);

// Converts an RTP timestamp to the NTP domain in milliseconds using the
// estimated |rtcp_measurements.params|.
bool RtpToNtpMs(int64_t rtp_timestamp,
                const RtcpMeasurements& rtcp_measurements,
                int64_t* rtp_timestamp_in_ms);

// Returns 1 there has been a forward wrap around, 0 if there has been no wrap
// around and -1 if there has been a backwards wrap around (i.e. reordering).
int CheckForWrapArounds(uint32_t rtp_timestamp, uint32_t rtcp_rtp_timestamp);

}  // namespace webrtc

#endif  // SYSTEM_WRAPPERS_INCLUDE_RTP_TO_NTP_H_
