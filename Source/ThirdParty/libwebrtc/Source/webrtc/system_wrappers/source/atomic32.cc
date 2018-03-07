/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/atomic32.h"

#include <assert.h>

#include "common_types.h"  // NOLINT(build/include)

namespace webrtc {

Atomic32::Atomic32(int32_t initial_value) : value_(initial_value) {}

Atomic32::~Atomic32() {}

int32_t Atomic32::operator++() {
  return ++value_;
}

int32_t Atomic32::operator--() {
  return --value_;
}

int32_t Atomic32::operator+=(int32_t value) {
  return value_ += value;
}

int32_t Atomic32::operator-=(int32_t value) {
  return value_ -= value;
}

bool Atomic32::CompareExchange(int32_t new_value, int32_t compare_value) {
  return value_.compare_exchange_strong(compare_value, new_value);
}

int32_t Atomic32::Value() const {
  return value_.load();
}

}  // namespace webrtc
