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
#include "CSSNoConversionDataRequiredToken.h"
#include "CSSParser.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserOptions.h"
#include "CSSSerializationContext.h"
#include "CalculationCategory.h"
#include "CalculationValue.h"
#include "Logging.h"
#include "StyleLengthResolution.h"
#include "StylePrimitiveNumericTypes.h"
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

RefPtr<CSSCalcValue> CSSCalcValue::parse(CSSParserTokenRange& tokens, const CSSParserContext& context, Calculation::Category category, CSS::Range range, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions propertyOptions)
{
    auto parserOptions = CSSCalc::ParserOptions {
        .category = category,
        .range = range,
        .allowedSymbols = WTFMove(symbolsAllowed),
        .propertyOptions = propertyOptions
    };
    auto simplificationOptions = CSSCalc::SimplificationOptions {
        .category = category,
        .range = range,
        .conversionData = std::nullopt,
        .symbolTable = { },
        .allowZeroValueLengthRemovalFromSum = false,
    };

    auto tree = CSSCalc::parseAndSimplify(tokens, context, parserOptions, simplificationOptions);
    if (!tree)
        return nullptr;

    RefPtr result = adoptRef(new CSSCalcValue(category, range, WTFMove(*tree)));
    LOG_WITH_STREAM(Calc, stream << "CSSCalcValue::create " << *result);
    return result;
}

Ref<CSSCalcValue> CSSCalcValue::create(const CalculationValue& value, const RenderStyle& style)
{
    auto tree = CSSCalc::fromCalculationValue(value, style);
    Ref result = adoptRef(*new CSSCalcValue(value.category(), { value.range().min, value.range().max }, WTFMove(tree)));
    LOG_WITH_STREAM(Calc, stream << "CSSCalcValue::create from CalculationValue: " << result);
    return result;
}

Ref<CSSCalcValue> CSSCalcValue::create(Calculation::Category category, CSS::Range range, CSSCalc::Tree&& tree)
{
    return adoptRef(*new CSSCalcValue(category, range, WTFMove(tree)));
}

Ref<CSSCalcValue> CSSCalcValue::copySimplified(const CSSToLengthConversionData& conversionData) const
{
    return copySimplified(conversionData, { });
}

Ref<CSSCalcValue> CSSCalcValue::copySimplified(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    auto simplificationOptions = CSSCalc::SimplificationOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = conversionData,
        .symbolTable = symbolTable,
        .allowZeroValueLengthRemovalFromSum = true,
    };

    if (!canSimplify(m_tree, simplificationOptions))
        return const_cast<CSSCalcValue&>(*this);

    return create(m_category, m_range, copyAndSimplify(m_tree, simplificationOptions));
}

Ref<CSSCalcValue> CSSCalcValue::copySimplified(NoConversionDataRequiredToken token) const
{
    return copySimplified(token, { });
}

Ref<CSSCalcValue> CSSCalcValue::copySimplified(NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) const
{
    auto simplificationOptions = CSSCalc::SimplificationOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = std::nullopt,
        .symbolTable = symbolTable,
        .allowZeroValueLengthRemovalFromSum = true,
    };

    if (!canSimplify(m_tree, simplificationOptions))
        return const_cast<CSSCalcValue&>(*this);

    return create(m_category, m_range, copyAndSimplify(m_tree, simplificationOptions));
}

CSSCalcValue::CSSCalcValue(Calculation::Category category, CSS::Range range, CSSCalc::Tree&& tree)
    : CSSValue(ClassType::Calculation)
    , m_category(category)
    , m_range(range)
    , m_tree(WTFMove(tree))
{
}

CSSCalcValue::~CSSCalcValue() = default;

CSSUnitType CSSCalcValue::primitiveType() const
{
    // This returns the CSSUnitType associated with the value returned by doubleValue, or, if CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH, that a call to createCalculationValue() is needed.

    switch (m_category) {
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
        if (WTF::holdsAlternative<CSSCalc::Percentage>(m_tree.root))
            return CSSUnitType::CSS_PERCENTAGE;
        return CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH;
    case Calculation::Category::AnglePercentage:
        if (!m_tree.type.percentHint)
            return CSSUnitType::CSS_DEG;
        if (WTF::holdsAlternative<CSSCalc::Percentage>(m_tree.root))
            return CSSUnitType::CSS_PERCENTAGE;
        return CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE;
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

String CSSCalcValue::customCSSText(const CSS::SerializationContext& context) const
{
    auto options = CSSCalc::SerializationOptions {
        .range = m_range,
        .serializationContext = context,
    };
    return CSSCalc::serializationForCSS(m_tree, options);
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
    if (m_category == Calculation::Category::Angle && std::isinf(value))
        return 0;

    if (m_category == Calculation::Category::Integer)
        value = std::floor(value + 0.5);

    return std::clamp(value, m_range.min, m_range.max);
}

double CSSCalcValue::doubleValue(const CSSToLengthConversionData& conversionData) const
{
    return doubleValue(conversionData, { });
}

double CSSCalcValue::doubleValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    auto options = CSSCalc::EvaluationOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = conversionData,
        .symbolTable = symbolTable
    };
    return clampToPermittedRange(CSSCalc::evaluateDouble(m_tree, options).value_or(0));
}

double CSSCalcValue::doubleValue(NoConversionDataRequiredToken token) const
{
    return doubleValue(token, { });
}

double CSSCalcValue::doubleValue(NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) const
{
    auto options = CSSCalc::EvaluationOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = std::nullopt,
        .symbolTable = symbolTable,
    };
    return clampToPermittedRange(CSSCalc::evaluateDouble(m_tree, options).value_or(0));
}

double CSSCalcValue::doubleValueDeprecated() const
{
    if (m_tree.requiresConversionData)
        ALWAYS_LOG_WITH_STREAM(stream << "ERROR: The value returned from CSSCalcValue::doubleValueDeprecated is likely incorrect as the calculation tree has unresolved units that require CSSToLengthConversionData to interpret. Update caller to use non-deprecated variant of this function.");

    return doubleValue(NoConversionDataRequiredToken { });
}

double CSSCalcValue::computeLengthPx(const CSSToLengthConversionData& conversionData) const
{
    return computeLengthPx(conversionData, { });
}

double CSSCalcValue::computeLengthPx(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    auto options = CSSCalc::EvaluationOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = conversionData,
        .symbolTable = symbolTable
    };
    return clampToPermittedRange(Style::computeNonCalcLengthDouble(CSSCalc::evaluateDouble(m_tree, options).value_or(0), CSS::LengthUnit::Px, conversionData));
}

Ref<CalculationValue> CSSCalcValue::createCalculationValue(const CSSToLengthConversionData& conversionData) const
{
    return createCalculationValue(conversionData, { });
}

Ref<CalculationValue> CSSCalcValue::createCalculationValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    auto options = CSSCalc::EvaluationOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = conversionData,
        .symbolTable = symbolTable
    };
    return CSSCalc::toCalculationValue(m_tree, options);
}

Ref<CalculationValue> CSSCalcValue::createCalculationValue(NoConversionDataRequiredToken token) const
{
    return createCalculationValue(token, { });
}

Ref<CalculationValue> CSSCalcValue::createCalculationValue(NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(!m_tree.requiresConversionData);

    auto options = CSSCalc::EvaluationOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = std::nullopt,
        .symbolTable = symbolTable
    };
    return CSSCalc::toCalculationValue(m_tree, options);
}

void CSSCalcValue::dump(TextStream& ts) const
{
    ts << indent << "(" << "CSSCalcValue";

    TextStream multilineStream;
    multilineStream.setIndent(ts.indent() + 2);

    multilineStream.dumpProperty("minimum value", m_range.min);
    multilineStream.dumpProperty("maximum value", m_range.max);
    multilineStream.dumpProperty("expression", customCSSText(CSS::defaultSerializationContext()));

    ts << multilineStream.release();
    ts << ")\n";
}

TextStream& operator<<(TextStream& ts, const CSSCalcValue& value)
{
    value.dump(ts);
    return ts;
}

} // namespace WebCore
