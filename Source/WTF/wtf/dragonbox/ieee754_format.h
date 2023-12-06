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

#include <wtf/dragonbox/detail/util.h>

namespace WTF {

namespace dragonbox {

// These classes expose encoding specs of IEEE-754-like floating-point formats.
// Currently available formats are IEEE754-binary32 & IEEE754-binary64.
struct ieee754_binary32 {
    static constexpr int32_t significand_bits = 23;
    static constexpr int32_t exponent_bits = 8;
    static constexpr int32_t min_exponent = -126;
    static constexpr int32_t max_exponent = 127;
    static constexpr int32_t exponent_bias = -127;
    static constexpr int32_t decimal_digits = 9;
};

struct ieee754_binary64 {
    static constexpr int32_t significand_bits = 52;
    static constexpr int32_t exponent_bits = 11;
    static constexpr int32_t min_exponent = -1022;
    static constexpr int32_t max_exponent = 1023;
    static constexpr int32_t exponent_bias = -1023;
    static constexpr int32_t decimal_digits = 17;
};

// A floating-point traits class defines ways to interpret a bit pattern of given size as an
// encoding of floating-point number. This is a default implementation of such a traits
// class, supporting ways to interpret 32-bits into a binary32-encoded floating-point number
// and to interpret 64-bits into a binary64-encoded floating-point number. Users might
// specialize this class to change the default behavior for certain types.
template<class T>
struct default_float_traits {
    // I don't know if there is a truly reliable way of detecting
    // IEEE-754 binary32/binary64 formats; I just did my best here.
    static_assert(std::numeric_limits<T>::is_iec559
        && std::numeric_limits<T>::radix == 2
        && (detail::physical_bits<T>::value == 32 || detail::physical_bits<T>::value == 64),
        "default_ieee754_traits only works for 32-bits or 64-bits types "
        "supporting binary32 or binary64 formats!");

    // The type that is being viewed.
    using type = T;

    // Refers to the format specification class.
    using format = typename std::conditional<detail::physical_bits<T>::value == 32, ieee754_binary32, ieee754_binary64>::type;

    // Defines an unsigned integer type that is large enough to carry a variable of type T.
    // Most of the operations will be done on this integer type.
    using carrier_uint = typename std::conditional<detail::physical_bits<T>::value == 32, uint32_t, uint64_t>::type;
    static_assert(sizeof(carrier_uint) == sizeof(T), "");

    // Number of bits in the above unsigned integer type.
    static constexpr int32_t carrier_bits = static_cast<int32_t>(detail::physical_bits<carrier_uint>::value);

    // Convert from carrier_uint into the original type.
    // Depending on the floating-point encoding format, this operation might not be possible
    // for some specific bit patterns. However, the contract is that u always denotes a
    // valid bit pattern, so this function must be assumed to be noexcept.
    static constexpr T carrier_to_float(carrier_uint u) noexcept
    {
        return bitwise_cast<T>(u);
    }

    // Same as above.
    static constexpr carrier_uint float_to_carrier(T x) noexcept
    {
        return bitwise_cast<carrier_uint>(x);
    }

    // Extract exponent bits from a bit pattern.
    // The result must be aligned to the LSB so that there is no additional zero paddings
    // on the right. This function does not do bias adjustment.
    static constexpr unsigned extract_exponent_bits(carrier_uint u) noexcept
    {
        static_assert(detail::value_bits<unsigned>::value > format::exponent_bits, "");
        return static_cast<unsigned>(u >> format::significand_bits) & ((static_cast<unsigned>(1) << format::exponent_bits) - 1);
    }

    // Extract significand bits from a bit pattern.
    // The result must be aligned to the LSB so that there is no additional zero paddings
    // on the right. The result does not contain the implicit bit.
    static constexpr carrier_uint extract_significand_bits(carrier_uint u) noexcept
    {
        return static_cast<carrier_uint>(u & static_cast<carrier_uint>((static_cast<carrier_uint>(1) << format::significand_bits) - 1));
    }

    // Remove the exponent bits and extract significand bits together with the sign bit.
    static constexpr carrier_uint remove_exponent_bits(carrier_uint u, unsigned exponent_bits) noexcept
    {
        return u ^ (static_cast<carrier_uint>(exponent_bits) << format::significand_bits);
    }

    // Shift the obtained signed significand bits to the left by 1 to remove the sign bit.
    static constexpr carrier_uint remove_sign_bit_and_shift(carrier_uint u) noexcept
    {
        return static_cast<carrier_uint>(static_cast<carrier_uint>(u) << 1);
    }

    // The actual value of exponent is obtained by adding this value to the extracted
    // exponent bits.
    static constexpr int32_t exponent_bias = 1 - (1 << (carrier_bits - format::significand_bits - 2));

    // Obtain the actual value of the binary exponent from the extracted exponent bits.
    static constexpr int32_t binary_exponent(unsigned exponent_bits) noexcept
    {
        return !exponent_bits ? format::min_exponent : static_cast<int32_t>(exponent_bits) + format::exponent_bias;
    }

    // Obtain the actual value of the binary exponent from the extracted significand bits
    // and exponent bits.
    static constexpr carrier_uint binary_significand(carrier_uint significand_bits, unsigned exponent_bits) noexcept
    {
        return !exponent_bits ? significand_bits : (significand_bits | (static_cast<carrier_uint>(1) << format::significand_bits));
    }

    /* Various boolean observer functions */

    static constexpr bool is_nonzero(carrier_uint u) noexcept { return u << 1; }
    static constexpr bool is_positive(carrier_uint u) noexcept
    {
        return u < (static_cast<carrier_uint>(1) << (format::significand_bits + format::exponent_bits));
    }
    static constexpr bool is_negative(carrier_uint u) noexcept { return !is_positive(u); }
    static constexpr bool is_finite(unsigned exponent_bits) noexcept
    {
        return exponent_bits != ((1u << format::exponent_bits) - 1);
    }
    static constexpr bool has_all_zero_significand_bits(carrier_uint u) noexcept
    {
        return !(u << 1);
    }
    static constexpr bool has_even_significand_bits(carrier_uint u) noexcept
    {
        return !(u % 2);
    }
};

// Convenient wrappers for floating-point traits classes.
// In order to reduce the argument passing overhead, these classes should be as simple as
// possible (e.g., no inheritance, no private non-static data member, etc.; this is an
// unfortunate fact about common ABI convention).

template<class T, class Traits = default_float_traits<T>>
struct float_bits;

template<class T, class Traits = default_float_traits<T>>
struct signed_significand_bits;

template<class T, class Traits>
struct float_bits {
    using type = T;
    using traits_type = Traits;
    using carrier_uint = typename traits_type::carrier_uint;

    carrier_uint u;

    float_bits() = default;
    constexpr explicit float_bits(carrier_uint bit_pattern) noexcept
        : u { bit_pattern }
    {
    }

    constexpr explicit float_bits(T float_value) noexcept
        : u { traits_type::float_to_carrier(float_value) }
    {
    }

    constexpr T to_float() const noexcept { return traits_type::carrier_to_float(u); }

    // Extract exponent bits from a bit pattern.
    // The result must be aligned to the LSB so that there is no additional zero paddings
    // on the right. This function does not do bias adjustment.
    constexpr unsigned extract_exponent_bits() const noexcept
    {
        return traits_type::extract_exponent_bits(u);
    }

    // Extract significand bits from a bit pattern.
    // The result must be aligned to the LSB so that there is no additional zero paddings
    // on the right. The result does not contain the implicit bit.
    constexpr carrier_uint extract_significand_bits() const noexcept
    {
        return traits_type::extract_significand_bits(u);
    }

    // Remove the exponent bits and extract significand bits together with the sign bit.
    constexpr signed_significand_bits<type, traits_type>
    remove_exponent_bits(unsigned exponent_bits) const noexcept
    {
        return signed_significand_bits<type, traits_type>(traits_type::remove_exponent_bits(u, exponent_bits));
    }

    // Obtain the actual value of the binary exponent from the extracted exponent bits.
    static constexpr int32_t binary_exponent(unsigned exponent_bits) noexcept
    {
        return traits_type::binary_exponent(exponent_bits);
    }
    constexpr int32_t binary_exponent() const noexcept
    {
        return binary_exponent(extract_exponent_bits());
    }

    // Obtain the actual value of the binary exponent from the extracted significand bits
    // and exponent bits.
    static constexpr carrier_uint binary_significand(carrier_uint significand_bits, unsigned exponent_bits) noexcept
    {
        return traits_type::binary_significand(significand_bits, exponent_bits);
    }
    constexpr carrier_uint binary_significand() const noexcept
    {
        return binary_significand(extract_significand_bits(), extract_exponent_bits());
    }

    constexpr bool is_nonzero() const noexcept { return traits_type::is_nonzero(u); }
    constexpr bool is_positive() const noexcept { return traits_type::is_positive(u); }
    constexpr bool is_negative() const noexcept { return traits_type::is_negative(u); }
    constexpr bool is_finite(unsigned exponent_bits) const noexcept
    {
        return traits_type::is_finite(exponent_bits);
    }
    constexpr bool is_finite() const noexcept
    {
        return traits_type::is_finite(extract_exponent_bits());
    }
    constexpr bool has_even_significand_bits() const noexcept
    {
        return traits_type::has_even_significand_bits(u);
    }
};

template<class T, class Traits>
struct signed_significand_bits {
    using type = T;
    using traits_type = Traits;
    using carrier_uint = typename traits_type::carrier_uint;

    carrier_uint u;

    signed_significand_bits() = default;
    constexpr explicit signed_significand_bits(carrier_uint bit_pattern) noexcept
        : u { bit_pattern }
    {
    }

    // Shift the obtained signed significand bits to the left by 1 to remove the sign bit.
    constexpr carrier_uint remove_sign_bit_and_shift() const noexcept
    {
        return traits_type::remove_sign_bit_and_shift(u);
    }

    constexpr bool is_positive() const noexcept { return traits_type::is_positive(u); }
    constexpr bool is_negative() const noexcept { return traits_type::is_negative(u); }
    constexpr bool has_all_zero_significand_bits() const noexcept
    {
        return traits_type::has_all_zero_significand_bits(u);
    }
    constexpr bool has_even_significand_bits() const noexcept
    {
        return traits_type::has_even_significand_bits(u);
    }
};

} // namespace dragonbox

} // namespace WTF
