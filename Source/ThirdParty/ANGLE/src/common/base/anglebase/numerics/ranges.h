// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_RANGES_H_
#define BASE_NUMERICS_RANGES_H_

#include <algorithm>
#include <cmath>

namespace base
{

template <typename T>
constexpr bool IsApproximatelyEqual(T lhs, T rhs, T tolerance)
{
    static_assert(std::is_arithmetic<T>::value, "Argument must be arithmetic");
    return std::abs(rhs - lhs) <= tolerance;
}

}  // namespace base

#endif  // BASE_NUMERICS_RANGES_H_
