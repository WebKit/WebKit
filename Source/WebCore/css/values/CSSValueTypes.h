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

#include "CSSValue.h"
#include <optional>
#include <tuple>
#include <utility>
#include <variant>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

// Wraps a variadic list of types, semantically marking them as serializing as "space separated".
template<typename... Ts> struct SpaceSeparatedTuple {
    constexpr SpaceSeparatedTuple(Ts&&... values)
        : value { std::make_tuple(std::forward<Ts>(values)...) }
    {
    }

    constexpr SpaceSeparatedTuple(const Ts&... values)
        : value { std::make_tuple(values...) }
    {
    }

    constexpr SpaceSeparatedTuple(std::tuple<Ts...>&& tuple)
        : value { WTFMove(tuple) }
    {
    }

    constexpr bool operator==(const SpaceSeparatedTuple<Ts...>&) const = default;

    std::tuple<Ts...> value;
};

// MARK: Serialization

template<typename T> void serializationForCSS(StringBuilder& builder, const std::optional<T>& optional)
{
    if (!optional)
        return;
    serializationForCSS(builder, *optional);
}

template<typename... Ts> void serializationForCSS(StringBuilder& builder, const std::variant<Ts...>& variant)
{
    WTF::switchOn(variant, [&](auto& alternative) { serializationForCSS(builder, alternative); });
}

template<typename... Ts> void serializationForCSS(StringBuilder& builder, const SpaceSeparatedTuple<Ts...>& tuple)
{
    auto separator = ""_s;
    auto caller = WTF::makeVisitor(
        [&]<typename T>(std::optional<T>& css) {
            if (!css)
                return;
            builder.append(std::exchange(separator, " "_s));
            serializationForCSS(builder, *css);
        },
        [&](auto& css) {
            builder.append(std::exchange(separator, " "_s));
            serializationForCSS(builder, css);
        }
    );

    WTF::apply([&](const auto& ...x) { (..., caller(x)); }, tuple.value);
}

// MARK: - Computed Style Dependencies

// What properties does this value rely on (eg, font-size for em units)?

template<typename T> void collectComputedStyleDependencies(ComputedStyleDependencies& dependencies, const std::optional<T>& optional)
{
    if (!optional)
        return;
    collectComputedStyleDependencies(dependencies, *optional);
}

template<typename... Ts> void collectComputedStyleDependencies(ComputedStyleDependencies& dependencies, const std::variant<Ts...>& variant)
{
    WTF::switchOn(variant, [&](auto& alternative) { collectComputedStyleDependencies(dependencies, alternative); });
}

template<typename... Ts> void collectComputedStyleDependencies(ComputedStyleDependencies& dependencies, const SpaceSeparatedTuple<Ts...>& value)
{
    WTF::apply([&](const auto& ...x) { (..., collectComputedStyleDependencies(dependencies, x)); }, value.value);
}

// MARK: - CSSValue Visitation

template<typename T> constexpr IterationStatus visitCSSValueChildren(const T&, const Function<IterationStatus(CSSValue&)>&)
{
    return IterationStatus::Continue;
}

template<typename T1, typename T2> IterationStatus visitCSSValueChildren(const SpaceSeparatedTuple<T1, T2>& pair, const Function<IterationStatus(CSSValue&)>& func)
{
    if (visitCSSValueChildren(get<0>(pair), func) == IterationStatus::Done)
        return IterationStatus::Done;
    if (visitCSSValueChildren(get<1>(pair), func) == IterationStatus::Done)
        return IterationStatus::Done;
    return IterationStatus::Continue;
}

template<typename... Ts> IterationStatus visitCSSValueChildren(const std::variant<Ts...>& variant, const Function<IterationStatus(CSSValue&)>& func)
{
    return WTF::switchOn(variant, [&](const auto& alternative) { return visitCSSValueChildren(alternative, func); } );
}

template<typename T> IterationStatus visitCSSValueChildren(const std::optional<T>& optional, const Function<IterationStatus(CSSValue&)>& func)
{
    return optional ? visitCSSValueChildren(*optional, func) : IterationStatus::Continue;
}

// MARK: Support for treating aggregate types as Tuple-like

template<size_t I, typename... Ts> decltype(auto) get(const SpaceSeparatedTuple<Ts...>& value)
{
    return std::get<I>(value.value);
}

} // namespace CSS
} // namespace WebCore

namespace std {

template<typename... Ts> class tuple_size<WebCore::CSS::SpaceSeparatedTuple<Ts...>> : public std::integral_constant<size_t, sizeof...(Ts)> { };
template<size_t I, typename... Ts> class tuple_element<I, WebCore::CSS::SpaceSeparatedTuple<Ts...>> {
public:
    using type = tuple_element_t<I, tuple<Ts...>>;
};

}
