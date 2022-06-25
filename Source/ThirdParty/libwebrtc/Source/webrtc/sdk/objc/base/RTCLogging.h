/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCMacros.h"

// Subset of rtc::LoggingSeverity.
typedef NS_ENUM(NSInteger, RTCLoggingSeverity) {
  RTCLoggingSeverityVerbose,
  RTCLoggingSeverityInfo,
  RTCLoggingSeverityWarning,
  RTCLoggingSeverityError,
  RTCLoggingSeverityNone,
};

// Wrapper for C++ RTC_LOG(sev) macros.
// Logs the log string to the webrtc logstream for the given severity.
RTC_EXTERN void RTCLogEx(RTCLoggingSeverity severity, NSString* log_string);

// Wrapper for rtc::LogMessage::LogToDebug.
// Sets the minimum severity to be logged to console.
RTC_EXTERN void RTCSetMinDebugLogLevel(RTCLoggingSeverity severity);

// Returns the filename with the path prefix removed.
RTC_EXTERN NSString* RTCFileName(const char* filePath);

// Some convenience macros.

#define RTCLogString(format, ...)                                           \
  [NSString stringWithFormat:@"(%@:%d %s): " format, RTCFileName(__FILE__), \
                             __LINE__, __FUNCTION__, ##__VA_ARGS__]

#define RTCLogFormat(severity, format, ...)                     \
  do {                                                          \
    NSString* log_string = RTCLogString(format, ##__VA_ARGS__); \
    RTCLogEx(severity, log_string);                             \
  } while (false)

#define RTCLogVerbose(format, ...) \
  RTCLogFormat(RTCLoggingSeverityVerbose, format, ##__VA_ARGS__)

#define RTCLogInfo(format, ...) \
  RTCLogFormat(RTCLoggingSeverityInfo, format, ##__VA_ARGS__)

#define RTCLogWarning(format, ...) \
  RTCLogFormat(RTCLoggingSeverityWarning, format, ##__VA_ARGS__)

#define RTCLogError(format, ...) \
  RTCLogFormat(RTCLoggingSeverityError, format, ##__VA_ARGS__)

#if !defined(NDEBUG)
#define RTCLogDebug(format, ...) RTCLogInfo(format, ##__VA_ARGS__)
#else
#define RTCLogDebug(format, ...) \
  do {                           \
  } while (false)
#endif

#define RTCLog(format, ...) RTCLogInfo(format, ##__VA_ARGS__)
