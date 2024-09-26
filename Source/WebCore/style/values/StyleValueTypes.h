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

#include "CSSCalcSymbolTable.h"
#include <optional>
#include <tuple>
#include <utility>
#include <variant>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class RenderStyle;

namespace Style {

class BuilderState;

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

// MARK: - Conversion from "Style to "CSS"

template<typename StyleType> decltype(auto) toCSS(const std::optional<StyleType>& value, const RenderStyle& style)
{
    return value.transform([&](auto&& v) { return toCSS(v, style); });
}

template<typename... StyleTypes> decltype(auto) toCSS(const SpaceSeparatedTuple<StyleTypes...>& value, const RenderStyle& style)
{
    using CSSTuple = std::tuple<decltype(toCSS(std::declval<StyleTypes>(), style))...>;
    return CSS::SpaceSeparatedTuple { WTF::apply([&](const auto& ...x) { return CSSTuple { toCSS(x, style)... }; }, value.value) };
}

// MARK: Support for treating aggregate types as Tuple-like

template<size_t I, typename... Ts> decltype(auto) get(const SpaceSeparatedTuple<Ts...>& tuple)
{
    return std::get<I>(tuple.value);
}

} // namespace Style

namespace CSS {

// MARK: - Conversion from "CSS" to "Style"

template<typename... CSSTypes> decltype(auto) toStyle(const std::variant<CSSTypes...>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    using StyleVariant = std::variant<std::decay_t<decltype(toStyle(std::declval<const CSSTypes&>(), conversionData, symbolTable))>...>;
    return WTF::switchOn(value, [&](const auto& alternative) { return StyleVariant { toStyle(alternative, conversionData, symbolTable) }; });
}

template<typename... CSSTypes> decltype(auto) toStyle(const std::variant<CSSTypes...>& value, Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable)
{
    using StyleVariant = std::variant<std::decay_t<decltype(toStyle(std::declval<const CSSTypes&>(), state, symbolTable))>...>;
    return WTF::switchOn(value, [&](const auto& alternative) { return StyleVariant { toStyle(alternative, state, symbolTable) }; });
}

template<typename CSSType> decltype(auto) toStyle(const std::optional<CSSType>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return value.transform([&](auto&& v) { return toStyle(v, conversionData, symbolTable); });
}

template<typename CSSType> decltype(auto) toStyle(const std::optional<CSSType>& value, Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable)
{
    return value.transform([&](auto&& v) { return toStyle(v, state, symbolTable); });
}

template<typename... CSSTypes> decltype(auto) toStyle(const SpaceSeparatedTuple<CSSTypes...>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    using StyleTuple = std::tuple<std::decay_t<decltype(toStyle(std::declval<const CSSTypes&>(), conversionData, symbolTable))>...>;
    return Style::SpaceSeparatedTuple { WTF::apply([&](const auto& ...x) { return StyleTuple { toStyle(x, conversionData, symbolTable)... }; }, value.value) };
}

template<typename... CSSTypes> decltype(auto) toStyle(const SpaceSeparatedTuple<CSSTypes...>& value, Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable)
{
    using StyleTuple = std::tuple<std::decay_t<decltype(toStyle(std::declval<const CSSTypes&>(), state, symbolTable))>...>;
    return Style::SpaceSeparatedTuple { WTF::apply([&](const auto& ...x) { return StyleTuple { toStyle(x, state, symbolTable)... }; }, value.value) };
}

template<typename CSSType> decltype(auto) toStyle(CSSType&& value, const CSSToLengthConversionData& conversionData)
{
    // Fallback when no CSSCalcSymbolTable is provided.
    return toStyle(std::forward<CSSType>(value), conversionData, CSSCalcSymbolTable { });
}

template<typename CSSType> decltype(auto) toStyle(CSSType&& value, Style::BuilderState& state)
{
    // Fallback when no CSSCalcSymbolTable is provided.
    return toStyle(std::forward<CSSType>(value), state, CSSCalcSymbolTable { });
}

// MARK: - Conversion from "CSS" to "Style" (no conversion data required)

template<typename... CSSTypes> decltype(auto) toStyleNoConversionDataRequired(const std::variant<CSSTypes...>& value, const CSSCalcSymbolTable& symbolTable)
{
    using StyleVariant = std::variant<std::decay_t<decltype(toStyleNoConversionDataRequired(std::declval<const CSSTypes&>(), symbolTable))>...>;
    return WTF::switchOn(value, [&](const auto& alternative) { return StyleVariant { toStyleNoConversionDataRequired(alternative, symbolTable) }; });
}

template<typename CSSType> decltype(auto) toStyleNoConversionDataRequired(const std::optional<CSSType>& value, const CSSCalcSymbolTable& symbolTable)
{
    return value.transform([&](auto&& v) { return toStyleNoConversionDataRequired(v, symbolTable); });
}

template<typename... CSSTypes> decltype(auto) toStyleNoConversionDataRequired(const SpaceSeparatedTuple<CSSTypes...>& value, const CSSCalcSymbolTable& symbolTable)
{
    using StyleTuple = std::tuple<std::decay_t<decltype(toStyleNoConversionDataRequired(std::declval<const CSSTypes&>()))>...>;
    return SpaceSeparatedTuple { WTF::apply([&](const auto& ...x) { return StyleTuple { toStyleNoConversionDataRequired(x, symbolTable)... }; }, value.value) };
}

template<typename CSSType> decltype(auto) toStyleNoConversionDataRequired(CSSType&& value)
{
    // Fallback when no CSSCalcSymbolTable is provided.
    return toStyleNoConversionDataRequired(std::forward<CSSType>(value), CSSCalcSymbolTable { });
}

} // namespace CSS
} // namespace WebCore

namespace std {

template<typename... Ts> class tuple_size<WebCore::Style::SpaceSeparatedTuple<Ts...>> : public std::integral_constant<size_t, sizeof...(Ts)> { };
template<size_t I, typename... Ts> class tuple_element<I, WebCore::Style::SpaceSeparatedTuple<Ts...>> {
public:
    using type = tuple_element_t<I, tuple<Ts...>>;
};

} // namespace std
