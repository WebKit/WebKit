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
    case TimingFunction::LinearFunction:
        ts << "linear";
        break;
    case TimingFunction::CubicBezierFunction: {
        auto& function = downcast<CubicBezierTimingFunction>(timingFunction);
        ts << "cubic-bezier(" << function.x1() << ", " << function.y1() << ", " <<  function.x2() << ", " << function.y2() << ")";
        break;
    }
    case TimingFunction::StepsFunction: {
        auto& function = downcast<StepsTimingFunction>(timingFunction);
        ts << "steps(" << function.numberOfSteps() << ", " << (function.stepAtStart() ? "start" : "end") << ")";
        break;
    }
    case TimingFunction::SpringFunction: {
        auto& function = downcast<SpringTimingFunction>(timingFunction);
        ts << "spring(" << function.mass() << " " << function.stiffness() << " " <<  function.damping() << " " << function.initialVelocity() << ")";
        break;
    }
    }
    return ts;
}

double TimingFunction::transformTime(double inputTime, double duration, bool before) const
{
    switch (m_type) {
    case TimingFunction::CubicBezierFunction: {
        auto& function = downcast<CubicBezierTimingFunction>(*this);
        if (function.isLinear())
            return inputTime;
        // The epsilon value we pass to UnitBezier::solve given that the animation is going to run over |dur| seconds. The longer the
        // animation, the more precision we need in the timing function result to avoid ugly discontinuities.
        auto epsilon = 1.0 / (1000.0 * duration);
        return UnitBezier(function.x1(), function.y1(), function.x2(), function.y2()).solve(inputTime, epsilon);
    }
    case TimingFunction::StepsFunction: {
        // https://drafts.csswg.org/css-timing/#step-timing-functions
        auto& function = downcast<StepsTimingFunction>(*this);
        auto steps = function.numberOfSteps();
        // 1. Calculate the current step as floor(input progress value × steps).
        auto currentStep = std::floor(inputTime * steps);
        // 2. If the step position property is start, increment current step by one.
        if (function.stepAtStart())
            currentStep++;
        // 3. If both of the following conditions are true:
        //    - the before flag is set, and
        //    - input progress value × steps mod 1 equals zero (that is, if input progress value × steps is integral), then
        //    decrement current step by one.
        if (before && !fmod(inputTime * steps, 1))
            currentStep--;
        // 4. If input progress value ≥ 0 and current step < 0, let current step be zero.
        if (inputTime >= 0 && currentStep < 0)
            currentStep = 0;
        // 5. If input progress value ≤ 1 and current step > steps, let current step be steps.
        if (inputTime <= 1 && currentStep > steps)
            currentStep = steps;
        // 6. The output progress value is current step / steps.
        return currentStep / steps;
    }
    case TimingFunction::SpringFunction: {
        auto& function = downcast<SpringTimingFunction>(*this);
        return SpringSolver(function.mass(), function.stiffness(), function.damping(), function.initialVelocity()).solve(inputTime * duration);
    }
    case TimingFunction::LinearFunction:
        return inputTime;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

ExceptionOr<RefPtr<TimingFunction>> TimingFunction::createFromCSSText(const String& cssText)
{
    StringBuilder cssString;
    cssString.append(getPropertyNameString(CSSPropertyAnimationTimingFunction));
    cssString.appendLiteral(": ");
    cssString.append(cssText);
    auto styleProperties = MutableStyleProperties::create();
    styleProperties->parseDeclaration(cssString.toString(), CSSParserContext(HTMLStandardMode));

    if (auto cssValue = styleProperties->getPropertyCSSValue(CSSPropertyAnimationTimingFunction)) {
        if (auto timingFunction = createFromCSSValue(*cssValue.get()))
            return WTFMove(timingFunction);
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
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::EaseIn);
        case CSSValueEaseOut:
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::EaseOut);
        case CSSValueEaseInOut:
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::EaseInOut);
        case CSSValueStepStart:
            return StepsTimingFunction::create(1, true);
        case CSSValueStepEnd:
            return StepsTimingFunction::create(1, false);
        default:
            return nullptr;
        }
    }

    if (is<CSSCubicBezierTimingFunctionValue>(value)) {
        auto& cubicTimingFunction = downcast<CSSCubicBezierTimingFunctionValue>(value);
        return CubicBezierTimingFunction::create(cubicTimingFunction.x1(), cubicTimingFunction.y1(), cubicTimingFunction.x2(), cubicTimingFunction.y2());
    }
    if (is<CSSStepsTimingFunctionValue>(value)) {
        auto& stepsTimingFunction = downcast<CSSStepsTimingFunctionValue>(value);
        return StepsTimingFunction::create(stepsTimingFunction.numberOfSteps(), stepsTimingFunction.stepAtStart());
    }
    if (is<CSSSpringTimingFunctionValue>(value)) {
        auto& springTimingFunction = downcast<CSSSpringTimingFunctionValue>(value);
        return SpringTimingFunction::create(springTimingFunction.mass(), springTimingFunction.stiffness(), springTimingFunction.damping(), springTimingFunction.initialVelocity());
    }

    return nullptr;
}

String TimingFunction::cssText() const
{
    if (m_type == TimingFunction::CubicBezierFunction) {
        auto& function = downcast<CubicBezierTimingFunction>(*this);
        if (function.x1() == 0.25 && function.y1() == 0.1 && function.x2() == 0.25 && function.y2() == 1.0)
            return "ease";
        if (function.x1() == 0.42 && !function.y1() && function.x2() == 1.0 && function.y2() == 1.0)
            return "ease-in";
        if (!function.x1() && !function.y1() && function.x2() == 0.58 && function.y2() == 1.0)
            return "ease-out";
        if (function.x1() == 0.42 && !function.y1() && function.x2() == 0.58 && function.y2() == 1.0)
            return "ease-in-out";
        return makeString("cubic-bezier(", function.x1(), ", ", function.y1(), ", ", function.x2(), ", ", function.y2(), ')');
    }

    if (m_type == TimingFunction::StepsFunction) {
        auto& function = downcast<StepsTimingFunction>(*this);
        if (!function.stepAtStart())
            return makeString("steps(", function.numberOfSteps(), ')');
    }

    TextStream stream;
    stream << *this;
    return stream.release();
}

} // namespace WebCore
