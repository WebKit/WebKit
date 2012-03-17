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

#ifndef CalculationValue_h
#define CalculationValue_h

#include "Length.h"
#include "LengthFunctions.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

enum CalcOperator {
    CalcAdd = '+',
    CalcSubtract = '-',
    CalcMultiply = '*',
    CalcDivide = '/'
};

enum CalculationPermittedValueRange {
    CalculationRangeAll,
    CalculationRangeNonNegative
};
        
class CalcExpressionNode {
public:
    virtual ~CalcExpressionNode()
    {
    }
    
    virtual float evaluate(float maxValue) const = 0;
};
    
class CalculationValue : public RefCounted<CalculationValue> {
public:
    static PassRefPtr<CalculationValue> create(PassOwnPtr<CalcExpressionNode> value, CalculationPermittedValueRange);
    float evaluate(float maxValue) const;
    
private:
    CalculationValue(PassOwnPtr<CalcExpressionNode> value, CalculationPermittedValueRange range)
        : m_value(value)
        , m_isNonNegative(range == CalculationRangeNonNegative)
    {
    }
    
    OwnPtr<CalcExpressionNode> m_value;
    bool m_isNonNegative;
};

class CalcExpressionNumber : public CalcExpressionNode {
public:
    explicit CalcExpressionNumber(float value)
        : m_value(value)
    {
    }

    virtual float evaluate(float) const 
    {
        return m_value;
    }
    
private:
    float m_value;
};

class CalcExpressionLength : public CalcExpressionNode {
public:
    explicit CalcExpressionLength(Length length)
        : m_length(length)
    {
    }

    virtual float evaluate(float maxValue) const
    {
        return floatValueForLength(m_length, maxValue);
    }
    
private:
    Length m_length;
};

class CalcExpressionBinaryOperation : public CalcExpressionNode {
public:
    CalcExpressionBinaryOperation(PassOwnPtr<CalcExpressionNode> leftSide, PassOwnPtr<CalcExpressionNode> rightSide, CalcOperator op)
        : m_leftSide(leftSide)
        , m_rightSide(rightSide)
        , m_operator(op)
    {
    }

    virtual float evaluate(float) const;

private:
    OwnPtr<CalcExpressionNode> m_leftSide;
    OwnPtr<CalcExpressionNode> m_rightSide;
    CalcOperator m_operator;
};

} // namespace WebCore

#endif // CalculationValue_h
