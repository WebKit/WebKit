/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_PROBE_RESULT_FAILURE_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_PROBE_RESULT_FAILURE_H_

#include <stdint.h>

#include <memory>

#include "api/rtc_event_log/rtc_event.h"

namespace webrtc {

enum class ProbeFailureReason {
  kInvalidSendReceiveInterval = 0,
  kInvalidSendReceiveRatio,
  kTimeout,
  kLast
};

class RtcEventProbeResultFailure final : public RtcEvent {
 public:
  static constexpr Type kType = Type::ProbeResultFailure;

  RtcEventProbeResultFailure(int32_t id, ProbeFailureReason failure_reason);
  ~RtcEventProbeResultFailure() override = default;

  Type GetType() const override { return kType; }
  bool IsConfigEvent() const override { return false; }

  std::unique_ptr<RtcEventProbeResultFailure> Copy() const;

  int32_t id() const { return id_; }
  ProbeFailureReason failure_reason() const { return failure_reason_; }

 private:
  RtcEventProbeResultFailure(const RtcEventProbeResultFailure& other);

  const int32_t id_;
  const ProbeFailureReason failure_reason_;
};

struct LoggedBweProbeFailureEvent {
  LoggedBweProbeFailureEvent() = default;
  LoggedBweProbeFailureEvent(int64_t timestamp_us,
                             int32_t id,
                             ProbeFailureReason failure_reason)
      : timestamp_us(timestamp_us), id(id), failure_reason(failure_reason) {}

  int64_t log_time_us() const { return timestamp_us; }
  int64_t log_time_ms() const { return timestamp_us / 1000; }

  int64_t timestamp_us;
  int32_t id;
  ProbeFailureReason failure_reason;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_PROBE_RESULT_FAILURE_H_
