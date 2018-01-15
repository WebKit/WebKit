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

#include "SpringSolver.h"
#include "UnitBezier.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, const TimingFunction& timingFunction)
{
    switch (timingFunction.type()) {
    case TimingFunction::LinearFunction:
        ts << "linear";
        break;
    case TimingFunction::CubicBezierFunction: {
        auto& function = static_cast<const CubicBezierTimingFunction&>(timingFunction);
        ts << "cubic-bezier(" << function.x1() << ", " << function.y1() << ", " <<  function.x2() << ", " << function.y2() << ")";
        break;
    }
    case TimingFunction::StepsFunction: {
        auto& function = static_cast<const StepsTimingFunction&>(timingFunction);
        ts << "steps(" << function.numberOfSteps() << ", " << (function.stepAtStart() ? "start" : "end") << ")";
        break;
    }
    case TimingFunction::FramesFunction: {
        auto& function = static_cast<const FramesTimingFunction&>(timingFunction);
        ts << "frames(" << function.numberOfFrames() << ")";
        break;
    }
    case TimingFunction::SpringFunction: {
        auto& function = static_cast<const SpringTimingFunction&>(timingFunction);
        ts << "spring(" << function.mass() << " " << function.stiffness() << " " <<  function.damping() << " " << function.initialVelocity() << ")";
        break;
    }
    }
    return ts;
}

double TimingFunction::transformTime(double inputTime, double duration) const
{
    switch (m_type) {
    case TimingFunction::CubicBezierFunction: {
        auto& function = *downcast<const CubicBezierTimingFunction>(this);
        // The epsilon value we pass to UnitBezier::solve given that the animation is going to run over |dur| seconds. The longer the
        // animation, the more precision we need in the timing function result to avoid ugly discontinuities.
        auto epsilon = 1.0 / (200.0 * duration);
        return UnitBezier(function.x1(), function.y1(), function.x2(), function.y2()).solve(inputTime, epsilon);
    }
    case TimingFunction::StepsFunction: {
        auto& function = *downcast<const StepsTimingFunction>(this);
        auto numberOfSteps = function.numberOfSteps();
        if (function.stepAtStart())
            return std::min(1.0, (std::floor(numberOfSteps * inputTime) + 1) / numberOfSteps);
        return std::floor(numberOfSteps * inputTime) / numberOfSteps;
    }
    case TimingFunction::FramesFunction: {
        // https://drafts.csswg.org/css-timing/#frames-timing-functions
        auto& function = *downcast<const FramesTimingFunction>(this);
        auto numberOfFrames = function.numberOfFrames();
        ASSERT(numberOfFrames > 1);
        auto outputTime = std::floor(inputTime * numberOfFrames) / (numberOfFrames - 1);
        if (inputTime <= 1 && outputTime > 1)
            return 1;
        return outputTime;
    }
    case TimingFunction::SpringFunction: {
        auto& function = *downcast<const SpringTimingFunction>(this);
        return SpringSolver(function.mass(), function.stiffness(), function.damping(), function.initialVelocity()).solve(inputTime * duration);
    }
    case TimingFunction::LinearFunction:
        return inputTime;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WebCore
