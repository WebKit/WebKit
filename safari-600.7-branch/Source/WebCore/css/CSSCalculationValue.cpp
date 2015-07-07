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

static PassRefPtr<CSSCalcExpressionNode> createCSS(const CalcExpressionNode&, const RenderStyle&);
static PassRefPtr<CSSCalcExpressionNode> createCSS(const Length&, const RenderStyle&);

static CalculationCategory unitCategory(CSSPrimitiveValue::UnitTypes type)
{
    switch (type) {
    case CSSPrimitiveValue::CSS_NUMBER:
    case CSSPrimitiveValue::CSS_PARSER_INTEGER:
        return CalcNumber;
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
        return CalcLength;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        return CalcPercent;
    case CSSPrimitiveValue::CSS_DEG:
    case CSSPrimitiveValue::CSS_RAD:
    case CSSPrimitiveValue::CSS_GRAD:
    case CSSPrimitiveValue::CSS_TURN:
        return CalcAngle;
    case CSSPrimitiveValue::CSS_MS:
    case CSSPrimitiveValue::CSS_S:
        return CalcTime;
    case CSSPrimitiveValue::CSS_HZ:
    case CSSPrimitiveValue::CSS_KHZ:
        return CalcFrequency;
    default:
        return CalcOther;
    }
}

static bool hasDoubleValue(CSSPrimitiveValue::UnitTypes type)
{
    switch (type) {
    case CSSPrimitiveValue::CSS_FR:
    case CSSPrimitiveValue::CSS_NUMBER:
    case CSSPrimitiveValue::CSS_PARSER_INTEGER:
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
    case CSSPrimitiveValue::CSS_URI:
    case CSSPrimitiveValue::CSS_IDENT:
    case CSSPrimitiveValue::CSS_ATTR:
    case CSSPrimitiveValue::CSS_COUNTER:
    case CSSPrimitiveValue::CSS_RECT:
    case CSSPrimitiveValue::CSS_RGBCOLOR:
    case CSSPrimitiveValue::CSS_PAIR:
    case CSSPrimitiveValue::CSS_UNICODE_RANGE:
    case CSSPrimitiveValue::CSS_PARSER_OPERATOR:
    case CSSPrimitiveValue::CSS_PARSER_HEXCOLOR:
    case CSSPrimitiveValue::CSS_PARSER_IDENTIFIER:
    case CSSPrimitiveValue::CSS_TURN:
    case CSSPrimitiveValue::CSS_COUNTER_NAME:
    case CSSPrimitiveValue::CSS_SHAPE:
    case CSSPrimitiveValue::CSS_QUAD:
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
    result.append("calc");
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
    static PassRef<CSSCalcPrimitiveValue> create(PassRefPtr<CSSPrimitiveValue> value, bool isInteger)
    {
        return adoptRef(*new CSSCalcPrimitiveValue(value, isInteger));
    }

    static PassRefPtr<CSSCalcPrimitiveValue> create(double value, CSSPrimitiveValue::UnitTypes type, bool isInteger)
    {
        if (std::isnan(value) || std::isinf(value))
            return nullptr;
        return adoptRef(new CSSCalcPrimitiveValue(CSSPrimitiveValue::create(value, type), isInteger));
    }

private:
    virtual bool isZero() const override
    {
        return !m_value->getDoubleValue();
    }

    virtual String customCSSText() const override
    {
        return m_value->cssText();
    }

    virtual std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData& conversionData) const override
    {
        switch (category()) {
        case CalcNumber:
            return std::make_unique<CalcExpressionNumber>(m_value->getFloatValue());
        case CalcLength:
            return std::make_unique<CalcExpressionLength>(Length(m_value->computeLength<float>(conversionData), WebCore::Fixed));
        case CalcPercent:
        case CalcPercentLength: {
            CSSPrimitiveValue* primitiveValue = m_value.get();
            return std::make_unique<CalcExpressionLength>(primitiveValue
                ? primitiveValue->convertToLength<FixedFloatConversion | PercentConversion | FractionConversion>(conversionData) : Length(Undefined));
        }
        // Only types that could be part of a Length expression can be converted
        // to a CalcExpressionNode. CalcPercentNumber makes no sense as a Length.
        case CalcPercentNumber:
        case CalcAngle:
        case CalcTime:
        case CalcFrequency:
        case CalcOther:
            ASSERT_NOT_REACHED();
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    virtual double doubleValue() const override
    {
        if (hasDoubleValue(primitiveType()))
            return m_value->getDoubleValue();
        ASSERT_NOT_REACHED();
        return 0;
    }

    virtual double computeLengthPx(const CSSToLengthConversionData& conversionData) const override
    {
        switch (category()) {
        case CalcLength:
            return m_value->computeLength<double>(conversionData);
        case CalcPercent:
        case CalcNumber:
            return m_value->getDoubleValue();
        case CalcPercentLength:
        case CalcPercentNumber:
        case CalcAngle:
        case CalcTime:
        case CalcFrequency:
        case CalcOther:
            ASSERT_NOT_REACHED();
            break;
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    virtual bool equals(const CSSCalcExpressionNode& other) const override
    {
        if (type() != other.type())
            return false;

        return compareCSSValuePtr(m_value, static_cast<const CSSCalcPrimitiveValue&>(other).m_value);
    }

    virtual Type type() const override { return CssCalcPrimitiveValue; }
    virtual CSSPrimitiveValue::UnitTypes primitiveType() const override
    {
        return CSSPrimitiveValue::UnitTypes(m_value->primitiveType());
    }

private:
    explicit CSSCalcPrimitiveValue(PassRefPtr<CSSPrimitiveValue> value, bool isInteger)
        : CSSCalcExpressionNode(unitCategory((CSSPrimitiveValue::UnitTypes)value->primitiveType()), isInteger)
        , m_value(value)
    {
    }

    RefPtr<CSSPrimitiveValue> m_value;
};

static const CalculationCategory addSubtractResult[CalcAngle][CalcAngle] = {
//    CalcNumber         CalcLength         CalcPercent        CalcPercentNumber  CalcPercentLength
    { CalcNumber,        CalcOther,         CalcPercentNumber, CalcPercentNumber, CalcOther }, //         CalcNumber
    { CalcOther,         CalcLength,        CalcPercentLength, CalcOther,         CalcPercentLength }, // CalcLength
    { CalcPercentNumber, CalcPercentLength, CalcPercent,       CalcPercentNumber, CalcPercentLength }, // CalcPercent
    { CalcPercentNumber, CalcOther,         CalcPercentNumber, CalcPercentNumber, CalcOther }, //         CalcPercentNumber
    { CalcOther,         CalcPercentLength, CalcPercentLength, CalcOther,         CalcPercentLength }, // CalcPercentLength
};

static CalculationCategory determineCategory(const CSSCalcExpressionNode& leftSide, const CSSCalcExpressionNode& rightSide, CalcOperator op)
{
    CalculationCategory leftCategory = leftSide.category();
    CalculationCategory rightCategory = rightSide.category();
    ASSERT(leftCategory < CalcOther);
    ASSERT(rightCategory < CalcOther);

    switch (op) {
    case CalcAdd:
    case CalcSubtract:
        if (leftCategory < CalcAngle && rightCategory < CalcAngle)
            return addSubtractResult[leftCategory][rightCategory];
        if (leftCategory == rightCategory)
            return leftCategory;
        return CalcOther;
    case CalcMultiply:
        if (leftCategory != CalcNumber && rightCategory != CalcNumber)
            return CalcOther;
        return leftCategory == CalcNumber ? rightCategory : leftCategory;
    case CalcDivide:
        if (rightCategory != CalcNumber || rightSide.isZero())
            return CalcOther;
        return leftCategory;
    }

    ASSERT_NOT_REACHED();
    return CalcOther;
}

static inline bool isIntegerResult(CalcOperator op, const CSSCalcExpressionNode& leftSide, const CSSCalcExpressionNode& rightSide)
{
    // Performs W3C spec's type checking for calc integers.
    // http://www.w3.org/TR/css3-values/#calc-type-checking
    return op != CalcDivide && leftSide.isInteger() && rightSide.isInteger();
}

class CSSCalcBinaryOperation final : public CSSCalcExpressionNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<CSSCalcBinaryOperation> create(CalcOperator op, PassRefPtr<CSSCalcExpressionNode> leftSide, PassRefPtr<CSSCalcExpressionNode> rightSide)
    {
        ASSERT(leftSide->category() < CalcOther);
        ASSERT(rightSide->category() < CalcOther);

        CalculationCategory newCategory = determineCategory(*leftSide, *rightSide, op);

        if (newCategory == CalcOther)
            return nullptr;

        return adoptRef(new CSSCalcBinaryOperation(newCategory, op, leftSide, rightSide));
    }

    static PassRefPtr<CSSCalcExpressionNode> createSimplified(CalcOperator op, PassRefPtr<CSSCalcExpressionNode> leftSide, PassRefPtr<CSSCalcExpressionNode> rightSide)
    {
        CalculationCategory leftCategory = leftSide->category();
        CalculationCategory rightCategory = rightSide->category();
        ASSERT(leftCategory < CalcOther);
        ASSERT(rightCategory < CalcOther);

        bool isInteger = isIntegerResult(op, *leftSide, *rightSide);

        // Simplify numbers.
        if (leftCategory == CalcNumber && rightCategory == CalcNumber) {
            CSSPrimitiveValue::UnitTypes evaluationType = isInteger ? CSSPrimitiveValue::CSS_PARSER_INTEGER : CSSPrimitiveValue::CSS_NUMBER;
            return CSSCalcPrimitiveValue::create(evaluateOperator(op, leftSide->doubleValue(), rightSide->doubleValue()), evaluationType, isInteger);
        }

        // Simplify addition and subtraction between same types.
        if (op == CalcAdd || op == CalcSubtract) {
            if (leftCategory == rightSide->category()) {
                CSSPrimitiveValue::UnitTypes leftType = leftSide->primitiveType();
                if (hasDoubleValue(leftType)) {
                    CSSPrimitiveValue::UnitTypes rightType = rightSide->primitiveType();
                    if (leftType == rightType)
                        return CSSCalcPrimitiveValue::create(evaluateOperator(op, leftSide->doubleValue(), rightSide->doubleValue()), leftType, isInteger);
                    CSSPrimitiveValue::UnitCategory leftUnitCategory = CSSPrimitiveValue::unitCategory(leftType);
                    if (leftUnitCategory != CSSPrimitiveValue::UOther && leftUnitCategory == CSSPrimitiveValue::unitCategory(rightType)) {
                        CSSPrimitiveValue::UnitTypes canonicalType = CSSPrimitiveValue::canonicalUnitTypeForCategory(leftUnitCategory);
                        if (canonicalType != CSSPrimitiveValue::CSS_UNKNOWN) {
                            double leftValue = leftSide->doubleValue() * CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(leftType);
                            double rightValue = rightSide->doubleValue() * CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(rightType);
                            return CSSCalcPrimitiveValue::create(evaluateOperator(op, leftValue, rightValue), canonicalType, isInteger);
                        }
                    }
                }
            }
        } else {
            // Simplify multiplying or dividing by a number for simplifiable types.
            ASSERT(op == CalcMultiply || op == CalcDivide);
            CSSCalcExpressionNode* numberSide = getNumberSide(*leftSide, *rightSide);
            if (!numberSide)
                return create(op, leftSide, rightSide);
            if (numberSide == leftSide && op == CalcDivide)
                return nullptr;
            CSSCalcExpressionNode* otherSide = leftSide == numberSide ? rightSide.get() : leftSide.get();

            double number = numberSide->doubleValue();
            if (std::isnan(number) || std::isinf(number))
                return nullptr;
            if (op == CalcDivide && !number)
                return nullptr;

            CSSPrimitiveValue::UnitTypes otherType = otherSide->primitiveType();
            if (hasDoubleValue(otherType))
                return CSSCalcPrimitiveValue::create(evaluateOperator(op, otherSide->doubleValue(), number), otherType, isInteger);
        }

        return create(op, leftSide, rightSide);
    }

private:
    virtual bool isZero() const override
    {
        return !doubleValue();
    }

    virtual std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData& conversionData) const override
    {
        std::unique_ptr<CalcExpressionNode> left(m_leftSide->createCalcExpression(conversionData));
        if (!left)
            return nullptr;
        std::unique_ptr<CalcExpressionNode> right(m_rightSide->createCalcExpression(conversionData));
        if (!right)
            return nullptr;
        return std::make_unique<CalcExpressionBinaryOperation>(WTF::move(left), WTF::move(right), m_operator);
    }

    virtual double doubleValue() const override
    {
        return evaluate(m_leftSide->doubleValue(), m_rightSide->doubleValue());
    }

    virtual double computeLengthPx(const CSSToLengthConversionData& conversionData) const override
    {
        const double leftValue = m_leftSide->computeLengthPx(conversionData);
        const double rightValue = m_rightSide->computeLengthPx(conversionData);
        return evaluate(leftValue, rightValue);
    }

    static String buildCssText(const String& leftExpression, const String& rightExpression, CalcOperator op)
    {
        StringBuilder result;
        result.append('(');
        result.append(leftExpression);
        result.append(' ');
        result.append(static_cast<char>(op));
        result.append(' ');
        result.append(rightExpression);
        result.append(')');

        return result.toString();
    }

    virtual String customCSSText() const override
    {
        return buildCssText(m_leftSide->customCSSText(), m_rightSide->customCSSText(), m_operator);
    }

    virtual bool equals(const CSSCalcExpressionNode& exp) const override
    {
        if (type() != exp.type())
            return false;

        const CSSCalcBinaryOperation& other = static_cast<const CSSCalcBinaryOperation&>(exp);
        return compareCSSValuePtr(m_leftSide, other.m_leftSide)
            && compareCSSValuePtr(m_rightSide, other.m_rightSide)
            && m_operator == other.m_operator;
    }

    virtual Type type() const override { return CssCalcBinaryOperation; }

    virtual CSSPrimitiveValue::UnitTypes primitiveType() const override
    {
        switch (category()) {
        case CalcNumber:
            ASSERT(m_leftSide->category() == CalcNumber && m_rightSide->category() == CalcNumber);
            if (isInteger())
                return CSSPrimitiveValue::CSS_PARSER_INTEGER;
            return CSSPrimitiveValue::CSS_NUMBER;
        case CalcLength:
        case CalcPercent: {
            if (m_leftSide->category() == CalcNumber)
                return m_rightSide->primitiveType();
            if (m_rightSide->category() == CalcNumber)
                return m_leftSide->primitiveType();
            CSSPrimitiveValue::UnitTypes leftType = m_leftSide->primitiveType();
            if (leftType == m_rightSide->primitiveType())
                return leftType;
            return CSSPrimitiveValue::CSS_UNKNOWN;
        }
        case CalcAngle:
            return CSSPrimitiveValue::CSS_DEG;
        case CalcTime:
            return CSSPrimitiveValue::CSS_MS;
        case CalcFrequency:
            return CSSPrimitiveValue::CSS_HZ;
        case CalcPercentLength:
        case CalcPercentNumber:
        case CalcOther:
            return CSSPrimitiveValue::CSS_UNKNOWN;
        }
        ASSERT_NOT_REACHED();
        return CSSPrimitiveValue::CSS_UNKNOWN;
    }

    CSSCalcBinaryOperation(CalculationCategory category, CalcOperator op, PassRefPtr<CSSCalcExpressionNode> leftSide, PassRefPtr<CSSCalcExpressionNode> rightSide)
        : CSSCalcExpressionNode(category, isIntegerResult(op, *leftSide, *rightSide))
        , m_leftSide(leftSide)
        , m_rightSide(rightSide)
        , m_operator(op)
    {
    }

    static CSSCalcExpressionNode* getNumberSide(CSSCalcExpressionNode& leftSide, CSSCalcExpressionNode& rightSide)
    {
        if (leftSide.category() == CalcNumber)
            return &leftSide;
        if (rightSide.category() == CalcNumber)
            return &rightSide;
        return nullptr;
    }

    double evaluate(double leftSide, double rightSide) const
    {
        return evaluateOperator(m_operator, leftSide, rightSide);
    }

    static double evaluateOperator(CalcOperator op, double leftValue, double rightValue)
    {
        switch (op) {
        case CalcAdd:
            return leftValue + rightValue;
        case CalcSubtract:
            return leftValue - rightValue;
        case CalcMultiply:
            return leftValue * rightValue;
        case CalcDivide:
            if (rightValue)
                return leftValue / rightValue;
            return std::numeric_limits<double>::quiet_NaN();
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    const RefPtr<CSSCalcExpressionNode> m_leftSide;
    const RefPtr<CSSCalcExpressionNode> m_rightSide;
    const CalcOperator m_operator;
};

static ParseState checkDepthAndIndex(int* depth, unsigned index, CSSParserValueList* tokens)
{
    (*depth)++;
    if (*depth > maxExpressionDepth)
        return TooDeep;
    if (index >= tokens->size())
        return NoMoreTokens;
    return OK;
}

class CSSCalcExpressionNodeParser {
public:
    PassRefPtr<CSSCalcExpressionNode> parseCalc(CSSParserValueList* tokens)
    {
        unsigned index = 0;
        Value result;
        bool ok = parseValueExpression(tokens, 0, &index, &result);
        ASSERT_WITH_SECURITY_IMPLICATION(index <= tokens->size());
        if (!ok || index != tokens->size())
            return nullptr;
        return result.value;
    }

private:
    struct Value {
        RefPtr<CSSCalcExpressionNode> value;
    };

    char operatorValue(CSSParserValueList* tokens, unsigned index)
    {
        if (index >= tokens->size())
            return 0;
        CSSParserValue* value = tokens->valueAt(index);
        if (value->unit != CSSParserValue::Operator)
            return 0;

        return value->iValue;
    }

    bool parseValue(CSSParserValueList* tokens, unsigned* index, Value* result)
    {
        CSSParserValue* parserValue = tokens->valueAt(*index);
        if (parserValue->unit == CSSParserValue::Operator || parserValue->unit == CSSParserValue::Function)
            return false;

        RefPtr<CSSValue> value = parserValue->createCSSValue();
        if (!value || !value->isPrimitiveValue())
            return false;

        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value.get());
        result->value = CSSCalcPrimitiveValue::create(primitiveValue, parserValue->isInt);

        ++*index;
        return true;
    }

    bool parseValueTerm(CSSParserValueList* tokens, int depth, unsigned* index, Value* result)
    {
        if (checkDepthAndIndex(&depth, *index, tokens) != OK)
            return false;

        if (operatorValue(tokens, *index) == '(') {
            unsigned currentIndex = *index + 1;
            if (!parseValueExpression(tokens, depth, &currentIndex, result))
                return false;

            if (operatorValue(tokens, currentIndex) != ')')
                return false;
            *index = currentIndex + 1;
            return true;
        }

        return parseValue(tokens, index, result);
    }

    bool parseValueMultiplicativeExpression(CSSParserValueList* tokens, int depth, unsigned* index, Value* result)
    {
        if (checkDepthAndIndex(&depth, *index, tokens) != OK)
            return false;

        if (!parseValueTerm(tokens, depth, index, result))
            return false;

        while (*index < tokens->size() - 1) {
            char operatorCharacter = operatorValue(tokens, *index);
            if (operatorCharacter != CalcMultiply && operatorCharacter != CalcDivide)
                break;
            ++*index;

            Value rhs;
            if (!parseValueTerm(tokens, depth, index, &rhs))
                return false;

            result->value = CSSCalcBinaryOperation::createSimplified(static_cast<CalcOperator>(operatorCharacter), result->value, rhs.value);
            if (!result->value)
                return false;
        }

        ASSERT_WITH_SECURITY_IMPLICATION(*index <= tokens->size());
        return true;
    }

    bool parseAdditiveValueExpression(CSSParserValueList* tokens, int depth, unsigned* index, Value* result)
    {
        if (checkDepthAndIndex(&depth, *index, tokens) != OK)
            return false;

        if (!parseValueMultiplicativeExpression(tokens, depth, index, result))
            return false;

        while (*index < tokens->size() - 1) {
            char operatorCharacter = operatorValue(tokens, *index);
            if (operatorCharacter != CalcAdd && operatorCharacter != CalcSubtract)
                break;
            ++*index;

            Value rhs;
            if (!parseValueMultiplicativeExpression(tokens, depth, index, &rhs))
                return false;

            result->value = CSSCalcBinaryOperation::createSimplified(static_cast<CalcOperator>(operatorCharacter), result->value, rhs.value);
            if (!result->value)
                return false;
        }

        ASSERT_WITH_SECURITY_IMPLICATION(*index <= tokens->size());
        return true;
    }

    bool parseValueExpression(CSSParserValueList* tokens, int depth, unsigned* index, Value* result)
    {
        return parseAdditiveValueExpression(tokens, depth, index, result);
    }
};

static inline PassRefPtr<CSSCalcBinaryOperation> createBlendHalf(const Length& length, const RenderStyle& style, float progress)
{
    return CSSCalcBinaryOperation::create(CalcMultiply, createCSS(length, style),
        CSSCalcPrimitiveValue::create(CSSPrimitiveValue::create(progress, CSSPrimitiveValue::CSS_NUMBER), !progress || progress == 1));
}

static PassRefPtr<CSSCalcExpressionNode> createCSS(const CalcExpressionNode& node, const RenderStyle& style)
{
    switch (node.type()) {
    case CalcExpressionNodeNumber: {
        float value = toCalcExpressionNumber(node).value();
        return CSSCalcPrimitiveValue::create(CSSPrimitiveValue::create(value, CSSPrimitiveValue::CSS_NUMBER), value == truncf(value));
    }
    case CalcExpressionNodeLength:
        return createCSS(toCalcExpressionLength(node).length(), style);
    case CalcExpressionNodeBinaryOperation: {
        auto& binaryNode = toCalcExpressionBinaryOperation(node);
        return CSSCalcBinaryOperation::create(binaryNode.getOperator(), createCSS(binaryNode.leftSide(), style), createCSS(binaryNode.rightSide(), style));
    }
    case CalcExpressionNodeBlendLength: {
        // FIXME: (http://webkit.org/b/122036) Create a CSSCalcExpressionNode equivalent of CalcExpressionBlendLength.
        auto& blend = toCalcExpressionBlendLength(node);
        float progress = blend.progress();
        return CSSCalcBinaryOperation::create(CalcAdd, createBlendHalf(blend.from(), style, 1 - progress), createBlendHalf(blend.to(), style, progress));
    }
    case CalcExpressionNodeUndefined:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

static PassRefPtr<CSSCalcExpressionNode> createCSS(const Length& length, const RenderStyle& style)
{
    switch (length.type()) {
    case Percent:
    case Fixed:
        return CSSCalcPrimitiveValue::create(CSSPrimitiveValue::create(length, &style), length.value() == trunc(length.value()));
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
        return nullptr;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

PassRefPtr<CSSCalcValue> CSSCalcValue::create(CSSParserString name, CSSParserValueList& parserValueList, CalculationPermittedValueRange range)
{
    CSSCalcExpressionNodeParser parser;
    RefPtr<CSSCalcExpressionNode> expression;

    if (equalIgnoringCase(name, "calc(") || equalIgnoringCase(name, "-webkit-calc("))
        expression = parser.parseCalc(&parserValueList);
    // FIXME: calc (http://webkit.org/b/16662) Add parsing for min and max here

    return expression ? adoptRef(new CSSCalcValue(expression.releaseNonNull(), range != CalculationRangeAll)) : nullptr;
}

PassRefPtr<CSSCalcValue> CSSCalcValue::create(const CalculationValue& value, const RenderStyle& style)
{
    RefPtr<CSSCalcExpressionNode> expression = createCSS(value.expression(), style);
    if (!expression)
        return nullptr;
    return adoptRef(new CSSCalcValue(expression.releaseNonNull(), value.shouldClampToNonNegative()));
}

} // namespace WebCore
