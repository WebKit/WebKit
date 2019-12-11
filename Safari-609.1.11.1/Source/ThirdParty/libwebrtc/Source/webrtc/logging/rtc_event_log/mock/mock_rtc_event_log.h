/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_MOCK_MOCK_RTC_EVENT_LOG_H_
#define LOGGING_RTC_EVENT_LOG_MOCK_MOCK_RTC_EVENT_LOG_H_

#include <memory>

#include "api/rtc_event_log/rtc_event_log.h"
#include "test/gmock.h"

namespace webrtc {

class MockRtcEventLog : public RtcEventLog {
 public:
  MockRtcEventLog();
  ~MockRtcEventLog();

  virtual bool StartLogging(std::unique_ptr<RtcEventLogOutput> output,
                            int64_t output_period_ms) {
    return StartLoggingProxy(output.get(), output_period_ms);
  }
  MOCK_METHOD2(StartLoggingProxy, bool(RtcEventLogOutput*, int64_t));

  MOCK_METHOD0(StopLogging, void());

  virtual void Log(std::unique_ptr<RtcEvent> event) {
    return LogProxy(event.get());
  }
  MOCK_METHOD1(LogProxy, void(RtcEvent*));
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_MOCK_MOCK_RTC_EVENT_LOG_H_
