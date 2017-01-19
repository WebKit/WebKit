/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_OBJC_RTC_UI_APPLICATION_H_
#define WEBRTC_BASE_OBJC_RTC_UI_APPLICATION_H_

#include "WebRTC/RTCMacros.h"

#if defined(WEBRTC_IOS)
/** Convenience function to get UIApplicationState from C++. */
RTC_EXTERN bool RTCIsUIApplicationActive();
#endif  // WEBRTC_IOS

#endif  // WEBRTC_BASE_OBJC_RTC_UI_APPLICATION_H_
