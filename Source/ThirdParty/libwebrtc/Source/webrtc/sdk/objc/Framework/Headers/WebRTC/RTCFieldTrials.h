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

typedef NS_OPTIONS(NSUInteger, RTCFieldTrialOptions) {
  RTCFieldTrialOptionsNone = 0,
  RTCFieldTrialOptionsImprovedBitrateEstimate = 1 << 0,
};

/** Must be called before any other call into WebRTC. See:
 *  webrtc/system_wrappers/include/field_trial_default.h
 */
RTC_EXTERN void RTCInitFieldTrials(RTCFieldTrialOptions options);
