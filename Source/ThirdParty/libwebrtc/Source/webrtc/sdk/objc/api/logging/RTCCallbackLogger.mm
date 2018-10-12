/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCCallbackLogger.h"

#include <memory>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/logsinks.h"

class CallbackLogSink : public rtc::LogSink {
 public:
  CallbackLogSink(void (^callbackHandler)(NSString *message)) {
    callback_handler_ = callbackHandler;
  }

  ~CallbackLogSink() override { callback_handler_ = nil; }

  void OnLogMessage(const std::string &message) override {
    if (callback_handler_) {
      callback_handler_([NSString stringWithUTF8String:message.c_str()]);
    }
  }

 private:
  void (^callback_handler_)(NSString *message);
};

@implementation RTCCallbackLogger {
  BOOL _hasStarted;
  std::unique_ptr<CallbackLogSink> _logSink;
}

@synthesize severity = _severity;

- (instancetype)init {
  self = [super init];
  if (self != nil) {
    _severity = RTCLoggingSeverityInfo;
  }
  return self;
}

- (void)dealloc {
  [self stop];
}

- (void)start:(nullable void (^)(NSString *))callback {
  if (_hasStarted) {
    return;
  }

  _logSink.reset(new CallbackLogSink(callback));

  rtc::LogMessage::AddLogToStream(_logSink.get(), [self rtcSeverity]);
  _hasStarted = YES;
}

- (void)stop {
  if (!_hasStarted) {
    return;
  }
  RTC_DCHECK(_logSink);
  rtc::LogMessage::RemoveLogToStream(_logSink.get());
  _hasStarted = NO;
  _logSink.reset();
}

#pragma mark - Private

- (rtc::LoggingSeverity)rtcSeverity {
  switch (_severity) {
    case RTCLoggingSeverityVerbose:
      return rtc::LS_VERBOSE;
    case RTCLoggingSeverityInfo:
      return rtc::LS_INFO;
    case RTCLoggingSeverityWarning:
      return rtc::LS_WARNING;
    case RTCLoggingSeverityError:
      return rtc::LS_ERROR;
  }
}

@end
