/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CSSPropertyParserConsumer+RawTypes.h"
#include <optional>
#include <variant>
#include <wtf/Forward.h>
#include <wtf/Ref.h>

namespace WebCore {

class CSSCalcSymbolTable;
class CSSCalcValue;

// Type-erased helpers to allow for shared code.
bool unevaluatedCalcEqual(const Ref<CSSCalcValue>&, const Ref<CSSCalcValue>&);

// `UnevaluatedCalc` annotates a `CSSCalcValue` with the raw value type that it
// will be evaluated to, allowing the processing of calc in generic code.
template<typename T> struct UnevaluatedCalc {
    using RawType = T;
    Ref<CSSCalcValue> calc;

    bool operator==(const UnevaluatedCalc<T>& other) const
    {
        return unevaluatedCalcEqual(calc, other.calc);
    }
};

AngleRaw evaluateCalc(const UnevaluatedCalc<AngleRaw>&, const CSSCalcSymbolTable&);
NumberRaw evaluateCalc(const UnevaluatedCalc<NumberRaw>&, const CSSCalcSymbolTable&);
PercentRaw evaluateCalc(const UnevaluatedCalc<PercentRaw>&, const CSSCalcSymbolTable&);
LengthRaw evaluateCalc(const UnevaluatedCalc<LengthRaw>&, const CSSCalcSymbolTable&);
ResolutionRaw evaluateCalc(const UnevaluatedCalc<ResolutionRaw>&, const CSSCalcSymbolTable&);
TimeRaw evaluateCalc(const UnevaluatedCalc<TimeRaw>&, const CSSCalcSymbolTable&);

template<typename Result, typename... Ts>
auto evaluateCalc(const std::variant<Ts...>& component, const CSSCalcSymbolTable& symbolTable) -> Result
{
    return WTF::switchOn(component, [&](auto part) -> Result {
        return evaluateCalc(part, symbolTable);
    });
}

template<typename Result, typename... Ts>
auto evaluateCalc(const std::optional<std::variant<Ts...>>& component, const CSSCalcSymbolTable& symbolTable) -> std::optional<Result>
{
    if (component)
        return evaluateCalc<Result>(*component, symbolTable);
    return std::nullopt;
}

template<typename T>
auto evaluateCalc(const T& component, const CSSCalcSymbolTable&) -> T
{
    return component;
}

} // namespace WebCore
