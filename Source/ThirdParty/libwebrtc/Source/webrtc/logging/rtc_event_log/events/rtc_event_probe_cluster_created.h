/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_PROBE_CLUSTER_CREATED_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_PROBE_CLUSTER_CREATED_H_

#include "logging/rtc_event_log/events/rtc_event.h"

namespace webrtc {

class RtcEventProbeClusterCreated final : public RtcEvent {
 public:
  RtcEventProbeClusterCreated(int id,
                              int bitrate_bps,
                              int min_probes,
                              int min_bytes);
  ~RtcEventProbeClusterCreated() override = default;

  Type GetType() const override;

  bool IsConfigEvent() const override;

  const int id_;
  const int bitrate_bps_;
  const int min_probes_;
  const int min_bytes_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_PROBE_CLUSTER_CREATED_H_
