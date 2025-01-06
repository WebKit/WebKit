/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleStepsEasingFunction.h"

#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "TimingFunction.h"

namespace WebCore {
namespace Style {

CSS::StepsEasingFunction toCSSStepsEasingFunction(const StepsTimingFunction& function, const RenderStyle&)
{
    auto position = function.stepPosition();
    if (!position)
        return { { CSS::StepsEasingParameters::JumpEnd { function.numberOfSteps() } } };

    switch (*position) {
    case StepsTimingFunction::StepPosition::JumpStart:
        return { { CSS::StepsEasingParameters::JumpStart { function.numberOfSteps() } } };

    case StepsTimingFunction::StepPosition::JumpEnd:
        return { { CSS::StepsEasingParameters::JumpEnd { function.numberOfSteps() } } };

    case StepsTimingFunction::StepPosition::JumpNone:
        return { { CSS::StepsEasingParameters::JumpNone { function.numberOfSteps() } } };

    case StepsTimingFunction::StepPosition::JumpBoth:
        return { { CSS::StepsEasingParameters::JumpBoth { function.numberOfSteps() } } };

    case StepsTimingFunction::StepPosition::Start:
        return { { CSS::StepsEasingParameters::Start { function.numberOfSteps() } } };

    case StepsTimingFunction::StepPosition::End:
        return { { CSS::StepsEasingParameters::End { function.numberOfSteps() } } };
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static StepsTimingFunction::StepPosition toStepPosition(CSS::Keyword::JumpStart)
{
    return StepsTimingFunction::StepPosition::JumpStart;
}

static StepsTimingFunction::StepPosition toStepPosition(CSS::Keyword::JumpEnd)
{
    return StepsTimingFunction::StepPosition::JumpEnd;
}

static StepsTimingFunction::StepPosition toStepPosition(CSS::Keyword::JumpBoth)
{
    return StepsTimingFunction::StepPosition::JumpBoth;
}

static StepsTimingFunction::StepPosition toStepPosition(CSS::Keyword::Start)
{
    return StepsTimingFunction::StepPosition::Start;
}

static StepsTimingFunction::StepPosition toStepPosition(CSS::Keyword::End)
{
    return StepsTimingFunction::StepPosition::End;
}

static StepsTimingFunction::StepPosition toStepPosition(CSS::Keyword::JumpNone)
{
    return StepsTimingFunction::StepPosition::JumpNone;
}

Ref<TimingFunction> createTimingFunction(const CSS::StepsEasingFunction& function, const CSSToLengthConversionData& conversionData)
{
    return WTF::switchOn(function->value,
        [&](const auto& value) -> Ref<TimingFunction> {
            return StepsTimingFunction::create(toStyle(value.steps, conversionData).value, toStepPosition(value.keyword));
        }
    );
}

Ref<TimingFunction> createTimingFunctionDeprecated(const CSS::StepsEasingFunction& function)
{
    if (!CSS::collectComputedStyleDependencies(function).canResolveDependenciesWithConversionData({ }))
        return StepsTimingFunction::create();

    return WTF::switchOn(function->value,
        [&](const auto& value) -> Ref<TimingFunction> {
            return StepsTimingFunction::create(toStyleNoConversionDataRequired(value.steps).value, toStepPosition(value.keyword));
        }
    );
}

} // namespace Style
} // namespace WebCore
