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

void CSSLinearTimingFunctionValue::customCSSText(StringBuilder& builder) const
{
    if (m_points.isEmpty()) {
        builder.append("linear"_s);
        return;
    }

    auto serializePoint = [](auto& builder, auto& point) {
        builder.append(FormattedNumber::fixedPrecision(point.value), ' ', FormattedNumber::fixedPrecision(point.progress * 100.0), '%');
    };

    builder.append("linear("_s, interleave(m_points, serializePoint, ", "_s), ')');
}

bool CSSLinearTimingFunctionValue::equals(const CSSLinearTimingFunctionValue& other) const
{
    return m_points == other.m_points;
}

String CSSCubicBezierTimingFunctionValue::customCSSText() const
{
    return makeString("cubic-bezier("_s, m_x1, ", "_s, m_y1, ", "_s, m_x2, ", "_s, m_y2, ')');
}

void CSSCubicBezierTimingFunctionValue::customCSSText(StringBuilder& builder) const
{
    builder.append("cubic-bezier("_s, m_x1, ", "_s, m_y1, ", "_s, m_x2, ", "_s, m_y2, ')');
}

bool CSSCubicBezierTimingFunctionValue::equals(const CSSCubicBezierTimingFunctionValue& other) const
{
    return m_x1 == other.m_x1 && m_x2 == other.m_x2 && m_y1 == other.m_y1 && m_y2 == other.m_y2;
}

static constexpr ASCIILiteral serializeStepPosition(std::optional<StepsTimingFunction::StepPosition> stepPosition)
{
    if (stepPosition) {
        switch (stepPosition.value()) {
        case StepsTimingFunction::StepPosition::JumpStart:
            return ", jump-start"_s;
        case StepsTimingFunction::StepPosition::JumpNone:
            return ", jump-none"_s;
        case StepsTimingFunction::StepPosition::JumpBoth:
            return  ", jump-both"_s;
        case StepsTimingFunction::StepPosition::Start:
            return ", start"_s;
        case StepsTimingFunction::StepPosition::JumpEnd:
        case StepsTimingFunction::StepPosition::End:
            break;
        }
    }

    return ""_s;
}

String CSSStepsTimingFunctionValue::customCSSText() const
{
    return makeString("steps("_s, m_steps, serializeStepPosition(m_stepPosition), ')');
}

void CSSStepsTimingFunctionValue::customCSSText(StringBuilder& builder) const
{
    builder.append("steps("_s, m_steps, serializeStepPosition(m_stepPosition), ')');
}

bool CSSStepsTimingFunctionValue::equals(const CSSStepsTimingFunctionValue& other) const
{
    return m_steps == other.m_steps && m_stepPosition == other.m_stepPosition;
}

String CSSSpringTimingFunctionValue::customCSSText() const
{
    return makeString("spring("_s, FormattedNumber::fixedPrecision(m_mass), ' ', FormattedNumber::fixedPrecision(m_stiffness), ' ', FormattedNumber::fixedPrecision(m_damping), ' ', FormattedNumber::fixedPrecision(m_initialVelocity), ')');
}

void CSSSpringTimingFunctionValue::customCSSText(StringBuilder& builder) const
{
    builder.append("spring("_s, FormattedNumber::fixedPrecision(m_mass), ' ', FormattedNumber::fixedPrecision(m_stiffness), ' ', FormattedNumber::fixedPrecision(m_damping), ' ', FormattedNumber::fixedPrecision(m_initialVelocity), ')');
}

bool CSSSpringTimingFunctionValue::equals(const CSSSpringTimingFunctionValue& other) const
{
    return m_mass == other.m_mass && m_stiffness == other.m_stiffness && m_damping == other.m_damping && m_initialVelocity == other.m_initialVelocity;
}

} // namespace WebCore
