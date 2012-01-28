/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "CSSStyleSelector.h"
#include "CSSValueList.h"
#include "Length.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
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
        return CalcLength;
    default:
        return CalcOther;
    }
}
    
String CSSCalcValue::customCssText() const
{
    return "";
}
    
CSSCalcExpressionNode::~CSSCalcExpressionNode() 
{
}
    
class CSSCalcPrimitiveValue : public CSSCalcExpressionNode {
public:

    static PassRefPtr<CSSCalcPrimitiveValue> create(CSSPrimitiveValue* value, bool isInteger)
    {
        return adoptRef(new CSSCalcPrimitiveValue(value, isInteger));
    }
    
    virtual String cssText() const
    {
        return m_value->cssText();
    }

    
private:
    explicit CSSCalcPrimitiveValue(CSSPrimitiveValue* value, bool isInteger)
        : CSSCalcExpressionNode(unitCategory((CSSPrimitiveValue::UnitTypes)value->primitiveType()), isInteger)
        , m_value(value)                 
    {
    }

    RefPtr<CSSPrimitiveValue> m_value;
};

static const CalculationCategory addSubtractResult[CalcOther][CalcOther] = {
    { CalcNumber,        CalcOther,         CalcPercentNumber, CalcPercentNumber, CalcOther },
    { CalcOther,         CalcLength,        CalcPercentLength, CalcOther,         CalcPercentLength },
    { CalcPercentNumber, CalcPercentLength, CalcPercent,       CalcPercentNumber, CalcPercentLength },
    { CalcPercentNumber, CalcOther,         CalcPercentNumber, CalcPercentNumber, CalcOther },
    { CalcOther,         CalcPercentLength, CalcPercentLength, CalcOther,         CalcPercentLength },
};    
    
class CSSCalcBinaryOperation : public CSSCalcExpressionNode {
public:
    static PassRefPtr<CSSCalcBinaryOperation> create(PassRefPtr<CSSCalcExpressionNode> leftSide, PassRefPtr<CSSCalcExpressionNode> rightSide, CalcOperator op)
    {
        CalculationCategory leftCategory = leftSide->category();
        CalculationCategory rightCategory = rightSide->category();
        CalculationCategory newCategory = CalcOther;
        
        ASSERT(leftCategory != CalcOther && rightCategory != CalcOther);
        
        switch (op) {
        case CalcAdd:
        case CalcSubtract:             
            if (leftCategory == CalcOther || rightCategory == CalcOther)
                return 0;
            newCategory = addSubtractResult[leftCategory][rightCategory];
            break;   
                
        case CalcMultiply:
            if (leftCategory != CalcNumber && rightCategory != CalcNumber) 
                return 0;
            
            newCategory = leftCategory == CalcNumber ? rightCategory : leftCategory;
            break;
                
        case CalcDivide:
        case CalcMod:
            if (rightCategory != CalcNumber || rightSide->isZero())
                return 0;
            newCategory = leftCategory;
            break;
        }
        
        if (newCategory == CalcOther)
            return 0;
            
        return adoptRef(new CSSCalcBinaryOperation(leftSide, rightSide, op, newCategory));
    }
    
private:
    CSSCalcBinaryOperation(PassRefPtr<CSSCalcExpressionNode> leftSide, PassRefPtr<CSSCalcExpressionNode> rightSide, CalcOperator op, CalculationCategory category)
        : CSSCalcExpressionNode(category, leftSide->isInteger() && rightSide->isInteger())
        , m_leftSide(leftSide)
        , m_rightSide(rightSide)
        , m_operator(op)
    {
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
        ASSERT(index <= tokens->size());
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

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value.get());
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
            if (operatorCharacter != CalcMultiply && operatorCharacter != CalcDivide && operatorCharacter != CalcMod)
                break;
            ++*index;

            Value rhs;
            if (!parseValueTerm(tokens, depth, index, &rhs))
                return false;

            result->value = CSSCalcBinaryOperation::create(result->value, rhs.value, static_cast<CalcOperator>(operatorCharacter));
            if (!result->value)
                return false;
        }

        ASSERT(*index <= tokens->size());
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

            result->value = CSSCalcBinaryOperation::create(result->value, rhs.value, static_cast<CalcOperator>(operatorCharacter));
            if (!result->value)
                return false;
        }

        ASSERT(*index <= tokens->size());
        return true;
    }

    bool parseValueExpression(CSSParserValueList* tokens, int depth, unsigned* index, Value* result)
    {
        return parseAdditiveValueExpression(tokens, depth, index, result);
    }
};

PassRefPtr<CSSCalcValue> CSSCalcValue::create(CSSParserString name, CSSParserValueList* parserValueList)
{    
    CSSCalcExpressionNodeParser parser;    
    RefPtr<CSSCalcExpressionNode> expression;
    
    if (equalIgnoringCase(name, "-webkit-calc("))
        expression = parser.parseCalc(parserValueList);    
    // FIXME calc (http://webkit.org/b/16662) Add parsing for min and max here

    return expression ? adoptRef(new CSSCalcValue(expression)) : 0;
}

} // namespace WebCore
