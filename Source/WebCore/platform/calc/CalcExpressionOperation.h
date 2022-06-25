/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CalcExpressionNode.h"
#include "CalcOperator.h"
#include "CalculationCategory.h"
#include <memory>
#include <wtf/Vector.h>

namespace WebCore {

class CalcExpressionOperation final : public CalcExpressionNode {
public:
    CalcExpressionOperation(Vector<std::unique_ptr<CalcExpressionNode>>&& children, CalcOperator, CalculationCategory destinationCategory = CalculationCategory::Other);

    CalcOperator getOperator() const { return m_operator; }
    CalculationCategory destinationCategory() const { return m_destinationCategory; }

    const Vector<std::unique_ptr<CalcExpressionNode>>& children() const { return m_children; }

private:
    float evaluate(float maxValue) const final;
    bool operator==(const CalcExpressionNode&) const final;
    void dump(TextStream&) const final;

    Vector<std::unique_ptr<CalcExpressionNode>> m_children;
    CalcOperator m_operator;
    CalculationCategory m_destinationCategory { CalculationCategory::Other };
};


inline CalcExpressionOperation::CalcExpressionOperation(Vector<std::unique_ptr<CalcExpressionNode>>&& children, CalcOperator op, CalculationCategory destinationCategory)
    : CalcExpressionNode(CalcExpressionNodeType::Operation)
    , m_children(WTFMove(children))
    , m_operator(op)
    , m_destinationCategory(destinationCategory)
{
}

bool operator==(const CalcExpressionOperation&, const CalcExpressionOperation&);

}

SPECIALIZE_TYPE_TRAITS_CALCEXPRESSION_NODE(CalcExpressionOperation, type() == WebCore::CalcExpressionNodeType::Operation)
