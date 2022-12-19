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
#include "CalcExpressionBlendLength.h"

#include "CalculationValue.h"
#include "LengthFunctions.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

CalcExpressionBlendLength::CalcExpressionBlendLength(Length from, Length to, double progress)
    : CalcExpressionNode(CalcExpressionNodeType::BlendLength)
    , m_from(from)
    , m_to(to)
    , m_progress(progress)
{
    // Flatten nesting of CalcExpressionBlendLength as a speculative fix for rdar://problem/30533005.
    // CalcExpressionBlendLength is only used as a result of animation and they don't nest in normal cases.
    if (m_from.isCalculated() && m_from.calculationValue().expression().type() == CalcExpressionNodeType::BlendLength)
        m_from = downcast<CalcExpressionBlendLength>(m_from.calculationValue().expression()).from();
    if (m_to.isCalculated() && m_to.calculationValue().expression().type() == CalcExpressionNodeType::BlendLength)
        m_to = downcast<CalcExpressionBlendLength>(m_to.calculationValue().expression()).to();
}

float CalcExpressionBlendLength::evaluate(float maxValue) const
{
    return (1.0 - m_progress) * floatValueForLength(m_from, maxValue) + m_progress * floatValueForLength(m_to, maxValue);
}

bool CalcExpressionBlendLength::operator==(const CalcExpressionNode& other) const
{
    return is<CalcExpressionBlendLength>(other) && *this == downcast<CalcExpressionBlendLength>(other);
}

void CalcExpressionBlendLength::dump(TextStream& ts) const
{
    ts << "blend(" << m_from << ", " << m_to << ", " << m_progress << ")";
}

}
