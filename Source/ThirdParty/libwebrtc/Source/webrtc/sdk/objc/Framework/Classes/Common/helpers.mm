/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#import <Foundation/Foundation.h>
#import <sys/sysctl.h>
#if defined(WEBRTC_IOS)
#import <UIKit/UIKit.h>
#endif

#include <memory>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "sdk/objc/Framework/Classes/Common/helpers.h"

namespace webrtc {
namespace ios {

#if defined(WEBRTC_IOS)
bool isOperatingSystemAtLeastVersion(double version) {
  return GetSystemVersion() >= version;
}
#endif

NSString* NSStringFromStdString(const std::string& stdString) {
  // std::string may contain null termination character so we construct
  // using length.
  return [[NSString alloc] initWithBytes:stdString.data()
                                  length:stdString.length()
                                encoding:NSUTF8StringEncoding];
}

std::string StdStringFromNSString(NSString* nsString) {
  NSData* charData = [nsString dataUsingEncoding:NSUTF8StringEncoding];
  return std::string(reinterpret_cast<const char*>([charData bytes]),
                     [charData length]);
}

bool CheckAndLogError(BOOL success, NSError* error) {
  if (!success) {
    NSString* msg =
        [NSString stringWithFormat:@"Error: %ld, %@, %@", (long)error.code,
                                   error.localizedDescription,
                                   error.localizedFailureReason];
    RTC_LOG(LS_ERROR) << StdStringFromNSString(msg);
    return false;
  }
  return true;
}

// TODO(henrika): see if it is possible to move to GetThreadName in
// platform_thread.h and base it on pthread methods instead.
std::string GetCurrentThreadDescription() {
  NSString* name = [NSString stringWithFormat:@"%@", [NSThread currentThread]];
  return StdStringFromNSString(name);
}

#if defined(WEBRTC_IOS)
std::string GetSystemName() {
  NSString* osName = [[UIDevice currentDevice] systemName];
  return StdStringFromNSString(osName);
}

std::string GetSystemVersionAsString() {
  NSString* osVersion = [[UIDevice currentDevice] systemVersion];
  return StdStringFromNSString(osVersion);
}

double GetSystemVersion() {
  static dispatch_once_t once_token;
  static double system_version;
  dispatch_once(&once_token, ^{
    system_version = [UIDevice currentDevice].systemVersion.doubleValue;
  });
  return system_version;
}

std::string GetDeviceType() {
  NSString* deviceModel = [[UIDevice currentDevice] model];
  return StdStringFromNSString(deviceModel);
}
#endif

std::string GetDeviceName() {
  size_t size;
  sysctlbyname("hw.machine", NULL, &size, NULL, 0);
  std::unique_ptr<char[]> machine;
  machine.reset(new char[size]);
  sysctlbyname("hw.machine", machine.get(), &size, NULL, 0);
  return std::string(machine.get());
}

std::string GetProcessName() {
  NSString* processName = [NSProcessInfo processInfo].processName;
  return StdStringFromNSString(processName);
}

int GetProcessID() {
  return [NSProcessInfo processInfo].processIdentifier;
}

std::string GetOSVersionString() {
  NSString* osVersion =
      [NSProcessInfo processInfo].operatingSystemVersionString;
  return StdStringFromNSString(osVersion);
}

int GetProcessorCount() {
  return [NSProcessInfo processInfo].processorCount;
}

#if defined(__IPHONE_9_0) && defined(__IPHONE_OS_VERSION_MAX_ALLOWED) \
    && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0
bool GetLowPowerModeEnabled() {
  if (isOperatingSystemAtLeastVersion(9.0)) {
    // lowPoweredModeEnabled is only available on iOS9+.
    return [NSProcessInfo processInfo].lowPowerModeEnabled;
  }
  RTC_LOG(LS_WARNING) << "webrtc::ios::GetLowPowerModeEnabled() is not "
                         "supported. Requires at least iOS 9.0";
  return false;
}
#endif

}  // namespace ios
}  // namespace webrtc

