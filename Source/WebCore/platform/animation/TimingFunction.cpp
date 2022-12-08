/*
 * Copyright (C) 2015 Apple Inc.  All rights reserved.
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
#include "TimingFunction.h"

#include "CSSTimingFunctionValue.h"
#include "SpringSolver.h"
#include "StyleProperties.h"
#include "UnitBezier.h"
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, const TimingFunction& timingFunction)
{
    switch (timingFunction.type()) {
    case TimingFunction::Type::LinearFunction:
        ts << "linear";
        break;
    case TimingFunction::Type::CubicBezierFunction: {
        auto& function = downcast<CubicBezierTimingFunction>(timingFunction);
        ts << "cubic-bezier(" << function.x1() << ", " << function.y1() << ", " <<  function.x2() << ", " << function.y2() << ")";
        break;
    }
    case TimingFunction::Type::StepsFunction: {
        auto& function = downcast<StepsTimingFunction>(timingFunction);
        ts << "steps(" << function.numberOfSteps();
        if (auto stepPosition = function.stepPosition()) {
            ts << ", ";
            switch (stepPosition.value()) {
            case StepsTimingFunction::StepPosition::JumpStart:
                ts << "jump-start";
                break;

            case StepsTimingFunction::StepPosition::JumpEnd:
                ts << "jump-end";
                break;

            case StepsTimingFunction::StepPosition::JumpNone:
                ts << "jump-none";
                break;

            case StepsTimingFunction::StepPosition::JumpBoth:
                ts << "jump-both";
                break;

            case StepsTimingFunction::StepPosition::Start:
                ts << "start";
                break;

            case StepsTimingFunction::StepPosition::End:
                ts << "end";
                break;
            }
        }
        ts << ")";
        break;
    }
    case TimingFunction::Type::SpringFunction: {
        auto& function = downcast<SpringTimingFunction>(timingFunction);
        ts << "spring(" << function.mass() << " " << function.stiffness() << " " <<  function.damping() << " " << function.initialVelocity() << ")";
        break;
    }
    }
    return ts;
}

double TimingFunction::transformProgress(double progress, double duration, bool before) const
{
    switch (type()) {
    case Type::CubicBezierFunction: {
        auto& function = downcast<CubicBezierTimingFunction>(*this);
        if (function.isLinear())
            return progress;
        // The epsilon value we pass to UnitBezier::solve given that the animation is going to run over |dur| seconds. The longer the
        // animation, the more precision we need in the timing function result to avoid ugly discontinuities.
        auto epsilon = 1.0 / (1000.0 * duration);
        return UnitBezier(function.x1(), function.y1(), function.x2(), function.y2()).solve(progress, epsilon);
    }
    case Type::StepsFunction: {
        // https://drafts.csswg.org/css-easing-1/#step-timing-functions
        auto& function = downcast<StepsTimingFunction>(*this);
        auto steps = function.numberOfSteps();
        auto stepPosition = function.stepPosition();
        // 1. Calculate the current step as floor(input progress value × steps).
        auto currentStep = std::floor(progress * steps);
        // 2. If the step position property is start, increment current step by one.
        if (stepPosition == StepsTimingFunction::StepPosition::JumpStart || stepPosition == StepsTimingFunction::StepPosition::Start || stepPosition == StepsTimingFunction::StepPosition::JumpBoth)
            ++currentStep;
        // 3. If both of the following conditions are true:
        //    - the before flag is set, and
        //    - input progress value × steps mod 1 equals zero (that is, if input progress value × steps is integral), then
        //    decrement current step by one.
        if (before && !fmod(progress * steps, 1))
            currentStep--;
        // 4. If input progress value ≥ 0 and current step < 0, let current step be zero.
        if (progress >= 0 && currentStep < 0)
            currentStep = 0;
        // 5. Calculate jumps based on the step position.
        if (stepPosition == StepsTimingFunction::StepPosition::JumpNone)
            --steps;
        else if (stepPosition == StepsTimingFunction::StepPosition::JumpBoth)
            ++steps;
        // 6. If input progress value ≤ 1 and current step > jumps, let current step be jumps.
        if (progress <= 1 && currentStep > steps)
            currentStep = steps;
        // 7. The output progress value is current step / jumps.
        return currentStep / steps;
    }
    case Type::SpringFunction: {
        auto& function = downcast<SpringTimingFunction>(*this);
        return SpringSolver(function.mass(), function.stiffness(), function.damping(), function.initialVelocity()).solve(progress * duration);
    }
    case Type::LinearFunction:
        return progress;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

ExceptionOr<RefPtr<TimingFunction>> TimingFunction::createFromCSSText(const String& cssText)
{
    auto properties = MutableStyleProperties::create();
    properties->parseDeclaration(makeString("animation-timing-function:", cssText), CSSParserContext(HTMLStandardMode));
    if (auto value = properties->getPropertyCSSValue(CSSPropertyAnimationTimingFunction)) {
        if (auto function = createFromCSSValue(*value))
            return function;
    }
    return Exception { TypeError };
}

RefPtr<TimingFunction> TimingFunction::createFromCSSValue(const CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        switch (downcast<CSSPrimitiveValue>(value).valueID()) {
        case CSSValueLinear:
            return LinearTimingFunction::create();
        case CSSValueEase:
            return CubicBezierTimingFunction::create();
        case CSSValueEaseIn:
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseIn);
        case CSSValueEaseOut:
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseOut);
        case CSSValueEaseInOut:
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseInOut);
        case CSSValueStepStart:
            return StepsTimingFunction::create(1, StepsTimingFunction::StepPosition::Start);
        case CSSValueStepEnd:
            return StepsTimingFunction::create(1, StepsTimingFunction::StepPosition::End);
        default:
            return nullptr;
        }
    }

    if (is<CSSCubicBezierTimingFunctionValue>(value)) {
        auto& cubicTimingFunction = downcast<CSSCubicBezierTimingFunctionValue>(value);
        return CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::Custom, cubicTimingFunction.x1(), cubicTimingFunction.y1(), cubicTimingFunction.x2(), cubicTimingFunction.y2());
    }
    if (is<CSSStepsTimingFunctionValue>(value)) {
        auto& stepsTimingFunction = downcast<CSSStepsTimingFunctionValue>(value);
        return StepsTimingFunction::create(stepsTimingFunction.numberOfSteps(), stepsTimingFunction.stepPosition());
    }
    if (is<CSSSpringTimingFunctionValue>(value)) {
        auto& springTimingFunction = downcast<CSSSpringTimingFunctionValue>(value);
        return SpringTimingFunction::create(springTimingFunction.mass(), springTimingFunction.stiffness(), springTimingFunction.damping(), springTimingFunction.initialVelocity());
    }

    return nullptr;
}

String TimingFunction::cssText() const
{
    if (type() == Type::CubicBezierFunction) {
        auto& function = downcast<CubicBezierTimingFunction>(*this);
        if (function.x1() == 0.25 && function.y1() == 0.1 && function.x2() == 0.25 && function.y2() == 1.0)
            return "ease"_s;
        if (function.x1() == 0.42 && !function.y1() && function.x2() == 1.0 && function.y2() == 1.0)
            return "ease-in"_s;
        if (!function.x1() && !function.y1() && function.x2() == 0.58 && function.y2() == 1.0)
            return "ease-out"_s;
        if (function.x1() == 0.42 && !function.y1() && function.x2() == 0.58 && function.y2() == 1.0)
            return "ease-in-out"_s;
        return makeString("cubic-bezier(", function.x1(), ", ", function.y1(), ", ", function.x2(), ", ", function.y2(), ')');
    }

    if (type() == Type::StepsFunction) {
        auto& function = downcast<StepsTimingFunction>(*this);
        if (function.stepPosition() == StepsTimingFunction::StepPosition::JumpEnd || function.stepPosition() == StepsTimingFunction::StepPosition::End)
            return makeString("steps(", function.numberOfSteps(), ')');
    }

    TextStream stream;
    stream << *this;
    return stream.release();
}

} // namespace WebCore
