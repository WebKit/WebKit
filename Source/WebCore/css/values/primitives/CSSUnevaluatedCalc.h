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

#include <WebCore/CSSPrimitiveNumericConcepts.h>
#include <WebCore/CSSValueTypes.h>
#include <optional>
#include <wtf/Forward.h>
#include <wtf/IterationStatus.h>
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class CSSCalcSymbolTable;
class CSSCalcValue;
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

void unevaluatedCalcRef(CSSCalcValue*);
void unevaluatedCalcDeref(CSSCalcValue*);

// `UnevaluatedCalc` annotates a `CSSCalcValue` with the raw value type that it
// will be evaluated to, allowing the processing of calc in generic code.

// Non-generic base type to allow code sharing and out-of-line definitions.
struct UnevaluatedCalcBase {
    UnevaluatedCalcBase(CSSCalcValue&);
    UnevaluatedCalcBase(Ref<CSSCalcValue>&&);

    UnevaluatedCalcBase(const UnevaluatedCalcBase&);
    UnevaluatedCalcBase(UnevaluatedCalcBase&&);
    UnevaluatedCalcBase& operator=(const UnevaluatedCalcBase&);
    UnevaluatedCalcBase& operator=(UnevaluatedCalcBase&&);
    ~UnevaluatedCalcBase();

    Ref<CSSCalcValue> protectedCalc() const;
    CSSCalcValue& leakRef() WARN_UNUSED_RETURN;

    bool requiresConversionData() const;

    void serializationForCSS(StringBuilder&, const CSS::SerializationContext&) const;
    void collectComputedStyleDependencies(ComputedStyleDependencies&) const;
    IterationStatus visitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>&) const;

    UnevaluatedCalcBase simplifyBase(const CSSToLengthConversionData&, const CSSCalcSymbolTable&) const;

    double evaluate(Calculation::Category, const Style::BuilderState&) const;
    double evaluate(Calculation::Category, const Style::BuilderState&, const CSSCalcSymbolTable&) const;
    double evaluate(Calculation::Category, const CSSToLengthConversionData&) const;
    double evaluate(Calculation::Category, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) const;
    double evaluate(Calculation::Category, NoConversionDataRequiredToken) const;
    double evaluate(Calculation::Category, NoConversionDataRequiredToken, const CSSCalcSymbolTable&) const;

    bool equal(const UnevaluatedCalcBase&) const;

private:
    Ref<CSSCalcValue> calc;
};

template<NumericRaw RawType> struct UnevaluatedCalc : UnevaluatedCalcBase {
    using UnevaluatedCalcBase::UnevaluatedCalcBase;
    using UnevaluatedCalcBase::operator=;

    using Raw = RawType;
    using UnitType = typename Raw::UnitType;
    using UnitTraits = typename Raw::UnitTraits;
    using ResolvedValueType = typename Raw::ResolvedValueType;
    static constexpr auto range = Raw::range;
    static constexpr auto category = Raw::category;

    bool operator==(const UnevaluatedCalc& other) const
    {
        return UnevaluatedCalcBase::equal(static_cast<const UnevaluatedCalcBase&>(other));
    }

    UnevaluatedCalc simplify(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
    {
        return static_cast<UnevaluatedCalc>(simplifyBase(conversionData, symbolTable));
    }
};

// MARK: - Requires Conversion Data

template<Calc T> bool requiresConversionData(const T& unevaluatedCalc)
{
    return unevaluatedCalc.requiresConversionData();
}

template<typename T> bool requiresConversionData(const T&)
{
    static_assert(!Calc<T>);
    return false;
}

template<typename... Ts> bool requiresConversionData(const Variant<Ts...>& component)
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

template<typename... Ts> constexpr bool isUnevaluatedCalc(const Variant<Ts...>& component)
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
    return unevaluatedCalc.simplify(conversionData, symbolTable);
}

template<typename T> auto simplifyUnevaluatedCalc(const T& component, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> T
{
    return component;
}

template<typename... Ts> auto simplifyUnevaluatedCalc(const Variant<Ts...>& component, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> Variant<Ts...>
{
    return WTF::switchOn(component, [&](auto alternative) -> bool { return simplifyUnevaluatedCalc(alternative, conversionData, symbolTable); });
}

template<typename T> decltype(auto) simplifyUnevaluatedCalc(const std::optional<T>& component, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return component ? std::make_optional(simplifyUnevaluatedCalc(*component, conversionData, symbolTable)) : std::nullopt;
}

// MARK: - Serialization

template<Calc T> struct Serialize<T> {
    inline void operator()(StringBuilder& builder, const SerializationContext& context, const T& value)
    {
        value.serializationForCSS(builder, context);
    }
};

// MARK: - Computed Style Dependencies

template<Calc T> struct ComputedStyleDependenciesCollector<T> {
    inline void operator()(ComputedStyleDependencies& dependencies, const T& value)
    {
        value.collectComputedStyleDependencies(dependencies);
    }
};

// MARK: - CSSValue Visitation

template<Calc T> struct CSSValueChildrenVisitor<T> {
    inline IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const T& value)
    {
        return value.visitChildren(func);
    }
};

} // namespace CSS
} // namespace WebCore

namespace WTF {
template<WebCore::CSS::Calc T> struct IsSmartPtr<T> {
    static constexpr bool value = true;
};

} // namespace WTF
