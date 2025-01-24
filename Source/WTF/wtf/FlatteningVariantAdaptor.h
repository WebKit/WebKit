/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include <wtf/Brigand.h>
#include <wtf/CompactVariant.h>
#include <wtf/StdLibExtras.h>
#include <wtf/VariantExtras.h>

namespace WTF {

// `FlatteningVariantAdaptor` adapts "Variant-Like" types and attempts to
// minimize memory usage by *flattening* nested "Variant-Like" types.
//
// For example, take the following usage:
//
//   using Foo = FlatteningVariantAdaptor<std::variant, std::variant<std::string, int>, double>;
//
// Here, `Foo` will be internally represented by:
//
//   std::variant<std::string, int, double>
//
// allowing a single index to be used for all options.
//
// To add support for flattening to variant-like type, clients can specialize
// `FlatteningVariantTraits` to expose the list of the nested types.
//
// Standard aliases for a flattening `std::variant` and `CompactVariant` are
// provided called `FlatteningVariant` and `FlatteningCompactVariant`.

// MARK: - FlatteningVariantTraits

template<typename T> struct FlatteningVariantTraits {
    using TypeList = brigand::list<T>;
};

template<typename... Ts> struct FlatteningVariantTraits<std::variant<Ts...>> {
    using TypeList = brigand::list<Ts...>;
};

template<typename... Ts> struct FlatteningVariantTraits<CompactVariant<Ts...>> {
    using TypeList = brigand::list<Ts...>;
};

// MARK: - FlatteningVariantAdaptor

template<template<typename...> typename VariantType, typename... Ts> class FlatteningVariantAdaptor {
    template<typename... Us> using Wrapper = VariantType<Us...>;
public:
    using TypeList = brigand::append<typename FlatteningVariantTraits<Ts>::TypeList...>;
    using Representation = brigand::wrap<TypeList, Wrapper>;

    template<typename T> static constexpr Representation extract(T&& value)
    {
        if constexpr (brigand::size<typename FlatteningVariantTraits<std::remove_cvref_t<T>>::TypeList>::value == 1)
            return std::forward<T>(value);
        else
            return WTF::switchOn(std::forward<T>(value), []<typename U>(U&& value) { return extract(std::forward<U>(value)); });
    }

    template<typename T> constexpr FlatteningVariantAdaptor(T&& value)
        : m_value { extract(std::forward<T>(value)) }
    {
    }

    // Checks if type `T` is included in the flattened list of types.
    template<typename T> constexpr bool holdsAlternative() const
    {
        return holdsAlternative<T>(m_value);
    }

    // Switches on the flattened list of types.
    template<typename... F> decltype(auto) switchOn(F&&... functors) const
    {
        return WTF::switchOn(m_value, std::forward<F>(functors)...);
    }

    // TODO: Add `holdsAlternativeFromOriginalList` that checks all flattened alternatives for the provided type.
    // TODO: Add `switchOnOriginalList` that reconstructs extracted values.

    bool operator==(const FlatteningVariantAdaptor&) const = default;

private:
    Representation m_value;
};

// Specialization of `FlatteningVariantTraits` for `FlatteningVariantAdaptor` itself so that
// recursively used `FlatteningVariantAdaptor` types can flatten together.
template<template<typename...> typename VariantType, typename... Ts> struct FlatteningVariantTraits<FlatteningVariantAdaptor<VariantType, Ts...>> {
    using TypeList = typename FlatteningVariantAdaptor<VariantType, Ts...>::TypeList;
};

// MARK: - Standard FlatteningVariantAdaptor Aliases

template<typename... Ts> using FlatteningVariant = FlatteningVariantAdaptor<std::variant, Ts...>;
template<typename... Ts> using FlatteningCompactVariant = FlatteningVariantAdaptor<CompactVariant, Ts...>;

} // namespace WTF

using WTF::FlatteningCompactVariant;
using WTF::FlatteningVariant;
using WTF::FlatteningVariantTraits;
