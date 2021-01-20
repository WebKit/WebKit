/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCFileLogger.h"

#include <memory>

#include "rtc_base/checks.h"
#include "rtc_base/file_rotating_stream.h"
#include "rtc_base/log_sinks.h"
#include "rtc_base/logging.h"

NSString *const kDefaultLogDirName = @"webrtc_logs";
NSUInteger const kDefaultMaxFileSize = 10 * 1024 * 1024; // 10MB.
const char *kRTCFileLoggerRotatingLogPrefix = "rotating_log";

@implementation RTCFileLogger {
  BOOL _hasStarted;
  NSString *_dirPath;
  NSUInteger _maxFileSize;
  std::unique_ptr<rtc::FileRotatingLogSink> _logSink;
}

@synthesize severity = _severity;
@synthesize rotationType = _rotationType;
@synthesize shouldDisableBuffering = _shouldDisableBuffering;

- (instancetype)init {
  NSArray *paths = NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSUserDomainMask, YES);
  NSString *documentsDirPath = [paths firstObject];
  NSString *defaultDirPath =
      [documentsDirPath stringByAppendingPathComponent:kDefaultLogDirName];
  return [self initWithDirPath:defaultDirPath
                   maxFileSize:kDefaultMaxFileSize];
}

- (instancetype)initWithDirPath:(NSString *)dirPath
                    maxFileSize:(NSUInteger)maxFileSize {
  return [self initWithDirPath:dirPath
                   maxFileSize:maxFileSize
                  rotationType:RTCFileLoggerTypeCall];
}

- (instancetype)initWithDirPath:(NSString *)dirPath
                    maxFileSize:(NSUInteger)maxFileSize
                   rotationType:(RTCFileLoggerRotationType)rotationType {
  NSParameterAssert(dirPath.length);
  NSParameterAssert(maxFileSize);
  if (self = [super init]) {
    BOOL isDir = NO;
    NSFileManager *fileManager = [NSFileManager defaultManager];
    if ([fileManager fileExistsAtPath:dirPath isDirectory:&isDir]) {
      if (!isDir) {
        // Bail if something already exists there.
        return nil;
      }
    } else {
      if (![fileManager createDirectoryAtPath:dirPath
                  withIntermediateDirectories:NO
                                   attributes:nil
                                        error:nil]) {
        // Bail if we failed to create a directory.
        return nil;
      }
    }
    _dirPath = dirPath;
    _maxFileSize = maxFileSize;
    _severity = RTCFileLoggerSeverityInfo;
  }
  return self;
}

- (void)dealloc {
  [self stop];
}

- (void)start {
  if (_hasStarted) {
    return;
  }
  switch (_rotationType) {
    case RTCFileLoggerTypeApp:
      _logSink.reset(
          new rtc::FileRotatingLogSink(_dirPath.UTF8String,
                                       kRTCFileLoggerRotatingLogPrefix,
                                       _maxFileSize,
                                       _maxFileSize / 10));
      break;
    case RTCFileLoggerTypeCall:
      _logSink.reset(
          new rtc::CallSessionFileRotatingLogSink(_dirPath.UTF8String,
                                                  _maxFileSize));
      break;
  }
  if (!_logSink->Init()) {
    RTC_LOG(LS_ERROR) << "Failed to open log files at path: " << _dirPath.UTF8String;
    _logSink.reset();
    return;
  }
  if (_shouldDisableBuffering) {
    _logSink->DisableBuffering();
  }
  rtc::LogMessage::LogThreads(true);
  rtc::LogMessage::LogTimestamps(true);
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

- (nullable NSData *)logData {
  if (_hasStarted) {
    return nil;
  }
  NSMutableData* logData = [NSMutableData data];
  std::unique_ptr<rtc::FileRotatingStreamReader> stream;
  switch(_rotationType) {
    case RTCFileLoggerTypeApp:
      stream = std::make_unique<rtc::FileRotatingStreamReader>(_dirPath.UTF8String,
                                                               kRTCFileLoggerRotatingLogPrefix);
      break;
    case RTCFileLoggerTypeCall:
      stream = std::make_unique<rtc::CallSessionFileRotatingStreamReader>(_dirPath.UTF8String);
      break;
  }
  size_t bufferSize = stream->GetSize();
  if (bufferSize == 0) {
    return logData;
  }
  // Allocate memory using malloc so we can pass it direcly to NSData without
  // copying.
  std::unique_ptr<uint8_t[]> buffer(static_cast<uint8_t*>(malloc(bufferSize)));
  size_t read = stream->ReadAll(buffer.get(), bufferSize);
  logData = [[NSMutableData alloc] initWithBytesNoCopy:buffer.release()
                                                length:read];
  return logData;
}

#pragma mark - Private

- (rtc::LoggingSeverity)rtcSeverity {
  switch (_severity) {
    case RTCFileLoggerSeverityVerbose:
      return rtc::LS_VERBOSE;
    case RTCFileLoggerSeverityInfo:
      return rtc::LS_INFO;
    case RTCFileLoggerSeverityWarning:
      return rtc::LS_WARNING;
    case RTCFileLoggerSeverityError:
      return rtc::LS_ERROR;
  }
}

@end
