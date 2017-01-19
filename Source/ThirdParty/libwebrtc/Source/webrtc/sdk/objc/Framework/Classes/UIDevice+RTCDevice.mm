/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/UIDevice+RTCDevice.h"

#include <memory>
#include <sys/sysctl.h>

@implementation UIDevice (RTCDevice)

+ (RTCDeviceType)deviceType {
  NSDictionary *machineNameToType = @{
    @"iPhone1,1": @(RTCDeviceTypeIPhone1G),
    @"iPhone1,2": @(RTCDeviceTypeIPhone3G),
    @"iPhone2,1": @(RTCDeviceTypeIPhone3GS),
    @"iPhone3,1": @(RTCDeviceTypeIPhone4),
    @"iPhone3,3": @(RTCDeviceTypeIPhone4Verizon),
    @"iPhone4,1": @(RTCDeviceTypeIPhone4S),
    @"iPhone5,1": @(RTCDeviceTypeIPhone5GSM),
    @"iPhone5,2": @(RTCDeviceTypeIPhone5GSM_CDMA),
    @"iPhone5,3": @(RTCDeviceTypeIPhone5CGSM),
    @"iPhone5,4": @(RTCDeviceTypeIPhone5CGSM_CDMA),
    @"iPhone6,1": @(RTCDeviceTypeIPhone5SGSM),
    @"iPhone6,2": @(RTCDeviceTypeIPhone5SGSM_CDMA),
    @"iPhone7,1": @(RTCDeviceTypeIPhone6Plus),
    @"iPhone7,2": @(RTCDeviceTypeIPhone6),
    @"iPhone8,1": @(RTCDeviceTypeIPhone6S),
    @"iPhone8,2": @(RTCDeviceTypeIPhone6SPlus),
    @"iPod1,1": @(RTCDeviceTypeIPodTouch1G),
    @"iPod2,1": @(RTCDeviceTypeIPodTouch2G),
    @"iPod3,1": @(RTCDeviceTypeIPodTouch3G),
    @"iPod4,1": @(RTCDeviceTypeIPodTouch4G),
    @"iPod5,1": @(RTCDeviceTypeIPodTouch5G),
    @"iPad1,1": @(RTCDeviceTypeIPad),
    @"iPad2,1": @(RTCDeviceTypeIPad2Wifi),
    @"iPad2,2": @(RTCDeviceTypeIPad2GSM),
    @"iPad2,3": @(RTCDeviceTypeIPad2CDMA),
    @"iPad2,4": @(RTCDeviceTypeIPad2Wifi2),
    @"iPad2,5": @(RTCDeviceTypeIPadMiniWifi),
    @"iPad2,6": @(RTCDeviceTypeIPadMiniGSM),
    @"iPad2,7": @(RTCDeviceTypeIPadMiniGSM_CDMA),
    @"iPad3,1": @(RTCDeviceTypeIPad3Wifi),
    @"iPad3,2": @(RTCDeviceTypeIPad3GSM_CDMA),
    @"iPad3,3": @(RTCDeviceTypeIPad3GSM),
    @"iPad3,4": @(RTCDeviceTypeIPad4Wifi),
    @"iPad3,5": @(RTCDeviceTypeIPad4GSM),
    @"iPad3,6": @(RTCDeviceTypeIPad4GSM_CDMA),
    @"iPad4,1": @(RTCDeviceTypeIPadAirWifi),
    @"iPad4,2": @(RTCDeviceTypeIPadAirCellular),
    @"iPad4,4": @(RTCDeviceTypeIPadMini2GWifi),
    @"iPad4,5": @(RTCDeviceTypeIPadMini2GCellular),
    @"i386": @(RTCDeviceTypeSimulatori386),
    @"x86_64": @(RTCDeviceTypeSimulatorx86_64),
  };

  size_t size = 0;
  sysctlbyname("hw.machine", NULL, &size, NULL, 0);
  std::unique_ptr<char[]> machine;
  machine.reset(new char[size]);
  sysctlbyname("hw.machine", machine.get(), &size, NULL, 0);
  NSString *machineName = [[NSString alloc] initWithCString:machine.get()
                                                   encoding:NSUTF8StringEncoding];
  RTCDeviceType deviceType = RTCDeviceTypeUnknown;
  NSNumber *typeNumber = machineNameToType[machineName];
  if (typeNumber) {
    deviceType = static_cast<RTCDeviceType>(typeNumber.integerValue);
  }
  return deviceType;
}

+ (NSString *)stringForDeviceType:(RTCDeviceType)deviceType {
  switch (deviceType) {
    case RTCDeviceTypeUnknown:
      return @"Unknown";
    case RTCDeviceTypeIPhone1G:
      return @"iPhone 1G";
    case RTCDeviceTypeIPhone3G:
      return @"iPhone 3G";
    case RTCDeviceTypeIPhone3GS:
      return @"iPhone 3GS";
    case RTCDeviceTypeIPhone4:
      return @"iPhone 4";
    case RTCDeviceTypeIPhone4Verizon:
      return @"iPhone 4 Verizon";
    case RTCDeviceTypeIPhone4S:
      return @"iPhone 4S";
    case RTCDeviceTypeIPhone5GSM:
      return @"iPhone 5 (GSM)";
    case RTCDeviceTypeIPhone5GSM_CDMA:
      return @"iPhone 5 (GSM+CDMA)";
    case RTCDeviceTypeIPhone5CGSM:
      return @"iPhone 5C (GSM)";
    case RTCDeviceTypeIPhone5CGSM_CDMA:
      return @"iPhone 5C (GSM+CDMA)";
    case RTCDeviceTypeIPhone5SGSM:
      return @"iPhone 5S (GSM)";
    case RTCDeviceTypeIPhone5SGSM_CDMA:
      return @"iPhone 5S (GSM+CDMA)";
    case RTCDeviceTypeIPhone6Plus:
      return @"iPhone 6 Plus";
    case RTCDeviceTypeIPhone6:
      return @"iPhone 6";
    case RTCDeviceTypeIPhone6S:
      return @"iPhone 6S";
    case RTCDeviceTypeIPhone6SPlus:
      return @"iPhone 6S Plus";
    case RTCDeviceTypeIPodTouch1G:
      return @"iPod Touch 1G";
    case RTCDeviceTypeIPodTouch2G:
      return @"iPod Touch 2G";
    case RTCDeviceTypeIPodTouch3G:
      return @"iPod Touch 3G";
    case RTCDeviceTypeIPodTouch4G:
      return @"iPod Touch 4G";
    case RTCDeviceTypeIPodTouch5G:
      return @"iPod Touch 5G";
    case RTCDeviceTypeIPad:
      return @"iPad";
    case RTCDeviceTypeIPad2Wifi:
      return @"iPad 2 (WiFi)";
    case RTCDeviceTypeIPad2GSM:
      return @"iPad 2 (GSM)";
    case RTCDeviceTypeIPad2CDMA:
      return @"iPad 2 (CDMA)";
    case RTCDeviceTypeIPad2Wifi2:
      return @"iPad 2 (WiFi) 2";
    case RTCDeviceTypeIPadMiniWifi:
      return @"iPad Mini (WiFi)";
    case RTCDeviceTypeIPadMiniGSM:
      return @"iPad Mini (GSM)";
    case RTCDeviceTypeIPadMiniGSM_CDMA:
      return @"iPad Mini (GSM+CDMA)";
    case RTCDeviceTypeIPad3Wifi:
      return @"iPad 3 (WiFi)";
    case RTCDeviceTypeIPad3GSM_CDMA:
      return @"iPad 3 (GSM+CDMA)";
    case RTCDeviceTypeIPad3GSM:
      return @"iPad 3 (GSM)";
    case RTCDeviceTypeIPad4Wifi:
      return @"iPad 4 (WiFi)";
    case RTCDeviceTypeIPad4GSM:
      return @"iPad 4 (GSM)";
    case RTCDeviceTypeIPad4GSM_CDMA:
      return @"iPad 4 (GSM+CDMA)";
    case RTCDeviceTypeIPadAirWifi:
      return @"iPad Air (WiFi)";
    case RTCDeviceTypeIPadAirCellular:
      return @"iPad Air (Cellular)";
    case RTCDeviceTypeIPadMini2GWifi:
      return @"iPad Mini 2G (Wifi)";
    case RTCDeviceTypeIPadMini2GCellular:
      return @"iPad Mini 2G (Cellular)";
    case RTCDeviceTypeSimulatori386:
      return @"i386 Simulator";
    case RTCDeviceTypeSimulatorx86_64:
      return @"x86_64 Simulator";
  }
  return @"Unknown";
}

@end
