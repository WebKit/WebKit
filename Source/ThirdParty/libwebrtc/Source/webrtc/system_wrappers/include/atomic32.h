/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Atomic, system independent 32-bit integer.  Unless you know what you're
// doing, use locks instead! :-)
//
// Note: assumes 32-bit (or higher) system
#ifndef SYSTEM_WRAPPERS_INCLUDE_ATOMIC32_H_
#define SYSTEM_WRAPPERS_INCLUDE_ATOMIC32_H_

#include <atomic>

#include <stddef.h>

#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/constructormagic.h"

namespace webrtc {

// DEPRECATED: Please use std::atomic<int32_t> instead.
// TODO(yuweih): Replace Atomic32 uses with std::atomic<int32_t> and remove this
// class. (bugs.webrtc.org/8428)
// 32 bit atomic variable.  Note that this class relies on the compiler to
// align the 32 bit value correctly (on a 32 bit boundary), so as long as you're
// not doing things like reinterpret_cast over some custom allocated memory
// without being careful with alignment, you should be fine.
class Atomic32 {
 public:
  Atomic32(int32_t initial_value = 0);
  ~Atomic32();

  // Prefix operator!
  int32_t operator++();
  int32_t operator--();

  int32_t operator+=(int32_t value);
  int32_t operator-=(int32_t value);

  // Sets the value atomically to new_value if the value equals compare value.
  // The function returns true if the exchange happened.
  bool CompareExchange(int32_t new_value, int32_t compare_value);
  int32_t Value() const;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(Atomic32);

  std::atomic<int32_t> value_;
};

}  // namespace webrtc

#endif  // SYSTEM_WRAPPERS_INCLUDE_ATOMIC32_H_
