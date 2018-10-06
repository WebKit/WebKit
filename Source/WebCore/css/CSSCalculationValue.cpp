/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "CSSCalculationValue.h"

#include "CSSParser.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValueMappings.h"
#include "StyleResolver.h"
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>

static const int maxExpressionDepth = 100;

enum ParseState {
    OK,
    TooDeep,
    NoMoreTokens
};

namespace WebCore {

static RefPtr<CSSCalcExpressionNode> createCSS(const CalcExpressionNode&, const RenderStyle&);
static RefPtr<CSSCalcExpressionNode> createCSS(const Length&, const RenderStyle&);

static CalculationCategory unitCategory(CSSPrimitiveValue::UnitType type)
{
    switch (type) {
    case CSSPrimitiveValue::CSS_NUMBER:
        return CalculationCategory::Number;
    case CSSPrimitiveValue::CSS_EMS:
    case CSSPrimitiveValue::CSS_EXS:
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PC:
    case CSSPrimitiveValue::CSS_REMS:
    case CSSPrimitiveValue::CSS_CHS:
    case CSSPrimitiveValue::CSS_VW:
    case CSSPrimitiveValue::CSS_VH:
    case CSSPrimitiveValue::CSS_VMIN:
    case CSSPrimitiveValue::CSS_VMAX:
        return CalculationCategory::Length;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        return CalculationCategory::Percent;
    case CSSPrimitiveValue::CSS_DEG:
    case CSSPrimitiveValue::CSS_RAD:
    case CSSPrimitiveValue::CSS_GRAD:
    case CSSPrimitiveValue::CSS_TURN:
        return CalculationCategory::Angle;
    case CSSPrimitiveValue::CSS_MS:
    case CSSPrimitiveValue::CSS_S:
        return CalculationCategory::Time;
    case CSSPrimitiveValue::CSS_HZ:
    case CSSPrimitiveValue::CSS_KHZ:
        return CalculationCategory::Frequency;
    default:
        return CalculationCategory::Other;
    }
}

static bool hasDoubleValue(CSSPrimitiveValue::UnitType type)
{
    switch (type) {
    case CSSPrimitiveValue::CSS_FR:
    case CSSPrimitiveValue::CSS_NUMBER:
    case CSSPrimitiveValue::CSS_PERCENTAGE:
    case CSSPrimitiveValue::CSS_EMS:
    case CSSPrimitiveValue::CSS_EXS:
    case CSSPrimitiveValue::CSS_CHS:
    case CSSPrimitiveValue::CSS_REMS:
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PC:
    case CSSPrimitiveValue::CSS_DEG:
    case CSSPrimitiveValue::CSS_RAD:
    case CSSPrimitiveValue::CSS_GRAD:
    case CSSPrimitiveValue::CSS_TURN:
    case CSSPrimitiveValue::CSS_MS:
    case CSSPrimitiveValue::CSS_S:
    case CSSPrimitiveValue::CSS_HZ:
    case CSSPrimitiveValue::CSS_KHZ:
    case CSSPrimitiveValue::CSS_DIMENSION:
    case CSSPrimitiveValue::CSS_VW:
    case CSSPrimitiveValue::CSS_VH:
    case CSSPrimitiveValue::CSS_VMIN:
    case CSSPrimitiveValue::CSS_VMAX:
    case CSSPrimitiveValue::CSS_DPPX:
    case CSSPrimitiveValue::CSS_DPI:
    case CSSPrimitiveValue::CSS_DPCM:
        return true;
    case CSSPrimitiveValue::CSS_UNKNOWN:
    case CSSPrimitiveValue::CSS_STRING:
    case CSSPrimitiveValue::CSS_FONT_FAMILY:
    case CSSPrimitiveValue::CSS_URI:
    case CSSPrimitiveValue::CSS_IDENT:
    case CSSPrimitiveValue::CSS_ATTR:
    case CSSPrimitiveValue::CSS_COUNTER:
    case CSSPrimitiveValue::CSS_RECT:
    case CSSPrimitiveValue::CSS_RGBCOLOR:
    case CSSPrimitiveValue::CSS_PAIR:
    case CSSPrimitiveValue::CSS_UNICODE_RANGE:
    case CSSPrimitiveValue::CSS_COUNTER_NAME:
    case CSSPrimitiveValue::CSS_SHAPE:
    case CSSPrimitiveValue::CSS_QUAD:
    case CSSPrimitiveValue::CSS_QUIRKY_EMS:
    case CSSPrimitiveValue::CSS_CALC:
    case CSSPrimitiveValue::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSPrimitiveValue::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSPrimitiveValue::CSS_PROPERTY_ID:
    case CSSPrimitiveValue::CSS_VALUE_ID:
#if ENABLE(DASHBOARD_SUPPORT)
    case CSSPrimitiveValue::CSS_DASHBOARD_REGION:
#endif
        return false;
    };
    ASSERT_NOT_REACHED();
    return false;
}

static String buildCssText(const String& expression)
{
    StringBuilder result;
    result.appendLiteral("calc");
    bool expressionHasSingleTerm = expression[0] != '(';
    if (expressionHasSingleTerm)
        result.append('(');
    result.append(expression);
    if (expressionHasSingleTerm)
        result.append(')');
    return result.toString();
}

String CSSCalcValue::customCSSText() const
{
    return buildCssText(m_expression->customCSSText());
}

bool CSSCalcValue::equals(const CSSCalcValue& other) const
{
    return compareCSSValue(m_expression, other.m_expression);
}

inline double CSSCalcValue::clampToPermittedRange(double value) const
{
    return m_shouldClampToNonNegative && value < 0 ? 0 : value;
}

double CSSCalcValue::doubleValue() const
{
    return clampToPermittedRange(m_expression->doubleValue());
}

double CSSCalcValue::computeLengthPx(const CSSToLengthConversionData& conversionData) const
{
    return clampToPermittedRange(m_expression->computeLengthPx(conversionData));
}

class CSSCalcPrimitiveValue final : public CSSCalcExpressionNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<CSSCalcPrimitiveValue> create(Ref<CSSPrimitiveValue>&& value, bool isInteger)
    {
        return adoptRef(*new CSSCalcPrimitiveValue(WTFMove(value), isInteger));
    }

    static RefPtr<CSSCalcPrimitiveValue> create(double value, CSSPrimitiveValue::UnitType type, bool isInteger)
    {
        if (!std::isfinite(value))
            return nullptr;
        return adoptRef(new CSSCalcPrimitiveValue(CSSPrimitiveValue::create(value, type), isInteger));
    }

private:
    bool isZero() const final
    {
        return !m_value->doubleValue();
    }

    String customCSSText() const final
    {
        return m_value->cssText();
    }

    std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData& conversionData) const final
    {
        switch (category()) {
        case CalculationCategory::Number:
            return std::make_unique<CalcExpressionNumber>(m_value->floatValue());
        case CalculationCategory::Length:
            return std::make_unique<CalcExpressionLength>(Length(m_value->computeLength<float>(conversionData), WebCore::Fixed));
        case CalculationCategory::Percent:
        case CalculationCategory::PercentLength: {
            return std::make_unique<CalcExpressionLength>(m_value->convertToLength<FixedFloatConversion | PercentConversion>(conversionData));
        }
        // Only types that could be part of a Length expression can be converted
        // to a CalcExpressionNode. CalculationCategory::PercentNumber makes no sense as a Length.
        case CalculationCategory::PercentNumber:
        case CalculationCategory::Angle:
        case CalculationCategory::Time:
        case CalculationCategory::Frequency:
        case CalculationCategory::Other:
            ASSERT_NOT_REACHED();
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    double doubleValue() const final
    {
        if (hasDoubleValue(primitiveType()))
            return m_value->doubleValue();
        ASSERT_NOT_REACHED();
        return 0;
    }

    double computeLengthPx(const CSSToLengthConversionData& conversionData) const final
    {
        switch (category()) {
        case CalculationCategory::Length:
            return m_value->computeLength<double>(conversionData);
        case CalculationCategory::Percent:
        case CalculationCategory::Number:
            return m_value->doubleValue();
        case CalculationCategory::PercentLength:
        case CalculationCategory::PercentNumber:
        case CalculationCategory::Angle:
        case CalculationCategory::Time:
        case CalculationCategory::Frequency:
        case CalculationCategory::Other:
            ASSERT_NOT_REACHED();
            break;
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    void collectDirectComputationalDependencies(HashSet<CSSPropertyID>& values) const final
    {
        m_value->collectDirectComputationalDependencies(values);
    }

    void collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>& values) const final
    {
        m_value->collectDirectRootComputationalDependencies(values);
    }

    bool equals(const CSSCalcExpressionNode& other) const final
    {
        if (type() != other.type())
            return false;

        return compareCSSValue(m_value, static_cast<const CSSCalcPrimitiveValue&>(other).m_value);
    }

    Type type() const final { return CssCalcPrimitiveValue; }
    CSSPrimitiveValue::UnitType primitiveType() const final
    {
        return CSSPrimitiveValue::UnitType(m_value->primitiveType());
    }

private:
    explicit CSSCalcPrimitiveValue(Ref<CSSPrimitiveValue>&& value, bool isInteger)
        : CSSCalcExpressionNode(unitCategory((CSSPrimitiveValue::UnitType)value->primitiveType()), isInteger)
        , m_value(WTFMove(value))
    {
    }

    Ref<CSSPrimitiveValue> m_value;
};

static const CalculationCategory addSubtractResult[static_cast<unsigned>(CalculationCategory::Angle)][static_cast<unsigned>(CalculationCategory::Angle)] = {
//    CalculationCategory::Number         CalculationCategory::Length         CalculationCategory::Percent        CalculationCategory::PercentNumber  CalculationCategory::PercentLength
    { CalculationCategory::Number,        CalculationCategory::Other,         CalculationCategory::PercentNumber, CalculationCategory::PercentNumber, CalculationCategory::Other }, //         CalculationCategory::Number
    { CalculationCategory::Other,         CalculationCategory::Length,        CalculationCategory::PercentLength, CalculationCategory::Other,         CalculationCategory::PercentLength }, // CalculationCategory::Length
    { CalculationCategory::PercentNumber, CalculationCategory::PercentLength, CalculationCategory::Percent,       CalculationCategory::PercentNumber, CalculationCategory::PercentLength }, // CalculationCategory::Percent
    { CalculationCategory::PercentNumber, CalculationCategory::Other,         CalculationCategory::PercentNumber, CalculationCategory::PercentNumber, CalculationCategory::Other }, //         CalculationCategory::PercentNumber
    { CalculationCategory::Other,         CalculationCategory::PercentLength, CalculationCategory::PercentLength, CalculationCategory::Other,         CalculationCategory::PercentLength }, // CalculationCategory::PercentLength
};

static CalculationCategory determineCategory(const CSSCalcExpressionNode& leftSide, const CSSCalcExpressionNode& rightSide, CalcOperator op)
{
    CalculationCategory leftCategory = leftSide.category();
    CalculationCategory rightCategory = rightSide.category();
    ASSERT(leftCategory < CalculationCategory::Other);
    ASSERT(rightCategory < CalculationCategory::Other);

    switch (op) {
    case CalcOperator::Add:
    case CalcOperator::Subtract:
        if (leftCategory < CalculationCategory::Angle && rightCategory < CalculationCategory::Angle)
            return addSubtractResult[static_cast<unsigned>(leftCategory)][static_cast<unsigned>(rightCategory)];
        if (leftCategory == rightCategory)
            return leftCategory;
        return CalculationCategory::Other;
    case CalcOperator::Multiply:
        if (leftCategory != CalculationCategory::Number && rightCategory != CalculationCategory::Number)
            return CalculationCategory::Other;
        return leftCategory == CalculationCategory::Number ? rightCategory : leftCategory;
    case CalcOperator::Divide:
        if (rightCategory != CalculationCategory::Number || rightSide.isZero())
            return CalculationCategory::Other;
        return leftCategory;
    case CalcOperator::Min:
    case CalcOperator::Max:
        ASSERT_NOT_REACHED();
        return CalculationCategory::Other;
    }

    ASSERT_NOT_REACHED();
    return CalculationCategory::Other;
}

static CalculationCategory resolvedTypeForMinOrMax(CalculationCategory category, CalculationCategory destinationCategory)
{
    switch (category) {
    case CalculationCategory::Number:
    case CalculationCategory::Length:
    case CalculationCategory::PercentNumber:
    case CalculationCategory::PercentLength:
    case CalculationCategory::Angle:
    case CalculationCategory::Time:
    case CalculationCategory::Frequency:
    case CalculationCategory::Other:
        return category;

    case CalculationCategory::Percent:
        if (destinationCategory == CalculationCategory::Length)
            return CalculationCategory::PercentLength;
        if (destinationCategory == CalculationCategory::Number)
            return CalculationCategory::PercentNumber;
        return category;
    }

    return CalculationCategory::Other;
}

static inline bool isIntegerResult(CalcOperator op, const CSSCalcExpressionNode& leftSide, const CSSCalcExpressionNode& rightSide)
{
    // Performs W3C spec's type checking for calc integers.
    // http://www.w3.org/TR/css3-values/#calc-type-checking
    return op != CalcOperator::Divide && leftSide.isInteger() && rightSide.isInteger();
}

static inline bool isIntegerResult(CalcOperator op, const Vector<Ref<CSSCalcExpressionNode>>& nodes)
{
    // Performs W3C spec's type checking for calc integers.
    // http://www.w3.org/TR/css3-values/#calc-type-checking
    if (op == CalcOperator::Divide)
        return false;

    for (auto& node : nodes) {
        if (!node->isInteger())
            return false;
    }

    return true;
}

static bool isSamePair(CalculationCategory a, CalculationCategory b, CalculationCategory x, CalculationCategory y)
{
    return (a == x && b == y) || (a == y && b == x);
}

class CSSCalcOperation final : public CSSCalcExpressionNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static RefPtr<CSSCalcOperation> create(CalcOperator op, RefPtr<CSSCalcExpressionNode>&& leftSide, RefPtr<CSSCalcExpressionNode>&& rightSide)
    {
        if (!leftSide || !rightSide)
            return nullptr;

        ASSERT(leftSide->category() < CalculationCategory::Other);
        ASSERT(rightSide->category() < CalculationCategory::Other);

        auto newCategory = determineCategory(*leftSide, *rightSide, op);
        if (newCategory == CalculationCategory::Other)
            return nullptr;

        return adoptRef(new CSSCalcOperation(newCategory, op, leftSide.releaseNonNull(), rightSide.releaseNonNull()));
    }

    static RefPtr<CSSCalcOperation> createMinOrMax(CalcOperator op, Vector<Ref<CSSCalcExpressionNode>>&& values, CalculationCategory destinationCategory)
    {
        ASSERT(op == CalcOperator::Min || op == CalcOperator::Max);

        std::optional<CalculationCategory> category = std::nullopt;
        for (auto& value : values) {
            auto valueCategory = resolvedTypeForMinOrMax(value->category(), destinationCategory);

            ASSERT(valueCategory < CalculationCategory::Other);
            if (!category) {
                if (valueCategory == CalculationCategory::Other)
                    return nullptr;
                category = valueCategory;
            }

            if (category != valueCategory) {
                if (isSamePair(category.value(), valueCategory, CalculationCategory::Length, CalculationCategory::PercentLength)) {
                    category = CalculationCategory::PercentLength;
                    continue;
                }
                if (isSamePair(category.value(), valueCategory, CalculationCategory::Number, CalculationCategory::PercentNumber)) {
                    category = CalculationCategory::PercentNumber;
                    continue;
                }
                return nullptr;
            }
        }

        return adoptRef(new CSSCalcOperation(category.value(), op, WTFMove(values)));
    }

    static RefPtr<CSSCalcExpressionNode> createSimplified(CalcOperator op, RefPtr<CSSCalcExpressionNode>&& leftSide, RefPtr<CSSCalcExpressionNode>&& rightSide)
    {
        if (!leftSide || !rightSide)
            return nullptr;

        auto leftCategory = leftSide->category();
        auto rightCategory = rightSide->category();
        ASSERT(leftCategory < CalculationCategory::Other);
        ASSERT(rightCategory < CalculationCategory::Other);

        bool isInteger = isIntegerResult(op, *leftSide, *rightSide);

        // Simplify numbers.
        if (leftCategory == CalculationCategory::Number && rightCategory == CalculationCategory::Number) {
            CSSPrimitiveValue::UnitType evaluationType = CSSPrimitiveValue::CSS_NUMBER;
            return CSSCalcPrimitiveValue::create(evaluateOperator(op, { leftSide->doubleValue(), rightSide->doubleValue() }), evaluationType, isInteger);
        }

        // Simplify addition and subtraction between same types.
        if (op == CalcOperator::Add || op == CalcOperator::Subtract) {
            if (leftCategory == rightSide->category()) {
                CSSPrimitiveValue::UnitType leftType = leftSide->primitiveType();
                if (hasDoubleValue(leftType)) {
                    CSSPrimitiveValue::UnitType rightType = rightSide->primitiveType();
                    if (leftType == rightType)
                        return CSSCalcPrimitiveValue::create(evaluateOperator(op, { leftSide->doubleValue(), rightSide->doubleValue() }), leftType, isInteger);
                    CSSPrimitiveValue::UnitCategory leftUnitCategory = CSSPrimitiveValue::unitCategory(leftType);
                    if (leftUnitCategory != CSSPrimitiveValue::UOther && leftUnitCategory == CSSPrimitiveValue::unitCategory(rightType)) {
                        CSSPrimitiveValue::UnitType canonicalType = CSSPrimitiveValue::canonicalUnitTypeForCategory(leftUnitCategory);
                        if (canonicalType != CSSPrimitiveValue::CSS_UNKNOWN) {
                            double leftValue = leftSide->doubleValue() * CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(leftType);
                            double rightValue = rightSide->doubleValue() * CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(rightType);
                            return CSSCalcPrimitiveValue::create(evaluateOperator(op, { leftValue, rightValue }), canonicalType, isInteger);
                        }
                    }
                }
            }
        } else {
            // Simplify multiplying or dividing by a number for simplifiable types.
            ASSERT(op == CalcOperator::Multiply || op == CalcOperator::Divide);
            auto* numberSide = getNumberSide(*leftSide, *rightSide);
            if (!numberSide)
                return create(op, leftSide.releaseNonNull(), rightSide.releaseNonNull());
            if (numberSide == leftSide && op == CalcOperator::Divide)
                return nullptr;
            auto& otherSide = leftSide == numberSide ? *rightSide : *leftSide;

            double number = numberSide->doubleValue();
            if (!std::isfinite(number))
                return nullptr;
            if (op == CalcOperator::Divide && !number)
                return nullptr;

            auto otherType = otherSide.primitiveType();
            if (hasDoubleValue(otherType))
                return CSSCalcPrimitiveValue::create(evaluateOperator(op, { otherSide.doubleValue(), number }), otherType, isInteger);
        }

        return create(op, leftSide.releaseNonNull(), rightSide.releaseNonNull());
    }

private:
    bool isZero() const final
    {
        return !doubleValue();
    }

    std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData& conversionData) const final
    {
        Vector<std::unique_ptr<CalcExpressionNode>> nodes;
        nodes.reserveInitialCapacity(m_children.size());

        for (auto& child : m_children) {
            auto node = child->createCalcExpression(conversionData);
            if (!node)
                return nullptr;
            nodes.uncheckedAppend(WTFMove(node));
        }
        return std::make_unique<CalcExpressionOperation>(WTFMove(nodes), m_operator);
    }

    double doubleValue() const final
    {
        Vector<double> doubleValues;
        for (auto& child : m_children)
            doubleValues.append(child->doubleValue());
        return evaluate(doubleValues);
    }

    double computeLengthPx(const CSSToLengthConversionData& conversionData) const final
    {
        Vector<double> doubleValues;
        for (auto& child : m_children)
            doubleValues.append(child->computeLengthPx(conversionData));
        return evaluate(doubleValues);
    }

    void collectDirectComputationalDependencies(HashSet<CSSPropertyID>& values) const final
    {
        for (auto& child : m_children)
            child->collectDirectComputationalDependencies(values);
    }

    void collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>& values) const final
    {
        for (auto& child : m_children)
            child->collectDirectRootComputationalDependencies(values);
    }

    static String buildCssText(Vector<String> childExpressions, CalcOperator op)
    {
        StringBuilder result;
        result.append('(');
        switch (op) {
        case CalcOperator::Add:
        case CalcOperator::Subtract:
        case CalcOperator::Multiply:
        case CalcOperator::Divide:
            ASSERT(childExpressions.size() == 2);
            result.append(childExpressions[0]);
            result.append(' ');
            result.append(static_cast<char>(op));
            result.append(' ');
            result.append(childExpressions[1]);
            break;
        case CalcOperator::Min:
        case CalcOperator::Max:
            ASSERT(!childExpressions.isEmpty());
            const char* functionName = op == CalcOperator::Min ? "min(" : "max(";
            result.append(functionName);
            result.append(childExpressions[0]);
            for (size_t i = 1; i < childExpressions.size(); ++i) {
                result.append(',');
                result.append(' ');
                result.append(childExpressions[i]);
            }
            result.append(')');
        }
        result.append(')');

        return result.toString();
    }

    String customCSSText() const final
    {
        Vector<String> cssTexts;
        for (auto& child : m_children)
            cssTexts.append(child->customCSSText());
        return buildCssText(cssTexts, m_operator);
    }

    bool equals(const CSSCalcExpressionNode& exp) const final
    {
        if (type() != exp.type())
            return false;

        const CSSCalcOperation& other = static_cast<const CSSCalcOperation&>(exp);

        if (m_children.size() != other.m_children.size() || m_operator != other.m_operator)
            return false;

        for (size_t i = 0; i < m_children.size(); ++i) {
            if (!compareCSSValue(m_children[i], other.m_children[i]))
                return false;
        }
        return true;
    }

    Type type() const final { return CssCalcOperation; }

    CSSPrimitiveValue::UnitType primitiveType() const final
    {
        switch (category()) {
        case CalculationCategory::Number:
#if !ASSERT_DISABLED
            for (auto& child : m_children)
                ASSERT(child->category() == CalculationCategory::Number);
#endif
            return CSSPrimitiveValue::CSS_NUMBER;
        case CalculationCategory::Length:
        case CalculationCategory::Percent: {
            if (m_children.isEmpty())
                return CSSPrimitiveValue::CSS_UNKNOWN;
            if (m_children.size() == 2) {
                if (m_children[0]->category() == CalculationCategory::Number)
                    return m_children[1]->primitiveType();
                if (m_children[1]->category() == CalculationCategory::Number)
                    return m_children[0]->primitiveType();
            }
            CSSPrimitiveValue::UnitType firstType = m_children[0]->primitiveType();
            for (auto& child : m_children) {
                if (firstType != child->primitiveType())
                    return CSSPrimitiveValue::CSS_UNKNOWN;
            }
            return firstType;
        }
        case CalculationCategory::Angle:
            return CSSPrimitiveValue::CSS_DEG;
        case CalculationCategory::Time:
            return CSSPrimitiveValue::CSS_MS;
        case CalculationCategory::Frequency:
            return CSSPrimitiveValue::CSS_HZ;
        case CalculationCategory::PercentLength:
        case CalculationCategory::PercentNumber:
        case CalculationCategory::Other:
            return CSSPrimitiveValue::CSS_UNKNOWN;
        }
        ASSERT_NOT_REACHED();
        return CSSPrimitiveValue::CSS_UNKNOWN;
    }

    CSSCalcOperation(CalculationCategory category, CalcOperator op, Ref<CSSCalcExpressionNode>&& leftSide, Ref<CSSCalcExpressionNode>&& rightSide)
        : CSSCalcExpressionNode(category, isIntegerResult(op, leftSide.get(), rightSide.get()))
        , m_operator(op)
    {
        m_children.append(WTFMove(leftSide));
        m_children.append(WTFMove(rightSide));
    }

    CSSCalcOperation(CalculationCategory category, CalcOperator op, Vector<Ref<CSSCalcExpressionNode>>&& children)
        : CSSCalcExpressionNode(category, isIntegerResult(op, children))
        , m_operator(op)
        , m_children(WTFMove(children))
    {
    }

    static CSSCalcExpressionNode* getNumberSide(CSSCalcExpressionNode& leftSide, CSSCalcExpressionNode& rightSide)
    {
        if (leftSide.category() == CalculationCategory::Number)
            return &leftSide;
        if (rightSide.category() == CalculationCategory::Number)
            return &rightSide;
        return nullptr;
    }

    double evaluate(const Vector<double>& children) const
    {
        return evaluateOperator(m_operator, children);
    }

    static double evaluateOperator(CalcOperator op, const Vector<double>& children)
    {
        switch (op) {
        case CalcOperator::Add:
            ASSERT(children.size() == 2);
            return children[0] + children[1];
        case CalcOperator::Subtract:
            ASSERT(children.size() == 2);
            return children[0] - children[1];
        case CalcOperator::Multiply:
            ASSERT(children.size() == 2);
            return children[0] * children[1];
        case CalcOperator::Divide:
            ASSERT(children.size() == 1 || children.size() == 2);
            if (children.size() == 1)
                return std::numeric_limits<double>::quiet_NaN();
            return children[0] / children[1];
        case CalcOperator::Min: {
            if (children.isEmpty())
                return std::numeric_limits<double>::quiet_NaN();
            double minimum = children[0];
            for (auto child : children)
                minimum = std::min(minimum, child);
            return minimum;
        }
        case CalcOperator::Max: {
            if (children.isEmpty())
                return std::numeric_limits<double>::quiet_NaN();
            double maximum = children[0];
            for (auto child : children)
                maximum = std::max(maximum, child);
            return maximum;
        }
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    const CalcOperator m_operator;
    Vector<Ref<CSSCalcExpressionNode>> m_children;
};

static ParseState checkDepthAndIndex(int* depth, CSSParserTokenRange tokens)
{
    (*depth)++;
    if (tokens.atEnd())
        return NoMoreTokens;
    if (*depth > maxExpressionDepth)
        return TooDeep;
    return OK;
}

class CSSCalcExpressionNodeParser {
public:
    explicit CSSCalcExpressionNodeParser(CalculationCategory destinationCategory)
        : m_destinationCategory(destinationCategory)
    { }

    RefPtr<CSSCalcExpressionNode> parseCalc(CSSParserTokenRange tokens, CSSValueID function)
    {
        Value result;
        tokens.consumeWhitespace();
        bool ok = false;
        if (function == CSSValueCalc || function == CSSValueWebkitCalc)
            ok = parseValueExpression(tokens, 0, &result);
        else if (function == CSSValueMin || function == CSSValueMax)
            ok = parseMinMaxExpression(tokens, function, 0, &result);
        if (!ok || !tokens.atEnd())
            return nullptr;
        return result.value;
    }
    
private:
    struct Value {
        RefPtr<CSSCalcExpressionNode> value;
    };
    
    char operatorValue(const CSSParserToken& token)
    {
        if (token.type() == DelimiterToken)
            return token.delimiter();
        return 0;
    }
    
    bool parseValue(CSSParserTokenRange& tokens, Value* result)
    {
        CSSParserToken token = tokens.consumeIncludingWhitespace();
        if (!(token.type() == NumberToken || token.type() == PercentageToken || token.type() == DimensionToken))
            return false;
        
        CSSPrimitiveValue::UnitType type = token.unitType();
        if (unitCategory(type) == CalculationCategory::Other)
            return false;
        
        bool isInteger = token.numericValueType() == IntegerValueType || (token.numericValueType() == NumberValueType && token.numericValue() == trunc(token.numericValue()));
        result->value = CSSCalcPrimitiveValue::create(CSSPrimitiveValue::create(token.numericValue(), type), isInteger);
        
        return true;
    }
    
    bool parseValueTerm(CSSParserTokenRange& tokens, int depth, Value* result)
    {
        if (checkDepthAndIndex(&depth, tokens) != OK)
            return false;

        auto functionId = tokens.peek().functionId();
        
        if (tokens.peek().type() == LeftParenthesisToken || functionId == CSSValueCalc) {
            CSSParserTokenRange innerRange = tokens.consumeBlock();
            tokens.consumeWhitespace();
            innerRange.consumeWhitespace();
            return parseValueExpression(innerRange, depth, result);
        }

        if (functionId == CSSValueMax || functionId == CSSValueMin) {
            CSSParserTokenRange innerRange = tokens.consumeBlock();
            tokens.consumeWhitespace();
            innerRange.consumeWhitespace();
            return parseMinMaxExpression(innerRange, functionId, depth, result);
        }
        
        return parseValue(tokens, result);
    }
    
    bool parseValueMultiplicativeExpression(CSSParserTokenRange& tokens, int depth, Value* result)
    {
        if (checkDepthAndIndex(&depth, tokens) != OK)
            return false;
        
        if (!parseValueTerm(tokens, depth, result))
            return false;
        
        while (!tokens.atEnd()) {
            char operatorCharacter = operatorValue(tokens.peek());
            if (operatorCharacter != static_cast<char>(CalcOperator::Multiply) && operatorCharacter != static_cast<char>(CalcOperator::Divide))
                break;
            tokens.consumeIncludingWhitespace();
            
            Value rhs;
            if (!parseValueTerm(tokens, depth, &rhs))
                return false;
            
            result->value = CSSCalcOperation::createSimplified(static_cast<CalcOperator>(operatorCharacter), WTFMove(result->value), WTFMove(rhs.value));

            if (!result->value)
                return false;
        }
        
        return true;
    }
    
    bool parseAdditiveValueExpression(CSSParserTokenRange& tokens, int depth, Value* result)
    {
        if (checkDepthAndIndex(&depth, tokens) != OK)
            return false;
        
        if (!parseValueMultiplicativeExpression(tokens, depth, result))
            return false;
        
        while (!tokens.atEnd()) {
            char operatorCharacter = operatorValue(tokens.peek());
            if (operatorCharacter != static_cast<char>(CalcOperator::Add) && operatorCharacter != static_cast<char>(CalcOperator::Subtract))
                break;
            if ((&tokens.peek() - 1)->type() != WhitespaceToken)
                return false; // calc(1px+ 2px) is invalid
            tokens.consume();
            if (tokens.peek().type() != WhitespaceToken)
                return false; // calc(1px +2px) is invalid
            tokens.consumeIncludingWhitespace();
            
            Value rhs;
            if (!parseValueMultiplicativeExpression(tokens, depth, &rhs))
                return false;
            
            result->value = CSSCalcOperation::createSimplified(static_cast<CalcOperator>(operatorCharacter), WTFMove(result->value), WTFMove(rhs.value));
            if (!result->value)
                return false;
        }
        
        return true;
    }

    bool parseMinMaxExpression(CSSParserTokenRange& tokens, CSSValueID minMaxFunction, int depth, Value* result)
    {
        if (checkDepthAndIndex(&depth, tokens) != OK)
            return false;

        CalcOperator op = (minMaxFunction == CSSValueMin) ? CalcOperator::Min : CalcOperator::Max;

        Value value;
        if (!parseValueExpression(tokens, depth, &value))
            return false;

        Vector<Ref<CSSCalcExpressionNode>> nodes;
        nodes.append(value.value.releaseNonNull());

        while (!tokens.atEnd()) {
            tokens.consumeWhitespace();
            if (tokens.consume().type() != CommaToken)
                return false;
            tokens.consumeWhitespace();

            if (!parseValueExpression(tokens, depth, &value))
                return false;

            nodes.append(value.value.releaseNonNull());
        }

        result->value = CSSCalcOperation::createMinOrMax(op, WTFMove(nodes), m_destinationCategory);
        return result->value;
    }

    bool parseValueExpression(CSSParserTokenRange& tokens, int depth, Value* result)
    {
        return parseAdditiveValueExpression(tokens, depth, result);
    }

    CalculationCategory m_destinationCategory;
};

static inline RefPtr<CSSCalcOperation> createBlendHalf(const Length& length, const RenderStyle& style, float progress)
{
    return CSSCalcOperation::create(CalcOperator::Multiply, createCSS(length, style),
        CSSCalcPrimitiveValue::create(CSSPrimitiveValue::create(progress, CSSPrimitiveValue::CSS_NUMBER), !progress || progress == 1));
}

static RefPtr<CSSCalcExpressionNode> createCSS(const CalcExpressionNode& node, const RenderStyle& style)
{
    switch (node.type()) {
    case CalcExpressionNodeType::Number: {
        float value = toCalcExpressionNumber(node).value();
        return CSSCalcPrimitiveValue::create(CSSPrimitiveValue::create(value, CSSPrimitiveValue::CSS_NUMBER), value == std::trunc(value));
    }
    case CalcExpressionNodeType::Length:
        return createCSS(toCalcExpressionLength(node).length(), style);
    case CalcExpressionNodeType::Operation: {
        auto& operationNode = toCalcExpressionOperation(node);
        auto& operationChildren = operationNode.children();
        CalcOperator op = operationNode.getOperator();
        if (op == CalcOperator::Min || op == CalcOperator::Max) {
            Vector<Ref<CSSCalcExpressionNode>> values;
            values.reserveInitialCapacity(operationChildren.size());
            for (auto& child : operationChildren) {
                auto cssNode = createCSS(*child, style);
                if (!cssNode)
                    return nullptr;
                values.uncheckedAppend(*cssNode);
            }
            return CSSCalcOperation::createMinOrMax(operationNode.getOperator(), WTFMove(values), CalculationCategory::Other);
        }

        if (operationChildren.size() == 2)
            return CSSCalcOperation::create(operationNode.getOperator(), createCSS(*operationChildren[0], style), createCSS(*operationChildren[1], style));

        return nullptr;
    }
    case CalcExpressionNodeType::BlendLength: {
        // FIXME: (http://webkit.org/b/122036) Create a CSSCalcExpressionNode equivalent of CalcExpressionBlendLength.
        auto& blend = toCalcExpressionBlendLength(node);
        float progress = blend.progress();
        return CSSCalcOperation::create(CalcOperator::Add, createBlendHalf(blend.from(), style, 1 - progress), createBlendHalf(blend.to(), style, progress));
    }
    case CalcExpressionNodeType::Undefined:
        ASSERT_NOT_REACHED();
    }
    return nullptr;
}

static RefPtr<CSSCalcExpressionNode> createCSS(const Length& length, const RenderStyle& style)
{
    switch (length.type()) {
    case Percent:
    case Fixed:
        return CSSCalcPrimitiveValue::create(CSSPrimitiveValue::create(length, style), length.value() == trunc(length.value()));
    case Calculated:
        return createCSS(length.calculationValue().expression(), style);
    case Auto:
    case Intrinsic:
    case MinIntrinsic:
    case MinContent:
    case MaxContent:
    case FillAvailable:
    case FitContent:
    case Relative:
    case Undefined:
        ASSERT_NOT_REACHED();
    }
    return nullptr;
}

RefPtr<CSSCalcValue> CSSCalcValue::create(CSSValueID function, const CSSParserTokenRange& tokens, CalculationCategory destinationCategory, ValueRange range)
{
    CSSCalcExpressionNodeParser parser(destinationCategory);
    auto expression = parser.parseCalc(tokens, function);
    if (!expression)
        return nullptr;
    return adoptRef(new CSSCalcValue(expression.releaseNonNull(), range != ValueRangeAll));
}
    
RefPtr<CSSCalcValue> CSSCalcValue::create(const CalculationValue& value, const RenderStyle& style)
{
    auto expression = createCSS(value.expression(), style);
    if (!expression)
        return nullptr;
    return adoptRef(new CSSCalcValue(expression.releaseNonNull(), value.shouldClampToNonNegative()));
}

} // namespace WebCore
