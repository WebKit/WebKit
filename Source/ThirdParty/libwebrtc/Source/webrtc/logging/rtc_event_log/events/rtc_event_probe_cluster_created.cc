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

namespace webrtc {

RtcEventProbeClusterCreated::RtcEventProbeClusterCreated(int id,
                                                         int bitrate_bps,
                                                         int min_probes,
                                                         int min_bytes)
    : id_(id),
      bitrate_bps_(bitrate_bps),
      min_probes_(min_probes),
      min_bytes_(min_bytes) {}

RtcEvent::Type RtcEventProbeClusterCreated::GetType() const {
  return RtcEvent::Type::ProbeClusterCreated;
}

bool RtcEventProbeClusterCreated::IsConfigEvent() const {
  return false;
}

}  // namespace webrtc
