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
#include "StyleCubicBezierEasingFunction.h"

#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "TimingFunction.h"

namespace WebCore {
namespace Style {

CSS::CubicBezierEasingFunction toCSSCubicBezierEasingFunction(const CubicBezierTimingFunction& function, const RenderStyle& style)
{
    ASSERT(function.timingFunctionPreset() == CubicBezierTimingFunction::TimingFunctionPreset::Custom);

    return CSS::CubicBezierEasingFunction {
        .parameters = {
            .value = {
                CSS::CubicBezierEasingParameters::Coordinate {
                    toCSS(Number<CSS::ClosedUnitRange> { function.x1() }, style), toCSS(Number<> { function.y1() }, style)
                },
                CSS::CubicBezierEasingParameters::Coordinate {
                    toCSS(Number<CSS::ClosedUnitRange> { function.x2() }, style), toCSS(Number<> { function.y2() }, style)
                },
            }
        }
    };
}

Ref<TimingFunction> createTimingFunction(const CSS::CubicBezierEasingFunction& function, const CSSToLengthConversionData& conversionData)
{
    return CubicBezierTimingFunction::create(
        toStyle(get<0>(get<0>(function->value)), conversionData).value,
        toStyle(get<1>(get<0>(function->value)), conversionData).value,
        toStyle(get<0>(get<1>(function->value)), conversionData).value,
        toStyle(get<1>(get<1>(function->value)), conversionData).value
    );
}

Ref<TimingFunction> createTimingFunctionDeprecated(const CSS::CubicBezierEasingFunction& function)
{
    if (!CSS::collectComputedStyleDependencies(function).canResolveDependenciesWithConversionData({ }))
        return CubicBezierTimingFunction::create();

    return CubicBezierTimingFunction::create(
        toStyleNoConversionDataRequired(get<0>(get<0>(function->value))).value,
        toStyleNoConversionDataRequired(get<1>(get<0>(function->value))).value,
        toStyleNoConversionDataRequired(get<0>(get<1>(function->value))).value,
        toStyleNoConversionDataRequired(get<1>(get<1>(function->value))).value
    );
}

} // namespace Style
} // namespace WebCore
