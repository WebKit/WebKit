/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "CSSValue.h"
#include "ExceptionOr.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class TimingFunction : public RefCounted<TimingFunction> {
public:
    virtual Ref<TimingFunction> clone() const = 0;

    virtual ~TimingFunction() = default;

    enum TimingFunctionType { LinearFunction, CubicBezierFunction, StepsFunction, SpringFunction };
    TimingFunctionType type() const { return m_type; }

    bool isLinearTimingFunction() const { return m_type == LinearFunction; }
    bool isCubicBezierTimingFunction() const { return m_type == CubicBezierFunction; }
    bool isStepsTimingFunction() const { return m_type == StepsFunction; }
    bool isSpringTimingFunction() const { return m_type == SpringFunction; }

    virtual bool operator==(const TimingFunction&) const = 0;
    bool operator!=(const TimingFunction& other) const { return !(*this == other); }

    static ExceptionOr<RefPtr<TimingFunction>> createFromCSSText(const String&);
    static RefPtr<TimingFunction> createFromCSSValue(const CSSValue&);
    double transformTime(double, double, bool before = false) const;
    String cssText() const;

protected:
    explicit TimingFunction(TimingFunctionType type)
        : m_type(type)
    {
    }

private:
    TimingFunctionType m_type;
};

class LinearTimingFunction final : public TimingFunction {
public:
    static Ref<LinearTimingFunction> create()
    {
        return adoptRef(*new LinearTimingFunction);
    }
    
    bool operator==(const TimingFunction& other) const final
    {
        return is<LinearTimingFunction>(other);
    }

private:
    LinearTimingFunction()
        : TimingFunction(LinearFunction)
    {
    }

    Ref<TimingFunction> clone() const final
    {
        return adoptRef(*new LinearTimingFunction);
    }
};

class CubicBezierTimingFunction final : public TimingFunction {
public:
    enum TimingFunctionPreset { Ease, EaseIn, EaseOut, EaseInOut, Custom };
    
    static Ref<CubicBezierTimingFunction> create(double x1, double y1, double x2, double y2)
    {
        return adoptRef(*new CubicBezierTimingFunction(Custom, x1, y1, x2, y2));
    }

    static Ref<CubicBezierTimingFunction> create()
    {
        return adoptRef(*new CubicBezierTimingFunction());
    }
    
    static Ref<CubicBezierTimingFunction> create(TimingFunctionPreset preset)
    {
        switch (preset) {
        case Ease:
            return adoptRef(*new CubicBezierTimingFunction);
        case EaseIn:
            return adoptRef(*new CubicBezierTimingFunction(EaseIn, 0.42, 0.0, 1.0, 1.0));
        case EaseOut:
            return adoptRef(*new CubicBezierTimingFunction(EaseOut, 0.0, 0.0, 0.58, 1.0));
        case EaseInOut:
            return adoptRef(*new CubicBezierTimingFunction(EaseInOut, 0.42, 0.0, 0.58, 1.0));
        case Custom:
            break;
        }
        ASSERT_NOT_REACHED();
        return adoptRef(*new CubicBezierTimingFunction);
    }

    bool operator==(const TimingFunction& other) const final
    {
        if (!is<CubicBezierTimingFunction>(other))
            return false;
        auto& otherCubic = downcast<CubicBezierTimingFunction>(other);
        if (m_timingFunctionPreset != otherCubic.m_timingFunctionPreset)
            return false;
        if (m_timingFunctionPreset != Custom)
            return true;
        return m_x1 == otherCubic.m_x1 && m_y1 == otherCubic.m_y1 && m_x2 == otherCubic.m_x2 && m_y2 == otherCubic.m_y2;
    }

    double x1() const { return m_x1; }
    double y1() const { return m_y1; }
    double x2() const { return m_x2; }
    double y2() const { return m_y2; }
    
    void setValues(double x1, double y1, double x2, double y2)
    {
        m_x1 = x1;
        m_y1 = y1;
        m_x2 = x2;
        m_y2 = y2;
    }

    TimingFunctionPreset timingFunctionPreset() const { return m_timingFunctionPreset; }
    void setTimingFunctionPreset(TimingFunctionPreset preset) { m_timingFunctionPreset = preset; }

    static const CubicBezierTimingFunction& defaultTimingFunction()
    {
        static const CubicBezierTimingFunction& function = create().leakRef();
        return function;
    }

    Ref<CubicBezierTimingFunction> createReversed() const
    {
        return create(1.0 - m_x2, 1.0 - m_y2, 1.0 - m_x1, 1.0 - m_y1);
    }

    bool isLinear() const
    {
        return (!m_x1 && !m_y1 && !m_x2 && !m_y2) || (m_x1 == 1.0 && m_y1 == 1.0 && m_x2 == 1.0 && m_y2 == 1.0) || (m_x1 == 0.0 && m_y1 == 0.0 && m_x2 == 1.0 && m_y2 == 1.0);
    }

private:
    explicit CubicBezierTimingFunction(TimingFunctionPreset preset = Ease, double x1 = 0.25, double y1 = 0.1, double x2 = 0.25, double y2 = 1.0)
        : TimingFunction(CubicBezierFunction)
        , m_x1(x1)
        , m_y1(y1)
        , m_x2(x2)
        , m_y2(y2)
        , m_timingFunctionPreset(preset)
    {
    }

    Ref<TimingFunction> clone() const final
    {
        return adoptRef(*new CubicBezierTimingFunction(m_timingFunctionPreset, m_x1, m_y1, m_x2, m_y2));
    }

    double m_x1;
    double m_y1;
    double m_x2;
    double m_y2;
    TimingFunctionPreset m_timingFunctionPreset;
};

class StepsTimingFunction final : public TimingFunction {
public:
    static Ref<StepsTimingFunction> create(int steps, bool stepAtStart)
    {
        return adoptRef(*new StepsTimingFunction(steps, stepAtStart));
    }
    static Ref<StepsTimingFunction> create()
    {
        return adoptRef(*new StepsTimingFunction(1, true));
    }
    
    bool operator==(const TimingFunction& other) const final
    {
        if (!is<StepsTimingFunction>(other))
            return false;
        auto& otherSteps = downcast<StepsTimingFunction>(other);
        return m_steps == otherSteps.m_steps && m_stepAtStart == otherSteps.m_stepAtStart;
    }
    
    int numberOfSteps() const { return m_steps; }
    void setNumberOfSteps(int steps) { m_steps = steps; }

    bool stepAtStart() const { return m_stepAtStart; }
    void setStepAtStart(bool stepAtStart) { m_stepAtStart = stepAtStart; }

private:
    StepsTimingFunction(int steps, bool stepAtStart)
        : TimingFunction(StepsFunction)
        , m_steps(steps)
        , m_stepAtStart(stepAtStart)
    {
    }

    Ref<TimingFunction> clone() const final
    {
        return adoptRef(*new StepsTimingFunction(m_steps, m_stepAtStart));
    }
    
    int m_steps;
    bool m_stepAtStart;
};

class SpringTimingFunction final : public TimingFunction {
public:
    static Ref<SpringTimingFunction> create(double mass, double stiffness, double damping, double initialVelocity)
    {
        return adoptRef(*new SpringTimingFunction(mass, stiffness, damping, initialVelocity));
    }

    static Ref<SpringTimingFunction> create()
    {
        // This create() function should only be used by the argument decoders, and it is expected that
        // real values will be filled in using setValues().
        return create(0, 0, 0, 0);
    }
    
    bool operator==(const TimingFunction& other) const final
    {
        if (!is<SpringTimingFunction>(other))
            return false;
        auto& otherSpring = downcast<SpringTimingFunction>(other);
        return m_mass == otherSpring.m_mass && m_stiffness == otherSpring.m_stiffness && m_damping == otherSpring.m_damping && m_initialVelocity == otherSpring.m_initialVelocity;
    }

    double mass() const { return m_mass; }
    double stiffness() const { return m_stiffness; }
    double damping() const { return m_damping; }
    double initialVelocity() const { return m_initialVelocity; }
    
    void setValues(double mass, double stiffness, double damping, double initialVelocity)
    {
        m_mass = mass;
        m_stiffness = stiffness;
        m_damping = damping;
        m_initialVelocity = initialVelocity;
    }

private:
    explicit SpringTimingFunction(double mass, double stiffness, double damping, double initialVelocity)
        : TimingFunction(SpringFunction)
        , m_mass(mass)
        , m_stiffness(stiffness)
        , m_damping(damping)
        , m_initialVelocity(initialVelocity)
    {
    }

    Ref<TimingFunction> clone() const final
    {
        return adoptRef(*new SpringTimingFunction(m_mass, m_stiffness, m_damping, m_initialVelocity));
    }

    double m_mass;
    double m_stiffness;
    double m_damping;
    double m_initialVelocity;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const TimingFunction&);

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_TIMINGFUNCTION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
static bool isType(const WebCore::TimingFunction& function) { return function.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_TIMINGFUNCTION(WebCore::LinearTimingFunction, isLinearTimingFunction())
SPECIALIZE_TYPE_TRAITS_TIMINGFUNCTION(WebCore::CubicBezierTimingFunction, isCubicBezierTimingFunction())
SPECIALIZE_TYPE_TRAITS_TIMINGFUNCTION(WebCore::StepsTimingFunction, isStepsTimingFunction())
SPECIALIZE_TYPE_TRAITS_TIMINGFUNCTION(WebCore::SpringTimingFunction, isSpringTimingFunction())
