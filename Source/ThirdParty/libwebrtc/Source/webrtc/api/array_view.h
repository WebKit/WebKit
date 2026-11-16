/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ARRAY_VIEW_H_
#define API_ARRAY_VIEW_H_

#include <cstddef>
#include <span>

#include "absl/base/macros.h"

namespace webrtc {

// A parameter pack of extents is used to ensure that ArrayView<T> inlines to
// span<T>, but only 0 or 1 extent is supported.
template <typename T, size_t... extent>
using ArrayView ABSL_DEPRECATE_AND_INLINE() = std::span<T, extent...>;

template <typename T>
ABSL_DEPRECATE_AND_INLINE()
inline std::span<T> MakeArrayView(T* data, size_t size) {
  return std::span(data, size);
}

}  //  namespace webrtc


#endif  // API_ARRAY_VIEW_H_
