/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_FALLTHROUGH_H_
#define RTC_BASE_SYSTEM_FALLTHROUGH_H_

// Macro to be used for switch-case fallthrough (required for enabling
// -Wimplicit-fallthrough warning on Clang).

// This macro definition must not be included from public headers! Because
// clang's diagnostic checks if there's a macro expanding to
// [[clang::fallthrough]] defined, and if so it suggests the first macro
// expanding to it. So if this macro is included in a public header, clang may
// suggest it instead of the client's own macro, which can cause confusion.

#ifdef __clang__
#define RTC_FALLTHROUGH() [[clang::fallthrough]]
#else
#define RTC_FALLTHROUGH() \
  do {                    \
  } while (0)
#endif

#endif  // RTC_BASE_SYSTEM_FALLTHROUGH_H_
