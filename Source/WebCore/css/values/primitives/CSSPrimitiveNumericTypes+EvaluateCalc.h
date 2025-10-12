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

#include <WebCore/CSSNoConversionDataRequiredToken.h>
#include <WebCore/CSSPrimitiveNumericTypes.h>
#include <WebCore/CSSUnevaluatedCalc.h>

namespace WebCore {
namespace CSS {

// MARK: - Requires Conversion Data

inline bool requiresConversionData(Numeric auto const& primitive)
{
    return WTF::switchOn(primitive, [&](const auto& value) { return requiresConversionData(value); });
}

// MARK: - Evaluation

// FIXME: Remove "evaluateCalc" family of functions once color code has moved to the "toStyle" family of functions.

template<Calc T> auto evaluateCalc(const T& calc, NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) -> typename T::Raw
{
    return { calc.evaluate(T::category, token, symbolTable) };
}

template<typename T> constexpr auto evaluateCalc(const T& component, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) -> T
{
    return component;
}

template<typename... Ts> auto evaluateCalcIfNoConversionDataRequired(const Variant<Ts...>& component, const CSSCalcSymbolTable& symbolTable) -> Variant<Ts...>
{
    return WTF::switchOn(component, [&](const auto& alternative) -> Variant<Ts...> {
        if (requiresConversionData(alternative))
            return alternative;
        return evaluateCalc(alternative, NoConversionDataRequiredToken { }, symbolTable);
    });
}

template<Numeric T> auto evaluateCalcIfNoConversionDataRequired(const T& component, const CSSCalcSymbolTable& symbolTable) -> T
{
    return WTF::switchOn(component, [&](const auto& alternative) -> T {
        if (requiresConversionData(alternative))
            return { alternative };
        return { evaluateCalc(alternative, NoConversionDataRequiredToken { }, symbolTable) };
    });
}

template<typename T> decltype(auto) evaluateCalcIfNoConversionDataRequired(const std::optional<T>& component, const CSSCalcSymbolTable& symbolTable)
{
    return component ? std::make_optional(evaluateCalcIfNoConversionDataRequired(*component, symbolTable)) : std::nullopt;
}

// MARK: - Is UnevaluatedCalc

inline bool isUnevaluatedCalc(Numeric auto const& value)
{
    return WTF::switchOn(value, [&](const auto& alternative) { return isUnevaluatedCalc(alternative); });
}

} // namespace CSS
} // namespace WebCore
