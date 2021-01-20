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

#include <wtf/text/StringBuilder.h>

namespace WebCore {

String CSSCubicBezierTimingFunctionValue::customCSSText() const
{
    return makeString("cubic-bezier(", m_x1, ", ", m_y1, ", ", m_x2, ", ", m_y2, ')');
}

bool CSSCubicBezierTimingFunctionValue::equals(const CSSCubicBezierTimingFunctionValue& other) const
{
    return m_x1 == other.m_x1 && m_x2 == other.m_x2 && m_y1 == other.m_y1 && m_y2 == other.m_y2;
}

String CSSStepsTimingFunctionValue::customCSSText() const
{
    StringBuilder builder;
    builder.appendLiteral("steps(");
    builder.appendNumber(m_steps);
    if (m_stepPosition) {
        switch (m_stepPosition.value()) {
        case StepsTimingFunction::StepPosition::JumpStart:
            builder.appendLiteral(", jump-start");
            break;

        case StepsTimingFunction::StepPosition::JumpNone:
            builder.appendLiteral(", jump-none");
            break;

        case StepsTimingFunction::StepPosition::JumpBoth:
            builder.appendLiteral(", jump-both");
            break;

        case StepsTimingFunction::StepPosition::Start:
            builder.appendLiteral(", start");
            break;

        case StepsTimingFunction::StepPosition::JumpEnd:
        case StepsTimingFunction::StepPosition::End:
            break;
        }
    }
    builder.appendLiteral(")");
    return builder.toString();
}

bool CSSStepsTimingFunctionValue::equals(const CSSStepsTimingFunctionValue& other) const
{
    return m_steps == other.m_steps && m_stepPosition == other.m_stepPosition;
}

String CSSSpringTimingFunctionValue::customCSSText() const
{
    StringBuilder builder;
    builder.appendLiteral("spring(");
    builder.append(FormattedNumber::fixedPrecision(m_mass));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(m_stiffness));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(m_damping));
    builder.append(' ');
    builder.append(FormattedNumber::fixedPrecision(m_initialVelocity));
    builder.append(')');
    return builder.toString();
}

bool CSSSpringTimingFunctionValue::equals(const CSSSpringTimingFunctionValue& other) const
{
    return m_mass == other.m_mass && m_stiffness == other.m_stiffness && m_damping == other.m_damping && m_initialVelocity == other.m_initialVelocity;
}


} // namespace WebCore
