/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_probe_cluster_created.h"

#include <cstdint>
#include <memory>

#include "absl/memory/memory.h"

namespace webrtc {

RtcEventProbeClusterCreated::RtcEventProbeClusterCreated(int32_t id,
                                                         int32_t bitrate_bps,
                                                         uint32_t min_probes,
                                                         uint32_t min_bytes)
    : id_(id),
      bitrate_bps_(bitrate_bps),
      min_probes_(min_probes),
      min_bytes_(min_bytes) {}

std::unique_ptr<RtcEventProbeClusterCreated> RtcEventProbeClusterCreated::Copy()
    const {
  return absl::WrapUnique(new RtcEventProbeClusterCreated(*this));
}

}  // namespace webrtc
