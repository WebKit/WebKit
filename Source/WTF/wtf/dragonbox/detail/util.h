/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * License header from dragonbox
 *    https://github.com/jk-jeon/dragonbox/blob/master/LICENSE-Boost
 *    https://github.com/jk-jeon/dragonbox/blob/master/LICENSE-Apache2-LLVM
 */

#pragma once

#include <algorithm>
#include <wtf/dtoa/utils.h>

namespace WTF {

namespace dragonbox {

namespace detail {

////////////////////////////////////////////////////////////////////////////////////////
// Some basic features for encoding/decoding IEEE-754 formats.
////////////////////////////////////////////////////////////////////////////////////////

template<class T>
typename std::add_rvalue_reference<T>::type declval() noexcept;

template<class T>
struct physical_bits {
    static constexpr size_t value = sizeof(T) * std::numeric_limits<unsigned char>::digits;
};

template<class T>
struct value_bits {
    static constexpr size_t value = std::numeric_limits<typename std::enable_if<std::is_unsigned<T>::value, T>::type>::digits;
};

////////////////////////////////////////////////////////////////////////////////////////
// Some simple utilities for constexpr computation.
////////////////////////////////////////////////////////////////////////////////////////

template <int32_t k, class Int>
constexpr Int compute_power(Int a) noexcept {
    static_assert(k >= 0, "");
    Int p = 1;
    for (int32_t i = 0; i < k; ++i)
        p *= a;
    return p;
}

template<int32_t a, class UInt>
constexpr int32_t count_factors(UInt n) noexcept
{
    static_assert(a > 1, "");
    int32_t c = 0;
    while (!(n % a)) {
        n /= a;
        ++c;
    }
    return c;
}

} // namespace detail

} // namespace dragonbox

} // namespace WTF
