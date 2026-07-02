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

#include <wtf/Int128.h>

namespace WTF {

namespace dragonbox {

namespace detail {

////////////////////////////////////////////////////////////////////////////////////////
// Utilities for wide unsigned integer arithmetic.
////////////////////////////////////////////////////////////////////////////////////////

namespace wuint {

inline constexpr uint64_t umul64(uint32_t x, uint32_t y) noexcept
{
    return x * static_cast<uint64_t>(y);
}

// Get 128-bit result of multiplication of two 64-bit unsigned integers.
inline constexpr UInt128 umul128(uint64_t x, uint64_t y) noexcept
{
    return static_cast<UInt128>(x) * static_cast<UInt128>(y);
}

inline constexpr uint64_t umul128_upper64(uint64_t x, uint64_t y) noexcept
{
    return static_cast<uint64_t>((static_cast<UInt128>(x) * static_cast<UInt128>(y)) >> 64);
}

// Get upper 128-bits of multiplication of a 64-bit unsigned integer and a 128-bit
// unsigned integer.
inline constexpr UInt128 umul192_upper128(uint64_t x, UInt128 y) noexcept
{
    uint64_t y_high = static_cast<uint64_t>(y >> 64);
    uint64_t y_low = static_cast<uint64_t>(y);
    auto r = umul128(x, y_high);
    r = r + static_cast<UInt128>(umul128_upper64(x, y_low));
    return r;
}

// Get upper 64-bits of multiplication of a 32-bit unsigned integer and a 64-bit
// unsigned integer.
inline constexpr uint64_t umul96_upper64(uint32_t x, uint64_t y) noexcept
{
    return umul128_upper64(static_cast<uint64_t>(x) << 32, y);
}

// Get lower 128-bits of multiplication of a 64-bit unsigned integer and a 128-bit
// unsigned integer.
inline constexpr UInt128 umul192_lower128(uint64_t x, UInt128 y) noexcept
{
    return static_cast<UInt128>(x) * y;
}

// Get lower 64-bits of multiplication of a 32-bit unsigned integer and a 64-bit
// unsigned integer.
constexpr uint64_t umul96_lower64(uint32_t x, uint64_t y) noexcept
{
    return static_cast<uint64_t>(x) * y;
}

} // namespace wuint

} // namespace detail

} // namespace dragonbox

} // namespace WTF
