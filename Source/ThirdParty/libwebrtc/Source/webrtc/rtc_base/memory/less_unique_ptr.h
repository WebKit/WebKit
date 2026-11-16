/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MEMORY_LESS_UNIQUE_PTR_H_
#define RTC_BASE_MEMORY_LESS_UNIQUE_PTR_H_

#include <memory>

namespace webrtc {

// Helper class that can compare unique_ptr and raw ptr. When used as comparator
// in std::map, std::set with std::unique_ptr<T> as a key, it allows
// heterogeneous lookup by a raw pointer.
struct less_unique_ptr {
  // Allow heterogeneous lookup, https://abseil.io/tips/144
  using is_transparent = void;

  template <typename T, typename U>
  bool operator()(const std::unique_ptr<T>& lhs,
                  const std::unique_ptr<U>& rhs) const {
    return lhs < rhs;
  }

  template <typename T, typename U>
  bool operator()(const std::unique_ptr<T>& lhs, const U* rhs) const {
    return lhs.get() < rhs;
  }

  template <typename T, typename U>
  bool operator()(T* lhs, const std::unique_ptr<U>& rhs) const {
    return lhs < rhs.get();
  }
};

}  //  namespace webrtc

#endif  // RTC_BASE_MEMORY_LESS_UNIQUE_PTR_H_
