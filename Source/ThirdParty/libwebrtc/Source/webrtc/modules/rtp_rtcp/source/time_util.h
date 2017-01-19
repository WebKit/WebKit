/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_TIME_UTIL_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_TIME_UTIL_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/system_wrappers/include/ntp_time.h"

namespace webrtc {

// Converts NTP timestamp to RTP timestamp.
inline uint32_t NtpToRtp(NtpTime ntp, uint32_t freq) {
  uint32_t tmp = (static_cast<uint64_t>(ntp.fractions()) * freq) >> 32;
  return ntp.seconds() * freq + tmp;
}
// Return the current RTP timestamp from the NTP timestamp
// returned by the specified clock.
inline uint32_t CurrentRtp(const Clock& clock, uint32_t freq) {
  return NtpToRtp(NtpTime(clock), freq);
}

// Helper function for compact ntp representation:
// RFC 3550, Section 4. Time Format.
// Wallclock time is represented using the timestamp format of
// the Network Time Protocol (NTP).
// ...
// In some fields where a more compact representation is
// appropriate, only the middle 32 bits are used; that is, the low 16
// bits of the integer part and the high 16 bits of the fractional part.
inline uint32_t CompactNtp(NtpTime ntp) {
  return (ntp.seconds() << 16) | (ntp.fractions() >> 16);
}
// Converts interval between compact ntp timestamps to milliseconds.
// This interval can be up to ~9.1 hours (2^15 seconds).
// Values close to 2^16 seconds consider negative and result in minimum rtt = 1.
int64_t CompactNtpRttToMs(uint32_t compact_ntp_interval);

}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_TIME_UTIL_H_
