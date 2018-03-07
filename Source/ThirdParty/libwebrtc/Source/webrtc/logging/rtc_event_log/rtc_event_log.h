/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_H_

#include <memory>
#include <string>
#include <utility>

#include "api/rtceventlogoutput.h"
#include "logging/rtc_event_log/events/rtc_event.h"

namespace webrtc {

class Clock;

enum PacketDirection { kIncomingPacket = 0, kOutgoingPacket };

class RtcEventLog {
 public:
  enum : size_t { kUnlimitedOutput = 0 };
  enum : int64_t { kImmediateOutput = 0 };

  // TODO(eladalon): Two stages are upcoming.
  // 1. Extend this to actually support the new encoding.
  // 2. Get rid of the legacy encoding, allowing us to get rid of this enum.
  enum class EncodingType { Legacy };

  virtual ~RtcEventLog() {}

  // Factory method to create an RtcEventLog object.
  static std::unique_ptr<RtcEventLog> Create(EncodingType encoding_type);
  // TODO(nisse): webrtc::Clock is deprecated. Delete this method and
  // above forward declaration of Clock when
  // webrtc/system_wrappers/include/clock.h is deleted.
  static std::unique_ptr<RtcEventLog> Create(const Clock*,
                                             EncodingType encoding_type) {
    return Create(encoding_type);
  }

  // Create an RtcEventLog object that does nothing.
  static std::unique_ptr<RtcEventLog> CreateNull();

  // Starts logging to a given output. The output might be limited in size,
  // and may close itself once it has reached the maximum size.
  virtual bool StartLogging(std::unique_ptr<RtcEventLogOutput> output,
                            int64_t output_period_ms) = 0;

  // Stops logging to file and waits until the file has been closed, after
  // which it would be permissible to read and/or modify it.
  virtual void StopLogging() = 0;

  // Log an RTC event (the type of event is determined by the subclass).
  virtual void Log(std::unique_ptr<RtcEvent> event) = 0;
};

// No-op implementation is used if flag is not set, or in tests.
class RtcEventLogNullImpl : public RtcEventLog {
 public:
  bool StartLogging(std::unique_ptr<RtcEventLogOutput>,
                    int64_t) override {
    return false;
  }
  void StopLogging() override {}
  void Log(std::unique_ptr<RtcEvent>) override {}
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_H_
