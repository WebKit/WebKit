/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_NEAR_MATCHER_H_
#define TEST_NEAR_MATCHER_H_

#include <ostream>

#include "absl/strings/str_cat.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {

template <typename AbsoluteT, typename RelativeT>
class NearMatcher {
 public:
  using is_gtest_matcher = void;

  NearMatcher(AbsoluteT expected, RelativeT max_error)
      : expected_(expected), max_error_(max_error) {}

  NearMatcher(const NearMatcher&) = default;
  NearMatcher& operator=(const NearMatcher&) = default;

  template <typename T>
  bool MatchAndExplain(const T& value, std::ostream* os) const {
    AbsoluteT upper_bound = expected_ + max_error_;
    if (value >= upper_bound) {
      if (os != nullptr) {
        *os << " >= upper bound " << absl::StrCat(upper_bound);
      }
      return false;
    }
    if constexpr (std::is_same_v<AbsoluteT, Timestamp>) {
      if (expected_ - AbsoluteT::Zero() < max_error_) {
        // Avoid negative `expected_ - max_error_`.
        bool in_range = value >= AbsoluteT::Zero();
        if (os != nullptr) {
          if (in_range) {
            *os << " in range [" << absl::StrCat(AbsoluteT::Zero()) << ","
                << absl::StrCat(upper_bound) << ")";
          } else {
            *os << " < lower bound " << absl::StrCat(AbsoluteT::Zero());
          }
        }
        return in_range;
      }
    }
    AbsoluteT lower_bound = expected_ - max_error_;
    bool in_range = value > lower_bound;
    if (os != nullptr) {
      if (in_range) {
        *os << " in range (" << absl::StrCat(lower_bound) << ","
            << absl::StrCat(upper_bound) << ")";
      } else {
        *os << " <= lower bound " << absl::StrCat(lower_bound);
      }
    }
    return in_range;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "is approximately " << absl::StrCat(expected_)
        << " (absolute error < " << absl::StrCat(max_error_) << ")";
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "isn't approximately " << absl::StrCat(expected_)
        << " (absolute error >= " << absl::StrCat(max_error_) << ")";
  }

 private:
  AbsoluteT expected_;
  RelativeT max_error_;
};

// Generic 'Near' matcher
template <typename AbsoluteT, typename RelativeT>
NearMatcher<AbsoluteT, RelativeT> Near(AbsoluteT expected,
                                       RelativeT max_error) {
  return NearMatcher<AbsoluteT, RelativeT>(expected, max_error);
}

// Specialization of 'Near' matcher for time types with default margin of 1ms.
template <typename T>
  requires(std::is_same_v<T, Timestamp> || std::is_same_v<T, TimeDelta>)
NearMatcher<T, TimeDelta> Near(T expected) {
  return Near(expected, /*max_error=*/TimeDelta::Millis(1));
}

}  // namespace webrtc

#endif  // TEST_NEAR_MATCHER_H_
