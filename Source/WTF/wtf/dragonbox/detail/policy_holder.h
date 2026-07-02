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

#include <wtf/dragonbox/detail/policy.h>

namespace WTF {

namespace dragonbox {

namespace detail {

////////////////////////////////////////////////////////////////////////////////////////
// Policy holder.
////////////////////////////////////////////////////////////////////////////////////////

namespace policy_impl {
// The library will specify a list of accepted kinds of policies and their defaults,
// and the user will pass a list of policies. The aim of helper classes/functions
// here is to do the following:
//   1. Check if the policy parameters given by the user are all valid; that means,
//      each of them should be of the kinds specified by the library.
//      If that's not the case, then the compilation fails.
//   2. Check if multiple policy parameters for the same kind is specified by the
//   user.
//      If that's the case, then the compilation fails.
//   3. Build a class deriving from all policies the user have given, and also from
//      the default policies if the user did not specify one for some kinds.
// A policy belongs to a certain kind if it is deriving from a base class.

// For a given kind, find a policy belonging to that kind.
// Check if there are more than one such policies.
enum class policy_found_info {
    not_found,
    unique,
    repeated
};
template<class Policy, policy_found_info info>
struct found_policy_pair {
    using policy = Policy;
    static constexpr auto found_info = info;
};

template<class Base, class DefaultPolicy>
struct base_default_pair {
    using base = Base;

    template<class FoundPolicyInfo, class... Policies>
    struct get_found_policy_pair_impl;

    template<class FoundPolicyInfo>
    struct get_found_policy_pair_impl<FoundPolicyInfo> {
        using type = FoundPolicyInfo;
    };

    template<class FoundPolicyInfo, class FirstPolicy, class... RemainingPolicies>
    struct get_found_policy_pair_impl<FoundPolicyInfo, FirstPolicy, RemainingPolicies...> {
        using type = typename std::conditional<
            std::is_base_of<Base, FirstPolicy>::value,
            typename std::conditional<
                FoundPolicyInfo::found_info == policy_found_info::not_found,
                typename get_found_policy_pair_impl<found_policy_pair<FirstPolicy, policy_found_info::unique>, RemainingPolicies...>::type,
                typename get_found_policy_pair_impl<found_policy_pair<FirstPolicy, policy_found_info::repeated>, RemainingPolicies...>::type>::type,
            typename get_found_policy_pair_impl<FoundPolicyInfo, RemainingPolicies...>::type>::type;
    };

    template<class... Policies>
    using get_found_policy_pair = typename get_found_policy_pair_impl<found_policy_pair<DefaultPolicy, policy_found_info::not_found>, Policies...>::type;
};
template<class... BaseDefaultPairs>
struct base_default_pair_list { };

// Check if a given policy belongs to one of the kinds specified by the library.
template<class Policy>
constexpr bool check_policy_validity(Policy, base_default_pair_list<>)
{
    return false;
}
template<class Policy, class FirstBaseDefaultPair, class... RemainingBaseDefaultPairs>
constexpr bool check_policy_validity(Policy, base_default_pair_list<FirstBaseDefaultPair, RemainingBaseDefaultPairs...>)
{
    return std::is_base_of<typename FirstBaseDefaultPair::base, Policy>::value || check_policy_validity(Policy { }, base_default_pair_list<RemainingBaseDefaultPairs...> { });
}

template<class BaseDefaultPairList>
constexpr bool check_policy_list_validity(BaseDefaultPairList)
{
    return true;
}

template<class BaseDefaultPairList, class FirstPolicy, class... RemainingPolicies>
constexpr bool check_policy_list_validity(BaseDefaultPairList, FirstPolicy, RemainingPolicies... remaining_policies)
{
    return check_policy_validity(FirstPolicy { }, BaseDefaultPairList { }) && check_policy_list_validity(BaseDefaultPairList { }, remaining_policies...);
}

// Build policy_holder.
template<bool repeated_, class... FoundPolicyPairs>
struct found_policy_pair_list {
    static constexpr bool repeated = repeated_;
};

template<class... Policies>
struct policy_holder : Policies... { };

template<class BaseDefaultPairList, class FoundPolicyPairList, class... Policies>
struct make_policy_holder_impl;

template<bool repeated, class... FoundPolicyPairs, class... Policies>
struct make_policy_holder_impl<base_default_pair_list<>, found_policy_pair_list<repeated, FoundPolicyPairs...>, Policies...> {
    using type = found_policy_pair_list<repeated, FoundPolicyPairs...>;
};

template<class FirstBaseDefaultPair, class... RemainingBaseDefaultPairs, bool repeated, class... FoundPolicyPairs, class... Policies>
struct make_policy_holder_impl<
    base_default_pair_list<FirstBaseDefaultPair, RemainingBaseDefaultPairs...>,
    found_policy_pair_list<repeated, FoundPolicyPairs...>,
    Policies...> {
    using new_found_policy_pair = typename FirstBaseDefaultPair::template get_found_policy_pair<Policies...>;

    using type = typename make_policy_holder_impl<
        base_default_pair_list<RemainingBaseDefaultPairs...>,
        found_policy_pair_list<(repeated || new_found_policy_pair::found_info == policy_found_info::repeated), new_found_policy_pair, FoundPolicyPairs...>,
        Policies...>::type;
};

template<class BaseDefaultPairList, class... Policies>
using policy_pair_list = typename make_policy_holder_impl<BaseDefaultPairList, found_policy_pair_list<false>, Policies...>::type;

template<class FoundPolicyPairList, class... RawPolicies>
struct convert_to_policy_holder_impl;

template<bool repeated, class... RawPolicies>
struct convert_to_policy_holder_impl<found_policy_pair_list<repeated>, RawPolicies...> {
    using type = policy_holder<RawPolicies...>;
};

template<bool repeated, class FirstFoundPolicyPair, class... RemainingFoundPolicyPairs, class... RawPolicies>
struct convert_to_policy_holder_impl<
    found_policy_pair_list<repeated, FirstFoundPolicyPair, RemainingFoundPolicyPairs...>,
    RawPolicies...> {
    using type = typename convert_to_policy_holder_impl<
        found_policy_pair_list<repeated, RemainingFoundPolicyPairs...>,
        typename FirstFoundPolicyPair::policy, RawPolicies...>::type;
};

template<class FoundPolicyPairList>
using convert_to_policy_holder = typename convert_to_policy_holder_impl<FoundPolicyPairList>::type;

template<class BaseDefaultPairList, class... Policies>
constexpr convert_to_policy_holder<policy_pair_list<BaseDefaultPairList, Policies...>> make_policy_holder(BaseDefaultPairList, [[maybe_unused]] Policies... policies) {
    static_assert(check_policy_list_validity(BaseDefaultPairList { }, Policies { }...), "jkj::dragonbox: an invalid policy is specified");
    static_assert(!policy_pair_list<BaseDefaultPairList, Policies...>::repeated, "jkj::dragonbox: each policy should be specified at most once");
    return { };
}
} // namespace policy_impl

template<class... Policies>
using to_decimal_policy_holder = decltype(policy_impl::make_policy_holder(
    policy_impl::base_default_pair_list<
        policy_impl::base_default_pair<policy_impl::sign::base, policy_impl::sign::return_sign>,
        policy_impl::base_default_pair<policy_impl::trailing_zero::base, policy_impl::trailing_zero::remove>,
        policy_impl::base_default_pair<policy_impl::decimal_to_binary_rounding::base, policy_impl::decimal_to_binary_rounding::nearest_to_even>,
        policy_impl::base_default_pair<policy_impl::binary_to_decimal_rounding::base, policy_impl::binary_to_decimal_rounding::to_even>,
        policy_impl::base_default_pair<policy_impl::cache::base, policy_impl::cache::full>> { },
    Policies { }...));

template<class FloatTraits, class... Policies>
using to_decimal_return_type = decimal_fp<typename FloatTraits::carrier_uint,
    to_decimal_policy_holder<Policies...>::return_has_sign,
    to_decimal_policy_holder<Policies...>::report_trailing_zeros>;

template<class Float, class FloatTraits, class PolicyHolder, class IntervalTypeProvider>
struct invoke_shorter_dispatcher {
    using unsigned_return_type = decimal_fp<typename FloatTraits::carrier_uint, false, PolicyHolder::report_trailing_zeros>;

    template<class... Args>
    ALWAYS_INLINE constexpr unsigned_return_type operator()(Args... args) noexcept
    {
        return impl<Float, FloatTraits>::template compute_nearest_shorter<
            unsigned_return_type,
            typename IntervalTypeProvider::shorter_interval_type,
            typename PolicyHolder::trailing_zero_policy,
            typename PolicyHolder::binary_to_decimal_rounding_policy,
            typename PolicyHolder::cache_policy>(args...);
    }
};

template<class Float, class FloatTraits, class PolicyHolder, class IntervalTypeProvider>
struct invoke_normal_dispatcher {
    using unsigned_return_type = decimal_fp<typename FloatTraits::carrier_uint, false, PolicyHolder::report_trailing_zeros>;

    template<class... Args>
    ALWAYS_INLINE constexpr unsigned_return_type operator()(Args... args) noexcept
    {
        return impl<Float, FloatTraits>::template compute_nearest_normal<
            unsigned_return_type,
            typename IntervalTypeProvider::normal_interval_type,
            typename PolicyHolder::trailing_zero_policy,
            typename PolicyHolder::binary_to_decimal_rounding_policy,
            typename PolicyHolder::cache_policy>(args...);
    }
};

template<class Float, class FloatTraits, class PolicyHolder, class IntervalTypeProvider>
constexpr decimal_fp<typename FloatTraits::carrier_uint, PolicyHolder::return_has_sign, PolicyHolder::report_trailing_zeros>
to_decimal_impl(signed_significand_bits<Float, FloatTraits> signed_significand_bits, unsigned exponent_bits) noexcept
{
    using unsigned_return_type = decimal_fp<typename FloatTraits::carrier_uint, false, PolicyHolder::report_trailing_zeros>;
    using format = typename FloatTraits::format;
    constexpr auto tag = IntervalTypeProvider::tag;

    auto two_fc = signed_significand_bits.remove_sign_bit_and_shift();
    auto exponent = static_cast<int32_t>(exponent_bits);

    if constexpr (tag == policy_impl::decimal_to_binary_rounding::tag_t::to_nearest) {
        // Is the input a normal number?
        if (exponent) {
            exponent += format::exponent_bias - format::significand_bits;

            // Shorter interval case; proceed like Schubfach.
            // One might think this condition is wrong, since when exponent_bits ==
            // 1 and two_fc == 0, the interval is actually regular. However, it
            // turns out that this seemingly wrong condition is actually fine,
            // because the end result is anyway the same.
            //
            // [binary32]
            // (fc-1/2) * 2^e = 1.175'494'28... * 10^-38
            // (fc-1/4) * 2^e = 1.175'494'31... * 10^-38
            //    fc    * 2^e = 1.175'494'35... * 10^-38
            // (fc+1/2) * 2^e = 1.175'494'42... * 10^-38
            //
            // Hence, shorter_interval_case will return 1.175'494'4 * 10^-38.
            // 1.175'494'3 * 10^-38 is also a correct shortest representation that
            // will be rejected if we assume shorter interval, but 1.175'494'4 *
            // 10^-38 is closer to the true value so it doesn't matter.
            //
            // [binary64]
            // (fc-1/2) * 2^e = 2.225'073'858'507'201'13... * 10^-308
            // (fc-1/4) * 2^e = 2.225'073'858'507'201'25... * 10^-308
            //    fc    * 2^e = 2.225'073'858'507'201'38... * 10^-308
            // (fc+1/2) * 2^e = 2.225'073'858'507'201'63... * 10^-308
            //
            // Hence, shorter_interval_case will return 2.225'073'858'507'201'4 *
            // 10^-308. This is indeed of the shortest length, and it is the unique
            // one closest to the true value among valid representations of the same
            // length.
            static_assert(std::is_same<format, ieee754_binary32>::value || std::is_same<format, ieee754_binary64>::value, "");

            if (!two_fc) {
                return PolicyHolder::handle_sign(
                    signed_significand_bits,
                    IntervalTypeProvider::invoke_shorter_interval_case(
                        signed_significand_bits,
                        invoke_shorter_dispatcher<Float, FloatTraits, PolicyHolder, IntervalTypeProvider> { },
                        exponent));
            }

            two_fc |= (decltype(two_fc)(1) << (format::significand_bits + 1));
        } else {
            // Is the input a subnormal number?
            exponent = format::min_exponent - format::significand_bits;
        }

        return PolicyHolder::handle_sign(
            signed_significand_bits,
            IntervalTypeProvider::invoke_normal_interval_case(
                signed_significand_bits,
                invoke_normal_dispatcher<Float, FloatTraits, PolicyHolder, IntervalTypeProvider> { },
                two_fc,
                exponent));
    } else if constexpr (tag == policy_impl::decimal_to_binary_rounding::tag_t::left_closed_directed) {
        // Is the input a normal number?
        if (exponent) {
            exponent += format::exponent_bias - format::significand_bits;
            two_fc |= (decltype(two_fc)(1) << (format::significand_bits + 1));
        } else {
            // Is the input a subnormal number?
            exponent = format::min_exponent - format::significand_bits;
        }

        return PolicyHolder::handle_sign(
            signed_significand_bits,
            detail::impl<Float, FloatTraits>::template compute_left_closed_directed<
                unsigned_return_type,
                typename PolicyHolder::trailing_zero_policy,
                typename PolicyHolder::cache_policy>(two_fc, exponent));
    } else {
        static_assert(tag == policy_impl::decimal_to_binary_rounding::tag_t::right_closed_directed, "");

        bool shorter_interval = false;

        // Is the input a normal number?
        if (exponent) {
            if (!two_fc && exponent != 1)
                shorter_interval = true;
            exponent += format::exponent_bias - format::significand_bits;
            two_fc |= (decltype(two_fc)(1) << (format::significand_bits + 1));
        } else {
            // Is the input a subnormal number?
            exponent = format::min_exponent - format::significand_bits;
        }

        return PolicyHolder::handle_sign(
            signed_significand_bits,
            detail::impl<Float, FloatTraits>::template compute_right_closed_directed<
                unsigned_return_type,
                typename PolicyHolder::trailing_zero_policy,
                typename PolicyHolder::cache_policy>(two_fc, exponent, shorter_interval));
    }
}

template<class Float, class FloatTraits, class PolicyHolder>
struct to_decimal_dispatcher {
    using return_type = decimal_fp<typename FloatTraits::carrier_uint, PolicyHolder::return_has_sign, PolicyHolder::report_trailing_zeros>;

    template<class IntervalTypeProvider, class... Args>
    ALWAYS_INLINE constexpr return_type operator()(IntervalTypeProvider, Args... args) noexcept
    {
        return to_decimal_impl<Float, FloatTraits, PolicyHolder, IntervalTypeProvider>(args...);
    }
};

} // namespace detail

} // namespace dragonbox

} // namespace WTF
