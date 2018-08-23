/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSInteger, RTCDeviceType) {
  RTCDeviceTypeUnknown,
  RTCDeviceTypeIPhone1G,
  RTCDeviceTypeIPhone3G,
  RTCDeviceTypeIPhone3GS,
  RTCDeviceTypeIPhone4,
  RTCDeviceTypeIPhone4Verizon,
  RTCDeviceTypeIPhone4S,
  RTCDeviceTypeIPhone5GSM,
  RTCDeviceTypeIPhone5GSM_CDMA,
  RTCDeviceTypeIPhone5CGSM,
  RTCDeviceTypeIPhone5CGSM_CDMA,
  RTCDeviceTypeIPhone5SGSM,
  RTCDeviceTypeIPhone5SGSM_CDMA,
  RTCDeviceTypeIPhone6Plus,
  RTCDeviceTypeIPhone6,
  RTCDeviceTypeIPhone6S,
  RTCDeviceTypeIPhone6SPlus,
  RTCDeviceTypeIPhone7,
  RTCDeviceTypeIPhone7Plus,
  RTCDeviceTypeIPhoneSE,
  RTCDeviceTypeIPhone8,
  RTCDeviceTypeIPhone8Plus,
  RTCDeviceTypeIPhoneX,
  RTCDeviceTypeIPodTouch1G,
  RTCDeviceTypeIPodTouch2G,
  RTCDeviceTypeIPodTouch3G,
  RTCDeviceTypeIPodTouch4G,
  RTCDeviceTypeIPodTouch5G,
  RTCDeviceTypeIPodTouch6G,
  RTCDeviceTypeIPad,
  RTCDeviceTypeIPad2Wifi,
  RTCDeviceTypeIPad2GSM,
  RTCDeviceTypeIPad2CDMA,
  RTCDeviceTypeIPad2Wifi2,
  RTCDeviceTypeIPadMiniWifi,
  RTCDeviceTypeIPadMiniGSM,
  RTCDeviceTypeIPadMiniGSM_CDMA,
  RTCDeviceTypeIPad3Wifi,
  RTCDeviceTypeIPad3GSM_CDMA,
  RTCDeviceTypeIPad3GSM,
  RTCDeviceTypeIPad4Wifi,
  RTCDeviceTypeIPad4GSM,
  RTCDeviceTypeIPad4GSM_CDMA,
  RTCDeviceTypeIPad5,
  RTCDeviceTypeIPad6,
  RTCDeviceTypeIPadAirWifi,
  RTCDeviceTypeIPadAirCellular,
  RTCDeviceTypeIPadAirWifiCellular,
  RTCDeviceTypeIPadAir2,
  RTCDeviceTypeIPadMini2GWifi,
  RTCDeviceTypeIPadMini2GCellular,
  RTCDeviceTypeIPadMini2GWifiCellular,
  RTCDeviceTypeIPadMini3,
  RTCDeviceTypeIPadMini4,
  RTCDeviceTypeIPadPro9Inch,
  RTCDeviceTypeIPadPro12Inch,
  RTCDeviceTypeIPadPro12Inch2,
  RTCDeviceTypeIPadPro10Inch,
  RTCDeviceTypeSimulatori386,
  RTCDeviceTypeSimulatorx86_64,
};

@interface UIDevice (RTCDevice)

+ (RTCDeviceType)deviceType;
+ (BOOL)isIOS11OrLater;

@end
