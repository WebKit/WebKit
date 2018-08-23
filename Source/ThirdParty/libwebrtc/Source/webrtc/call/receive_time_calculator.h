/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_RECEIVE_TIME_CALCULATOR_H_
#define CALL_RECEIVE_TIME_CALCULATOR_H_

#include <stdint.h>
#include <memory>

#include "absl/types/optional.h"

namespace webrtc {

// The receive time calculator serves the purpose of combining packet time
// stamps with a safely incremental clock. This assumes that the packet time
// stamps are based on lower layer timestamps that have more accurate time
// increments since they are based on the exact receive time. They might
// however, have large jumps due to clock resets in the system. To compensate
// this they are combined with a safe clock source that is guaranteed to be
// consistent, but it will not be able to measure the exact time when a packet
// is received.
class ReceiveTimeCalculator {
 public:
  static std::unique_ptr<ReceiveTimeCalculator> CreateFromFieldTrial();
  // The min delta is used to correct for jumps backwards in time, to allow some
  // packet reordering a small negative value is appropriate to use. The max
  // delta difference is used as margin when detecting when packet time
  // increases more than the safe clock. This should be larger than the largest
  // expected sysmtem induced delay in the safe clock timestamp.
  ReceiveTimeCalculator(int64_t min_delta_ms, int64_t max_delta_diff_ms);
  int64_t ReconcileReceiveTimes(int64_t packet_time_us_, int64_t safe_time_us_);

 private:
  const int64_t min_delta_us_;
  const int64_t max_delta_diff_us_;
  absl::optional<int64_t> receive_time_offset_us_;
  int64_t last_packet_time_us_ = 0;
  int64_t last_safe_time_us_ = 0;
};
}  // namespace webrtc
#endif  // CALL_RECEIVE_TIME_CALCULATOR_H_
