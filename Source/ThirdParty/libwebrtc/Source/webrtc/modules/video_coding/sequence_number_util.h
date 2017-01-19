/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_SEQUENCE_NUMBER_UTIL_H_
#define WEBRTC_MODULES_VIDEO_CODING_SEQUENCE_NUMBER_UTIL_H_

#include <limits>
#include <type_traits>

#include "webrtc/base/mod_ops.h"

namespace webrtc {

// Test if the sequence number |a| is ahead or at sequence number |b|.
//
// If |M| is an even number and the two sequence numbers are at max distance
// from each other, then the sequence number with the highest value is
// considered to be ahead.
template <typename T, T M>
inline bool AheadOrAt(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  const T maxDist = M / 2;
  if (!(M & 1) && MinDiff<T, M>(a, b) == maxDist)
    return b < a;
  return ForwardDiff<T, M>(b, a) <= maxDist;
}

template <typename T>
inline bool AheadOrAt(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  const T maxDist = std::numeric_limits<T>::max() / 2 + T(1);
  if (a - b == maxDist)
    return b < a;
  return ForwardDiff(b, a) < maxDist;
}

// Test if the sequence number |a| is ahead of sequence number |b|.
//
// If |M| is an even number and the two sequence numbers are at max distance
// from each other, then the sequence number with the highest value is
// considered to be ahead.
template <typename T, T M>
inline bool AheadOf(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  return a != b && AheadOrAt<T, M>(a, b);
}

template <typename T>
inline bool AheadOf(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  return a != b && AheadOrAt(a, b);
}

namespace internal {

template <typename T, typename M>
struct SeqNumComp;

template <typename T, T M>
struct SeqNumComp<T, std::integral_constant<T, M>> {
  bool operator()(T a, T b) const { return AheadOf<T, M>(a, b); }
};

template <typename T>
struct SeqNumComp<T, std::integral_constant<T, T(0)>> {
  bool operator()(T a, T b) const { return AheadOf<T>(a, b); }
};

}  // namespace internal

// Comparator used to compare sequence numbers in a continuous fashion.
//
// WARNING! If used to sort sequence numbers of length M then the interval
//          covered by the sequence numbers may not be larger than floor(M/2).
template <typename T, T M = 0>
struct AscendingSeqNumComp
    : private internal::SeqNumComp<T, std::integral_constant<T, M>> {
  bool operator()(T a, T b) const {
    return internal::SeqNumComp<T, std::integral_constant<T, M>>::operator()(a,
                                                                             b);
  }
};

// Comparator used to compare sequence numbers in a continuous fashion.
//
// WARNING! If used to sort sequence numbers of length M then the interval
//          covered by the sequence numbers may not be larger than floor(M/2).
template <typename T, T M = 0>
struct DescendingSeqNumComp
    : private internal::SeqNumComp<T, std::integral_constant<T, M>> {
  bool operator()(T a, T b) const {
    return internal::SeqNumComp<T, std::integral_constant<T, M>>::operator()(b,
                                                                             a);
  }
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_SEQUENCE_NUMBER_UTIL_H_
