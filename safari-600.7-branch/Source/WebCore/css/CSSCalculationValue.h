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

#ifndef CSSCalculationValue_h
#define CSSCalculationValue_h

#include "CSSParserValues.h"
#include "CSSPrimitiveValue.h"
#include "CalculationValue.h"

namespace WebCore {

class CSSToLengthConversionData;
class RenderStyle;

enum CalculationCategory {
    CalcNumber = 0,
    CalcLength,
    CalcPercent,
    CalcPercentNumber,
    CalcPercentLength,
    CalcAngle,
    CalcTime,
    CalcFrequency,
    CalcOther
};

class CSSCalcExpressionNode : public RefCounted<CSSCalcExpressionNode> {
public:
    enum Type {
        CssCalcPrimitiveValue = 1,
        CssCalcBinaryOperation
    };

    virtual ~CSSCalcExpressionNode() { }
    virtual bool isZero() const = 0;
    virtual std::unique_ptr<CalcExpressionNode> createCalcExpression(const CSSToLengthConversionData&) const = 0;
    virtual double doubleValue() const = 0;
    virtual double computeLengthPx(const CSSToLengthConversionData&) const = 0;
    virtual String customCSSText() const = 0;
    virtual bool equals(const CSSCalcExpressionNode& other) const { return m_category == other.m_category && m_isInteger == other.m_isInteger; }
    virtual Type type() const = 0;
    virtual CSSPrimitiveValue::UnitTypes primitiveType() const = 0;

    CalculationCategory category() const { return m_category; }
    bool isInteger() const { return m_isInteger; }

protected:
    CSSCalcExpressionNode(CalculationCategory category, bool isInteger)
        : m_category(category)
        , m_isInteger(isInteger)
    {
    }

private:
    CalculationCategory m_category;
    bool m_isInteger;
};

class CSSCalcValue : public CSSValue {
public:
    static PassRefPtr<CSSCalcValue> create(CSSParserString name, CSSParserValueList& arguments, CalculationPermittedValueRange);
    static PassRefPtr<CSSCalcValue> create(const CalculationValue&, const RenderStyle&);

    CalculationCategory category() const { return m_expression->category(); }
    bool isInt() const { return m_expression->isInteger(); }
    double doubleValue() const;
    bool isNegative() const { return m_expression->doubleValue() < 0; }
    double computeLengthPx(const CSSToLengthConversionData&) const;

    PassRef<CalculationValue> createCalculationValue(const CSSToLengthConversionData&) const;

    String customCSSText() const;
    bool equals(const CSSCalcValue&) const;

private:
    CSSCalcValue(PassRef<CSSCalcExpressionNode>, bool shouldClampToNonNegative);

    double clampToPermittedRange(double) const;

    const Ref<CSSCalcExpressionNode> m_expression;
    const bool m_shouldClampToNonNegative;
};

inline CSSCalcValue::CSSCalcValue(PassRef<CSSCalcExpressionNode> expression, bool shouldClampToNonNegative)
    : CSSValue(CalculationClass)
    , m_expression(WTF::move(expression))
    , m_shouldClampToNonNegative(shouldClampToNonNegative)
{
}

inline PassRef<CalculationValue> CSSCalcValue::createCalculationValue(const CSSToLengthConversionData& conversionData) const
{
    return CalculationValue::create(m_expression->createCalcExpression(conversionData),
        m_shouldClampToNonNegative ? CalculationRangeNonNegative : CalculationRangeAll);
}

CSS_VALUE_TYPE_CASTS(CSSCalcValue, isCalcValue())

} // namespace WebCore

#endif // CSSCalculationValue_h
