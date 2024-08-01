/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCFieldTrials.h"

#include <memory>

#import "base/RTCLogging.h"

#include "system_wrappers/include/field_trial.h"

NSString * const kRTCFieldTrialAudioSendSideBweKey = @"WebRTC-Audio-SendSideBwe";
NSString * const kRTCFieldTrialAudioForceNoTWCCKey = @"WebRTC-Audio-ForceNoTWCC";
NSString * const kRTCFieldTrialAudioForceABWENoTWCCKey = @"WebRTC-Audio-ABWENoTWCC";
NSString * const kRTCFieldTrialSendSideBweWithOverheadKey = @"WebRTC-SendSideBwe-WithOverhead";
NSString * const kRTCFieldTrialFlexFec03AdvertisedKey = @"WebRTC-FlexFEC-03-Advertised";
NSString * const kRTCFieldTrialFlexFec03Key = @"WebRTC-FlexFEC-03";
NSString * const kRTCFieldTrialH264HighProfileKey = @"WebRTC-H264HighProfile";
NSString * const kRTCFieldTrialMinimizeResamplingOnMobileKey =
    @"WebRTC-Audio-MinimizeResamplingOnMobile";
NSString * const kRTCFieldTrialEnabledValue = @"Enabled";

static std::unique_ptr<char[]> gFieldTrialInitString;

void RTCInitFieldTrialDictionary(NSDictionary<NSString *, NSString *> *fieldTrials) {
  if (!fieldTrials) {
    RTCLogWarning(@"No fieldTrials provided.");
    return;
  }
  // Assemble the keys and values into the field trial string.
  // We don't perform any extra format checking. That should be done by the underlying WebRTC calls.
  NSMutableString *fieldTrialInitString = [NSMutableString string];
  for (NSString *key in fieldTrials) {
    NSString *fieldTrialEntry = [NSString stringWithFormat:@"%@/%@/", key, fieldTrials[key]];
    [fieldTrialInitString appendString:fieldTrialEntry];
  }
  size_t len = fieldTrialInitString.length + 1;
  gFieldTrialInitString.reset(new char[len]);
  if (![fieldTrialInitString getCString:gFieldTrialInitString.get()
                              maxLength:len
                               encoding:NSUTF8StringEncoding]) {
    RTCLogError(@"Failed to convert field trial string.");
    return;
  }
  webrtc::field_trial::InitFieldTrialsFromString(gFieldTrialInitString.get());
}
