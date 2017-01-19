/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_IOS)

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#import <sys/sysctl.h>
#import <UIKit/UIKit.h>

#include <memory>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/utility/include/helpers_ios.h"

namespace webrtc {
namespace ios {

bool isOperatingSystemAtLeastVersion(double version) {
  return GetSystemVersion() >= version;
}

// Internal helper method used by GetDeviceName() to return device name.
const char* LookUpRealName(const char* raw_name) {
  // Lookup table which maps raw device names to real (human readable) names.
  struct {
    const char* raw_name;
    const char* real_name;
  } device_names[] = {
      {"iPhone1,1", "iPhone 1G"},
      {"iPhone1,2", "iPhone 3G"},
      {"iPhone2,1", "iPhone 3GS"},
      {"iPhone3,1", "iPhone 4"},
      {"iPhone3,3", "Verizon iPhone 4"},
      {"iPhone4,1", "iPhone 4S"},
      {"iPhone5,1", "iPhone 5 (GSM)"},
      {"iPhone5,2", "iPhone 5 (GSM+CDMA)"},
      {"iPhone5,3", "iPhone 5c (GSM)"},
      {"iPhone5,4", "iPhone 5c (GSM+CDMA)"},
      {"iPhone6,1", "iPhone 5s (GSM)"},
      {"iPhone6,2", "iPhone 5s (GSM+CDMA)"},
      {"iPhone7,1", "iPhone 6 Plus"},
      {"iPhone7,2", "iPhone 6"},
      {"iPhone8,1", "iPhone 6s"},
      {"iPhone8,2", "iPhone 6s Plus"},
      {"iPod1,1", "iPod Touch 1G"},
      {"iPod2,1", "iPod Touch 2G"},
      {"iPod3,1", "iPod Touch 3G"},
      {"iPod4,1", "iPod Touch 4G"},
      {"iPod5,1", "iPod Touch 5G"},
      {"iPad1,1", "iPad"},
      {"iPad2,1", "iPad 2 (WiFi)"},
      {"iPad2,2", "iPad 2 (GSM)"},
      {"iPad2,3", "iPad 2 (CDMA)"},
      {"iPad2,4", "iPad 2 (WiFi)"},
      {"iPad2,5", "iPad Mini (WiFi)"},
      {"iPad2,6", "iPad Mini (GSM)"},
      {"iPad2,7", "iPad Mini (GSM+CDMA)"},
      {"iPad3,1", "iPad 3 (WiFi)"},
      {"iPad3,2", "iPad 3 (GSM+CDMA)"},
      {"iPad3,3", "iPad 3 (GSM)"},
      {"iPad3,4", "iPad 4 (WiFi)"},
      {"iPad3,5", "iPad 4 (GSM)"},
      {"iPad3,6", "iPad 4 (GSM+CDMA)"},
      {"iPad4,1", "iPad Air (WiFi)"},
      {"iPad4,2", "iPad Air (Cellular)"},
      {"iPad4,4", "iPad mini 2G (WiFi)"},
      {"iPad4,5", "iPad mini 2G (Cellular)"},
      {"i386", "Simulator"},
      {"x86_64", "Simulator"},
  };

  for (auto& d : device_names) {
    if (strcmp(d.raw_name, raw_name) == 0)
      return d.real_name;
  }
  LOG(LS_WARNING) << "Failed to find device name (" << raw_name << ")";
  return "";
}

// TODO(henrika): move to shared location.
// See https://code.google.com/p/webrtc/issues/detail?id=4773 for details.
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
    LOG(LS_ERROR) << StdStringFromNSString(msg);
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

std::string GetAudioSessionCategory() {
  NSString* category = [[AVAudioSession sharedInstance] category];
  return StdStringFromNSString(category);
}

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

std::string GetDeviceName() {
  size_t size;
  sysctlbyname("hw.machine", NULL, &size, NULL, 0);
  std::unique_ptr<char[]> machine;
  machine.reset(new char[size]);
  sysctlbyname("hw.machine", machine.get(), &size, NULL, 0);
  return std::string(LookUpRealName(machine.get()));
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
  LOG(LS_WARNING) << "webrtc::ios::GetLowPowerModeEnabled() is not "
                     "supported. Requires at least iOS 9.0";
  return false;
}
#endif

}  // namespace ios
}  // namespace webrtc

#endif  // defined(WEBRTC_IOS)
