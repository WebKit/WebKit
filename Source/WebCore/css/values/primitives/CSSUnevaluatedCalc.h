/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include <WebCore/CSSPrimitiveNumericConcepts.h>
#include <WebCore/CSSValueTypes.h>
#include <optional>
#include <wtf/Forward.h>
#include <wtf/IterationStatus.h>
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class CSSCalcSymbolsAllowed;
class CSSCalcSymbolTable;
class CSSParserTokenRange;
class CSSToLengthConversionData;
class RenderStyle;
struct ComputedStyleDependencies;
struct CSSPropertyParserOptions;

namespace Style {
class BuilderState;
class UnevaluatedCalculationBase;
}

namespace CSSCalc {
class Value;
}

namespace CSS {

struct PropertyParserState;

enum class Category : uint8_t;

// Type-erased helpers to allow for shared code.

void NODELETE unevaluatedCalcRef(CSSCalc::Value*);
void unevaluatedCalcDeref(CSSCalc::Value*);

// `UnevaluatedCalc` annotates a `CSSCalc::Value` with the raw value type that it
// will be evaluated to, allowing the processing of calc in generic code.

// Non-generic base type to allow code sharing and out-of-line definitions.
class UnevaluatedCalcBase {
public:
    WEBCORE_EXPORT UnevaluatedCalcBase(CSSCalc::Value&);
    UnevaluatedCalcBase(Ref<CSSCalc::Value>&&);
    UnevaluatedCalcBase(Category, Range, const Style::UnevaluatedCalculationBase&, const RenderStyle&);

    UnevaluatedCalcBase(const UnevaluatedCalcBase&);
    UnevaluatedCalcBase(UnevaluatedCalcBase&&);
    UnevaluatedCalcBase& operator=(const UnevaluatedCalcBase&);
    UnevaluatedCalcBase& operator=(UnevaluatedCalcBase&&);

    WEBCORE_EXPORT ~UnevaluatedCalcBase();

    static std::optional<UnevaluatedCalcBase> parseBase(CSSParserTokenRange&, PropertyParserState&, Category, Range, CSSCalcSymbolsAllowed&&, const CSSPropertyParserOptions&);

    CSSCalc::Value& calcValue() const { return m_calc; }
    [[nodiscard]] CSSCalc::Value& NODELETE leakRef();

    Category NODELETE runtimeCategory() const;
    CSSUnitType NODELETE primitiveType() const;
    bool NODELETE rootNodeIsPercentage() const;
    bool NODELETE requiresConversionData() const;

    bool canBeCastedTo(Category) const;

    void serializationForCSS(StringBuilder&, const SerializationContext&) const;
    void collectComputedStyleDependencies(ComputedStyleDependencies&) const;

    UnevaluatedCalcBase simplifyBase(const CSSToLengthConversionData&) const;
    UnevaluatedCalcBase simplifyBase(const CSSToLengthConversionData&, const CSSCalcSymbolTable&) const;
    UnevaluatedCalcBase simplifyBase(NoConversionDataRequiredToken) const;
    UnevaluatedCalcBase simplifyBase(NoConversionDataRequiredToken, const CSSCalcSymbolTable&) const;

    double evaluate(const Style::BuilderState&) const;
    double evaluate(const Style::BuilderState&, const CSSCalcSymbolTable&) const;
    double evaluate(const CSSToLengthConversionData&) const;
    double evaluate(const CSSToLengthConversionData&, const CSSCalcSymbolTable&) const;
    double evaluate(NoConversionDataRequiredToken) const;
    double evaluate(NoConversionDataRequiredToken, const CSSCalcSymbolTable&) const;
    double evaluateDeprecated() const;

    Style::UnevaluatedCalculationBase createCalculationValue(const CSSToLengthConversionData&) const;
    Style::UnevaluatedCalculationBase createCalculationValue(const CSSToLengthConversionData&, const CSSCalcSymbolTable&) const;
    Style::UnevaluatedCalculationBase createCalculationValue(NoConversionDataRequiredToken) const;
    Style::UnevaluatedCalculationBase createCalculationValue(NoConversionDataRequiredToken, const CSSCalcSymbolTable&) const;

    bool operator==(const UnevaluatedCalcBase&) const;

private:
    Ref<CSSCalc::Value> m_calc;
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

    explicit UnevaluatedCalc(const Style::UnevaluatedCalculationBase& value, const RenderStyle& style)
        : UnevaluatedCalcBase(category, range, value, style)
    {
    }

    explicit UnevaluatedCalc(UnevaluatedCalcBase&& base)
        : UnevaluatedCalcBase(WTF::move(base))
    {
    }

    explicit UnevaluatedCalc(const UnevaluatedCalcBase& base)
        : UnevaluatedCalcBase(base)
    {
    }

    static std::optional<UnevaluatedCalc> parse(CSSParserTokenRange& tokens, PropertyParserState& state, CSSCalcSymbolsAllowed&& symbolsAllowed, const CSSPropertyParserOptions& options)
    {
        if (auto result = parseBase(tokens, state, category, range, WTF::move(symbolsAllowed), options))
            return UnevaluatedCalc(WTF::move(*result));
        return std::nullopt;
    }

    bool operator==(const UnevaluatedCalc& other) const
    {
        return UnevaluatedCalcBase::operator==(static_cast<const UnevaluatedCalcBase&>(other));
    }

    UnevaluatedCalc simplify(const CSSToLengthConversionData& conversionData) const
    {
        return UnevaluatedCalc(simplifyBase(conversionData));
    }
    UnevaluatedCalc simplify(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
    {
        return UnevaluatedCalc(simplifyBase(conversionData, symbolTable));
    }
    UnevaluatedCalc simplify(NoConversionDataRequiredToken token) const
    {
        return UnevaluatedCalc(simplifyBase(token));
    }
    UnevaluatedCalc simplify(NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) const
    {
        return UnevaluatedCalc(simplifyBase(token, symbolTable));
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
    return WTF::switchOn(component, [&](auto alternative) -> Variant<Ts...> { return simplifyUnevaluatedCalc(alternative, conversionData, symbolTable); });
}

template<typename T> decltype(auto) simplifyUnevaluatedCalc(const std::optional<T>& component, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return component ? std::make_optional(simplifyUnevaluatedCalc(*component, conversionData, symbolTable)) : std::nullopt;
}

// MARK: - Serialization

template<> struct Serialize<UnevaluatedCalcBase> {
    inline void operator()(StringBuilder& builder, const SerializationContext& context, const UnevaluatedCalcBase& value)
    {
        value.serializationForCSS(builder, context);
    }
};

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
    inline IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const T&)
    {
        return IterationStatus::Continue;
    }
};

} // namespace CSS
} // namespace WebCore

namespace WTF {
template<WebCore::CSS::Calc T> struct IsSmartPtr<T> {
    static constexpr bool value = true;
};

} // namespace WTF
