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

#include "CSSPrimitiveNumericTypes.h"
#include "CSSUnevaluatedCalc.h"

namespace WebCore {
namespace CSS {

// MARK: - Evaluation

// FIXME: Remove "evaluateCalc" family of functions once color code has moved to the "toStyle" family of functions.

template<RawNumeric T> auto evaluateCalcNoConversionDataRequired(const UnevaluatedCalc<T>& calc, const CSSCalcSymbolTable& symbolTable) -> T
{
    return { unevaluatedCalcEvaluateNoConversionDataRequired(calc.protectedCalc(), symbolTable, T::category) };
}

template<typename T> constexpr auto evaluateCalcNoConversionDataRequired(const T& component, const CSSCalcSymbolTable&) -> T
{
    return component;
}

template<typename... Ts> auto evaluateCalcIfNoConversionDataRequired(const std::variant<Ts...>& component, const CSSCalcSymbolTable& symbolTable) -> std::variant<Ts...>
{
    return WTF::switchOn(component, [&](auto part) -> std::variant<Ts...> {
        if (requiresConversionData(part))
            return part;
        return evaluateCalcNoConversionDataRequired(part, symbolTable);
    });
}

template<typename T> auto evaluateCalcIfNoConversionDataRequired(const PrimitiveNumeric<T>& component, const CSSCalcSymbolTable& symbolTable) -> PrimitiveNumeric<T>
{
    return { evaluateCalcIfNoConversionDataRequired(component.value, symbolTable) };
}

template<typename T> decltype(auto) evaluateCalcIfNoConversionDataRequired(const std::optional<T>& component, const CSSCalcSymbolTable& symbolTable)
{
    return component ? std::make_optional(evaluateCalcIfNoConversionDataRequired(*component, symbolTable)) : std::nullopt;
}

} // namespace CSS
} // namespace WebCore
