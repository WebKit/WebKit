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
#include <wtf/Brigand.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class CSSCalcSymbolTable;
class CSSCalcValue;

// Type-erased helpers to allow for shared code.
bool unevaluatedCalcEqual(const Ref<CSSCalcValue>&, const Ref<CSSCalcValue>&);
void unevaluatedCalcSerialization(StringBuilder&, const Ref<CSSCalcValue>&);

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

// MARK: - Utility templates

template<typename T>
struct IsUnevaluatedCalc : public std::integral_constant<bool, WTF::IsTemplate<T, UnevaluatedCalc>::value> { };

template<typename TypeList>
struct TypesMinusUnevaluatedCalc {
    using ResultTypeList = brigand::remove_if<TypeList, IsUnevaluatedCalc<brigand::_1>>;
    using type = VariantOrSingle<ResultTypeList>;
};
template<typename... Ts>
using TypesMinusUnevaluatedCalcType = typename TypesMinusUnevaluatedCalc<brigand::list<Ts...>>::type;

template<typename>
struct TypePlusUnevaluatedCalc;

template<> struct TypePlusUnevaluatedCalc<AngleRaw> {
    using type = brigand::list<AngleRaw, UnevaluatedCalc<AngleRaw>>;
};
template<> struct TypePlusUnevaluatedCalc<PercentRaw> {
    using type = brigand::list<PercentRaw, UnevaluatedCalc<PercentRaw>>;
};
template<> struct TypePlusUnevaluatedCalc<NumberRaw> {
    using type = brigand::list<NumberRaw, UnevaluatedCalc<NumberRaw>>;
};
template<> struct TypePlusUnevaluatedCalc<LengthRaw> {
    using type = brigand::list<LengthRaw, UnevaluatedCalc<LengthRaw>>;
};
template<> struct TypePlusUnevaluatedCalc<ResolutionRaw> {
    using type = brigand::list<ResolutionRaw, UnevaluatedCalc<ResolutionRaw>>;
};
template<> struct TypePlusUnevaluatedCalc<TimeRaw> {
    using type = brigand::list<TimeRaw, UnevaluatedCalc<TimeRaw>>;
};
template<> struct TypePlusUnevaluatedCalc<NoneRaw> {
    using type = brigand::list<NoneRaw>;
};
template<> struct TypePlusUnevaluatedCalc<SymbolRaw> {
    using type = brigand::list<SymbolRaw>;
};

template<typename TypeList>
struct TypesPlusUnevaluatedCalc {
    using ResultTypeList = brigand::flatten<brigand::transform<TypeList, TypePlusUnevaluatedCalc<brigand::_1>>>;
    using type = VariantOrSingle<ResultTypeList>;
};
template<typename... Ts>
using TypesPlusUnevaluatedCalcType = typename TypesPlusUnevaluatedCalc<brigand::list<Ts...>>::type;


// MARK: - Serialization

template<typename T>
void serializationForCSS(StringBuilder& builder, const UnevaluatedCalc<T>& unevaluatedCalc)
{
    unevaluatedCalcSerialization(builder, unevaluatedCalc.calc);
}

// MARK: - Evaluation

AngleRaw evaluateCalc(const UnevaluatedCalc<AngleRaw>&, const CSSCalcSymbolTable&);
NumberRaw evaluateCalc(const UnevaluatedCalc<NumberRaw>&, const CSSCalcSymbolTable&);
PercentRaw evaluateCalc(const UnevaluatedCalc<PercentRaw>&, const CSSCalcSymbolTable&);
LengthRaw evaluateCalc(const UnevaluatedCalc<LengthRaw>&, const CSSCalcSymbolTable&);
ResolutionRaw evaluateCalc(const UnevaluatedCalc<ResolutionRaw>&, const CSSCalcSymbolTable&);
TimeRaw evaluateCalc(const UnevaluatedCalc<TimeRaw>&, const CSSCalcSymbolTable&);

template<typename T>
auto evaluateCalc(const T& component, const CSSCalcSymbolTable&) -> T
{
    return component;
}

template<typename... Ts>
auto evaluateCalc(const std::variant<Ts...>& component, const CSSCalcSymbolTable& symbolTable) -> TypesMinusUnevaluatedCalcType<Ts...>
{
    return WTF::switchOn(component, [&](auto part) -> TypesMinusUnevaluatedCalcType<Ts...> {
        return evaluateCalc(part, symbolTable);
    });
}

template<typename... Ts>
auto evaluateCalc(const std::optional<std::variant<Ts...>>& component, const CSSCalcSymbolTable& symbolTable) -> std::optional<TypesMinusUnevaluatedCalcType<Ts...>>
{
    if (component)
        return evaluateCalc(*component, symbolTable);
    return std::nullopt;
}

} // namespace WebCore
