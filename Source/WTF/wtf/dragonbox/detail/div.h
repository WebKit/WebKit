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

#include <wtf/dragonbox/detail/log.h>
#include <wtf/dragonbox/detail/util.h>
#include <wtf/dragonbox/detail/wuint.h>

namespace WTF {

namespace dragonbox {

namespace detail {

////////////////////////////////////////////////////////////////////////////////////////
// Utilities for fast divisibility tests.
////////////////////////////////////////////////////////////////////////////////////////

namespace div {

// Replace n by floor(n / 10^N).
// Returns true if and only if n is divisible by 10^N.
// Precondition: n <= 10^(N+1)
// !!It takes an in-out parameter!!
template<int32_t N>
struct divide_by_pow10_info;

template<>
struct divide_by_pow10_info<1> {
    static constexpr uint32_t magic_number = 6554;
    static constexpr int32_t shift_amount = 16;
};

template<>
struct divide_by_pow10_info<2> {
    static constexpr uint32_t magic_number = 656;
    static constexpr int32_t shift_amount = 16;
};

template<int32_t N>
constexpr bool check_divisibility_and_divide_by_pow10(uint32_t& n) noexcept
{
    // Make sure the computation for max_n does not overflow.
    static_assert(N + 1 <= log::floor_log10_pow2(31), "");
    ASSERT(n <= compute_power<N + 1>(static_cast<uint32_t>(10)));

    using info = divide_by_pow10_info<N>;
    n *= info::magic_number;

    constexpr auto mask = static_cast<uint32_t>(static_cast<uint32_t>(1) << info::shift_amount) - 1;
    bool result = ((n & mask) < info::magic_number);

    n >>= info::shift_amount;
    return result;
}

// Compute floor(n / 10^N) for small n and N.
// Precondition: n <= 10^(N+1)
template<int32_t N>
constexpr uint32_t small_division_by_pow10(uint32_t n) noexcept
{
    // Make sure the computation for max_n does not overflow.
    static_assert(N + 1 <= log::floor_log10_pow2(31), "");
    ASSERT(n <= compute_power<N + 1>(static_cast<uint32_t>(10)));

    return (n * divide_by_pow10_info<N>::magic_number) >> divide_by_pow10_info<N>::shift_amount;
}

// Compute floor(n / 10^N) for small N.
// Precondition: n <= n_max
template<int32_t N, class UInt, UInt n_max>
constexpr UInt divide_by_pow10(UInt n) noexcept
{
    static_assert(N >= 0, "");

    // Specialize for 32-bit division by 100.
    // Compiler is supposed to generate the identical code for just writing
    // "n / 100", but for some reason MSVC generates an inefficient code
    // (mul + mov for no apparent reason, instead of single imul),
    // so we does this manually.
    if constexpr (std::is_same<UInt, uint32_t>::value && N == 2) {
        return static_cast<uint32_t>(wuint::umul64(n, static_cast<uint32_t>(1374389535)) >> 37);
    } else if constexpr (std::is_same<UInt, uint64_t>::value && N == 3 && n_max <= static_cast<uint64_t>(15534100272597517998ull)) {
        // Specialize for 64-bit division by 1000.
        // Ensure that the correctness condition is met.
        return wuint::umul128_upper64(n, static_cast<uint64_t>(2361183241434822607ull)) >> 7;
    } else {
        constexpr auto divisor = compute_power<N>(UInt(10));
        return n / divisor;
    }
}

} // namespace div

} // namespace detail

} // namespace dragonbox

} // namespace WTF
