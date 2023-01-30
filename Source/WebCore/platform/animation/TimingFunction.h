/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
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
#include <wtf/EnumTraits.h>
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

    enum class Type { LinearFunction, CubicBezierFunction, StepsFunction, SpringFunction };
    virtual Type type() const = 0;

    bool isLinearTimingFunction() const { return type() == Type::LinearFunction; }
    bool isCubicBezierTimingFunction() const { return type() == Type::CubicBezierFunction; }
    bool isStepsTimingFunction() const { return type() == Type::StepsFunction; }
    bool isSpringTimingFunction() const { return type() == Type::SpringFunction; }

    virtual bool operator==(const TimingFunction&) const = 0;
    bool operator!=(const TimingFunction& other) const { return !(*this == other); }

    static ExceptionOr<RefPtr<TimingFunction>> createFromCSSText(const String&);
    static RefPtr<TimingFunction> createFromCSSValue(const CSSValue&);
    double transformProgress(double progress, double duration, bool before = false) const;
    String cssText() const;
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

    static const LinearTimingFunction& sharedLinearTimingFunction()
    {
        static const LinearTimingFunction& function = create().leakRef();
        return function;
    }

private:
    Type type() const final { return Type::LinearFunction; }
    Ref<TimingFunction> clone() const final
    {
        return adoptRef(*new LinearTimingFunction);
    }
};

class CubicBezierTimingFunction final : public TimingFunction {
public:
    enum class TimingFunctionPreset : uint8_t { Ease, EaseIn, EaseOut, EaseInOut, Custom };

    static Ref<CubicBezierTimingFunction> create()
    {
        return create(TimingFunctionPreset::Ease, 0.25, 0.1, 0.25, 1.0);
    }

    static Ref<CubicBezierTimingFunction> create(TimingFunctionPreset preset, double x1, double y1, double x2, double y2)
    {
        return adoptRef(*new CubicBezierTimingFunction(preset, x1, y1, x2, y2));
    }

    static Ref<CubicBezierTimingFunction> create(TimingFunctionPreset preset)
    {
        switch (preset) {
        case TimingFunctionPreset::Ease:
            return create(TimingFunctionPreset::Ease, 0.25, 0.1, 0.25, 1.0);
        case TimingFunctionPreset::EaseIn:
            return create(TimingFunctionPreset::EaseIn, 0.42, 0.0, 1.0, 1.0);
        case TimingFunctionPreset::EaseOut:
            return create(TimingFunctionPreset::EaseOut, 0.0, 0.0, 0.58, 1.0);
        case TimingFunctionPreset::EaseInOut:
            return create(TimingFunctionPreset::EaseInOut, 0.42, 0.0, 0.58, 1.0);
        case TimingFunctionPreset::Custom:
            break;
        }
        ASSERT_NOT_REACHED();
        return create();
    }

    bool operator==(const TimingFunction& other) const final
    {
        if (!is<CubicBezierTimingFunction>(other))
            return false;
        auto& otherCubic = downcast<CubicBezierTimingFunction>(other);
        if (m_timingFunctionPreset != otherCubic.m_timingFunctionPreset)
            return false;
        if (m_timingFunctionPreset != TimingFunctionPreset::Custom)
            return true;
        return m_x1 == otherCubic.m_x1 && m_y1 == otherCubic.m_y1 && m_x2 == otherCubic.m_x2 && m_y2 == otherCubic.m_y2;
    }

    double x1() const { return m_x1; }
    double y1() const { return m_y1; }
    double x2() const { return m_x2; }
    double y2() const { return m_y2; }

    TimingFunctionPreset timingFunctionPreset() const { return m_timingFunctionPreset; }
    void setTimingFunctionPreset(TimingFunctionPreset preset) { m_timingFunctionPreset = preset; }

    static const CubicBezierTimingFunction& defaultTimingFunction()
    {
        static const CubicBezierTimingFunction& function = create().leakRef();
        return function;
    }

    Ref<CubicBezierTimingFunction> createReversed() const
    {
        return create(TimingFunctionPreset::Custom, 1.0 - m_x2, 1.0 - m_y2, 1.0 - m_x1, 1.0 - m_y1);
    }

    bool isLinear() const
    {
        return (!m_x1 && !m_y1 && !m_x2 && !m_y2) || (m_x1 == 1.0 && m_y1 == 1.0 && m_x2 == 1.0 && m_y2 == 1.0) || (m_x1 == 0.0 && m_y1 == 0.0 && m_x2 == 1.0 && m_y2 == 1.0);
    }

private:
    explicit CubicBezierTimingFunction(TimingFunctionPreset preset, double x1, double y1, double x2, double y2)
        : m_x1(x1)
        , m_y1(y1)
        , m_x2(x2)
        , m_y2(y2)
        , m_timingFunctionPreset(preset)
    {
    }

    Type type() const final { return Type::CubicBezierFunction; }
    Ref<TimingFunction> clone() const final
    {
        return adoptRef(*new CubicBezierTimingFunction(m_timingFunctionPreset, m_x1, m_y1, m_x2, m_y2));
    }

    const double m_x1;
    const double m_y1;
    const double m_x2;
    const double m_y2;
    TimingFunctionPreset m_timingFunctionPreset;
};

class StepsTimingFunction final : public TimingFunction {
public:
    enum class StepPosition : uint8_t {
        JumpStart,
        JumpEnd,
        JumpNone,
        JumpBoth,
        Start,
        End,
    };

    static Ref<StepsTimingFunction> create(int steps, std::optional<StepPosition> stepPosition)
    {
        return adoptRef(*new StepsTimingFunction(steps, stepPosition));
    }
    static Ref<StepsTimingFunction> create()
    {
        return adoptRef(*new StepsTimingFunction(1, StepPosition::End));
    }
    
    bool operator==(const TimingFunction& other) const final
    {
        if (!is<StepsTimingFunction>(other))
            return false;
        auto& otherSteps = downcast<StepsTimingFunction>(other);

        if (m_steps != otherSteps.m_steps)
            return false;

        if (m_stepPosition == otherSteps.m_stepPosition)
            return true;

        if (!m_stepPosition && *otherSteps.m_stepPosition == StepPosition::End)
            return true;

        if (!otherSteps.m_stepPosition && *m_stepPosition == StepPosition::End)
            return true;

        return false;
    }
    
    int numberOfSteps() const { return m_steps; }

    std::optional<StepPosition> stepPosition() const { return m_stepPosition; }

private:
    StepsTimingFunction(int steps, std::optional<StepPosition> stepPosition)
        : m_steps(steps)
        , m_stepPosition(stepPosition)
    {
    }

    Type type() const final { return Type::StepsFunction; }
    Ref<TimingFunction> clone() const final
    {
        return adoptRef(*new StepsTimingFunction(m_steps, m_stepPosition));
    }
    
    const int m_steps;
    const std::optional<StepPosition> m_stepPosition;
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

private:
    explicit SpringTimingFunction(double mass, double stiffness, double damping, double initialVelocity)
        : m_mass(mass)
        , m_stiffness(stiffness)
        , m_damping(damping)
        , m_initialVelocity(initialVelocity)
    {
    }

    Type type() const final { return Type::SpringFunction; }
    Ref<TimingFunction> clone() const final
    {
        return adoptRef(*new SpringTimingFunction(m_mass, m_stiffness, m_damping, m_initialVelocity));
    }

    const double m_mass;
    const double m_stiffness;
    const double m_damping;
    const double m_initialVelocity;
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

namespace WTF {

template<> struct EnumTraits<WebCore::CubicBezierTimingFunction::TimingFunctionPreset> {
    using values = EnumValues<
        WebCore::CubicBezierTimingFunction::TimingFunctionPreset,
        WebCore::CubicBezierTimingFunction::TimingFunctionPreset::Ease,
        WebCore::CubicBezierTimingFunction::TimingFunctionPreset::EaseIn,
        WebCore::CubicBezierTimingFunction::TimingFunctionPreset::EaseOut,
        WebCore::CubicBezierTimingFunction::TimingFunctionPreset::EaseInOut,
        WebCore::CubicBezierTimingFunction::TimingFunctionPreset::Custom
    >;
};

template<> struct EnumTraits<WebCore::StepsTimingFunction::StepPosition> {
    using values = EnumValues<
        WebCore::StepsTimingFunction::StepPosition,
        WebCore::StepsTimingFunction::StepPosition::JumpStart,
        WebCore::StepsTimingFunction::StepPosition::JumpEnd,
        WebCore::StepsTimingFunction::StepPosition::JumpNone,
        WebCore::StepsTimingFunction::StepPosition::JumpBoth,
        WebCore::StepsTimingFunction::StepPosition::Start,
        WebCore::StepsTimingFunction::StepPosition::End
    >;
};

template<> struct EnumTraits<WebCore::TimingFunction::Type> {
    using values = EnumValues<
        WebCore::TimingFunction::Type,
        WebCore::TimingFunction::Type::LinearFunction,
        WebCore::TimingFunction::Type::CubicBezierFunction,
        WebCore::TimingFunction::Type::StepsFunction,
        WebCore::TimingFunction::Type::SpringFunction
    >;
};

} // namespace WTF
