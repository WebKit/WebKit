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
#include "CSSCalcTree+ComputedStyleDependencies.h"
#include "CSSCalcTree+Evaluation.h"
#include "CSSCalcTree+Parser.h"
#include "CSSCalcTree+Serialization.h"
#include "CSSCalcTree+Simplification.h"
#include "CSSNoConversionDataRequiredToken.h"
#include "CSSParser.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericCategory.h"
#include "CSSPropertyParserOptions.h"
#include "CSSSerializationContext.h"
#include "Logging.h"
#include "StyleCalculationTree+Conversion.h"
#include "StyleCalculationValue.h"
#include "StyleLengthResolution.h"
#include "StylePrimitiveNumericTypes.h"
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace CSSCalc {

RefPtr<Value> Value::parse(CSSParserTokenRange& tokens, CSS::PropertyParserState& state, CSS::Category category, CSS::Range range, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions propertyOptions)
{
    auto parserOptions = ParserOptions {
        .category = category,
        .range = range,
        .allowedSymbols = WTF::move(symbolsAllowed),
        .propertyOptions = propertyOptions
    };
    auto simplificationOptions = SimplificationOptions {
        .category = category,
        .range = range,
        .conversionData = std::nullopt,
        .symbolTable = { },
        .allowZeroValueLengthRemovalFromSum = false,
    };

    auto tree = parseAndSimplify(tokens, state, parserOptions, simplificationOptions);
    if (!tree)
        return nullptr;

    RefPtr result = adoptRef(new Value(category, range, WTF::move(*tree)));
    LOG_WITH_STREAM(Calc, stream << "Value::create " << *result);
    return result;
}

Ref<Value> Value::create(const Style::Calculation::Value& value, const RenderStyle& style)
{
    auto category = value.category();
    auto range = value.range();

    auto toCSSOptions = Style::Calculation::ToCSSOptions {
        .category = category,
        .range = range,
        .style = style,
    };
    return Value::create(category, range, Style::Calculation::toCSS(value.tree(), toCSSOptions));
}

Ref<Value> Value::create(CSS::Category category, CSS::Range range, CSSCalc::Tree&& tree)
{
    return adoptRef(*new Value(category, range, WTF::move(tree)));
}

Ref<Value> Value::copySimplified(const CSSToLengthConversionData& conversionData) const
{
    return copySimplified(conversionData, { });
}

Ref<Value> Value::copySimplified(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    auto simplificationOptions = SimplificationOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = conversionData,
        .symbolTable = symbolTable,
        .allowZeroValueLengthRemovalFromSum = true,
    };

    if (!canSimplify(m_tree, simplificationOptions))
        return const_cast<Value&>(*this);

    return create(m_category, m_range, copyAndSimplify(m_tree, simplificationOptions));
}

Ref<Value> Value::copySimplified(NoConversionDataRequiredToken token) const
{
    return copySimplified(token, { });
}

Ref<Value> Value::copySimplified(NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) const
{
    auto simplificationOptions = SimplificationOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = std::nullopt,
        .symbolTable = symbolTable,
        .allowZeroValueLengthRemovalFromSum = true,
    };

    if (!canSimplify(m_tree, simplificationOptions))
        return const_cast<Value&>(*this);

    return create(m_category, m_range, copyAndSimplify(m_tree, simplificationOptions));
}

Value::Value(CSS::Category category, CSS::Range range, CSSCalc::Tree&& tree)
    : m_category(category)
    , m_range(range)
    , m_tree(WTF::move(tree))
{
}

Value::~Value() = default;

CSSUnitType Value::primitiveType() const
{
    // This returns the CSSUnitType associated with the value returned by doubleValue, or, if CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH, that a call to createCalculationValue() is needed.

    switch (m_category) {
    case CSS::Category::Integer:
        return CSSUnitType::CSS_INTEGER;
    case CSS::Category::Number:
        return CSSUnitType::CSS_NUMBER;
    case CSS::Category::Percentage:
        return CSSUnitType::CSS_PERCENTAGE;
    case CSS::Category::Length:
        return CSSUnitType::CSS_PX;
    case CSS::Category::Angle:
        return CSSUnitType::CSS_DEG;
    case CSS::Category::Time:
        return CSSUnitType::CSS_S;
    case CSS::Category::Frequency:
        return CSSUnitType::CSS_HZ;
    case CSS::Category::Resolution:
        return CSSUnitType::CSS_DPPX;
    case CSS::Category::Flex:
        return CSSUnitType::CSS_FR;
    case CSS::Category::LengthPercentage:
        if (!m_tree.type.percentHint)
            return CSSUnitType::CSS_PX;
        if (WTF::holdsAlternative<Percentage>(m_tree.root))
            return CSSUnitType::CSS_PERCENTAGE;
        return CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH;
    case CSS::Category::AnglePercentage:
        if (!m_tree.type.percentHint)
            return CSSUnitType::CSS_DEG;
        if (WTF::holdsAlternative<Percentage>(m_tree.root))
            return CSSUnitType::CSS_PERCENTAGE;
        return CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE;
    }

    ASSERT_NOT_REACHED();
    return CSSUnitType::CSS_NUMBER;
}

void Value::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    WebCore::CSSCalc::collectComputedStyleDependencies(m_tree, dependencies);
}

String Value::cssText(const CSS::SerializationContext& context) const
{
    auto options = SerializationOptions {
        .range = m_range,
        .serializationContext = context,
    };
    return serializationForCSS(m_tree, options);
}

bool Value::equals(const Value& other) const
{
    return m_tree.root == other.m_tree.root;
}

inline double Value::clampToPermittedRange(double value) const
{
    // If a top-level calculation would produce a value whose numeric part is NaN,
    // it instead act as though the numeric part is 0.
    value = std::isnan(value) ? 0 : value;

    // If an <angle> must be converted due to exceeding the implementation-defined range of supported values,
    // it must be clamped to the nearest supported multiple of 360deg.
    if (m_category == CSS::Category::Angle && std::isinf(value))
        return 0;

    if (m_category == CSS::Category::Integer)
        value = std::floor(value + 0.5);

    return std::clamp(value, m_range.min, m_range.max);
}

double Value::doubleValue(const CSSToLengthConversionData& conversionData) const
{
    return doubleValue(conversionData, { });
}

double Value::doubleValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    auto options = EvaluationOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = conversionData,
        .symbolTable = symbolTable
    };
    return clampToPermittedRange(evaluateDouble(m_tree, options).value_or(0));
}

double Value::doubleValue(NoConversionDataRequiredToken token) const
{
    return doubleValue(token, { });
}

double Value::doubleValue(NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) const
{
    auto options = EvaluationOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = std::nullopt,
        .symbolTable = symbolTable,
    };
    return clampToPermittedRange(evaluateDouble(m_tree, options).value_or(0));
}

double Value::doubleValueDeprecated() const
{
    if (m_tree.requiresConversionData)
        ALWAYS_LOG_WITH_STREAM(stream << "ERROR: The value returned from Value::doubleValueDeprecated is likely incorrect as the calculation tree has unresolved units that require CSSToLengthConversionData to interpret. Update caller to use non-deprecated variant of this function.");

    return doubleValue(NoConversionDataRequiredToken { });
}

double Value::computeLengthPx(const CSSToLengthConversionData& conversionData) const
{
    return computeLengthPx(conversionData, { });
}

double Value::computeLengthPx(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    auto options = EvaluationOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = conversionData,
        .symbolTable = symbolTable
    };
    return clampToPermittedRange(Style::computeNonCalcLengthDouble(evaluateDouble(m_tree, options).value_or(0), CSS::LengthUnit::Px, conversionData));
}

Ref<Style::Calculation::Value> Value::createCalculationValue(const CSSToLengthConversionData& conversionData) const
{
    return createCalculationValue(conversionData, { });
}

Ref<Style::Calculation::Value> Value::createCalculationValue(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    auto toStyleOptions = Style::Calculation::ToStyleOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = conversionData,
        .symbolTable = symbolTable
    };

    return Style::Calculation::Value::create(m_category, m_range, Style::Calculation::toStyle(m_tree, toStyleOptions));
}

Ref<Style::Calculation::Value> Value::createCalculationValue(NoConversionDataRequiredToken token) const
{
    return createCalculationValue(token, { });
}

Ref<Style::Calculation::Value> Value::createCalculationValue(NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT(!m_tree.requiresConversionData);

    auto toStyleOptions = Style::Calculation::ToStyleOptions {
        .category = m_category,
        .range = m_range,
        .conversionData = std::nullopt,
        .symbolTable = symbolTable
    };
    return Style::Calculation::Value::create(m_category, m_range, Style::Calculation::toStyle(m_tree, toStyleOptions));
}

void Value::dump(TextStream& ts) const
{
    ts << indent << '(' << "Value"_s;

    TextStream multilineStream;
    multilineStream.setIndent(ts.indent() + 2);

    multilineStream.dumpProperty("minimum value"_s, m_range.min);
    multilineStream.dumpProperty("maximum value"_s, m_range.max);
    multilineStream.dumpProperty("expression"_s, cssText(CSS::defaultSerializationContext()));

    ts << multilineStream.release();
    ts << ")\n"_s;
}

TextStream& operator<<(TextStream& ts, const Value& value)
{
    value.dump(ts);
    return ts;
}

} // namespace CSSCalc
} // namespace WebCore
