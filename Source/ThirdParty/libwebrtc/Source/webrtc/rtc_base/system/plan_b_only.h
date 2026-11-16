/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_PLAN_B_ONLY_H_
#define RTC_BASE_SYSTEM_PLAN_B_ONLY_H_

#if defined(WEBRTC_DEPRECATE_PLAN_B)
#define PLAN_B_ONLY [[deprecated]]
#else
#define PLAN_B_ONLY
#endif

#ifdef __clang__
#define RTC_ALLOW_PLAN_B_DEPRECATION_BEGIN() \
  _Pragma("clang diagnostic push")           \
      _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#define RTC_ALLOW_PLAN_B_DEPRECATION_END() _Pragma("clang diagnostic pop")
#elif defined(_MSC_VER)
#define RTC_ALLOW_PLAN_B_DEPRECATION_BEGIN() \
  __pragma(warning(push)) __pragma(warning(disable : 4996))
#define RTC_ALLOW_PLAN_B_DEPRECATION_END() __pragma(warning(pop))
#else
#define RTC_ALLOW_PLAN_B_DEPRECATION_BEGIN()
#define RTC_ALLOW_PLAN_B_DEPRECATION_END()
#endif

#endif  // RTC_BASE_SYSTEM_PLAN_B_ONLY_H_
