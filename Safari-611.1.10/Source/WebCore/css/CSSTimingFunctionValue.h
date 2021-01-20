/*
 * Copyright (C) 2007, 2008, 2012, 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSValue.h"
#include "TimingFunction.h"

namespace WebCore {

class CSSCubicBezierTimingFunctionValue final : public CSSValue {
public:
    static Ref<CSSCubicBezierTimingFunctionValue> create(double x1, double y1, double x2, double y2)
    {
        return adoptRef(*new CSSCubicBezierTimingFunctionValue(x1, y1, x2, y2));
    }

    String customCSSText() const;

    double x1() const { return m_x1; }
    double y1() const { return m_y1; }
    double x2() const { return m_x2; }
    double y2() const { return m_y2; }

    bool equals(const CSSCubicBezierTimingFunctionValue&) const;

private:
    CSSCubicBezierTimingFunctionValue(double x1, double y1, double x2, double y2)
        : CSSValue(CubicBezierTimingFunctionClass)
        , m_x1(x1)
        , m_y1(y1)
        , m_x2(x2)
        , m_y2(y2)
    {
    }

    double m_x1;
    double m_y1;
    double m_x2;
    double m_y2;
};

class CSSStepsTimingFunctionValue final : public CSSValue {
public:
    static Ref<CSSStepsTimingFunctionValue> create(int steps, Optional<StepsTimingFunction::StepPosition> stepPosition)
    {
        return adoptRef(*new CSSStepsTimingFunctionValue(steps, stepPosition));
    }

    int numberOfSteps() const { return m_steps; }
    Optional<StepsTimingFunction::StepPosition> stepPosition() const { return m_stepPosition; }

    String customCSSText() const;

    bool equals(const CSSStepsTimingFunctionValue&) const;

private:
    CSSStepsTimingFunctionValue(int steps, Optional<StepsTimingFunction::StepPosition> stepPosition)
        : CSSValue(StepsTimingFunctionClass)
        , m_steps(steps)
        , m_stepPosition(stepPosition)
    {
    }

    int m_steps;
    Optional<StepsTimingFunction::StepPosition> m_stepPosition;
};

class CSSSpringTimingFunctionValue final : public CSSValue {
public:
    static Ref<CSSSpringTimingFunctionValue> create(double mass, double stiffness, double damping, double initialVelocity)
    {
        return adoptRef(*new CSSSpringTimingFunctionValue(mass, stiffness, damping, initialVelocity));
    }

    double mass() const { return m_mass; }
    double stiffness() const { return m_stiffness; }
    double damping() const { return m_damping; }
    double initialVelocity() const { return m_initialVelocity; }

    String customCSSText() const;

    bool equals(const CSSSpringTimingFunctionValue&) const;

private:
    CSSSpringTimingFunctionValue(double mass, double stiffness, double damping, double initialVelocity)
        : CSSValue(SpringTimingFunctionClass)
        , m_mass(mass)
        , m_stiffness(stiffness)
        , m_damping(damping)
        , m_initialVelocity(initialVelocity)
    {
    }

    double m_mass;
    double m_stiffness;
    double m_damping;
    double m_initialVelocity;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCubicBezierTimingFunctionValue, isCubicBezierTimingFunctionValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSStepsTimingFunctionValue, isStepsTimingFunctionValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSSpringTimingFunctionValue, isSpringTimingFunctionValue())
