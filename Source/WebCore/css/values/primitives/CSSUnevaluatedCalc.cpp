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
#include "CSSCalcTree+ComputedStyleDependencies.h"
#include "CSSCalcTree+Evaluation.h"
#include "CSSCalcTree+Parser.h"
#include "CSSCalcTree+Serialization.h"
#include "CSSCalcTree+Simplification.h"
#include "CSSCalcValue.h"
#include "CSSNoConversionDataRequiredToken.h"
#include "CSSPropertyParserOptions.h"
#include "CSSSerializationContext.h"
#include "StyleBuilderState.h"
#include "StyleCalculationTree+Conversion.h"
#include "StyleCalculationValue.h"
#include "StyleUnevaluatedCalculation.h"
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace CSS {

static UnevaluatedCalcBase simplify(const CSSCalc::Value& value, std::optional<CSSToLengthConversionData>&& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    auto simplificationOptions = CSSCalc::SimplificationOptions {
        .category = value.category(),
        .range = value.range(),
        .conversionData = WTF::move(conversionData),
        .symbolTable = symbolTable,
        .allowZeroValueLengthRemovalFromSum = true,
    };

    if (!CSSCalc::canSimplify(value.tree(), simplificationOptions))
        return const_cast<CSSCalc::Value&>(value);

    return CSSCalc::Value::create(value.category(), value.range(), CSSCalc::copyAndSimplify(value.tree(), simplificationOptions));
}

static Style::UnevaluatedCalculationBase createCalculationValue(const CSSCalc::Value& value, std::optional<CSSToLengthConversionData>&& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    auto toStyleOptions = Style::Calculation::ToStyleOptions {
        .category = value.category(),
        .range = value.range(),
        .conversionData = WTF::move(conversionData),
        .symbolTable = symbolTable
    };
    return Style::UnevaluatedCalculationBase { Style::Calculation::Value::create(Style::Calculation::toStyle(value.tree(), toStyleOptions)) };
}

static inline double clampToPermittedRange(Category category, Range range, double value)
{
    // If a top-level calculation would produce a value whose numeric part is NaN,
    // it instead act as though the numeric part is 0.
    value = std::isnan(value) ? 0 : value;

    // Signed zeros do not escape a top-level calculation; they are censored into the
    // "unsigned" (positive) zero. https://drafts.csswg.org/css-values-4/#calc-ieee
    value = value ? value : 0.0;

    // If an <angle> must be converted due to exceeding the implementation-defined range of supported values,
    // it must be clamped to the nearest supported multiple of 360deg.
    if (category == Category::Angle && std::isinf(value))
        return 0;

    if (category == Category::Integer)
        value = std::floor(value + 0.5);

    return CSS::clampToRange<double>(value, range);
}

static double evaluate(const CSSCalc::Value& value, std::optional<CSSToLengthConversionData>&& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    auto options = CSSCalc::EvaluationOptions {
        .category = value.category(),
        .range = value.range(),
        .conversionData = WTF::move(conversionData),
        .symbolTable = symbolTable
    };
    return clampToPermittedRange(value.category(), value.range(), evaluateDouble(value.tree(), options).value_or(0));
}

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

UnevaluatedCalcBase::UnevaluatedCalcBase(Category category, Range range, const Style::UnevaluatedCalculationBase& value)
    : m_calc {
        CSSCalc::Value::create(
            category,
            range,
            Style::Calculation::toCSS(
                value.calculation().tree(),
                Style::Calculation::ToCSSOptions {
                    .category = category,
                    .range = range,
                }
            )
        )
    }
{
}

UnevaluatedCalcBase::UnevaluatedCalcBase(const UnevaluatedCalcBase&) = default;
UnevaluatedCalcBase::UnevaluatedCalcBase(UnevaluatedCalcBase&&) = default;
UnevaluatedCalcBase& UnevaluatedCalcBase::operator=(const UnevaluatedCalcBase&) = default;
UnevaluatedCalcBase& UnevaluatedCalcBase::operator=(UnevaluatedCalcBase&&) = default;

UnevaluatedCalcBase::~UnevaluatedCalcBase() = default;

std::optional<UnevaluatedCalcBase> UnevaluatedCalcBase::parseBase(CSSParserTokenRange& tokens, PropertyParserState& state, Category category, Range range, CSSCalcSymbolsAllowed&& symbolsAllowed, const CSSPropertyParserOptions& propertyOptions)
{
    auto parserOptions = CSSCalc::ParserOptions {
        .category = category,
        .range = range,
        .allowedSymbols = WTF::move(symbolsAllowed),
        .propertyOptions = propertyOptions
    };
    auto simplificationOptions = CSSCalc::SimplificationOptions {
        .category = category,
        .range = range,
        .conversionData = std::nullopt,
        .symbolTable = { },
        .allowZeroValueLengthRemovalFromSum = false,
    };

    auto tree = CSSCalc::parseAndSimplify(tokens, state, parserOptions, simplificationOptions);
    if (!tree)
        return std::nullopt;

    return UnevaluatedCalcBase { CSSCalc::Value::create(category, range, WTF::move(*tree)) };
}

CSSCalc::Value& UnevaluatedCalcBase::leakRef()
{
    return m_calc.leakRef();
}

bool UnevaluatedCalcBase::operator==(const UnevaluatedCalcBase& other) const
{
    return calcValue().tree().root == other.calcValue().tree().root;
}

Category UnevaluatedCalcBase::runtimeCategory() const
{
    return calcValue().category();
}

CSSUnitType UnevaluatedCalcBase::primitiveType() const
{
    // This returns the CSSUnitType associated with the value returned by evaluate(), or, if CSSUnitType::CalcPercentageWithLength, that a call to createCalculationValue() is needed.

    switch (runtimeCategory()) {
    case Category::Integer:
        return CSSUnitType::Integer;
    case Category::Number:
        return CSSUnitType::Number;
    case Category::Percentage:
        return CSSUnitType::Percentage;
    case Category::Length:
        return CSSUnitType::Px;
    case Category::Angle:
        return CSSUnitType::Deg;
    case Category::Time:
        return CSSUnitType::S;
    case Category::Frequency:
        return CSSUnitType::Hz;
    case Category::Resolution:
        return CSSUnitType::Dppx;
    case Category::Flex:
        return CSSUnitType::Fr;
    case Category::LengthPercentage:
        if (!calcValue().tree().type.percentHint)
            return CSSUnitType::Px;
        if (WTF::holdsAlternative<CSSCalc::Percentage>(calcValue().tree().root))
            return CSSUnitType::Percentage;
        return CSSUnitType::CalcPercentageWithLength;
    case Category::AnglePercentage:
        if (!calcValue().tree().type.percentHint)
            return CSSUnitType::Deg;
        if (WTF::holdsAlternative<CSSCalc::Percentage>(calcValue().tree().root))
            return CSSUnitType::Percentage;
        return CSSUnitType::CalcPercentageWithAngle;
    }

    ASSERT_NOT_REACHED();
    return CSSUnitType::Number;
}

bool UnevaluatedCalcBase::rootNodeIsPercentage() const
{
    return WTF::holdsAlternative<CSSCalc::Percentage>(calcValue().tree().root);
}

bool UnevaluatedCalcBase::requiresConversionData() const
{
    return calcValue().tree().requiresConversionData;
}

bool UnevaluatedCalcBase::canBeCastedTo(Category targetCategory) const
{
    switch (runtimeCategory()) {
    case Category::Integer:
    case Category::Number:
        return targetCategory == Category::Integer
            || targetCategory == Category::Number;
    case Category::Percentage:
        return targetCategory == Category::Percentage
            || targetCategory == Category::AnglePercentage
            || targetCategory == Category::LengthPercentage;
    case Category::Length:
        return targetCategory == Category::Length
            || targetCategory == Category::LengthPercentage;
    case Category::Angle:
        return targetCategory == Category::Angle
            || targetCategory == Category::AnglePercentage;
    case Category::Time:
        return targetCategory == Category::Time;
    case Category::Frequency:
        return targetCategory == Category::Frequency;
    case Category::Resolution:
        return targetCategory == Category::Resolution;
    case Category::Flex:
        return targetCategory == Category::Flex;
    case Category::LengthPercentage:
        return targetCategory == Category::LengthPercentage;
    case Category::AnglePercentage:
        return targetCategory == Category::AnglePercentage;
    }

    ASSERT_NOT_REACHED();
    return false;
}

WTF::String UnevaluatedCalcBase::serializationForCSS(const SerializationContext& context) const
{
    auto options = CSSCalc::SerializationOptions {
        .range = calcValue().range(),
        .serializationContext = context,
    };
    return CSSCalc::serializationForCSS(calcValue().tree(), options);
}

void UnevaluatedCalcBase::serializationForCSS(StringBuilder& builder, const SerializationContext& context) const
{
    auto options = CSSCalc::SerializationOptions {
        .range = calcValue().range(),
        .serializationContext = context,
    };
    CSSCalc::serializationForCSS(builder, calcValue().tree(), options);
}

void UnevaluatedCalcBase::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    CSSCalc::collectComputedStyleDependencies(calcValue().tree(), dependencies);
}

UnevaluatedCalcBase UnevaluatedCalcBase::simplifyBase(const Style::BuilderState& state) const
{
    return CSS::simplify(protect(calcValue()), state.cssToLengthConversionData(), CSSCalcSymbolTable { });
}

UnevaluatedCalcBase UnevaluatedCalcBase::simplifyBase(const Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable) const
{
    return CSS::simplify(protect(calcValue()), state.cssToLengthConversionData(), symbolTable);
}

UnevaluatedCalcBase UnevaluatedCalcBase::simplifyBase(const CSSToLengthConversionData& conversionData) const
{
    return CSS::simplify(protect(calcValue()), conversionData, CSSCalcSymbolTable { });
}

UnevaluatedCalcBase UnevaluatedCalcBase::simplifyBase(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    return CSS::simplify(protect(calcValue()), conversionData, symbolTable);
}

UnevaluatedCalcBase UnevaluatedCalcBase::simplifyBase(NoConversionDataRequiredToken) const
{
    return CSS::simplify(protect(calcValue()), std::nullopt, CSSCalcSymbolTable { });
}

UnevaluatedCalcBase UnevaluatedCalcBase::simplifyBase(NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) const
{
    return CSS::simplify(protect(calcValue()), std::nullopt, symbolTable);
}

double UnevaluatedCalcBase::evaluate(const Style::BuilderState& state) const
{
    return CSS::evaluate(protect(calcValue()), state.cssToLengthConversionData(), CSSCalcSymbolTable { });
}

double UnevaluatedCalcBase::evaluate(const Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable) const
{
    return CSS::evaluate(protect(calcValue()), state.cssToLengthConversionData(), symbolTable);
}

double UnevaluatedCalcBase::evaluate(const CSSToLengthConversionData& conversionData) const
{
    return CSS::evaluate(protect(calcValue()), conversionData, CSSCalcSymbolTable { });
}

double UnevaluatedCalcBase::evaluate(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    return CSS::evaluate(protect(calcValue()), conversionData, symbolTable);
}

double UnevaluatedCalcBase::evaluate(NoConversionDataRequiredToken) const
{
    return CSS::evaluate(protect(calcValue()), std::nullopt, CSSCalcSymbolTable { });
}

double UnevaluatedCalcBase::evaluate(NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) const
{
    return CSS::evaluate(protect(calcValue()), std::nullopt, symbolTable);
}

double UnevaluatedCalcBase::evaluateDeprecated() const
{
    if (requiresConversionData())
        ALWAYS_LOG_WITH_STREAM(stream << "ERROR: The value returned from UnevaluatedCalcBase::evaluateDeprecated is likely incorrect as the calculation tree has unresolved units that require CSSToLengthConversionData to interpret. Update caller to use non-deprecated variant of this function.");

    return CSS::evaluate(protect(calcValue()), std::nullopt, CSSCalcSymbolTable { });
}

Style::UnevaluatedCalculationBase UnevaluatedCalcBase::createCalculationValue(const Style::BuilderState& state) const
{
    return CSS::createCalculationValue(protect(calcValue()), state.cssToLengthConversionData(), CSSCalcSymbolTable { });
}

Style::UnevaluatedCalculationBase UnevaluatedCalcBase::createCalculationValue(const Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable) const
{
    return CSS::createCalculationValue(protect(calcValue()), state.cssToLengthConversionData(), symbolTable);
}

Style::UnevaluatedCalculationBase UnevaluatedCalcBase::createCalculationValue(const CSSToLengthConversionData& conversionData) const
{
    return CSS::createCalculationValue(protect(calcValue()), conversionData, CSSCalcSymbolTable { });
}

Style::UnevaluatedCalculationBase UnevaluatedCalcBase::createCalculationValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    return CSS::createCalculationValue(protect(calcValue()), conversionData, symbolTable);
}

Style::UnevaluatedCalculationBase UnevaluatedCalcBase::createCalculationValue(NoConversionDataRequiredToken) const
{
    return CSS::createCalculationValue(protect(calcValue()), std::nullopt, CSSCalcSymbolTable { });
}

Style::UnevaluatedCalculationBase UnevaluatedCalcBase::createCalculationValue(NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) const
{
    return CSS::createCalculationValue(protect(calcValue()), std::nullopt, symbolTable);
}

TextStream& operator<<(TextStream& ts, const UnevaluatedCalcBase& value)
{
    ts << indent << '(' << "UnevaluatedCalcBase"_s;

    TextStream multilineStream;
    multilineStream.setIndent(ts.indent() + 2);

    multilineStream.dumpProperty("minimum value"_s, value.calcValue().range().min);
    multilineStream.dumpProperty("maximum value"_s, value.calcValue().range().max);
    multilineStream.dumpProperty("expression"_s, value.serializationForCSS(defaultSerializationContext()));

    ts << multilineStream.release();
    ts << ")\n"_s;

    return ts;
}

} // namespace CSS
} // namespace WebCore
