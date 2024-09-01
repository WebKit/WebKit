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

// FIXME: This function needs CSSToLengthConversionData to properly resolve calc() values.
RefPtr<TimingFunction> createTimingFunction(const CSSValue&);

class CSSLinearTimingFunctionValue final : public CSSValue {
public:
    struct LinearStop {
        struct Length {
            Ref<CSSPrimitiveValue> input;       // <percentage>
            RefPtr<CSSPrimitiveValue> extra;    // <percentage>

            bool operator==(const Length&) const;
        };

        Ref<CSSPrimitiveValue> output;          // <number>
        std::optional<Length> input;

        bool operator==(const LinearStop&) const;
    };

    static Ref<CSSLinearTimingFunctionValue> create(Vector<LinearStop> stops)
    {
        return adoptRef(*new CSSLinearTimingFunctionValue(WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSLinearTimingFunctionValue&) const;

    // FIXME: This function needs CSSToLengthConversionData to properly resolve calc() values.
    RefPtr<TimingFunction> createTimingFunction() const;

private:
    CSSLinearTimingFunctionValue(Vector<LinearStop>&& stops)
        : CSSValue(LinearTimingFunctionClass)
        , m_stops(WTFMove(stops))
    {
        ASSERT(m_stops.size() >= 2);
    }

    Vector<LinearStop> m_stops;
};

class CSSCubicBezierTimingFunctionValue final : public CSSValue {
public:
    static Ref<CSSCubicBezierTimingFunctionValue> create(Ref<CSSPrimitiveValue> x1, Ref<CSSPrimitiveValue> y1, Ref<CSSPrimitiveValue> x2, Ref<CSSPrimitiveValue> y2)
    {
        return adoptRef(*new CSSCubicBezierTimingFunctionValue(WTFMove(x1), WTFMove(y1), WTFMove(x2), WTFMove(y2)));
    }

    String customCSSText() const;
    bool equals(const CSSCubicBezierTimingFunctionValue&) const;

    // FIXME: This function needs CSSToLengthConversionData to properly resolve calc() values.
    RefPtr<TimingFunction> createTimingFunction() const;

private:
    CSSCubicBezierTimingFunctionValue(Ref<CSSPrimitiveValue>&& x1, Ref<CSSPrimitiveValue>&& y1, Ref<CSSPrimitiveValue>&& x2, Ref<CSSPrimitiveValue>&& y2)
        : CSSValue(CubicBezierTimingFunctionClass)
        , m_x1(WTFMove(x1))
        , m_y1(WTFMove(y1))
        , m_x2(WTFMove(x2))
        , m_y2(WTFMove(y2))
    {
        ASSERT(m_x1->isNumber());
        ASSERT(m_y1->isNumber());
        ASSERT(m_x2->isNumber());
        ASSERT(m_y2->isNumber());
    }

    Ref<CSSPrimitiveValue> m_x1; // <number [0,1]>
    Ref<CSSPrimitiveValue> m_y1; // <number>
    Ref<CSSPrimitiveValue> m_x2; // <number [0,1]>
    Ref<CSSPrimitiveValue> m_y2; // <number>
};

class CSSStepsTimingFunctionValue final : public CSSValue {
public:
    static Ref<CSSStepsTimingFunctionValue> create(Ref<CSSPrimitiveValue> steps, std::optional<StepsTimingFunction::StepPosition> stepPosition)
    {
        return adoptRef(*new CSSStepsTimingFunctionValue(WTFMove(steps), stepPosition));
    }

    String customCSSText() const;
    bool equals(const CSSStepsTimingFunctionValue&) const;

    // FIXME: This function needs CSSToLengthConversionData to properly resolve calc() values.
    RefPtr<TimingFunction> createTimingFunction() const;

private:
    CSSStepsTimingFunctionValue(Ref<CSSPrimitiveValue>&& steps, std::optional<StepsTimingFunction::StepPosition> stepPosition)
        : CSSValue(StepsTimingFunctionClass)
        , m_steps(WTFMove(steps))
        , m_stepPosition(stepPosition)
    {
        ASSERT(m_steps->isInteger());
    }

    Ref<CSSPrimitiveValue> m_steps; // <integer [1,∞] or [2,∞]>
    std::optional<StepsTimingFunction::StepPosition> m_stepPosition;
};

class CSSSpringTimingFunctionValue final : public CSSValue {
public:
    static Ref<CSSSpringTimingFunctionValue> create(Ref<CSSPrimitiveValue> mass, Ref<CSSPrimitiveValue> stiffness, Ref<CSSPrimitiveValue> damping, Ref<CSSPrimitiveValue> initialVelocity)
    {
        return adoptRef(*new CSSSpringTimingFunctionValue(WTFMove(mass), WTFMove(stiffness), WTFMove(damping), WTFMove(initialVelocity)));
    }

    String customCSSText() const;
    bool equals(const CSSSpringTimingFunctionValue&) const;

    // FIXME: This function needs CSSToLengthConversionData to properly resolve calc() values.
    RefPtr<TimingFunction> createTimingFunction() const;

private:
    CSSSpringTimingFunctionValue(Ref<CSSPrimitiveValue>&& mass, Ref<CSSPrimitiveValue>&& stiffness, Ref<CSSPrimitiveValue>&& damping, Ref<CSSPrimitiveValue>&& initialVelocity)
        : CSSValue(SpringTimingFunctionClass)
        , m_mass(WTFMove(mass))
        , m_stiffness(WTFMove(stiffness))
        , m_damping(WTFMove(damping))
        , m_initialVelocity(WTFMove(initialVelocity))
    {
        ASSERT(m_mass->isNumber());
        ASSERT(m_stiffness->isNumber());
        ASSERT(m_damping->isNumber());
        ASSERT(m_initialVelocity->isNumber());
    }

    Ref<CSSPrimitiveValue> m_mass;              // <number [>0,∞]>
    Ref<CSSPrimitiveValue> m_stiffness;         // <number [>0,∞]>
    Ref<CSSPrimitiveValue> m_damping;           // <number [0,∞]>
    Ref<CSSPrimitiveValue> m_initialVelocity;   // <number>
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSLinearTimingFunctionValue, isLinearTimingFunctionValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCubicBezierTimingFunctionValue, isCubicBezierTimingFunctionValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSStepsTimingFunctionValue, isStepsTimingFunctionValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSSpringTimingFunctionValue, isSpringTimingFunctionValue())
