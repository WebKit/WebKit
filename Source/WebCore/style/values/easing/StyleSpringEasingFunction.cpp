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
#include "StyleSpringEasingFunction.h"

#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "TimingFunction.h"

namespace WebCore {
namespace Style {

CSS::SpringEasingFunction toCSSSpringEasingFunction(const SpringTimingFunction& function, const RenderStyle& style)
{
    return CSS::SpringEasingFunction {
        .parameters = {
            .mass = toCSS(Number<CSS::SpringEasingParameters::Positive> { function.mass() }, style),
            .stiffness = toCSS(Number<CSS::SpringEasingParameters::Positive> { function.stiffness() }, style),
            .damping = toCSS(Number<CSS::Nonnegative> { function.damping() }, style),
            .initialVelocity = toCSS(Number<> { function.initialVelocity() }, style),
        }
    };
}

Ref<TimingFunction> createTimingFunction(const CSS::SpringEasingFunction& function, const CSSToLengthConversionData& conversionData)
{
    return SpringTimingFunction::create(
        toStyle(function->mass, conversionData).value,
        toStyle(function->stiffness, conversionData).value,
        toStyle(function->damping, conversionData).value,
        toStyle(function->initialVelocity, conversionData).value
    );
}

Ref<TimingFunction> createTimingFunctionDeprecated(const CSS::SpringEasingFunction& function)
{
    if (!CSS::collectComputedStyleDependencies(function).canResolveDependenciesWithConversionData({ }))
        return SpringTimingFunction::create(1, 1, 0, 0);

    return SpringTimingFunction::create(
        toStyleNoConversionDataRequired(function->mass).value,
        toStyleNoConversionDataRequired(function->stiffness).value,
        toStyleNoConversionDataRequired(function->damping).value,
        toStyleNoConversionDataRequired(function->initialVelocity).value
    );
}

} // namespace Style
} // namespace WebCore
