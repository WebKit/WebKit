/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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
#include "CSSValueList.h"
#include "Length.h"
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

static CalculationCategory unitCategory(CSSPrimitiveValue::UnitTypes type)
{
    switch (type) {
    case CSSPrimitiveValue::CSS_NUMBER:
    case CSSPrimitiveValue::CSS_PARSER_INTEGER:
        return CalcNumber;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        return CalcPercent;
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
        return CalcLength;
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
    return compareCSSValuePtr(m_expression, other.m_expression);
}

double CSSCalcValue::clampToPermittedRange(double value) const
{
    return m_nonNegative && value < 0 ? 0 : value;
}

double CSSCalcValue::doubleValue() const
{
    return clampToPermittedRange(m_expression->doubleValue());
}

double CSSCalcValue::computeLengthPx(const RenderStyle* currentStyle, const RenderStyle* rootStyle, double multiplier, bool computingFontSize) const
{
    return clampToPermittedRange(m_expression->computeLengthPx(currentStyle, rootStyle, multiplier, computingFontSize));
}

CSSCalcExpressionNode::~CSSCalcExpressionNode()
{
}

class CSSCalcPrimitiveValue : public CSSCalcExpressionNode {
    WTF_MAKE_FAST_ALLOCATED;
public:

    static PassRefPtr<CSSCalcPrimitiveValue> create(PassRefPtr<CSSPrimitiveValue> value, bool isInteger)
    {
        return adoptRef(new CSSCalcPrimitiveValue(value, isInteger));
    }

    static PassRefPtr<CSSCalcPrimitiveValue> create(double value, CSSPrimitiveValue::UnitTypes type, bool isInteger)
    {
        if (std::isnan(value) || std::isinf(value))
            return 0;
        return adoptRef(new CSSCalcPrimitiveValue(CSSPrimitiveValue::create(value, type), isInteger));
    }

    virtual bool isZero() const
    {
        return !m_value->getDoubleValue();
    }

    virtual String customCSSText() const
    {
        return m_value->cssText();
    }

    virtual std::unique_ptr<CalcExpressionNode> toCalcValue(const RenderStyle* style, const RenderStyle* rootStyle, double zoom) const
    {
        switch (m_category) {
        case CalcNumber:
            return std::make_unique<CalcExpressionNumber>(m_value->getFloatValue());
        case CalcLength:
            return std::make_unique<CalcExpressionNumber>(m_value->computeLength<float>(style, rootStyle, zoom));
        case CalcPercent:
        case CalcPercentLength: {
            CSSPrimitiveValue* primitiveValue = m_value.get();
            return std::make_unique<CalcExpressionLength>(primitiveValue
                ? primitiveValue->convertToLength<FixedFloatConversion | PercentConversion | FractionConversion>(style, rootStyle, zoom) : Length(Undefined));
        }
        // Only types that could be part of a Length expression can be converted
        // to a CalcExpressionNode. CalcPercentNumber makes no sense as a Length.
        case CalcPercentNumber:
        case CalcOther:
            ASSERT_NOT_REACHED();
        }
        return nullptr;
    }

    virtual double doubleValue() const
    {
        if (hasDoubleValue(primitiveType()))
            return m_value->getDoubleValue();
        ASSERT_NOT_REACHED();
        return 0;
    }

    virtual double computeLengthPx(const RenderStyle* currentStyle, const RenderStyle* rootStyle, double multiplier, bool computingFontSize) const
    {
        switch (m_category) {
        case CalcLength:
            return m_value->computeLength<double>(currentStyle, rootStyle, multiplier, computingFontSize);
        case CalcPercent:
        case CalcNumber:
            return m_value->getDoubleValue();
        case CalcPercentLength:
        case CalcPercentNumber:
        case CalcOther:
            ASSERT_NOT_REACHED();
            break;
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    virtual bool equals(const CSSCalcExpressionNode& other) const
    {
        if (type() != other.type())
            return false;

        return compareCSSValuePtr(m_value, static_cast<const CSSCalcPrimitiveValue&>(other).m_value);
    }

    virtual Type type() const { return CssCalcPrimitiveValue; }
    virtual CSSPrimitiveValue::UnitTypes primitiveType() const
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

static const CalculationCategory addSubtractResult[CalcOther][CalcOther] = {
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

    if (leftCategory == CalcOther || rightCategory == CalcOther)
        return CalcOther;

    switch (op) {
    case CalcAdd:
    case CalcSubtract:
        return addSubtractResult[leftCategory][rightCategory];
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

static bool isIntegerResult(const CSSCalcExpressionNode* leftSide, const CSSCalcExpressionNode* rightSide, CalcOperator op)
{
    // Performs W3C spec's type checking for calc integers.
    // http://www.w3.org/TR/css3-values/#calc-type-checking
    return op != CalcDivide && leftSide->isInteger() && rightSide->isInteger();
}

class CSSCalcBinaryOperation : public CSSCalcExpressionNode {

public:
    static PassRefPtr<CSSCalcExpressionNode> create(PassRefPtr<CSSCalcExpressionNode> leftSide, PassRefPtr<CSSCalcExpressionNode> rightSide, CalcOperator op)
    {
        ASSERT(leftSide->category() != CalcOther && rightSide->category() != CalcOther);

        CalculationCategory newCategory = determineCategory(*leftSide, *rightSide, op);

        if (newCategory == CalcOther)
            return 0;

        return adoptRef(new CSSCalcBinaryOperation(leftSide, rightSide, op, newCategory));
    }

    static PassRefPtr<CSSCalcExpressionNode> createSimplified(PassRefPtr<CSSCalcExpressionNode> leftSide, PassRefPtr<CSSCalcExpressionNode> rightSide, CalcOperator op)
    {
        CalculationCategory leftCategory = leftSide->category();
        CalculationCategory rightCategory = rightSide->category();
        ASSERT(leftCategory != CalcOther && rightCategory != CalcOther);

        bool isInteger = isIntegerResult(leftSide.get(), rightSide.get(), op);

        // Simplify numbers.
        if (leftCategory == CalcNumber && rightCategory == CalcNumber) {
            CSSPrimitiveValue::UnitTypes evaluationType = isInteger ? CSSPrimitiveValue::CSS_PARSER_INTEGER : CSSPrimitiveValue::CSS_NUMBER;
            return CSSCalcPrimitiveValue::create(evaluateOperator(leftSide->doubleValue(), rightSide->doubleValue(), op), evaluationType, isInteger);
        }

        // Simplify addition and subtraction between same types.
        if (op == CalcAdd || op == CalcSubtract) {
            if (leftCategory == rightSide->category()) {
                CSSPrimitiveValue::UnitTypes leftType = leftSide->primitiveType();
                if (hasDoubleValue(leftType)) {
                    CSSPrimitiveValue::UnitTypes rightType = rightSide->primitiveType();
                    if (leftType == rightType)
                        return CSSCalcPrimitiveValue::create(evaluateOperator(leftSide->doubleValue(), rightSide->doubleValue(), op), leftType, isInteger);
                    CSSPrimitiveValue::UnitCategory leftUnitCategory = CSSPrimitiveValue::unitCategory(leftType);
                    if (leftUnitCategory != CSSPrimitiveValue::UOther && leftUnitCategory == CSSPrimitiveValue::unitCategory(rightType)) {
                        CSSPrimitiveValue::UnitTypes canonicalType = CSSPrimitiveValue::canonicalUnitTypeForCategory(leftUnitCategory);
                        if (canonicalType != CSSPrimitiveValue::CSS_UNKNOWN) {
                            double leftValue = leftSide->doubleValue() * CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(leftType);
                            double rightValue = rightSide->doubleValue() * CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(rightType);
                            return CSSCalcPrimitiveValue::create(evaluateOperator(leftValue, rightValue, op), canonicalType, isInteger);
                        }
                    }
                }
            }
        } else {
            // Simplify multiplying or dividing by a number for simplifiable types.
            ASSERT(op == CalcMultiply || op == CalcDivide);
            CSSCalcExpressionNode* numberSide = getNumberSide(leftSide.get(), rightSide.get());
            if (!numberSide)
                return create(leftSide, rightSide, op);
            if (numberSide == leftSide && op == CalcDivide)
                return 0;
            CSSCalcExpressionNode* otherSide = leftSide == numberSide ? rightSide.get() : leftSide.get();

            double number = numberSide->doubleValue();
            if (std::isnan(number) || std::isinf(number))
                return 0;
            if (op == CalcDivide && !number)
                return 0;

            CSSPrimitiveValue::UnitTypes otherType = otherSide->primitiveType();
            if (hasDoubleValue(otherType))
                return CSSCalcPrimitiveValue::create(evaluateOperator(otherSide->doubleValue(), number, op), otherType, isInteger);
        }

        return create(leftSide, rightSide, op);
    }

    virtual bool isZero() const
    {
        return !doubleValue();
    }

    virtual std::unique_ptr<CalcExpressionNode> toCalcValue(const RenderStyle* style, const RenderStyle* rootStyle, double zoom) const
    {
        std::unique_ptr<CalcExpressionNode> left(m_leftSide->toCalcValue(style, rootStyle, zoom));
        if (!left)
            return nullptr;
        std::unique_ptr<CalcExpressionNode> right(m_rightSide->toCalcValue(style, rootStyle, zoom));
        if (!right)
            return nullptr;
        return std::make_unique<CalcExpressionBinaryOperation>(std::move(left), std::move(right), m_operator);
    }

    virtual double doubleValue() const
    {
        return evaluate(m_leftSide->doubleValue(), m_rightSide->doubleValue());
    }

    virtual double computeLengthPx(const RenderStyle* currentStyle, const RenderStyle* rootStyle, double multiplier, bool computingFontSize) const
    {
        const double leftValue = m_leftSide->computeLengthPx(currentStyle, rootStyle, multiplier, computingFontSize);
        const double rightValue = m_rightSide->computeLengthPx(currentStyle, rootStyle, multiplier, computingFontSize);
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

    virtual String customCSSText() const
    {
        return buildCssText(m_leftSide->customCSSText(), m_rightSide->customCSSText(), m_operator);
    }


    virtual bool equals(const CSSCalcExpressionNode& exp) const
    {
        if (type() != exp.type())
            return false;

        const CSSCalcBinaryOperation& other = static_cast<const CSSCalcBinaryOperation&>(exp);
        return compareCSSValuePtr(m_leftSide, other.m_leftSide)
            && compareCSSValuePtr(m_rightSide, other.m_rightSide)
            && m_operator == other.m_operator;
    }

    virtual Type type() const { return CssCalcBinaryOperation; }

    virtual CSSPrimitiveValue::UnitTypes primitiveType() const
    {
        switch (m_category) {
        case CalcNumber:
            ASSERT(m_leftSide->category() == CalcNumber && m_rightSide->category() == CalcNumber);
            if (m_isInteger)
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
        case CalcPercentLength:
        case CalcPercentNumber:
        case CalcOther:
            return CSSPrimitiveValue::CSS_UNKNOWN;
        }
        ASSERT_NOT_REACHED();
        return CSSPrimitiveValue::CSS_UNKNOWN;
    }

private:
    CSSCalcBinaryOperation(PassRefPtr<CSSCalcExpressionNode> leftSide, PassRefPtr<CSSCalcExpressionNode> rightSide, CalcOperator op, CalculationCategory category)
        : CSSCalcExpressionNode(category, isIntegerResult(leftSide.get(), rightSide.get(), op))
        , m_leftSide(leftSide)
        , m_rightSide(rightSide)
        , m_operator(op)
    {
    }

    static CSSCalcExpressionNode* getNumberSide(CSSCalcExpressionNode* leftSide, CSSCalcExpressionNode* rightSide)
    {
        if (leftSide->category() == CalcNumber)
            return leftSide;
        if (rightSide->category() == CalcNumber)
            return rightSide;
        return 0;
    }

    double evaluate(double leftSide, double rightSide) const
    {
        return evaluateOperator(leftSide, rightSide, m_operator);
    }

    static double evaluateOperator(double leftValue, double rightValue, CalcOperator op)
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
            return 0;
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

            result->value = CSSCalcBinaryOperation::createSimplified(result->value, rhs.value, static_cast<CalcOperator>(operatorCharacter));
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

            result->value = CSSCalcBinaryOperation::createSimplified(result->value, rhs.value, static_cast<CalcOperator>(operatorCharacter));
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

PassRefPtr<CSSCalcExpressionNode> CSSCalcValue::createExpressionNode(PassRefPtr<CSSPrimitiveValue> value, bool isInteger)
{
    return CSSCalcPrimitiveValue::create(value, isInteger);
}

PassRefPtr<CSSCalcExpressionNode> CSSCalcValue::createExpressionNode(PassRefPtr<CSSCalcExpressionNode> leftSide, PassRefPtr<CSSCalcExpressionNode> rightSide, CalcOperator op)
{
    return CSSCalcBinaryOperation::create(leftSide, rightSide, op);
}

PassRefPtr<CSSCalcExpressionNode> CSSCalcValue::createExpressionNode(const CalcExpressionNode* node, const RenderStyle* style)
{
    switch (node->type()) {
    case CalcExpressionNodeNumber: {
        float value = toCalcExpressionNumber(node)->value();
        return createExpressionNode(CSSPrimitiveValue::create(value, CSSPrimitiveValue::CSS_NUMBER), value == trunc(value));
    }
    case CalcExpressionNodeLength:
        return createExpressionNode(toCalcExpressionLength(node)->length(), style);
    case CalcExpressionNodeBinaryOperation: {
        const CalcExpressionBinaryOperation* binaryNode = toCalcExpressionBinaryOperation(node);
        return createExpressionNode(createExpressionNode(binaryNode->leftSide(), style), createExpressionNode(binaryNode->rightSide(), style), binaryNode->getOperator());
    }
    case CalcExpressionNodeBlendLength: {
        // FIXME: (http://webkit.org/b/122036) Create a CSSCalcExpressionNode equivalent of CalcExpressionBlendLength.
        const CalcExpressionBlendLength* blendNode = toCalcExpressionBlendLength(node);
        const double progress = blendNode->progress();
        const bool isInteger = !progress || (progress == 1);
        return createExpressionNode(
            createExpressionNode(
                createExpressionNode(blendNode->from(), style),
                createExpressionNode(CSSPrimitiveValue::create(1 - progress, CSSPrimitiveValue::CSS_NUMBER), isInteger),
                CalcMultiply),
            createExpressionNode(
                createExpressionNode(blendNode->to(), style),
                createExpressionNode(CSSPrimitiveValue::create(progress, CSSPrimitiveValue::CSS_NUMBER), isInteger),
                CalcMultiply),
            CalcAdd);
    }
    case CalcExpressionNodeUndefined:
        ASSERT_NOT_REACHED();
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

PassRefPtr<CSSCalcExpressionNode> CSSCalcValue::createExpressionNode(const Length& length, const RenderStyle* style)
{
    switch (length.type()) {
    case Percent:
    case ViewportPercentageWidth:
    case ViewportPercentageHeight:
    case ViewportPercentageMin:
    case ViewportPercentageMax:
    case Fixed:
        return createExpressionNode(CSSPrimitiveValue::create(length, style), length.value() == trunc(length.value()));
    case Calculated:
        return createExpressionNode(length.calculationValue()->expression(), style);
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
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

PassRefPtr<CSSCalcValue> CSSCalcValue::create(CSSParserString name, CSSParserValueList* parserValueList, CalculationPermittedValueRange range)
{
    CSSCalcExpressionNodeParser parser;
    RefPtr<CSSCalcExpressionNode> expression;

    if (equalIgnoringCase(name, "calc(") || equalIgnoringCase(name, "-webkit-calc("))
        expression = parser.parseCalc(parserValueList);
    // FIXME: calc (http://webkit.org/b/16662) Add parsing for min and max here

    return expression ? adoptRef(new CSSCalcValue(expression, range)) : 0;
}

PassRef<CSSCalcValue> CSSCalcValue::create(PassRefPtr<CSSCalcExpressionNode> expression, CalculationPermittedValueRange range)
{
    return adoptRef(*new CSSCalcValue(expression, range));
}

} // namespace WebCore
