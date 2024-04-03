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

#include "config.h"
#include "CalcExpressionOperation.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

float CalcExpressionOperation::evaluate(float maxValue) const
{
    return evaluateCalcExpression(m_operator, m_children, [&](auto& child) {
        return child->evaluate(maxValue);
    });
}

bool CalcExpressionOperation::operator==(const CalcExpressionNode& other) const
{
    auto* otherExpressionOperation = dynamicDowncast<CalcExpressionOperation>(other);
    return otherExpressionOperation && *this == *otherExpressionOperation;
}

bool operator==(const CalcExpressionOperation& a, const CalcExpressionOperation& b)
{
    if (a.getOperator() != b.getOperator() || a.destinationCategory() != b.destinationCategory())
        return false;
    if (a.children().size() != b.children().size())
        return false;
    for (unsigned i = 0; i < a.children().size(); ++i) {
        if (!(*a.children()[i] == *b.children()[i]))
            return false;
    }
    return true;
}

void CalcExpressionOperation::dump(TextStream& ts) const
{
    if (m_operator == CalcOperator::Min || m_operator == CalcOperator::Max) {
        ts << m_operator << "(";
        size_t childrenCount = m_children.size();
        for (size_t i = 0; i < childrenCount; i++) {
            ts << m_children[i].get();
            if (i < childrenCount - 1)
                ts << ", ";
        }
        ts << ")";
    } else
        ts << m_children[0].get() << " " << m_operator << " " << m_children[1].get();
}

}
