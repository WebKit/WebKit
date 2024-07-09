/*
 * Copyright (C) 2007, 2013, 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSTimingFunctionValue.h"

#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

String CSSLinearTimingFunctionValue::customCSSText() const
{
    if (m_points.isEmpty())
        return "linear"_s;

    StringBuilder builder;
    builder.append("linear("_s);
    for (size_t i = 0; i < m_points.size(); ++i) {
        if (i)
            builder.append(", "_s);

        const auto& point = m_points[i];
        builder.append(FormattedNumber::fixedPrecision(point.value), ' ', FormattedNumber::fixedPrecision(point.progress * 100.0), '%');
    }
    builder.append(')');
    return builder.toString();
}

bool CSSLinearTimingFunctionValue::equals(const CSSLinearTimingFunctionValue& other) const
{
    return m_points == other.m_points;
}

String CSSCubicBezierTimingFunctionValue::customCSSText() const
{
    return makeString("cubic-bezier("_s, m_x1, ", "_s, m_y1, ", "_s, m_x2, ", "_s, m_y2, ')');
}

bool CSSCubicBezierTimingFunctionValue::equals(const CSSCubicBezierTimingFunctionValue& other) const
{
    return m_x1 == other.m_x1 && m_x2 == other.m_x2 && m_y1 == other.m_y1 && m_y2 == other.m_y2;
}

String CSSStepsTimingFunctionValue::customCSSText() const
{
    ASCIILiteral position = ""_s;
    if (m_stepPosition) {
        switch (m_stepPosition.value()) {
        case StepsTimingFunction::StepPosition::JumpStart:
            position = ", jump-start"_s;
            break;
        case StepsTimingFunction::StepPosition::JumpNone:
            position = ", jump-none"_s;
            break;
        case StepsTimingFunction::StepPosition::JumpBoth:
            position = ", jump-both"_s;
            break;
        case StepsTimingFunction::StepPosition::Start:
            position = ", start"_s;
            break;
        case StepsTimingFunction::StepPosition::JumpEnd:
        case StepsTimingFunction::StepPosition::End:
            break;
        }
    }
    return makeString("steps("_s, m_steps, position, ')');
}

bool CSSStepsTimingFunctionValue::equals(const CSSStepsTimingFunctionValue& other) const
{
    return m_steps == other.m_steps && m_stepPosition == other.m_stepPosition;
}

String CSSSpringTimingFunctionValue::customCSSText() const
{
    return makeString("spring("_s, FormattedNumber::fixedPrecision(m_mass), ' ', FormattedNumber::fixedPrecision(m_stiffness), ' ', FormattedNumber::fixedPrecision(m_damping), ' ', FormattedNumber::fixedPrecision(m_initialVelocity), ')');
}

bool CSSSpringTimingFunctionValue::equals(const CSSSpringTimingFunctionValue& other) const
{
    return m_mass == other.m_mass && m_stiffness == other.m_stiffness && m_damping == other.m_damping && m_initialVelocity == other.m_initialVelocity;
}

} // namespace WebCore
