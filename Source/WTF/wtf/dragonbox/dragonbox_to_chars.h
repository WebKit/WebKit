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

#include <wtf/dragonbox/dragonbox.h>

namespace WTF {

namespace dragonbox {

namespace detail {

template <class Float, class FloatTraits, Mode mode, PrintTrailingZero print_trailing_zero>
WTF_EXPORT_PRIVATE extern char* to_chars_impl(typename FloatTraits::carrier_uint significand, int32_t exponent, char* buffer);
WTF_EXPORT_PRIVATE extern char* to_shortest(const uint64_t significand, int32_t exponent, char* buffer);

template <>
WTF_EXPORT_PRIVATE char* to_chars_impl<double, default_float_traits<double>, Mode::ToExponential, PrintTrailingZero::No>(uint64_t significand, int32_t exponent, char* buffer);

template <>
WTF_EXPORT_PRIVATE char* to_chars_impl<float, default_float_traits<float>, Mode::ToExponential, PrintTrailingZero::No>(uint32_t significand, int32_t exponent, char* buffer);

// Avoid needless ABI overhead incurred by tag dispatch.
template<Mode mode, class PolicyHolder, class Float, class FloatTraits>
char* to_chars_n_impl(float_bits<Float, FloatTraits> br, char* buffer) noexcept
{
    auto const exponent_bits = br.extract_exponent_bits();
    auto const s = br.remove_exponent_bits(exponent_bits);

    if (br.is_finite(exponent_bits)) {
        if (s.is_negative() && br.is_nonzero()) {
            *buffer = '-';
            ++buffer;
        }
        if (br.is_nonzero()) {
            auto result = to_decimal<Float, FloatTraits>(s,
                exponent_bits,
                policy::sign::ignore,
                policy::trailing_zero::ignore,
                typename PolicyHolder::decimal_to_binary_rounding_policy { },
                typename PolicyHolder::binary_to_decimal_rounding_policy { },
                typename PolicyHolder::cache_policy { });

            switch (mode) {
            case Mode::ToShortest:
                return to_shortest(result.significand, result.exponent, buffer);
            case Mode::ToExponential:
                return to_chars_impl<Float, FloatTraits, Mode::ToExponential, PrintTrailingZero::No>(result.significand, result.exponent, buffer);
            default:
                return nullptr;
            }
        } else {
            switch (mode) {
            case Mode::ToShortest:
                *buffer++ = '0';
                return buffer;
            case Mode::ToExponential:
                memcpy(buffer, "0e+0", 4);
                return buffer + 4;
            default:
                return nullptr;
            }
        }
    } else {
        if (s.has_all_zero_significand_bits()) {
            if (s.is_negative()) {
                *buffer = '-';
                ++buffer;
            }
            memcpy(buffer, "Infinity", 8);
            return buffer + 8;
        }
        memcpy(buffer, "NaN", 3);
        return buffer + 3;
    }
}

// Returns the next-to-end position
template<Mode mode, class Float, class FloatTraits = default_float_traits<Float>, class... Policies>
char* to_chars_n(Float x, char* buffer, Policies... policies) noexcept
{
    // using namespace detail::policy_impl;
    using policy_holder = decltype(make_policy_holder(
        policy_impl::base_default_pair_list<policy_impl::base_default_pair<policy_impl::decimal_to_binary_rounding::base, policy_impl::decimal_to_binary_rounding::nearest_to_even>,
            policy_impl::base_default_pair<policy_impl::binary_to_decimal_rounding::base, policy_impl::binary_to_decimal_rounding::to_even>,
            policy_impl::base_default_pair<policy_impl::cache::base, policy_impl::cache::full>> { },
        policies...));

    return to_chars_n_impl<mode, policy_holder>(float_bits<Float, FloatTraits>(x), buffer);
}

// Null-terminate and bypass the return value of fp_to_chars_n
template<Mode mode, class Float, class FloatTraits = default_float_traits<Float>, class... Policies>
char* to_chars(Float x, char* buffer, Policies... policies) noexcept
{
    auto ptr = to_chars_n<mode, Float, FloatTraits>(x, buffer, policies...);
    *ptr = '\0';
    return ptr;
}

} // namespace detail

typedef WTF::double_conversion::StringBuilder StringBuilder;

// See `ToExponential` in double-conversion.h for detailed definitons.
template<class Float>
void ToExponential(Float value, StringBuilder* result_builder)
{
    ASSERT((std::is_same<Float, double>::value) || (std::is_same<Float, float>::value));
    constexpr size_t buffer_length = 1 + (std::is_same<Float, float>::value ? to_exponential_max_string_length<ieee754_binary32>() : to_exponential_max_string_length<ieee754_binary64>());
    char buffer[buffer_length];
    detail::to_chars<Mode::ToExponential>(value, buffer);
    result_builder->AddString(buffer);
}

// See `ToShortest` in double-conversion.h for detailed definitons.
template<class Float>
void ToShortest(Float value, StringBuilder* result_builder)
{
    ASSERT((std::is_same<Float, double>::value) || (std::is_same<Float, float>::value));
    constexpr size_t buffer_length = 1 + (std::is_same<Float, float>::value ? max_string_length<ieee754_binary32>() : max_string_length<ieee754_binary64>());
    char buffer[buffer_length];
    detail::to_chars<Mode::ToShortest>(value, buffer);
    result_builder->AddString(buffer);
}

} // namespace dragonbox

} // namespace WTF
