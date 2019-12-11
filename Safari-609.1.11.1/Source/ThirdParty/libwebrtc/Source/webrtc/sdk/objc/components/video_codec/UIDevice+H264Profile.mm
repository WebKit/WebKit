/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "UIDevice+H264Profile.h"
#import "helpers/UIDevice+RTCDevice.h"

#include <algorithm>

namespace {

using namespace webrtc::H264;

struct SupportedH264Profile {
  const RTCDeviceType deviceType;
  const ProfileLevelId profile;
};

constexpr SupportedH264Profile kH264MaxSupportedProfiles[] = {
    // iPhones with at least iOS 9
    {RTCDeviceTypeIPhoneXS, {kProfileHigh, kLevel5_2}},      // https://support.apple.com/kb/SP779
    {RTCDeviceTypeIPhoneXSMax, {kProfileHigh, kLevel5_2}},   // https://support.apple.com/kb/SP780
    {RTCDeviceTypeIPhoneXR, {kProfileHigh, kLevel5_2}},      // https://support.apple.com/kb/SP781
    {RTCDeviceTypeIPhoneX, {kProfileHigh, kLevel5_2}},       // https://support.apple.com/kb/SP770
    {RTCDeviceTypeIPhone8, {kProfileHigh, kLevel5_2}},       // https://support.apple.com/kb/SP767
    {RTCDeviceTypeIPhone8Plus, {kProfileHigh, kLevel5_2}},   // https://support.apple.com/kb/SP768
    {RTCDeviceTypeIPhone7, {kProfileHigh, kLevel5_1}},       // https://support.apple.com/kb/SP743
    {RTCDeviceTypeIPhone7Plus, {kProfileHigh, kLevel5_1}},   // https://support.apple.com/kb/SP744
    {RTCDeviceTypeIPhoneSE, {kProfileHigh, kLevel4_2}},      // https://support.apple.com/kb/SP738
    {RTCDeviceTypeIPhone6S, {kProfileHigh, kLevel4_2}},      // https://support.apple.com/kb/SP726
    {RTCDeviceTypeIPhone6SPlus, {kProfileHigh, kLevel4_2}},  // https://support.apple.com/kb/SP727
    {RTCDeviceTypeIPhone6, {kProfileHigh, kLevel4_2}},       // https://support.apple.com/kb/SP705
    {RTCDeviceTypeIPhone6Plus, {kProfileHigh, kLevel4_2}},   // https://support.apple.com/kb/SP706
    {RTCDeviceTypeIPhone5SGSM, {kProfileHigh, kLevel4_2}},   // https://support.apple.com/kb/SP685
    {RTCDeviceTypeIPhone5SGSM_CDMA,
     {kProfileHigh, kLevel4_2}},                           // https://support.apple.com/kb/SP685
    {RTCDeviceTypeIPhone5GSM, {kProfileHigh, kLevel4_1}},  // https://support.apple.com/kb/SP655
    {RTCDeviceTypeIPhone5GSM_CDMA,
     {kProfileHigh, kLevel4_1}},                            // https://support.apple.com/kb/SP655
    {RTCDeviceTypeIPhone5CGSM, {kProfileHigh, kLevel4_1}},  // https://support.apple.com/kb/SP684
    {RTCDeviceTypeIPhone5CGSM_CDMA,
     {kProfileHigh, kLevel4_1}},                         // https://support.apple.com/kb/SP684
    {RTCDeviceTypeIPhone4S, {kProfileHigh, kLevel4_1}},  // https://support.apple.com/kb/SP643

    // iPods with at least iOS 9
    {RTCDeviceTypeIPodTouch6G, {kProfileMain, kLevel4_1}},  // https://support.apple.com/kb/SP720
    {RTCDeviceTypeIPodTouch5G, {kProfileMain, kLevel3_1}},  // https://support.apple.com/kb/SP657

    // iPads with at least iOS 9
    {RTCDeviceTypeIPad2Wifi, {kProfileHigh, kLevel4_1}},     // https://support.apple.com/kb/SP622
    {RTCDeviceTypeIPad2GSM, {kProfileHigh, kLevel4_1}},      // https://support.apple.com/kb/SP622
    {RTCDeviceTypeIPad2CDMA, {kProfileHigh, kLevel4_1}},     // https://support.apple.com/kb/SP622
    {RTCDeviceTypeIPad2Wifi2, {kProfileHigh, kLevel4_1}},    // https://support.apple.com/kb/SP622
    {RTCDeviceTypeIPadMiniWifi, {kProfileHigh, kLevel4_1}},  // https://support.apple.com/kb/SP661
    {RTCDeviceTypeIPadMiniGSM, {kProfileHigh, kLevel4_1}},   // https://support.apple.com/kb/SP661
    {RTCDeviceTypeIPadMiniGSM_CDMA,
     {kProfileHigh, kLevel4_1}},                              // https://support.apple.com/kb/SP661
    {RTCDeviceTypeIPad3Wifi, {kProfileHigh, kLevel4_1}},      // https://support.apple.com/kb/SP647
    {RTCDeviceTypeIPad3GSM_CDMA, {kProfileHigh, kLevel4_1}},  // https://support.apple.com/kb/SP647
    {RTCDeviceTypeIPad3GSM, {kProfileHigh, kLevel4_1}},       // https://support.apple.com/kb/SP647
    {RTCDeviceTypeIPad4Wifi, {kProfileHigh, kLevel4_1}},      // https://support.apple.com/kb/SP662
    {RTCDeviceTypeIPad4GSM, {kProfileHigh, kLevel4_1}},       // https://support.apple.com/kb/SP662
    {RTCDeviceTypeIPad4GSM_CDMA, {kProfileHigh, kLevel4_1}},  // https://support.apple.com/kb/SP662
    {RTCDeviceTypeIPad5, {kProfileHigh, kLevel4_2}},          // https://support.apple.com/kb/SP751
    {RTCDeviceTypeIPad6, {kProfileHigh, kLevel4_2}},          // https://support.apple.com/kb/SP774
    {RTCDeviceTypeIPadAirWifi, {kProfileHigh, kLevel4_2}},    // https://support.apple.com/kb/SP692
    {RTCDeviceTypeIPadAirCellular,
     {kProfileHigh, kLevel4_2}},  // https://support.apple.com/kb/SP692
    {RTCDeviceTypeIPadAirWifiCellular,
     {kProfileHigh, kLevel4_2}},                               // https://support.apple.com/kb/SP692
    {RTCDeviceTypeIPadAir2, {kProfileHigh, kLevel4_2}},        // https://support.apple.com/kb/SP708
    {RTCDeviceTypeIPadMini2GWifi, {kProfileHigh, kLevel4_2}},  // https://support.apple.com/kb/SP693
    {RTCDeviceTypeIPadMini2GCellular,
     {kProfileHigh, kLevel4_2}},  // https://support.apple.com/kb/SP693
    {RTCDeviceTypeIPadMini2GWifiCellular,
     {kProfileHigh, kLevel4_2}},                               // https://support.apple.com/kb/SP693
    {RTCDeviceTypeIPadMini3, {kProfileHigh, kLevel4_2}},       // https://support.apple.com/kb/SP709
    {RTCDeviceTypeIPadMini4, {kProfileHigh, kLevel4_2}},       // https://support.apple.com/kb/SP725
    {RTCDeviceTypeIPadPro9Inch, {kProfileHigh, kLevel4_2}},    // https://support.apple.com/kb/SP739
    {RTCDeviceTypeIPadPro12Inch, {kProfileHigh, kLevel4_2}},   // https://support.apple.com/kb/sp723
    {RTCDeviceTypeIPadPro12Inch2, {kProfileHigh, kLevel4_2}},  // https://support.apple.com/kb/SP761
    {RTCDeviceTypeIPadPro10Inch, {kProfileHigh, kLevel4_2}},   // https://support.apple.com/kb/SP762
};

absl::optional<ProfileLevelId> FindMaxSupportedProfileForDevice(RTCDeviceType deviceType) {
  const auto* result = std::find_if(std::begin(kH264MaxSupportedProfiles),
                                    std::end(kH264MaxSupportedProfiles),
                                    [deviceType](const SupportedH264Profile& supportedProfile) {
                                      return supportedProfile.deviceType == deviceType;
                                    });
  if (result != std::end(kH264MaxSupportedProfiles)) {
    return result->profile;
  }
  return absl::nullopt;
}

}  // namespace

@implementation UIDevice (H264Profile)

+ (absl::optional<webrtc::H264::ProfileLevelId>)maxSupportedH264Profile {
  return FindMaxSupportedProfileForDevice([self deviceType]);
}

@end
