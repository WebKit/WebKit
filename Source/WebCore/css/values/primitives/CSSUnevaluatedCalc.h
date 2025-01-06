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

#include "CSSCalcValue.h"
#include "CSSPrimitiveNumericConcepts.h"
#include "CSSValueTypes.h"
#include <optional>
#include <variant>
#include <wtf/Brigand.h>
#include <wtf/Forward.h>
#include <wtf/IterationStatus.h>
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class CSSCalcSymbolTable;
class CSSToLengthConversionData;
struct ComputedStyleDependencies;
struct NoConversionDataRequiredToken;

namespace Calculation {
enum class Category : uint8_t;
}

namespace Style {
class BuilderState;
}

namespace CSS {

// Type-erased helpers to allow for shared code.
bool unevaluatedCalcEqual(const Ref<CSSCalcValue>&, const Ref<CSSCalcValue>&);
bool unevaluatedCalcRequiresConversionData(const Ref<CSSCalcValue>&);

void unevaluatedCalcSerialization(StringBuilder&, const Ref<CSSCalcValue>&);
void unevaluatedCalcCollectComputedStyleDependencies(ComputedStyleDependencies&, const Ref<CSSCalcValue>&);

Ref<CSSCalcValue> unevaluatedCalcSimplify(const Ref<CSSCalcValue>&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&);

double unevaluatedCalcEvaluate(const Ref<CSSCalcValue>&, Calculation::Category, const Style::BuilderState&);
double unevaluatedCalcEvaluate(const Ref<CSSCalcValue>&, Calculation::Category, const Style::BuilderState&, const CSSCalcSymbolTable&);
double unevaluatedCalcEvaluate(const Ref<CSSCalcValue>&, Calculation::Category, const CSSToLengthConversionData&);
double unevaluatedCalcEvaluate(const Ref<CSSCalcValue>&, Calculation::Category, const CSSToLengthConversionData&, const CSSCalcSymbolTable&);
double unevaluatedCalcEvaluate(const Ref<CSSCalcValue>&, Calculation::Category, NoConversionDataRequiredToken);
double unevaluatedCalcEvaluate(const Ref<CSSCalcValue>&, Calculation::Category, NoConversionDataRequiredToken, const CSSCalcSymbolTable&);

// `UnevaluatedCalc` annotates a `CSSCalcValue` with the raw value type that it
// will be evaluated to, allowing the processing of calc in generic code.
template<NumericRaw RawType> struct UnevaluatedCalc {
    using Raw = RawType;
    using UnitType = typename Raw::UnitType;
    using UnitTraits = typename Raw::UnitTraits;
    using ResolvedValueType = typename Raw::ResolvedValueType;
    static constexpr auto range = Raw::range;
    static constexpr auto category = Raw::category;

    UnevaluatedCalc(Ref<CSSCalcValue>&& value)
        : calc { WTFMove(value) }
    {
    }

    Ref<CSSCalcValue> protectedCalc() const
    {
        return calc;
    }

    bool operator==(const UnevaluatedCalc& other) const
    {
        return unevaluatedCalcEqual(calc, other.calc);
    }

private:
    Ref<CSSCalcValue> calc;
};

// MARK: - Requires Conversion Data

template<Calc T> bool requiresConversionData(const T& unevaluatedCalc)
{
    return unevaluatedCalcRequiresConversionData(unevaluatedCalc.protectedCalc());
}

template<typename T> bool requiresConversionData(const T&)
{
    static_assert(!Calc<T>);
    return false;
}

template<typename... Ts> bool requiresConversionData(const std::variant<Ts...>& component)
{
    return WTF::switchOn(component, [&](auto alternative) -> bool { return requiresConversionData(alternative); });
}

template<typename T> bool requiresConversionData(const std::optional<T>& component)
{
    return component && requiresConversionData(*component);
}

// MARK: - Is UnevaluatedCalc

template<typename T> constexpr bool isUnevaluatedCalc(const T&)
{
    return Calc<T>;
}

template<typename... Ts> constexpr bool isUnevaluatedCalc(const std::variant<Ts...>& component)
{
    return WTF::switchOn(component, [&](auto alternative) -> bool { return isUnevaluatedCalc(alternative); });
}

template<typename T> constexpr bool isUnevaluatedCalc(const std::optional<T>& component)
{
    return component && isUnevaluatedCalc(*component);
}

// MARK: Simplify

template<Calc T> auto simplifyUnevaluatedCalc(const T& unevaluatedCalc, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> T
{
    return { unevaluatedCalcSimplify(unevaluatedCalc, conversionData, symbolTable) };
}

template<typename T> auto simplifyUnevaluatedCalc(const T& component, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> T
{
    return component;
}

template<typename... Ts> auto simplifyUnevaluatedCalc(const std::variant<Ts...>& component, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> std::variant<Ts...>
{
    return WTF::switchOn(component, [&](auto alternative) -> bool { return simplifyUnevaluatedCalc(alternative, conversionData, symbolTable); });
}

template<typename T> decltype(auto) simplifyUnevaluatedCalc(const std::optional<T>& component, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return component ? std::make_optional(simplifyUnevaluatedCalc(*component, conversionData, symbolTable)) : std::nullopt;
}

// MARK: - Serialization

template<Calc T> struct Serialize<T> {
    inline void operator()(StringBuilder& builder, const T& value)
    {
        unevaluatedCalcSerialization(builder, value.protectedCalc());
    }
};

// MARK: - Computed Style Dependencies

template<Calc T> struct ComputedStyleDependenciesCollector<T> {
    inline void operator()(ComputedStyleDependencies& dependencies, const T& value)
    {
        unevaluatedCalcCollectComputedStyleDependencies(dependencies, value.protectedCalc());
    }
};

// MARK: - CSSValue Visitation

template<Calc T> struct CSSValueChildrenVisitor<T> {
    inline IterationStatus operator()(const Function<IterationStatus(CSSValue&)>& func, const T& value)
    {
        return func(value.protectedCalc());
    }
};

} // namespace CSS
} // namespace WebCore

template<WebCore::CSS::Calc T> struct WTF::IsSmartPtr<T> {
    static constexpr bool value = true;
};
