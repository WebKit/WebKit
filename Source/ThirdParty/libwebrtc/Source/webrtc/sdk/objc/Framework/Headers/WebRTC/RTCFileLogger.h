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

#import <WebRTC/RTCMacros.h>

typedef NS_ENUM(NSUInteger, RTCFileLoggerSeverity) {
  RTCFileLoggerSeverityVerbose,
  RTCFileLoggerSeverityInfo,
  RTCFileLoggerSeverityWarning,
  RTCFileLoggerSeverityError
};

typedef NS_ENUM(NSUInteger, RTCFileLoggerRotationType) {
  RTCFileLoggerTypeCall,
  RTCFileLoggerTypeApp,
};

NS_ASSUME_NONNULL_BEGIN

// This class intercepts WebRTC logs and saves them to a file. The file size
// will not exceed the given maximum bytesize. When the maximum bytesize is
// reached, logs are rotated according to the rotationType specified.
// For kRTCFileLoggerTypeCall, logs from the beginning and the end
// are preserved while the middle section is overwritten instead.
// For kRTCFileLoggerTypeApp, the oldest log is overwritten.
// This class is not threadsafe.
RTC_EXPORT
@interface RTCFileLogger : NSObject

// The severity level to capture. The default is kRTCFileLoggerSeverityInfo.
@property(nonatomic, assign) RTCFileLoggerSeverity severity;

// The rotation type for this file logger. The default is
// kRTCFileLoggerTypeCall.
@property(nonatomic, readonly) RTCFileLoggerRotationType rotationType;

// Disables buffering disk writes. Should be set before |start|. Buffering
// is enabled by default for performance.
@property(nonatomic, assign) BOOL shouldDisableBuffering;

// Default constructor provides default settings for dir path, file size and
// rotation type.
- (instancetype)init;

// Create file logger with default rotation type.
- (instancetype)initWithDirPath:(NSString *)dirPath
                    maxFileSize:(NSUInteger)maxFileSize;

- (instancetype)initWithDirPath:(NSString *)dirPath
                    maxFileSize:(NSUInteger)maxFileSize
                   rotationType:(RTCFileLoggerRotationType)rotationType
    NS_DESIGNATED_INITIALIZER;

// Starts writing WebRTC logs to disk if not already started. Overwrites any
// existing file(s).
- (void)start;

// Stops writing WebRTC logs to disk. This method is also called on dealloc.
- (void)stop;

// Returns the current contents of the logs, or nil if start has been called
// without a stop.
- (NSData *)logData;

@end

NS_ASSUME_NONNULL_END

