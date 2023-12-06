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

#include <wtf/dragonbox/detail/bits.h>
#include <wtf/dragonbox/detail/div.h>
#include <wtf/dragonbox/detail/policy_holder.h>
#include <wtf/dragonbox/detail/util.h>
#include <wtf/dragonbox/ieee754_format.h>

namespace WTF {

namespace dragonbox {

namespace detail {
////////////////////////////////////////////////////////////////////////////////////////
// The main algorithm.
////////////////////////////////////////////////////////////////////////////////////////

template<class Float, class FloatTraits>
struct impl : private FloatTraits, private FloatTraits::format {
    using format = typename FloatTraits::format;
    using carrier_uint = typename FloatTraits::carrier_uint;

    using FloatTraits::carrier_bits;
    using format::decimal_digits;
    using format::exponent_bias;
    using format::max_exponent;
    using format::min_exponent;
    using format::significand_bits;

    static constexpr int32_t kappa = std::is_same<format, ieee754_binary32>::value ? 1 : 2;
    static_assert(kappa >= 1, "");
    static_assert(carrier_bits >= significand_bits + 2 + log::floor_log2_pow10(kappa + 1), "");

    static constexpr int32_t min(int32_t x, int32_t y) noexcept { return x < y ? x : y; }
    static constexpr int32_t max(int32_t x, int32_t y) noexcept { return x > y ? x : y; }

    static constexpr int32_t min_k = min(
        -log::floor_log10_pow2_minus_log10_4_over_3(static_cast<int32_t>(max_exponent - significand_bits)),
        -log::floor_log10_pow2(static_cast<int32_t>(max_exponent - significand_bits)) + kappa);
    static_assert(min_k >= cache_holder<format>::min_k, "");

    // We do invoke shorter_interval_case for exponent == min_exponent case,
    // so we should not add 1 here.
    static constexpr int32_t max_k = max(
        -log::floor_log10_pow2_minus_log10_4_over_3(static_cast<int32_t>(min_exponent - significand_bits /*+ 1*/)),
        -log::floor_log10_pow2(static_cast<int32_t>(min_exponent - significand_bits)) + kappa);
    static_assert(max_k <= cache_holder<format>::max_k, "");

    using cache_entry_type = typename cache_holder<format>::cache_entry_type;
    static constexpr auto cache_bits = cache_holder<format>::cache_bits;

    static constexpr int32_t case_shorter_interval_left_endpoint_lower_threshold = 2;
    static constexpr int32_t case_shorter_interval_left_endpoint_upper_threshold = 2 + log::floor_log2(compute_power<count_factors<5>((carrier_uint(1) << (significand_bits + 2)) - 1) + 1>(10) / 3);

    static constexpr int32_t case_shorter_interval_right_endpoint_lower_threshold = 0;
    static constexpr int32_t case_shorter_interval_right_endpoint_upper_threshold = 2 + log::floor_log2(compute_power<count_factors<5>((carrier_uint(1) << (significand_bits + 1)) + 1) + 1>(10) / 3);

    static constexpr int32_t shorter_interval_tie_lower_threshold = -log::floor_log5_pow2_minus_log5_3(significand_bits + 4) - 2 - significand_bits;
    static constexpr int32_t shorter_interval_tie_upper_threshold = -log::floor_log5_pow2(significand_bits + 2) - 2 - significand_bits;

    struct compute_mul_result {
        carrier_uint integer_part;
        bool is_integer;
    };
    struct compute_mul_parity_result {
        bool parity;
        bool is_integer;
    };
    template<class FloatFormat, class Dummy = void>
    struct compute_mul_impl;

    //// The main algorithm assumes the input is a normal/subnormal finite number

    template<class ReturnType, class IntervalType, class TrailingZeroPolicy, class BinaryToDecimalRoundingPolicy, class CachePolicy, class... AdditionalArgs>
    ALWAYS_INLINE static constexpr ReturnType compute_nearest_normal(carrier_uint const two_fc, int32_t const binary_exponent, AdditionalArgs... additional_args) noexcept
    {
        //////////////////////////////////////////////////////////////////////
        // Step 1: Schubfach multiplier calculation
        //////////////////////////////////////////////////////////////////////

        IntervalType interval_type { additional_args... };

        // Compute k and beta.
        int32_t const minus_k = log::floor_log10_pow2(binary_exponent) - kappa;
        auto const cache = CachePolicy::template get_cache<format>(-minus_k);
        int32_t const beta = binary_exponent + log::floor_log2_pow10(-minus_k);

        // Compute zi and deltai.
        // 10^kappa <= deltai < 10^(kappa + 1)
        auto const deltai = compute_mul_impl<format>::compute_delta(cache, beta);
        // For the case of binary32, the result of integer check is not correct for
        // 29711844 * 2^-82
        // = 6.1442653300000000008655037797566933477355632930994033813476... * 10^-18
        // and 29711844 * 2^-81
        // = 1.2288530660000000001731007559513386695471126586198806762695... * 10^-17,
        // and they are the unique counterexamples. However, since 29711844 is even,
        // this does not cause any problem for the endpoints calculations; it can only
        // cause a problem when we need to perform integer check for the center.
        // Fortunately, with these inputs, that branch is never executed, so we are
        // fine.
        auto const z_result = compute_mul_impl<format>::compute_mul((two_fc | 1) << beta, cache);

        //////////////////////////////////////////////////////////////////////
        // Step 2: Try larger divisor; remove trailing zeros if necessary
        //////////////////////////////////////////////////////////////////////

        constexpr auto big_divisor = compute_power<kappa + 1>(static_cast<uint32_t>(10));
        constexpr auto small_divisor = compute_power<kappa>(static_cast<uint32_t>(10));

        // Using an upper bound on zi, we might be able to optimize the division
        // better than the compiler; we are computing zi / big_divisor here.
        carrier_uint decimal_significand = div::divide_by_pow10<kappa + 1, carrier_uint, (carrier_uint(1) << (significand_bits + 1)) * big_divisor - 1>(z_result.integer_part);
        auto r = static_cast<uint32_t>(z_result.integer_part - big_divisor * decimal_significand);

        do {
            if (r < deltai) {
                // Exclude the right endpoint if necessary.
                if (!r && (z_result.is_integer & !interval_type.include_right_endpoint())) {
                    if constexpr (BinaryToDecimalRoundingPolicy::tag == policy_impl::binary_to_decimal_rounding::tag_t::do_not_care) {
                        decimal_significand *= 10;
                        --decimal_significand;
                        return TrailingZeroPolicy::template no_trailing_zeros<impl,
                            ReturnType>(
                            decimal_significand, minus_k + kappa);
                    } else {
                        --decimal_significand;
                        r = big_divisor;
                        break;
                    }
                }
            } else if (r > deltai)
                break;
            else {
                // r == deltai; compare fractional parts.
                auto const x_result = compute_mul_impl<format>::compute_mul_parity(two_fc - 1, cache, beta);

                if (!(x_result.parity | (x_result.is_integer & interval_type.include_left_endpoint())))
                    break;
            }

            // We may need to remove trailing zeros.
            return TrailingZeroPolicy::template on_trailing_zeros<impl, ReturnType>(decimal_significand, minus_k + kappa + 1);
        } while (false);

        //////////////////////////////////////////////////////////////////////
        // Step 3: Find the significand with the smaller divisor
        //////////////////////////////////////////////////////////////////////

        decimal_significand *= 10;

        if constexpr (BinaryToDecimalRoundingPolicy::tag == policy_impl::binary_to_decimal_rounding::tag_t::do_not_care) {
            // Normally, we want to compute
            // significand += r / small_divisor
            // and return, but we need to take care of the case that the resulting
            // value is exactly the right endpoint, while that is not included in the
            // interval.
            if (!interval_type.include_right_endpoint()) {
                // Is r divisible by 10^kappa?
                if (z_result.is_integer && div::check_divisibility_and_divide_by_pow10<kappa>(r)) {
                    // This should be in the interval.
                    decimal_significand += r - 1;
                } else
                    decimal_significand += r;
            } else
                decimal_significand += div::small_division_by_pow10<kappa>(r);
        } else {
            auto dist = r - (deltai / 2) + (small_divisor / 2);
            bool const approx_y_parity = (dist ^ (small_divisor / 2)) & 1;

            // Is dist divisible by 10^kappa?
            bool const divisible_by_small_divisor = div::check_divisibility_and_divide_by_pow10<kappa>(dist);

            // Add dist / 10^kappa to the significand.
            decimal_significand += dist;

            if (divisible_by_small_divisor) {
                // Check z^(f) >= epsilon^(f).
                // We have either yi == zi - epsiloni or yi == (zi - epsiloni) - 1,
                // where yi == zi - epsiloni if and only if z^(f) >= epsilon^(f).
                // Since there are only 2 possibilities, we only need to care about the
                // parity. Also, zi and r should have the same parity since the divisor
                // is an even number.
                auto const y_result = compute_mul_impl<format>::compute_mul_parity(two_fc, cache, beta);
                if (y_result.parity != approx_y_parity)
                    --decimal_significand;
                else {
                    // If z^(f) >= epsilon^(f), we might have a tie
                    // when z^(f) == epsilon^(f), or equivalently, when y is an integer.
                    // For tie-to-up case, we can just choose the upper one.
                    if (BinaryToDecimalRoundingPolicy::prefer_round_down(decimal_significand) & y_result.is_integer)
                        --decimal_significand;
                }
            }
        }
        return TrailingZeroPolicy::template no_trailing_zeros<impl, ReturnType>(decimal_significand, minus_k + kappa);
    }

    template<class ReturnType, class IntervalType, class TrailingZeroPolicy, class BinaryToDecimalRoundingPolicy, class CachePolicy, class... AdditionalArgs>
    static constexpr ReturnType compute_nearest_shorter(int32_t const binary_exponent, AdditionalArgs... additional_args) noexcept
    {
        IntervalType interval_type { additional_args... };

        // Compute k and beta.
        int32_t const minus_k = log::floor_log10_pow2_minus_log10_4_over_3(binary_exponent);
        int32_t const beta = binary_exponent + log::floor_log2_pow10(-minus_k);

        // Compute xi and zi.
        auto const cache = CachePolicy::template get_cache<format>(-minus_k);

        auto xi = compute_mul_impl<format>::compute_left_endpoint_for_shorter_interval_case(cache, beta);
        auto zi = compute_mul_impl<format>::compute_right_endpoint_for_shorter_interval_case(cache, beta);

        // If we don't accept the right endpoint and
        // if the right endpoint is an integer, decrease it.
        if (!interval_type.include_right_endpoint() && is_right_endpoint_integer_shorter_interval(binary_exponent))
            --zi;
        // If we don't accept the left endpoint or
        // if the left endpoint is not an integer, increase it.
        if (!interval_type.include_left_endpoint() || !is_left_endpoint_integer_shorter_interval(binary_exponent))
            ++xi;

        // Try bigger divisor.
        carrier_uint decimal_significand = zi / 10;

        // If succeed, remove trailing zeros if necessary and return.
        if (decimal_significand * 10 >= xi)
            return TrailingZeroPolicy::template on_trailing_zeros<impl, ReturnType>(decimal_significand, minus_k + 1);

        // Otherwise, compute the round-up of y.
        decimal_significand = compute_mul_impl<format>::compute_round_up_for_shorter_interval_case(cache, beta);

        // When tie occurs, choose one of them according to the rule.
        if (BinaryToDecimalRoundingPolicy::prefer_round_down(decimal_significand) && binary_exponent >= shorter_interval_tie_lower_threshold && binary_exponent <= shorter_interval_tie_upper_threshold)
            --decimal_significand;
        else if (decimal_significand < xi)
            ++decimal_significand;
        return TrailingZeroPolicy::template no_trailing_zeros<impl, ReturnType>(decimal_significand, minus_k);
    }

    template<class ReturnType, class TrailingZeroPolicy, class CachePolicy>
    ALWAYS_INLINE static constexpr ReturnType compute_left_closed_directed(carrier_uint const two_fc, int32_t binary_exponent) noexcept
    {
        //////////////////////////////////////////////////////////////////////
        // Step 1: Schubfach multiplier calculation
        //////////////////////////////////////////////////////////////////////

        // Compute k and beta.
        int32_t const minus_k = log::floor_log10_pow2(binary_exponent) - kappa;
        auto const cache = CachePolicy::template get_cache<format>(-minus_k);
        int32_t const beta = binary_exponent + log::floor_log2_pow10(-minus_k);

        // Compute xi and deltai.
        // 10^kappa <= deltai < 10^(kappa + 1)
        auto const deltai = compute_mul_impl<format>::compute_delta(cache, beta);
        auto x_result = compute_mul_impl<format>::compute_mul(two_fc << beta, cache);

        // Deal with the unique exceptional cases
        // 29711844 * 2^-82
        // = 6.1442653300000000008655037797566933477355632930994033813476... * 10^-18
        // and 29711844 * 2^-81
        // = 1.2288530660000000001731007559513386695471126586198806762695... * 10^-17
        // for binary32.
        if constexpr (std::is_same<format, ieee754_binary32>::value) {
            if (binary_exponent <= -80)
                x_result.is_integer = false;
        }

        if (!x_result.is_integer)
            ++x_result.integer_part;

        //////////////////////////////////////////////////////////////////////
        // Step 2: Try larger divisor; remove trailing zeros if necessary
        //////////////////////////////////////////////////////////////////////

        constexpr auto big_divisor = compute_power<kappa + 1>(static_cast<uint32_t>(10));

        // Using an upper bound on xi, we might be able to optimize the division
        // better than the compiler; we are computing xi / big_divisor here.
        carrier_uint decimal_significand = div::divide_by_pow10<kappa + 1, carrier_uint, (carrier_uint(1) << (significand_bits + 1)) * big_divisor - 1>(x_result.integer_part);
        auto r = static_cast<uint32_t>(x_result.integer_part - big_divisor * decimal_significand);

        if (r) {
            ++decimal_significand;
            r = big_divisor - r;
        }

        do {
            if (r > deltai)
                break;
            if (r == deltai) {
                // Compare the fractional parts.
                // This branch is never taken for the exceptional cases
                // 2f_c = 29711482, e = -81
                // (6.1442649164096937243516663440523473127541365101933479309082... *
                // 10^-18) and 2f_c = 29711482, e = -80
                // (1.2288529832819387448703332688104694625508273020386695861816... *
                // 10^-17).
                auto const z_result = compute_mul_impl<format>::compute_mul_parity(two_fc + 2, cache, beta);
                if (z_result.parity || z_result.is_integer)
                    break;
            }

            // The ceiling is inside, so we are done.
            return TrailingZeroPolicy::template on_trailing_zeros<impl, ReturnType>(decimal_significand, minus_k + kappa + 1);
        } while (false);

        //////////////////////////////////////////////////////////////////////
        // Step 3: Find the significand with the smaller divisor
        //////////////////////////////////////////////////////////////////////

        decimal_significand *= 10;
        decimal_significand -= div::small_division_by_pow10<kappa>(r);
        return TrailingZeroPolicy::template no_trailing_zeros<impl, ReturnType>(decimal_significand, minus_k + kappa);
    }

    template<class ReturnType, class TrailingZeroPolicy, class CachePolicy>
    ALWAYS_INLINE static constexpr ReturnType compute_right_closed_directed(carrier_uint const two_fc, int32_t const binary_exponent, bool shorter_interval) noexcept
    {
        //////////////////////////////////////////////////////////////////////
        // Step 1: Schubfach multiplier calculation
        //////////////////////////////////////////////////////////////////////

        // Compute k and beta.
        int32_t const minus_k = log::floor_log10_pow2(binary_exponent - (shorter_interval ? 1 : 0)) - kappa;
        auto const cache = CachePolicy::template get_cache<format>(-minus_k);
        int32_t const beta = binary_exponent + log::floor_log2_pow10(-minus_k);

        // Compute zi and deltai.
        // 10^kappa <= deltai < 10^(kappa + 1)
        auto const deltai = shorter_interval
            ? compute_mul_impl<format>::compute_delta(cache, beta - 1)
            : compute_mul_impl<format>::compute_delta(cache, beta);
        carrier_uint const zi = compute_mul_impl<format>::compute_mul(two_fc << beta, cache).integer_part;

        //////////////////////////////////////////////////////////////////////
        // Step 2: Try larger divisor; remove trailing zeros if necessary
        //////////////////////////////////////////////////////////////////////

        constexpr auto big_divisor = compute_power<kappa + 1>(static_cast<uint32_t>(10));

        // Using an upper bound on zi, we might be able to optimize the division better
        // than the compiler; we are computing zi / big_divisor here.
        carrier_uint decimal_significand = div::divide_by_pow10<kappa + 1, carrier_uint, (carrier_uint(1) << (significand_bits + 1)) * big_divisor - 1>(zi);
        auto const r = static_cast<uint32_t>(zi - big_divisor * decimal_significand);

        do {
            if (r > deltai)
                break;
            if (r == deltai) {
                // Compare the fractional parts.
                if (!compute_mul_impl<format>::compute_mul_parity(two_fc - (shorter_interval ? 1 : 2), cache, beta).parity)
                    break;
            }

            // The floor is inside, so we are done.
            return TrailingZeroPolicy::template on_trailing_zeros<impl, ReturnType>(decimal_significand, minus_k + kappa + 1);
        } while (false);

        //////////////////////////////////////////////////////////////////////
        // Step 3: Find the significand with the small divisor
        //////////////////////////////////////////////////////////////////////

        decimal_significand *= 10;
        decimal_significand += div::small_division_by_pow10<kappa>(r);
        return TrailingZeroPolicy::template no_trailing_zeros<impl, ReturnType>(decimal_significand, minus_k + kappa);
    }

    // Remove trailing zeros from n and return the number of zeros removed.
    ALWAYS_INLINE static constexpr int32_t remove_trailing_zeros(carrier_uint& n) noexcept
    {
        ASSERT(n);

        if constexpr (std::is_same<format, ieee754_binary32>::value) {
            constexpr auto mod_inv_5 = static_cast<uint32_t>(0xcccccccd);
            constexpr auto mod_inv_25 = mod_inv_5 * mod_inv_5;

            int32_t s = 0;
            while (true) {
                auto q = bits::rotr(n * mod_inv_25, 2);
                if (q <= std::numeric_limits<uint32_t>::max() / 100) {
                    n = q;
                    s += 2;
                } else
                    break;
            }
            auto q = bits::rotr(n * mod_inv_5, 1);
            if (q <= std::numeric_limits<uint32_t>::max() / 10) {
                n = q;
                s |= 1;
            }

            return s;
        } else {
            static_assert(std::is_same<format, ieee754_binary64>::value, "");

            // Divide by 10^8 and reduce to 32-bits if divisible.
            // Since ret_value.significand <= (2^53 * 1000 - 1) / 1000 < 10^16,
            // n is at most of 16 digits.

            // This magic number is ceil(2^90 / 10^8).
            constexpr auto magic_number = static_cast<uint64_t>(12379400392853802749ull);
            auto nm = wuint::umul128(n, magic_number);
            uint64_t nm_high = static_cast<uint64_t>(nm >> 64);
            uint64_t nm_low = static_cast<uint64_t>(nm);

            // Is n is divisible by 10^8?
            if (!(nm_high & ((static_cast<uint64_t>(1) << (90 - 64)) - 1)) && nm_low < magic_number) {
                // If yes, work with the quotient.
                auto n32 = static_cast<uint32_t>(nm_high >> (90 - 64));

                constexpr auto mod_inv_5 = static_cast<uint32_t>(0xcccccccd);
                constexpr auto mod_inv_25 = mod_inv_5 * mod_inv_5;

                int32_t s = 8;
                while (true) {
                    auto q = bits::rotr(n32 * mod_inv_25, 2);
                    if (q <= std::numeric_limits<uint32_t>::max() / 100) {
                        n32 = q;
                        s += 2;
                    } else
                        break;
                }
                auto q = bits::rotr(n32 * mod_inv_5, 1);
                if (q <= std::numeric_limits<uint32_t>::max() / 10) {
                    n32 = q;
                    s |= 1;
                }

                n = n32;
                return s;
            }

            // If n is not divisible by 10^8, work with n itself.
            constexpr auto mod_inv_5 = static_cast<uint64_t>(0xcccccccccccccccd);
            constexpr auto mod_inv_25 = mod_inv_5 * mod_inv_5;

            int32_t s = 0;
            while (true) {
                auto q = bits::rotr(n * mod_inv_25, 2);
                if (q <= std::numeric_limits<uint64_t>::max() / 100) {
                    n = q;
                    s += 2;
                } else
                    break;
            }
            auto q = bits::rotr(n * mod_inv_5, 1);
            if (q <= std::numeric_limits<uint64_t>::max() / 10) {
                n = q;
                s |= 1;
            }

            return s;
        }
    }

    template<class Dummy>
    struct compute_mul_impl<ieee754_binary32, Dummy> {
        static constexpr compute_mul_result compute_mul(carrier_uint u, cache_entry_type const& cache) noexcept
        {
            auto r = wuint::umul96_upper64(u, cache);
            return { carrier_uint(r >> 32), !carrier_uint(r) };
        }

        static constexpr uint32_t compute_delta(cache_entry_type const& cache, int32_t beta) noexcept
        {
            return static_cast<uint32_t>(cache >> (cache_bits - 1 - beta));
        }

        static constexpr compute_mul_parity_result compute_mul_parity(carrier_uint two_f, cache_entry_type const& cache, int32_t beta) noexcept
        {
            ASSERT(beta >= 1);
            ASSERT(beta < 64);

            auto r = wuint::umul96_lower64(two_f, cache);
            return { static_cast<bool>((r >> (64 - beta)) & 1), !static_cast<bool>(static_cast<uint32_t>(r >> (32 - beta))) };
        }

        static constexpr carrier_uint
        compute_left_endpoint_for_shorter_interval_case(cache_entry_type const& cache, int32_t beta) noexcept
        {
            return carrier_uint((cache - (cache >> (significand_bits + 2))) >> (cache_bits - significand_bits - 1 - beta));
        }

        static constexpr carrier_uint
        compute_right_endpoint_for_shorter_interval_case(cache_entry_type const& cache, int32_t beta) noexcept
        {
            return carrier_uint((cache + (cache >> (significand_bits + 1))) >> (cache_bits - significand_bits - 1 - beta));
        }

        static constexpr carrier_uint
        compute_round_up_for_shorter_interval_case(cache_entry_type const& cache, int32_t beta) noexcept
        {
            return (carrier_uint(cache >> (cache_bits - significand_bits - 2 - beta)) + 1) / 2;
        }
    };

    template<class Dummy>
    struct compute_mul_impl<ieee754_binary64, Dummy> {
        static constexpr compute_mul_result compute_mul(carrier_uint u, cache_entry_type const& cache) noexcept
        {
            auto r = wuint::umul192_upper128(u, cache);
            uint64_t r_high = static_cast<uint64_t>(r >> 64);
            uint64_t r_low = static_cast<uint64_t>(r);
            return { r_high, !r_low };
        }

        static constexpr uint32_t compute_delta(cache_entry_type const& cache, int32_t beta) noexcept
        {
            uint64_t cache_high = static_cast<uint64_t>(cache >> 64);
            return static_cast<uint32_t>(cache_high >> (carrier_bits - 1 - beta));
        }

        static constexpr compute_mul_parity_result compute_mul_parity(carrier_uint two_f, cache_entry_type const& cache, int32_t beta) noexcept
        {
            ASSERT(beta >= 1);
            ASSERT(beta < 64);

            auto r = wuint::umul192_lower128(two_f, cache);
            uint64_t r_high = static_cast<uint64_t>(r >> 64);
            uint64_t r_low = static_cast<uint64_t>(r);
            return { static_cast<bool>((r_high >> (64 - beta)) & 1), !static_cast<bool>(((r_high << beta) | (r_low >> (64 - beta)))) };
        }

        static constexpr carrier_uint
        compute_left_endpoint_for_shorter_interval_case(cache_entry_type const& cache, int32_t beta) noexcept
        {
            uint64_t cache_high = static_cast<uint64_t>(cache >> 64);
            return (cache_high - (cache_high >> (significand_bits + 2))) >> (carrier_bits - significand_bits - 1 - beta);
        }

        static constexpr carrier_uint
        compute_right_endpoint_for_shorter_interval_case(cache_entry_type const& cache, int32_t beta) noexcept
        {
            uint64_t cache_high = static_cast<uint64_t>(cache >> 64);
            return (cache_high + (cache_high >> (significand_bits + 1))) >> (carrier_bits - significand_bits - 1 - beta);
        }

        static constexpr carrier_uint
        compute_round_up_for_shorter_interval_case(cache_entry_type const& cache, int32_t beta) noexcept
        {
            uint64_t cache_high = static_cast<uint64_t>(cache >> 64);
            return ((cache_high >> (carrier_bits - significand_bits - 2 - beta)) + 1) / 2;
        }
    };

    static constexpr bool
    is_right_endpoint_integer_shorter_interval(int32_t exponent) noexcept
    {
        return exponent >= case_shorter_interval_right_endpoint_lower_threshold && exponent <= case_shorter_interval_right_endpoint_upper_threshold;
    }

    static constexpr bool is_left_endpoint_integer_shorter_interval(int32_t exponent) noexcept
    {
        return exponent >= case_shorter_interval_left_endpoint_lower_threshold && exponent <= case_shorter_interval_left_endpoint_upper_threshold;
    }
};

}

////////////////////////////////////////////////////////////////////////////////////////
// The interface function.
////////////////////////////////////////////////////////////////////////////////////////

template<class Float, class FloatTraits = default_float_traits<Float>, class... Policies>
ALWAYS_INLINE constexpr detail::to_decimal_return_type<FloatTraits, Policies...>
to_decimal(signed_significand_bits<Float, FloatTraits> signed_significand_bits, unsigned exponent_bits, Policies...) noexcept
{
    // Build policy holder type.
    using policy_holder = detail::to_decimal_policy_holder<Policies...>;

    return policy_holder::delegate(
        signed_significand_bits,
        detail::to_decimal_dispatcher<Float, FloatTraits, policy_holder> { },
        signed_significand_bits,
        exponent_bits);
}

template<class Float, class FloatTraits = default_float_traits<Float>, class... Policies>
ALWAYS_INLINE constexpr detail::to_decimal_return_type<FloatTraits, Policies...>
to_decimal(Float x, Policies... policies) noexcept
{
    auto const br = float_bits<Float, FloatTraits>(x);
    auto const exponent_bits = br.extract_exponent_bits();
    auto const s = br.remove_exponent_bits(exponent_bits);
    ASSERT(br.is_finite());

    return to_decimal<Float, FloatTraits>(s, exponent_bits, policies...);
}

// ------------------------------------------------------------------------------------------

enum class Mode : uint8_t {
    ToShortest = 1,
    ToExponential = 2,
};

enum class PrintTrailingZero : uint8_t {
    Yes,
    No,
};

struct PrintConfig {
    Mode mode;
    PrintTrailingZero print_trailing_zero;
};

template<class FloatFormat>
ALWAYS_INLINE constexpr size_t to_exponential_max_string_length()
{
    // Maximum required buffer size for exponential mode (excluding null-terminator)
    //     If ieee754_binary32 then sign(1) + significand(9) + decimal_point(1) + exp_marker(1) + exp_sign(1) + exp(2).
    //     If ieee754_binary64 then sign(1) + significand(17) + decimal_point(1) + exp_marker(1) + exp_sign(1) + exp(3).
    return std::is_same<FloatFormat, ieee754_binary32>::value ? (1 + 9 + 1 + 1 + 1 + 2) : (1 + 17 + 1 + 1 + 1 + 3);
}

template<class FloatFormat>
ALWAYS_INLINE constexpr size_t to_shortest_max_string_length()
{
    constexpr int32_t decimal_in_shortest_low = WTF::double_conversion::default_decimal_in_shortest_low;
    constexpr int32_t decimal_in_shortest_high = WTF::double_conversion::default_decimal_in_shortest_high;
    static_assert(decimal_in_shortest_low <= 0);
    static_assert(decimal_in_shortest_high >= FloatFormat::decimal_digits);

    // Maximum required buffer size for exponential mode (excluding null-terminator)
    //     If ieee754_binary32 then sign(1) + significand(9) + decimal_point(1) + max(-low, (high-9)).
    //     If ieee754_binary64 then sign(1) + significand(17) + decimal_point(1) + max(-low, (high-17)).
    return 1 + FloatFormat::decimal_digits + 1 + std::max(-decimal_in_shortest_low, decimal_in_shortest_high - FloatFormat::decimal_digits);
}

template<class FloatFormat>
ALWAYS_INLINE constexpr size_t max_string_length()
{
    // Maximum required buffer size (excluding null-terminator)
    return std::max(to_exponential_max_string_length<FloatFormat>(), to_shortest_max_string_length<FloatFormat>());
}

ALWAYS_INLINE constexpr bool valid_shortest_representation(int32_t decimal_point)
{
    constexpr int32_t decimal_in_shortest_low = WTF::double_conversion::default_decimal_in_shortest_low;
    constexpr int32_t decimal_in_shortest_high = WTF::double_conversion::default_decimal_in_shortest_high;

    int32_t exponent = decimal_point - 1;
    return double_conversion::validShortestRepresentation(exponent, decimal_in_shortest_low, decimal_in_shortest_high);
}

ALWAYS_INLINE uint32_t count_digits_base10_with_max_17(uint64_t v)
{
    if (v >= 10000000000000000ull)
        return 17;
    if (v >= 1000000000000000ull)
        return 16;
    if (v >= 100000000000000ull)
        return 15;
    if (v >= 10000000000000ull)
        return 14;
    if (v >= 1000000000000ull)
        return 13;
    if (v >= 100000000000ull)
        return 12;
    if (v >= 10000000000ull)
        return 11;
    if (v >= 1000000000)
        return 10;
    if (v >= 100000000)
        return 9;
    if (v >= 10000000)
        return 8;
    if (v >= 1000000)
        return 7;
    if (v >= 100000)
        return 6;
    if (v >= 10000)
        return 5;
    if (v >= 1000)
        return 4;
    if (v >= 100)
        return 3;
    if (v >= 10)
        return 2;
    return 1;
}

ALWAYS_INLINE uint64_t compute_power_with_max_16(uint64_t base, uint32_t k)
{
    ASSERT(0 < k && k < ieee754_binary64::decimal_digits);
    switch (k) {
    case 1:
        return detail::compute_power<1>(base);
    case 2:
        return detail::compute_power<2>(base);
    case 3:
        return detail::compute_power<3>(base);
    case 4:
        return detail::compute_power<4>(base);
    case 5:
        return detail::compute_power<5>(base);
    case 6:
        return detail::compute_power<6>(base);
    case 7:
        return detail::compute_power<7>(base);
    case 8:
        return detail::compute_power<8>(base);
    case 9:
        return detail::compute_power<9>(base);
    case 10:
        return detail::compute_power<10>(base);
    case 11:
        return detail::compute_power<11>(base);
    case 12:
        return detail::compute_power<12>(base);
    case 13:
        return detail::compute_power<13>(base);
    case 14:
        return detail::compute_power<14>(base);
    case 15:
        return detail::compute_power<15>(base);
    case 16:
        return detail::compute_power<16>(base);
    default:
        return 0;
    }
}

} // namespace dragonbox

} // namespace WTF
