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
#include "StyleEasingFunction.h"

#include "CSSEasingFunction.h"
#include "CSSEasingFunctionValue.h"
#include "CSSPrimitiveValue.h"
#include "StyleCubicBezierEasingFunction.h"
#include "StyleLinearEasingFunction.h"
#include "StyleSpringEasingFunction.h"
#include "StyleStepsEasingFunction.h"
#include "TimingFunction.h"

namespace WebCore {
namespace Style {

CSS::EasingFunction toCSSEasingFunction(const TimingFunction& function, const RenderStyle& style)
{
    switch (function.type()) {
    case TimingFunction::Type::CubicBezierFunction: {
        auto& cubicBezierFunction = downcast<CubicBezierTimingFunction>(function);
        switch (cubicBezierFunction.timingFunctionPreset()) {
        case CubicBezierTimingFunction::TimingFunctionPreset::Ease:
            return { CSS::Keyword::Ease { } };
        case CubicBezierTimingFunction::TimingFunctionPreset::EaseIn:
            return { CSS::Keyword::EaseIn { } };
        case CubicBezierTimingFunction::TimingFunctionPreset::EaseOut:
            return { CSS::Keyword::EaseOut { } };
        case CubicBezierTimingFunction::TimingFunctionPreset::EaseInOut:
            return { CSS::Keyword::EaseInOut { } };
        case CubicBezierTimingFunction::TimingFunctionPreset::Custom:
            break;
        }
        return { toCSSCubicBezierEasingFunction(cubicBezierFunction, style) };
    }

    case TimingFunction::Type::LinearFunction: {
        auto& linearFunction = uncheckedDowncast<LinearTimingFunction>(function);
        if (linearFunction.points().isEmpty())
            return { CSS::Keyword::Linear { } };
        return { toCSSLinearEasingFunction(linearFunction, style) };
    }

    case TimingFunction::Type::StepsFunction:
        return { toCSSStepsEasingFunction(downcast<StepsTimingFunction>(function), style) };

    case TimingFunction::Type::SpringFunction:
        return { toCSSSpringEasingFunction(downcast<SpringTimingFunction>(function), style) };
    }

    RELEASE_ASSERT_NOT_REACHED();
}

Ref<TimingFunction> createTimingFunction(const CSS::EasingFunction& function, const CSSToLengthConversionData& conversionData)
{
    return WTF::switchOn(function.value,
        [&](const CSS::Keyword::Linear&) -> Ref<TimingFunction> {
            return LinearTimingFunction::create();
        },
        [&](const CSS::LinearEasingFunction& function) -> Ref<TimingFunction> {
            return createTimingFunction(function, conversionData);
        },
        [&](const CSS::Keyword::Ease&) -> Ref<TimingFunction> {
            return CubicBezierTimingFunction::create();
        },
        [&](const CSS::Keyword::EaseIn&) -> Ref<TimingFunction> {
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseIn);
        },
        [&](const CSS::Keyword::EaseOut&) -> Ref<TimingFunction> {
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseOut);
        },
        [&](const CSS::Keyword::EaseInOut&) -> Ref<TimingFunction> {
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseInOut);
        },
        [&](const CSS::CubicBezierEasingFunction& function) -> Ref<TimingFunction> {
            return createTimingFunction(function, conversionData);
        },
        [&](const CSS::Keyword::StepStart&) -> Ref<TimingFunction> {
            return StepsTimingFunction::create(1, StepsTimingFunction::StepPosition::Start);
        },
        [&](const CSS::Keyword::StepEnd&) -> Ref<TimingFunction> {
            return StepsTimingFunction::create(1, StepsTimingFunction::StepPosition::End);
        },
        [&](const CSS::StepsEasingFunction& function) -> Ref<TimingFunction> {
            return createTimingFunction(function, conversionData);
        },
        [&](const CSS::SpringEasingFunction& function) -> Ref<TimingFunction> {
            return createTimingFunction(function, conversionData);
        }
    );
}

Ref<TimingFunction> createTimingFunctionDeprecated(const CSS::EasingFunction& function)
{
    return WTF::switchOn(function.value,
        [&](const CSS::Keyword::Linear&) -> Ref<TimingFunction> {
            return LinearTimingFunction::create();
        },
        [&](const CSS::LinearEasingFunction& function) -> Ref<TimingFunction> {
            return createTimingFunctionDeprecated(function);
        },
        [&](const CSS::Keyword::Ease&) -> Ref<TimingFunction> {
            return CubicBezierTimingFunction::create();
        },
        [&](const CSS::Keyword::EaseIn&) -> Ref<TimingFunction> {
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseIn);
        },
        [&](const CSS::Keyword::EaseOut&) -> Ref<TimingFunction> {
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseOut);
        },
        [&](const CSS::Keyword::EaseInOut&) -> Ref<TimingFunction> {
            return CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseInOut);
        },
        [&](const CSS::CubicBezierEasingFunction& function) -> Ref<TimingFunction> {
            return createTimingFunctionDeprecated(function);
        },
        [&](const CSS::Keyword::StepStart&) -> Ref<TimingFunction> {
            return StepsTimingFunction::create(1, StepsTimingFunction::StepPosition::Start);
        },
        [&](const CSS::Keyword::StepEnd&) -> Ref<TimingFunction> {
            return StepsTimingFunction::create(1, StepsTimingFunction::StepPosition::End);
        },
        [&](const CSS::StepsEasingFunction& function) -> Ref<TimingFunction> {
            return createTimingFunctionDeprecated(function);
        },
        [&](const CSS::SpringEasingFunction& function) -> Ref<TimingFunction> {
            return createTimingFunctionDeprecated(function);
        }
    );
}

static RefPtr<TimingFunction> createTimingFunctionFromValueID(CSSValueID valueID)
{
    switch (valueID) {
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
        return { };
    }
}

RefPtr<TimingFunction> createTimingFunction(const CSSValue& value, const CSSToLengthConversionData& conversionData)
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return createTimingFunctionFromValueID(primitiveValue->valueID());
    if (RefPtr easingFunctionValue = dynamicDowncast<CSSEasingFunctionValue>(value))
        return createTimingFunction(easingFunctionValue->easingFunction(), conversionData);
    return { };
}

RefPtr<TimingFunction> createTimingFunctionDeprecated(const CSSValue& value)
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return createTimingFunctionFromValueID(primitiveValue->valueID());
    if (auto easingFunctionValue = dynamicDowncast<CSSEasingFunctionValue>(value))
        return createTimingFunctionDeprecated(easingFunctionValue->easingFunction());
    return { };
}

} // namespace Style
} // namespace WebCore
