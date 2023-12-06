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

namespace WTF {

namespace dragonbox {

////////////////////////////////////////////////////////////////////////////////////////
// Return types for the main interface function.
////////////////////////////////////////////////////////////////////////////////////////

template<class UInt, bool is_signed, bool trailing_zero_flag>
struct decimal_fp;

template<class UInt>
struct decimal_fp<UInt, false, false> {
    using carrier_uint = UInt;

    carrier_uint significand;
    int32_t exponent;
};

template<class UInt>
struct decimal_fp<UInt, true, false> {
    using carrier_uint = UInt;

    carrier_uint significand;
    int32_t exponent;
    bool is_negative;
};

template<class UInt>
struct decimal_fp<UInt, false, true> {
    using carrier_uint = UInt;

    carrier_uint significand;
    int32_t exponent;
    bool may_have_trailing_zeros;
};

template<class UInt>
struct decimal_fp<UInt, true, true> {
    using carrier_uint = UInt;

    carrier_uint significand;
    int32_t exponent;
    bool may_have_trailing_zeros;
    bool is_negative;
};

template<class UInt, bool trailing_zero_flag = false>
using unsigned_decimal_fp = decimal_fp<UInt, false, trailing_zero_flag>;

template<class UInt, bool trailing_zero_flag = false>
using signed_decimal_fp = decimal_fp<UInt, true, trailing_zero_flag>;

template<class UInt>
constexpr signed_decimal_fp<UInt, false>
add_sign_to_unsigned_decimal_fp(bool is_negative, unsigned_decimal_fp<UInt, false> r) noexcept
{
    return { r.significand, r.exponent, is_negative };
}

template<class UInt>
constexpr signed_decimal_fp<UInt, true>
add_sign_to_unsigned_decimal_fp(bool is_negative, unsigned_decimal_fp<UInt, true> r) noexcept
{
    return { r.significand, r.exponent, r.may_have_trailing_zeros, is_negative };
}

namespace detail {

template<class UnsignedDecimalFp>
struct unsigned_decimal_fp_to_signed;

template<class UInt, bool trailing_zero_flag>
struct unsigned_decimal_fp_to_signed<unsigned_decimal_fp<UInt, trailing_zero_flag>> {
    using type = signed_decimal_fp<UInt, trailing_zero_flag>;
};

template<class UnsignedDecimalFp>
using unsigned_decimal_fp_to_signed_t = typename unsigned_decimal_fp_to_signed<UnsignedDecimalFp>::type;

} // namespace detail

} // namespace dragonbox

} // namespace WTF
