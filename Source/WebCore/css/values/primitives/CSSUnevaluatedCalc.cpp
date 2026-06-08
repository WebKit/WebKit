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

#include "config.h"
#include "CSSUnevaluatedCalc.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcSymbolsAllowed.h"
#include "CSSCalcValue.h"
#include "CSSNoConversionDataRequiredToken.h"
#include "CSSPropertyParserOptions.h"
#include "StyleBuilderState.h"
#include "StyleCalculationValue.h"
#include "StyleUnevaluatedCalculation.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

void unevaluatedCalcRef(CSSCalc::Value* calc)
{
    calc->ref();
}

void unevaluatedCalcDeref(CSSCalc::Value* calc)
{
    calc->deref();
}

UnevaluatedCalcBase::UnevaluatedCalcBase(CSSCalc::Value& value)
    : m_calc { value }
{
}

UnevaluatedCalcBase::UnevaluatedCalcBase(Ref<CSSCalc::Value>&& value)
    : m_calc { WTF::move(value) }
{
}

UnevaluatedCalcBase::UnevaluatedCalcBase(Category category, Range range, const Style::UnevaluatedCalculationBase& value, const RenderStyle& style)
    : m_calc { CSSCalc::Value::create(category, range, protect(value.calculation()), style) }
{
}

UnevaluatedCalcBase::UnevaluatedCalcBase(const UnevaluatedCalcBase&) = default;
UnevaluatedCalcBase::UnevaluatedCalcBase(UnevaluatedCalcBase&&) = default;
UnevaluatedCalcBase& UnevaluatedCalcBase::operator=(const UnevaluatedCalcBase&) = default;
UnevaluatedCalcBase& UnevaluatedCalcBase::operator=(UnevaluatedCalcBase&&) = default;

UnevaluatedCalcBase::~UnevaluatedCalcBase() = default;

std::optional<UnevaluatedCalcBase> UnevaluatedCalcBase::parseBase(CSSParserTokenRange& tokens, PropertyParserState& state, Category category, Range range, CSSCalcSymbolsAllowed&& symbolsAllowed, const CSSPropertyParserOptions& options)
{
    if (RefPtr value = CSSCalc::Value::parse(tokens, state, category, range, WTF::move(symbolsAllowed), options))
        return UnevaluatedCalcBase { value.releaseNonNull() };
    return std::nullopt;
}

CSSCalc::Value& UnevaluatedCalcBase::leakRef()
{
    return m_calc.leakRef();
}

bool UnevaluatedCalcBase::operator==(const UnevaluatedCalcBase& other) const
{
    return protect(calcValue())->equals(other.m_calc.get());
}

Category UnevaluatedCalcBase::runtimeCategory() const
{
    return calcValue().category();
}

CSSUnitType UnevaluatedCalcBase::primitiveType() const
{
    return calcValue().primitiveType();
}

bool UnevaluatedCalcBase::rootNodeIsPercentage() const
{
    return calcValue().rootNodeIsPercentage();
}

bool UnevaluatedCalcBase::requiresConversionData() const
{
    return calcValue().requiresConversionData();
}

bool UnevaluatedCalcBase::canBeCastedTo(Category targetCategory) const
{
    switch (runtimeCategory()) {
    case CSS::Category::Integer:
    case CSS::Category::Number:
        return targetCategory == CSS::Category::Integer
            || targetCategory == CSS::Category::Number;
    case CSS::Category::Percentage:
        return targetCategory == CSS::Category::Percentage
            || targetCategory == CSS::Category::AnglePercentage
            || targetCategory == CSS::Category::LengthPercentage;
    case CSS::Category::Length:
        return targetCategory == CSS::Category::Length
            || targetCategory == CSS::Category::LengthPercentage;
    case CSS::Category::Angle:
        return targetCategory == CSS::Category::Angle
            || targetCategory == CSS::Category::AnglePercentage;
    case CSS::Category::Time:
        return targetCategory == CSS::Category::Time;
    case CSS::Category::Frequency:
        return targetCategory == CSS::Category::Frequency;
    case CSS::Category::Resolution:
        return targetCategory == CSS::Category::Resolution;
    case CSS::Category::Flex:
        return targetCategory == CSS::Category::Flex;
    case CSS::Category::LengthPercentage:
        return targetCategory == CSS::Category::LengthPercentage;
    case CSS::Category::AnglePercentage:
        return targetCategory == CSS::Category::AnglePercentage;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void UnevaluatedCalcBase::serializationForCSS(StringBuilder& builder, const CSS::SerializationContext& context) const
{
    builder.append(protect(calcValue())->cssText(context));
}

void UnevaluatedCalcBase::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    protect(calcValue())->collectComputedStyleDependencies(dependencies);
}

UnevaluatedCalcBase UnevaluatedCalcBase::simplifyBase(const CSSToLengthConversionData& conversionData) const
{
    return UnevaluatedCalcBase { protect(calcValue())->copySimplified(conversionData) };
}

UnevaluatedCalcBase UnevaluatedCalcBase::simplifyBase(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    return UnevaluatedCalcBase { protect(calcValue())->copySimplified(conversionData, symbolTable) };
}

UnevaluatedCalcBase UnevaluatedCalcBase::simplifyBase(NoConversionDataRequiredToken token) const
{
    return UnevaluatedCalcBase { protect(calcValue())->copySimplified(token) };
}

UnevaluatedCalcBase UnevaluatedCalcBase::simplifyBase(NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) const
{
    return UnevaluatedCalcBase { protect(calcValue())->copySimplified(token, symbolTable) };
}

double UnevaluatedCalcBase::evaluate(const Style::BuilderState& state) const
{
    return evaluate(state.cssToLengthConversionData(), { });
}

double UnevaluatedCalcBase::evaluate(const Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable) const
{
    return evaluate(state.cssToLengthConversionData(), symbolTable);
}

double UnevaluatedCalcBase::evaluate(const CSSToLengthConversionData& conversionData) const
{
    return evaluate(conversionData, { });
}

double UnevaluatedCalcBase::evaluate(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    return protect(calcValue())->doubleValue(conversionData, symbolTable);
}

double UnevaluatedCalcBase::evaluate(NoConversionDataRequiredToken token) const
{
    return evaluate(token, { });
}

double UnevaluatedCalcBase::evaluate(NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) const
{
    return protect(calcValue())->doubleValue(token, symbolTable);
}

double UnevaluatedCalcBase::evaluateDeprecated() const
{
    return protect(calcValue())->doubleValueDeprecated();
}

Style::UnevaluatedCalculationBase UnevaluatedCalcBase::createCalculationValue(const CSSToLengthConversionData& conversionData) const
{
    return Style::UnevaluatedCalculationBase { protect(calcValue())->createCalculationValue(conversionData) };
}

Style::UnevaluatedCalculationBase UnevaluatedCalcBase::createCalculationValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    return Style::UnevaluatedCalculationBase { protect(calcValue())->createCalculationValue(conversionData, symbolTable) };
}

Style::UnevaluatedCalculationBase UnevaluatedCalcBase::createCalculationValue(NoConversionDataRequiredToken token) const
{
    return Style::UnevaluatedCalculationBase { protect(calcValue())->createCalculationValue(token) };
}

Style::UnevaluatedCalculationBase UnevaluatedCalcBase::createCalculationValue(NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) const
{
    return Style::UnevaluatedCalculationBase { protect(calcValue())->createCalculationValue(token, symbolTable) };
}

} // namespace CSS
} // namespace WebCore
