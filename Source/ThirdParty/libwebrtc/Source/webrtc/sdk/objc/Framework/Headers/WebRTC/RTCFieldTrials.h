/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCMacros.h>

/** The only valid value for the following if set is kRTCFieldTrialEnabledValue. */
RTC_EXTERN NSString * const kRTCFieldTrialAudioSendSideBweKey;
RTC_EXTERN NSString * const kRTCFieldTrialSendSideBweWithOverheadKey;
RTC_EXTERN NSString * const kRTCFieldTrialFlexFec03AdvertisedKey;
RTC_EXTERN NSString * const kRTCFieldTrialFlexFec03Key;
RTC_EXTERN NSString * const kRTCFieldTrialImprovedBitrateEstimateKey;
RTC_EXTERN NSString * const kRTCFieldTrialH264HighProfileKey;

/** The valid value for field trials above. */
RTC_EXTERN NSString * const kRTCFieldTrialEnabledValue;

/** Use a string returned by RTCFieldTrialMedianSlopeFilterValue as the value. */
RTC_EXTERN NSString * const kRTCFieldTrialMedianSlopeFilterKey;
RTC_EXTERN NSString *RTCFieldTrialMedianSlopeFilterValue(
    size_t windowSize, double thresholdGain);

/** Use a string returned by RTCFieldTrialTrendlineFilterValue as the value. */
RTC_EXTERN NSString * const kRTCFieldTrialTrendlineFilterKey;
/** Returns a valid value for kRTCFieldTrialTrendlineFilterKey. */
RTC_EXTERN NSString *RTCFieldTrialTrendlineFilterValue(
    size_t windowSize, double smoothingCoeff, double thresholdGain);

/** Initialize field trials using a dictionary mapping field trial keys to their values. See above
 *  for valid keys and values.
 *  Must be called before any other call into WebRTC. See:
 *  webrtc/system_wrappers/include/field_trial_default.h
 */
RTC_EXTERN void RTCInitFieldTrialDictionary(NSDictionary<NSString *, NSString *> *fieldTrials);
