/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <variant>
#include <wtf/StdLibExtras.h>
#include <wtf/VectorTraits.h>

namespace WTF {

// MARK: - Utility concepts/traits for std::variant.

template<typename>       struct VariantAllAlternativesCanCopyWithMemcpyHelper;
template<typename... Ts> struct VariantAllAlternativesCanCopyWithMemcpyHelper<std::variant<Ts...>> : std::integral_constant<bool, all<VectorTraits<Ts>::canCopyWithMemcpy...>> { };
template<typename V>     concept VariantAllAlternativesCanCopyWithMemcpy = VariantAllAlternativesCanCopyWithMemcpyHelper<V>::value;

template<typename>       struct VariantAllAlternativesCanMoveWithMemcpyHelper;
template<typename... Ts> struct VariantAllAlternativesCanMoveWithMemcpyHelper<std::variant<Ts...>> : std::integral_constant<bool, all<VectorTraits<Ts>::canMoveWithMemcpy...>> { };
template<typename V>     concept VariantAllAlternativesCanMoveWithMemcpy = VariantAllAlternativesCanMoveWithMemcpyHelper<V>::value;

// MARK: - Best match for std::variant construction.

// `VariantBestMatch` picks the type `T` from `Ts...` in `std::variant<Ts...>` that will be used when the
// `std::variant<Ts...>` is constructed from type `Arg`. Implementation based off of libc++.

struct VariantNoNarrowingCheck {
    template<typename D, typename S> using apply = std::type_identity_t<D>;
};
struct VariantNarrowingCheck {
    template<typename D> static auto test(D(&&)[1]) -> std::type_identity_t<D>;
    template<typename D, typename S> using apply = decltype(test<D>({ std::declval<S>() }));
};
template<typename D, typename S> using VariantCheckForNarrowing = typename std::conditional_t<std::is_arithmetic_v<D>, VariantNarrowingCheck, VariantNoNarrowingCheck>::template apply<D, S>;
template<typename T, size_t I> struct VariantOverload {
    template<typename U> auto operator()(T, U&&) const -> VariantCheckForNarrowing<T, U>;
};
template<typename... Bases> struct VariantAllOverloads : Bases... {
    void operator()() const;
    using Bases::operator()...;
};
template<typename Seq>   struct VariantMakeOverloadsImpl;
template<size_t... I> struct VariantMakeOverloadsImpl<std::index_sequence<I...> > {
    template<typename... Ts> using apply = VariantAllOverloads<VariantOverload<Ts, I>...>;
};
template<typename... Ts> using VariantMakeOverloads = typename VariantMakeOverloadsImpl<std::make_index_sequence<sizeof...(Ts)> >::template apply<Ts...>;
template<typename T, typename... Ts> using VariantBestMatchImpl = typename std::invoke_result_t<VariantMakeOverloads<Ts...>, T, T>;

template<typename V, typename Arg>     struct VariantBestMatch;
template<typename Arg, typename... Ts> struct VariantBestMatch<std::variant<Ts...>, Arg> {
    using type = VariantBestMatchImpl<Arg, Ts...>;
};

// MARK: - Type switching for std::variant

// Calls a zero argument functor with a template argument corresponding to the index's mapped type.
//
// e.g.
//   using Variant = std::variant<int, float>;
//
//   Variant foo = 5;
//   typeForIndex<Variant>(
//       foo.index(), /* index will be 0 for first parameter, <int> */
//       []<typename T>() {
//           if constexpr (std::is_same_v<T, int>) {
//               print("we got an int");  <--- this will get called
//           } else if constexpr (std::is_same_v<T, float>) {
//               print("we got an float");  <--- this will NOT get called
//           }
//       }
//   );

template<typename V, typename F> constexpr decltype(auto) typeForIndex(size_t index, NOESCAPE F&& f)
{
    return visitAtIndex<0, std::variant_size_v<std::remove_cvref_t<V>>>(
        index,
        [&]<size_t I>() ALWAYS_INLINE_LAMBDA {
           return f.template operator()<std::variant_alternative_t<I, std::remove_cvref_t<V>>>();
        }
    );
}

} // namespace WTF
