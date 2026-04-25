/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Syncable is used by RtpStreamsSynchronizer in VideoReceiveStreamInterface,
// and implemented by AudioReceiveStreamInterface.

#ifndef CALL_SYNCABLE_H_
#define CALL_SYNCABLE_H_

#include <cstdint>
#include <optional>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {

class Syncable {
 public:
  struct Info {
    // Local time when the the last RTP packet was received.
    Timestamp latest_receive_time = Timestamp::Zero();
    // RTP timestamp of the last RTP packet received.
    uint32_t latest_received_capture_rtp_timestamp = 0;

    // NTP and RTP timestamp from the last RTCP sender report received.
    uint32_t capture_time_rtp = 0;
    NtpTime capture_time_ntp;

    // Current playout delay for the given `Syncable`.
    TimeDelta current_delay;
  };

  // Mapping between capture/render time in RTP timestamps and local clock.
  struct PlayoutInfo {
    Timestamp time;
    uint32_t rtp_timestamp;
  };

  virtual ~Syncable();

  virtual uint32_t id() const = 0;
  virtual std::optional<Info> GetInfo() const = 0;
  virtual std::optional<PlayoutInfo> GetPlayoutRtpTimestamp() const = 0;
  virtual bool SetMinimumPlayoutDelay(TimeDelta delay) = 0;
  virtual void SetEstimatedPlayoutNtpTimestamp(NtpTime ntp_time,
                                               Timestamp time) = 0;
};
}  // namespace webrtc

#endif  // CALL_SYNCABLE_H_
