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

#include <wtf/dragonbox/detail/cache_holder.h>
#include <wtf/dragonbox/detail/decimal_fp.h>
#include <wtf/dragonbox/detail/log.h>
#include <wtf/dragonbox/detail/wuint.h>
#include <wtf/dragonbox/ieee754_format.h>

namespace WTF {

namespace dragonbox {

namespace detail {

////////////////////////////////////////////////////////////////////////////////////////
// Policies.
////////////////////////////////////////////////////////////////////////////////////////

// Forward declare the implementation class.
template<class Float, class FloatTraits = default_float_traits<Float>>
struct impl;

namespace policy_impl {

// Sign policies.
namespace sign {
struct base { };

struct ignore : base {
    using sign_policy = ignore;
    static constexpr bool return_has_sign = false;

    template<class SignedSignificandBits, class UnsignedDecimalFp>
    static constexpr UnsignedDecimalFp handle_sign(SignedSignificandBits, UnsignedDecimalFp r) noexcept
    {
        return r;
    }
};

struct return_sign : base {
    using sign_policy = return_sign;
    static constexpr bool return_has_sign = true;

    template<class SignedSignificandBits, class UnsignedDecimalFp>
    static constexpr unsigned_decimal_fp_to_signed_t<UnsignedDecimalFp>
    handle_sign(SignedSignificandBits s, UnsignedDecimalFp r) noexcept
    {
        return add_sign_to_unsigned_decimal_fp(s.is_negative(), r);
    }
};
} // namespace sign

// Trailing zero policies.
namespace trailing_zero {
struct base { };

struct ignore : base {
    using trailing_zero_policy = ignore;
    static constexpr bool report_trailing_zeros = false;

    template<class Impl, class ReturnType>
    static constexpr ReturnType
    on_trailing_zeros(typename Impl::carrier_uint significand, int32_t exponent) noexcept
    {
        return { significand, exponent };
    }

    template<class Impl, class ReturnType>
    static constexpr ReturnType
    no_trailing_zeros(typename Impl::carrier_uint significand, int32_t exponent) noexcept
    {
        return { significand, exponent };
    }
};

struct remove : base {
    using trailing_zero_policy = remove;
    static constexpr bool report_trailing_zeros = false;

    template<class Impl, class ReturnType>
    ALWAYS_INLINE static constexpr ReturnType
    on_trailing_zeros(typename Impl::carrier_uint significand, int32_t exponent) noexcept
    {
        return { (exponent += Impl::remove_trailing_zeros(significand), significand), exponent };
    }

    template<class Impl, class ReturnType>
    static constexpr ReturnType
    no_trailing_zeros(typename Impl::carrier_uint significand, int32_t exponent) noexcept
    {
        return { significand, exponent };
    }
};

struct report : base {
    using trailing_zero_policy = report;
    static constexpr bool report_trailing_zeros = true;

    template<class Impl, class ReturnType>
    static constexpr ReturnType
    on_trailing_zeros(typename Impl::carrier_uint significand, int32_t exponent) noexcept
    {
        return { significand, exponent, true };
    }

    template<class Impl, class ReturnType>
    static constexpr ReturnType
    no_trailing_zeros(typename Impl::carrier_uint significand, int32_t exponent) noexcept
    {
        return { significand, exponent, false };
    }
};
} // namespace trailing_zero

// Decimal-to-binary rounding mode policies.
namespace decimal_to_binary_rounding {
struct base { };

enum class tag_t { to_nearest,
    left_closed_directed,
    right_closed_directed };
namespace interval_type {
struct symmetric_boundary {
    static constexpr bool is_symmetric = true;
    bool is_closed;
    constexpr bool include_left_endpoint() const noexcept { return is_closed; }
    constexpr bool include_right_endpoint() const noexcept { return is_closed; }
};
struct asymmetric_boundary {
    static constexpr bool is_symmetric = false;
    bool is_left_closed;
    constexpr bool include_left_endpoint() const noexcept
    {
        return is_left_closed;
    }
    constexpr bool include_right_endpoint() const noexcept
    {
        return !is_left_closed;
    }
};
struct closed {
    static constexpr bool is_symmetric = true;
    static constexpr bool include_left_endpoint() noexcept { return true; }
    static constexpr bool include_right_endpoint() noexcept { return true; }
};
struct open {
    static constexpr bool is_symmetric = true;
    static constexpr bool include_left_endpoint() noexcept { return false; }
    static constexpr bool include_right_endpoint() noexcept { return false; }
};
struct left_closed_right_open {
    static constexpr bool is_symmetric = false;
    static constexpr bool include_left_endpoint() noexcept { return true; }
    static constexpr bool include_right_endpoint() noexcept { return false; }
};
struct right_closed_left_open {
    static constexpr bool is_symmetric = false;
    static constexpr bool include_left_endpoint() noexcept { return false; }
    static constexpr bool include_right_endpoint() noexcept { return true; }
};
} // namespace interval_type

struct nearest_to_even : base {
    using decimal_to_binary_rounding_policy = nearest_to_even;
    static constexpr auto tag = tag_t::to_nearest;
    using normal_interval_type = interval_type::symmetric_boundary;
    using shorter_interval_type = interval_type::closed;

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(declval<nearest_to_even>(), Args { }...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(nearest_to_even { }, args...);
    }

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }..., false)) invoke_normal_interval_case(SignedSignificandBits s, Func f, Args... args) noexcept
    {
        return f(args..., s.has_even_significand_bits());
    }
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }...)) invoke_shorter_interval_case(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(args...);
    }
};
struct nearest_to_odd : base {
    using decimal_to_binary_rounding_policy = nearest_to_odd;
    static constexpr auto tag = tag_t::to_nearest;
    using normal_interval_type = interval_type::symmetric_boundary;
    using shorter_interval_type = interval_type::open;

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(declval<nearest_to_odd>(), Args { }...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(nearest_to_odd { }, args...);
    }

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }..., false)) invoke_normal_interval_case(SignedSignificandBits s, Func f, Args... args) noexcept
    {
        return f(args..., !s.has_even_significand_bits());
    }
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }...)) invoke_shorter_interval_case(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(args...);
    }
};
struct nearest_toward_plus_infinity : base {
    using decimal_to_binary_rounding_policy = nearest_toward_plus_infinity;
    static constexpr auto tag = tag_t::to_nearest;
    using normal_interval_type = interval_type::asymmetric_boundary;
    using shorter_interval_type = interval_type::asymmetric_boundary;

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(declval<nearest_toward_plus_infinity>(), Args { }...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(nearest_toward_plus_infinity { }, args...);
    }

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }..., false)) invoke_normal_interval_case(SignedSignificandBits s, Func f, Args... args) noexcept
    {
        return f(args..., !s.is_negative());
    }
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }..., false)) invoke_shorter_interval_case(SignedSignificandBits s, Func f, Args... args) noexcept
    {
        return f(args..., !s.is_negative());
    }
};
struct nearest_toward_minus_infinity : base {
    using decimal_to_binary_rounding_policy = nearest_toward_minus_infinity;
    static constexpr auto tag = tag_t::to_nearest;
    using normal_interval_type = interval_type::asymmetric_boundary;
    using shorter_interval_type = interval_type::asymmetric_boundary;

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(declval<nearest_toward_minus_infinity>(), Args { }...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(nearest_toward_minus_infinity { }, args...);
    }

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }..., false)) invoke_normal_interval_case(SignedSignificandBits s, Func f, Args... args) noexcept
    {
        return f(args..., s.is_negative());
    }
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }..., false)) invoke_shorter_interval_case(SignedSignificandBits s, Func f, Args... args) noexcept
    {
        return f(args..., s.is_negative());
    }
};
struct nearest_toward_zero : base {
    using decimal_to_binary_rounding_policy = nearest_toward_zero;
    static constexpr auto tag = tag_t::to_nearest;
    using normal_interval_type = interval_type::right_closed_left_open;
    using shorter_interval_type = interval_type::right_closed_left_open;

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(declval<nearest_toward_zero>(), Args { }...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(nearest_toward_zero { }, args...);
    }

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }...)) invoke_normal_interval_case(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(args...);
    }
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }...)) invoke_shorter_interval_case(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(args...);
    }
};
struct nearest_away_from_zero : base {
    using decimal_to_binary_rounding_policy = nearest_away_from_zero;
    static constexpr auto tag = tag_t::to_nearest;
    using normal_interval_type = interval_type::left_closed_right_open;
    using shorter_interval_type = interval_type::left_closed_right_open;

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(declval<nearest_away_from_zero>(), Args { }...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(nearest_away_from_zero { }, args...);
    }

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }...)) invoke_normal_interval_case(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(args...);
    }
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }...)) invoke_shorter_interval_case(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(args...);
    }
};

namespace detail {
struct nearest_always_closed {
    static constexpr auto tag = tag_t::to_nearest;
    using normal_interval_type = interval_type::closed;
    using shorter_interval_type = interval_type::closed;

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }...)) invoke_normal_interval_case(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(args...);
    }
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }...)) invoke_shorter_interval_case(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(args...);
    }
};
struct nearest_always_open {
    static constexpr auto tag = tag_t::to_nearest;
    using normal_interval_type = interval_type::open;
    using shorter_interval_type = interval_type::open;

    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }...)) invoke_normal_interval_case(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(args...);
    }
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(Args { }...)) invoke_shorter_interval_case(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(args...);
    }
};
} // namespace detail

struct nearest_to_even_static_boundary : base {
    using decimal_to_binary_rounding_policy = nearest_to_even_static_boundary;
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(detail::nearest_always_closed { }, Args { }...)) delegate(SignedSignificandBits s, Func f, Args... args) noexcept
    {
        return s.has_even_significand_bits()
            ? f(detail::nearest_always_closed { }, args...)
            : f(detail::nearest_always_open { }, args...);
    }
};
struct nearest_to_odd_static_boundary : base {
    using decimal_to_binary_rounding_policy = nearest_to_odd_static_boundary;
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(detail::nearest_always_closed { }, Args { }...)) delegate(SignedSignificandBits s, Func f, Args... args) noexcept
    {
        return s.has_even_significand_bits()
            ? f(detail::nearest_always_open { }, args...)
            : f(detail::nearest_always_closed { }, args...);
    }
};
struct nearest_toward_plus_infinity_static_boundary : base {
    using decimal_to_binary_rounding_policy = nearest_toward_plus_infinity_static_boundary;
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(nearest_toward_zero { }, Args { }...)) delegate(SignedSignificandBits s, Func f, Args... args) noexcept
    {
        return s.is_negative() ? f(nearest_toward_zero { }, args...) : f(nearest_away_from_zero { }, args...);
    }
};
struct nearest_toward_minus_infinity_static_boundary : base {
    using decimal_to_binary_rounding_policy = nearest_toward_minus_infinity_static_boundary;
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(nearest_toward_zero { }, Args { }...)) delegate(SignedSignificandBits s, Func f, Args... args) noexcept
    {
        return s.is_negative() ? f(nearest_away_from_zero { }, args...) : f(nearest_toward_zero { }, args...);
    }
};

namespace detail {
struct left_closed_directed {
    static constexpr auto tag = tag_t::left_closed_directed;
};
struct right_closed_directed {
    static constexpr auto tag = tag_t::right_closed_directed;
};
} // namespace detail

struct toward_plus_infinity : base {
    using decimal_to_binary_rounding_policy = toward_plus_infinity;
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(detail::left_closed_directed { }, Args { }...)) delegate(SignedSignificandBits s, Func f, Args... args) noexcept
    {
        return s.is_negative() ? f(detail::left_closed_directed { }, args...) : f(detail::right_closed_directed { }, args...);
    }
};
struct toward_minus_infinity : base {
    using decimal_to_binary_rounding_policy = toward_minus_infinity;
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(detail::left_closed_directed { }, Args { }...)) delegate(SignedSignificandBits s, Func f, Args... args) noexcept
    {
        return s.is_negative() ? f(detail::right_closed_directed { }, args...) : f(detail::left_closed_directed { }, args...);
    }
};
struct toward_zero : base {
    using decimal_to_binary_rounding_policy = toward_zero;
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(detail::left_closed_directed { }, Args { }...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(detail::left_closed_directed { }, args...);
    }
};
struct away_from_zero : base {
    using decimal_to_binary_rounding_policy = away_from_zero;
    template<class SignedSignificandBits, class Func, class... Args>
    ALWAYS_INLINE static constexpr decltype(Func { }(detail::right_closed_directed { }, Args { }...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept
    {
        return f(detail::right_closed_directed { }, args...);
    }
};
} // namespace decimal_to_binary_rounding

// Binary-to-decimal rounding policies.
// (Always assumes nearest rounding modes.)
namespace binary_to_decimal_rounding {
struct base { };

enum class tag_t { do_not_care,
    to_even,
    to_odd,
    away_from_zero,
    toward_zero };

struct do_not_care : base {
    using binary_to_decimal_rounding_policy = do_not_care;
    static constexpr auto tag = tag_t::do_not_care;

    template<class CarrierUInt>
    static constexpr bool prefer_round_down(CarrierUInt) noexcept
    {
        return false;
    }
};

struct to_even : base {
    using binary_to_decimal_rounding_policy = to_even;
    static constexpr auto tag = tag_t::to_even;

    template<class CarrierUInt>
    static constexpr bool prefer_round_down(CarrierUInt significand) noexcept { return significand % 2; }
};

struct to_odd : base {
    using binary_to_decimal_rounding_policy = to_odd;
    static constexpr auto tag = tag_t::to_odd;

    template<class CarrierUInt>
    static constexpr bool prefer_round_down(CarrierUInt significand) noexcept { return !(significand % 2); }
};

struct away_from_zero : base {
    using binary_to_decimal_rounding_policy = away_from_zero;
    static constexpr auto tag = tag_t::away_from_zero;

    template<class CarrierUInt>
    static constexpr bool prefer_round_down(CarrierUInt) noexcept
    {
        return false;
    }
};

struct toward_zero : base {
    using binary_to_decimal_rounding_policy = toward_zero;
    static constexpr auto tag = tag_t::toward_zero;

    template<class CarrierUInt>
    static constexpr bool prefer_round_down(CarrierUInt) noexcept
    {
        return true;
    }
};
} // namespace binary_to_decimal_rounding

// Cache policies.
namespace cache {
struct base { };

struct full : base {
    using cache_policy = full;
    template<class FloatFormat>
    static constexpr typename cache_holder<FloatFormat>::cache_entry_type get_cache(int32_t k) noexcept
    {
        ASSERT(k >= cache_holder<FloatFormat>::min_k && k <= cache_holder<FloatFormat>::max_k);
        return cache_holder<FloatFormat>::cache[size_t(k - cache_holder<FloatFormat>::min_k)];
    }
};

struct compact : base {
    using cache_policy = compact;

    template<class FloatFormat, class Dummy = void>
    struct get_cache_impl {
        static constexpr typename cache_holder<FloatFormat>::cache_entry_type get_cache(int32_t k) noexcept
        {
            return full::get_cache<FloatFormat>(k);
        }
    };

    template<class Dummy>
    struct get_cache_impl<ieee754_binary64, Dummy> {
        static constexpr cache_holder<ieee754_binary64>::cache_entry_type get_cache(int32_t k) noexcept
        {
            // Compute the base index.
            auto const cache_index = static_cast<int32_t>(static_cast<uint32_t>(k - cache_holder<ieee754_binary64>::min_k) / compressed_cache_detail<>::compression_ratio);
            auto const kb = cache_index * compressed_cache_detail<>::compression_ratio + cache_holder<ieee754_binary64>::min_k;
            auto const offset = k - kb;

            // Get the base cache.
            auto const base_cache = compressed_cache_detail<>::compressed_cache.table[cache_index];

            if (!offset)
                return base_cache;

            uint64_t base_cache_high = static_cast<uint64_t>(base_cache >> 64);
            uint64_t base_cache_low = static_cast<uint64_t>(base_cache);

            // Compute the required amount of bit-shift.
            auto const alpha = log::floor_log2_pow10(kb + offset) - log::floor_log2_pow10(kb) - offset;
            ASSERT(alpha > 0 && alpha < 64);

            // Try to recover the real cache.
            auto const pow5 = compressed_cache_detail<>::pow5.table[offset];
            auto recovered_cache = wuint::umul128(base_cache_high, pow5);
            auto const middle_low = wuint::umul128(base_cache_low, pow5);

            uint64_t middle_low_high = static_cast<uint64_t>(middle_low >> 64);
            uint64_t middle_low_low = static_cast<uint64_t>(middle_low);
            uint64_t recovered_cache_high = static_cast<uint64_t>(recovered_cache >> 64);
            uint64_t recovered_cache_low = static_cast<uint64_t>(recovered_cache);

            recovered_cache += middle_low_high;

            auto const high_to_middle = recovered_cache_high << (64 - alpha);
            auto const middle_to_low = recovered_cache_low << (64 - alpha);

            recovered_cache = (static_cast<UInt128>((recovered_cache_low >> alpha) | high_to_middle) << 64)
                + static_cast<UInt128>((middle_low_low >> alpha) | middle_to_low);

            ASSERT(recovered_cache_low + 1);
            recovered_cache += 1;

            return recovered_cache;
        }
    };

    template<class FloatFormat>
    static constexpr typename cache_holder<FloatFormat>::cache_entry_type get_cache(int32_t k) noexcept
    {
        ASSERT(k >= cache_holder<FloatFormat>::min_k && k <= cache_holder<FloatFormat>::max_k);
        return get_cache_impl<FloatFormat>::get_cache(k);
    }
};
} // namespace cache

} // namespace policy_impl

} // namespace detail

namespace policy {
namespace sign {
inline constexpr auto ignore = detail::policy_impl::sign::ignore { };
inline constexpr auto return_sign = detail::policy_impl::sign::return_sign { };
}

namespace trailing_zero {
inline constexpr auto ignore = detail::policy_impl::trailing_zero::ignore { };
inline constexpr auto remove = detail::policy_impl::trailing_zero::remove { };
inline constexpr auto report = detail::policy_impl::trailing_zero::report { };
}

namespace decimal_to_binary_rounding {
inline constexpr auto nearest_to_even = detail::policy_impl::decimal_to_binary_rounding::nearest_to_even { };
inline constexpr auto nearest_to_odd = detail::policy_impl::decimal_to_binary_rounding::nearest_to_odd { };
inline constexpr auto nearest_toward_plus_infinity = detail::policy_impl::decimal_to_binary_rounding::nearest_toward_plus_infinity { };
inline constexpr auto nearest_toward_minus_infinity = detail::policy_impl::decimal_to_binary_rounding::nearest_toward_minus_infinity { };
inline constexpr auto nearest_toward_zero = detail::policy_impl::decimal_to_binary_rounding::nearest_toward_zero { };
inline constexpr auto nearest_away_from_zero = detail::policy_impl::decimal_to_binary_rounding::nearest_away_from_zero { };

inline constexpr auto nearest_to_even_static_boundary = detail::policy_impl::decimal_to_binary_rounding::nearest_to_even_static_boundary { };
inline constexpr auto nearest_to_odd_static_boundary = detail::policy_impl::decimal_to_binary_rounding::nearest_to_odd_static_boundary { };
inline constexpr auto nearest_toward_plus_infinity_static_boundary = detail::policy_impl::decimal_to_binary_rounding::nearest_toward_plus_infinity_static_boundary { };
inline constexpr auto nearest_toward_minus_infinity_static_boundary = detail::policy_impl::decimal_to_binary_rounding::nearest_toward_minus_infinity_static_boundary { };

inline constexpr auto toward_plus_infinity = detail::policy_impl::decimal_to_binary_rounding::toward_plus_infinity { };
inline constexpr auto toward_minus_infinity = detail::policy_impl::decimal_to_binary_rounding::toward_minus_infinity { };
inline constexpr auto toward_zero = detail::policy_impl::decimal_to_binary_rounding::toward_zero { };
inline constexpr auto away_from_zero = detail::policy_impl::decimal_to_binary_rounding::away_from_zero { };
}

namespace binary_to_decimal_rounding {
inline constexpr auto do_not_care = detail::policy_impl::binary_to_decimal_rounding::do_not_care { };
inline constexpr auto to_even = detail::policy_impl::binary_to_decimal_rounding::to_even { };
inline constexpr auto to_odd = detail::policy_impl::binary_to_decimal_rounding::to_odd { };
inline constexpr auto away_from_zero = detail::policy_impl::binary_to_decimal_rounding::away_from_zero { };
inline constexpr auto toward_zero = detail::policy_impl::binary_to_decimal_rounding::toward_zero { };
}

namespace cache {
inline constexpr auto full = detail::policy_impl::cache::full { };
inline constexpr auto compact = detail::policy_impl::cache::compact { };
}
}

} // namespace dragonbox

} // namespace WTF
