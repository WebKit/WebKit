/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_DTLS_WRITABLE_STATE_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_DTLS_WRITABLE_STATE_H_

#include <memory>

#include "api/rtc_event_log/rtc_event.h"
#include "api/units/timestamp.h"

namespace webrtc {

class RtcEventDtlsWritableState : public RtcEvent {
 public:
  static constexpr Type kType = Type::DtlsWritableState;

  explicit RtcEventDtlsWritableState(bool writable);
  ~RtcEventDtlsWritableState() override;

  Type GetType() const override { return kType; }
  bool IsConfigEvent() const override { return false; }

  std::unique_ptr<RtcEventDtlsWritableState> Copy() const;

  bool writable() const { return writable_; }

 private:
  RtcEventDtlsWritableState(const RtcEventDtlsWritableState& other);

  const bool writable_;
};

struct LoggedDtlsWritableState {
  LoggedDtlsWritableState() = default;
  explicit LoggedDtlsWritableState(bool writable) : writable(writable) {}

  int64_t log_time_us() const { return timestamp.us(); }
  int64_t log_time_ms() const { return timestamp.ms(); }

  Timestamp timestamp = Timestamp::MinusInfinity();
  bool writable;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_DTLS_WRITABLE_STATE_H_
