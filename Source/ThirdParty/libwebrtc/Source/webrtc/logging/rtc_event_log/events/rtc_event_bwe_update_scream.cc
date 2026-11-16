/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_bwe_update_scream.h"

#include <cstdint>
#include <memory>

#include "absl/memory/memory.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"

namespace webrtc {

RtcEventBweUpdateScream::RtcEventBweUpdateScream(DataSize ref_window,
                                                 DataSize data_in_flight,
                                                 DataRate target_rate,
                                                 TimeDelta smoothed_rtt,
                                                 TimeDelta avg_queue_delay,
                                                 uint32_t l4s_marked_permille)
    : ref_window_bytes_(ref_window.bytes()),
      data_in_flight_bytes_(data_in_flight.bytes()),
      target_rate_kbps_(target_rate.kbps()),
      smoothed_rtt_ms_(smoothed_rtt.ms()),
      avg_queue_delay_ms_(avg_queue_delay.ms_or(0)),
      l4s_marked_permille_(l4s_marked_permille) {}

RtcEventBweUpdateScream::~RtcEventBweUpdateScream() = default;

std::unique_ptr<RtcEventBweUpdateScream> RtcEventBweUpdateScream::Copy() const {
  return absl::WrapUnique(new RtcEventBweUpdateScream(*this));
}

}  // namespace webrtc
