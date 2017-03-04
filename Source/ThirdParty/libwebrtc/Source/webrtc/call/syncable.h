/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Syncable is used by RtpStreamsSynchronizer in VideoReceiveStream, and
// implemented by AudioReceiveStream.

#ifndef WEBRTC_CALL_SYNCABLE_H_
#define WEBRTC_CALL_SYNCABLE_H_

#include <stdint.h>

#include "webrtc/base/optional.h"

namespace webrtc {

class Syncable {
 public:
  struct Info {
    int64_t latest_receive_time_ms = 0;
    uint32_t latest_received_capture_timestamp = 0;
    uint32_t capture_time_ntp_secs = 0;
    uint32_t capture_time_ntp_frac = 0;
    uint32_t capture_time_source_clock = 0;
    int current_delay_ms = 0;
  };

  virtual ~Syncable();

  virtual int id() const = 0;
  virtual rtc::Optional<Info> GetInfo() const = 0;
  virtual uint32_t GetPlayoutTimestamp() const = 0;
  virtual void SetMinimumPlayoutDelay(int delay_ms) = 0;
};
}  // namespace webrtc

#endif  // WEBRTC_CALL_SYNCABLE_H_
