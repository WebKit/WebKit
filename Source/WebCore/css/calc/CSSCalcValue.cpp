/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSCalcValue.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcTree+CalculationValue.h"
#include "CSSCalcTree+ComputedStyleDependencies.h"
#include "CSSCalcTree+Evaluation.h"
#include "CSSCalcTree+Parser.h"
#include "CSSCalcTree+Serialization.h"
#include "CSSCalcTree+Simplification.h"
#include "CSSParser.h"
#include "CSSParserTokenRange.h"
#include "CalculationCategory.h"
#include "CalculationValue.h"
#include "Logging.h"
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

bool CSSCalcValue::isCalcFunction(CSSValueID functionId)
{
    return CSSCalc::isCalcFunction(functionId);
}

RefPtr<CSSCalcValue> CSSCalcValue::parse(CSSValueID function, const CSSParserTokenRange& tokens, Calculation::Category category, ValueRange range, CSSCalcSymbolsAllowed symbolsAllowed)
{
    auto parserOptions = CSSCalc::ParserOptions {
        .category = category,
        .allowedSymbols = WTFMove(symbolsAllowed),
        .range = range
    };

    auto simplificationOptions = CSSCalc::SimplificationOptions {
        .category = category,
        .conversionData = std::nullopt,
        .symbolTable = { },
        .allowZeroValueLengthRemovalFromSum = false,
    };

    auto tree = CSSCalc::parseAndSimplify(tokens, function, parserOptions, simplificationOptions);
    if (!tree)
        return nullptr;

    RefPtr result = adoptRef(new CSSCalcValue(WTFMove(*tree)));
    LOG_WITH_STREAM(Calc, stream << "CSSCalcValue::create " << *result);
    return result;
}

Ref<CSSCalcValue> CSSCalcValue::create(const CalculationValue& value, const RenderStyle& style)
{
    auto tree = CSSCalc::fromCalculationValue(value, style);
    Ref result = adoptRef(*new CSSCalcValue(WTFMove(tree)));
    LOG_WITH_STREAM(Calc, stream << "CSSCalcValue::create from CalculationValue: " << result);
    return result;
}

Ref<CSSCalcValue> CSSCalcValue::create(CSSCalc::Tree&& tree)
{
    return adoptRef(*new CSSCalcValue(WTFMove(tree)));
}

Ref<CSSCalcValue> CSSCalcValue::copySimplified(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    auto simplificationOptions = CSSCalc::SimplificationOptions {
        .category = m_tree.category,
        .conversionData = conversionData,
        .symbolTable = symbolTable,
        .allowZeroValueLengthRemovalFromSum = true,
    };

    return create(copyAndSimplify(m_tree, simplificationOptions));
}

CSSCalcValue::CSSCalcValue(CSSCalc::Tree&& tree)
    : CSSValue(ClassType::Calculation)
    , m_tree(WTFMove(tree))
{
}

CSSCalcValue::~CSSCalcValue() = default;

Calculation::Category CSSCalcValue::category() const
{
    return m_tree.category;
}

CSSUnitType CSSCalcValue::primitiveType() const
{
    // This returns the CSSUnitType associated with the value returned by doubleValue, or, if CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH, that a call to createCalculationValue() is needed.

    switch (m_tree.category) {
    case Calculation::Category::Integer:
        return CSSUnitType::CSS_INTEGER;
    case Calculation::Category::Number:
        return CSSUnitType::CSS_NUMBER;
    case Calculation::Category::Percentage:
        return CSSUnitType::CSS_PERCENTAGE;
    case Calculation::Category::Length:
        return CSSUnitType::CSS_PX;
    case Calculation::Category::Angle:
        return CSSUnitType::CSS_DEG;
    case Calculation::Category::Time:
        return CSSUnitType::CSS_S;
    case Calculation::Category::Frequency:
        return CSSUnitType::CSS_HZ;
    case Calculation::Category::Resolution:
        return CSSUnitType::CSS_DPPX;
    case Calculation::Category::Flex:
        return CSSUnitType::CSS_FR;
    case Calculation::Category::LengthPercentage:
        if (!m_tree.type.percentHint)
            return CSSUnitType::CSS_PX;
        if (std::holds_alternative<CSSCalc::Percentage>(m_tree.root))
            return CSSUnitType::CSS_PERCENTAGE;
        return CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH;
    }

    ASSERT_NOT_REACHED();
    return CSSUnitType::CSS_NUMBER;
}

bool CSSCalcValue::requiresConversionData() const
{
    return m_tree.requiresConversionData;
}

void CSSCalcValue::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    CSSCalc::collectComputedStyleDependencies(m_tree, dependencies);
}

String CSSCalcValue::customCSSText() const
{
    return CSSCalc::serializationForCSS(m_tree);
}

bool CSSCalcValue::equals(const CSSCalcValue& other) const
{
    return m_tree.root == other.m_tree.root;
}

inline double CSSCalcValue::clampToPermittedRange(double value) const
{
    // If a top-level calculation would produce a value whose numeric part is NaN,
    // it instead act as though the numeric part is 0.
    value = std::isnan(value) ? 0 : value;

    // If an <angle> must be converted due to exceeding the implementation-defined range of supported values,
    // it must be clamped to the nearest supported multiple of 360deg.
    if (m_tree.category == Calculation::Category::Angle && std::isinf(value))
        return 0;

    if (m_tree.category == Calculation::Category::Integer)
        value = std::floor(value + 0.5);

    return m_tree.range == ValueRange::NonNegative && value < 0 ? 0 : value;
}

double CSSCalcValue::doubleValueNoConversionDataRequired(const CSSCalcSymbolTable& symbolTable) const
{
    auto options = CSSCalc::EvaluationOptions {
        .conversionData = std::nullopt,
        .symbolTable = symbolTable,
    };
    return clampToPermittedRange(CSSCalc::evaluateDouble(m_tree, options).value_or(0));
}

double CSSCalcValue::doubleValueDeprecated(const CSSCalcSymbolTable& symbolTable) const
{
    if (m_tree.requiresConversionData) {
        ALWAYS_LOG_WITH_STREAM(stream << "ERROR: The value returned from CSSCalcValue::doubleValueDeprecated is likely incorrect as the calculation tree has unresolved units that require CSSToLengthConversionData to interpret. Update caller to use non-deprecated variant of this function.");
        return 0;
    }

    auto options = CSSCalc::EvaluationOptions {
        .conversionData = std::nullopt,
        .symbolTable = symbolTable,
    };
    return clampToPermittedRange(CSSCalc::evaluateDouble(m_tree, options).value_or(0));
}

double CSSCalcValue::doubleValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    auto options = CSSCalc::EvaluationOptions {
        .conversionData = conversionData,
        .symbolTable = symbolTable
    };
    return clampToPermittedRange(CSSCalc::evaluateDouble(m_tree, options).value_or(0));
}

double CSSCalcValue::computeLengthPx(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    auto options = CSSCalc::EvaluationOptions {
        .conversionData = conversionData,
        .symbolTable = symbolTable
    };
    return clampToPermittedRange(CSSPrimitiveValue::computeNonCalcLengthDouble(conversionData, CSSUnitType::CSS_PX, CSSCalc::evaluateDouble(m_tree, options).value_or(0)));
}

Ref<CalculationValue> CSSCalcValue::createCalculationValueNoConversionDataRequired(const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(!m_tree.requiresConversionData);

    auto options = CSSCalc::EvaluationOptions {
        .conversionData = std::nullopt,
        .symbolTable = symbolTable
    };
    return CSSCalc::toCalculationValue(m_tree, options);
}

Ref<CalculationValue> CSSCalcValue::createCalculationValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    auto options = CSSCalc::EvaluationOptions {
        .conversionData = conversionData,
        .symbolTable = symbolTable
    };
    return CSSCalc::toCalculationValue(m_tree, options);
}

NumberRaw CSSCalcValue::numberValueNoConversionDataRequired(const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::Number);
    return { doubleValueNoConversionDataRequired(symbolTable) };
}

NumberRaw CSSCalcValue::numberValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::Number);
    return { doubleValue(conversionData, symbolTable) };
}

PercentageRaw CSSCalcValue::percentageValueNoConversionDataRequired(const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::Percentage);
    return { doubleValueNoConversionDataRequired(symbolTable) };
}

PercentageRaw CSSCalcValue::percentageValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::Percentage);
    return { doubleValue(conversionData, symbolTable) };
}

AngleRaw CSSCalcValue::angleValueNoConversionDataRequired(const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::Angle);
    return { CSSUnitType::CSS_DEG, doubleValueNoConversionDataRequired(symbolTable) };
}

AngleRaw CSSCalcValue::angleValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::Angle);
    return { CSSUnitType::CSS_DEG, doubleValue(conversionData, symbolTable) };
}

LengthRaw CSSCalcValue::lengthValueNoConversionDataRequired(const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::Length);
    return { CSSUnitType::CSS_PX, doubleValueNoConversionDataRequired(symbolTable) };
}

LengthRaw CSSCalcValue::lengthValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::Length);
    return { CSSUnitType::CSS_PX, doubleValue(conversionData, symbolTable) };
}

ResolutionRaw CSSCalcValue::resolutionValueNoConversionDataRequired(const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::Resolution);
    return { CSSUnitType::CSS_DPPX, doubleValueNoConversionDataRequired(symbolTable) };
}

ResolutionRaw CSSCalcValue::resolutionValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::Resolution);
    return { CSSUnitType::CSS_DPPX, doubleValue(conversionData, symbolTable) };
}

TimeRaw CSSCalcValue::timeValueNoConversionDataRequired(const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::Time);
    return { CSSUnitType::CSS_S, doubleValueNoConversionDataRequired(symbolTable) };
}

TimeRaw CSSCalcValue::timeValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::Time);
    return { CSSUnitType::CSS_S, doubleValue(conversionData, symbolTable) };
}

Length CSSCalcValue::lengthPercentageValueNoConversionDataRequired(const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::LengthPercentage);

    if (!m_tree.type.percentHint)
        return Length { doubleValueNoConversionDataRequired(symbolTable), LengthType::Fixed };
    if (std::holds_alternative<CSSCalc::Percentage>(m_tree.root))
        return Length { doubleValueNoConversionDataRequired(symbolTable), LengthType::Percent };
    return Length { createCalculationValueNoConversionDataRequired(symbolTable) };
}

Length CSSCalcValue::lengthPercentageValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(m_tree.category == Calculation::Category::LengthPercentage);

    if (!m_tree.type.percentHint)
        return Length { doubleValue(conversionData, symbolTable), LengthType::Fixed };
    if (std::holds_alternative<CSSCalc::Percentage>(m_tree.root))
        return Length { doubleValue(conversionData, symbolTable), LengthType::Percent };
    return Length { createCalculationValue(conversionData, symbolTable) };
}

void CSSCalcValue::dump(TextStream& ts) const
{
    ts << indent << "(" << "CSSCalcValue";

    TextStream multilineStream;
    multilineStream.setIndent(ts.indent() + 2);

    multilineStream.dumpProperty("should clamp non-negative", m_tree.range == ValueRange::NonNegative);
    multilineStream.dumpProperty("expression", CSSCalc::serializationForCSS(m_tree));

    ts << multilineStream.release();
    ts << ")\n";
}

TextStream& operator<<(TextStream& ts, const CSSCalcValue& value)
{
    value.dump(ts);
    return ts;
}

} // namespace WebCore
