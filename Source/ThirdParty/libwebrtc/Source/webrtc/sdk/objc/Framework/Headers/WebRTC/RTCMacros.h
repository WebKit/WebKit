/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_FRAMEWORK_HEADERS_WEBRTC_RTCMACROS_H_
#define SDK_OBJC_FRAMEWORK_HEADERS_WEBRTC_RTCMACROS_H_

#define RTC_EXPORT __attribute__((visibility("default")))

#if defined(__cplusplus)
#define RTC_EXTERN extern "C" RTC_EXPORT
#else
#define RTC_EXTERN extern RTC_EXPORT
#endif

#ifdef __OBJC__
#define RTC_FWD_DECL_OBJC_CLASS(classname) @class classname
#else
#define RTC_FWD_DECL_OBJC_CLASS(classname) typedef struct objc_object classname
#endif

#endif  // SDK_OBJC_FRAMEWORK_HEADERS_WEBRTC_RTCMACROS_H_
