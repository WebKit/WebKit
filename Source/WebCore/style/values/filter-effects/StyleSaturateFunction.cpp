/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleSaturateFunction.h"

#include "AnimationUtilities.h"
#include "CSSFilterFunctionDescriptor.h"
#include "CSSSaturateFunction.h"
#include "FEColorMatrix.h"
#include "FilterOperation.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

Saturate Saturate::passthroughForInterpolation()
{
    return { .value = CSSFilterFunctionDescriptor<CSSValueSaturate>::initialValueForInterpolation };
}

bool Saturate::transformColor(SRGBA<float>& color) const
{
    color = makeFromComponentsClamping<SRGBA<float>>(saturationColorMatrix(evaluate<float>(value)).transformedColorComponents(asColorComponents(color.resolved())));
    return true;
}

// MARK: - Conversion

auto ToCSS<Saturate>::operator()(const Saturate& value, const RenderStyle& style) -> CSS::Saturate
{
    return { .value = CSS::Saturate::Parameter { toCSS(value.value, style) } };
}

auto ToStyle<CSS::Saturate>::operator()(const CSS::Saturate& value, const BuilderState& state) -> Saturate
{
    if (auto parameter = value.value) {
        return { .value = WTF::switchOn(*parameter,
            [&](const CSS::Saturate::Parameter::Number& number) -> Saturate::Parameter {
                return { toStyle(number, state) };
            },
            [&](const CSS::Saturate::Parameter::Percentage& percentage) -> Saturate::Parameter {
                return { toStyle(percentage, state).value / 100 };
            }
        ) };
    }
    return { .value = CSSFilterFunctionDescriptor<CSSValueSaturate>::defaultValue };
}

// MARK: - Blending

auto Blending<Saturate>::blend(const Saturate& from, const Saturate& to, const BlendingContext& context) -> Saturate
{
    // Accumulate needs to be special cased for filter functions with "initial values
    // for interpolation of 1" to use the formula "Vresult = Va + Vb - 1".
    // https://drafts.csswg.org/filter-effects/#accumulation
    static_assert(CSSFilterFunctionDescriptor<CSSValueSaturate>::initialValueForInterpolation == 1_css_number);

    if (context.compositeOperation == CompositeOperation::Accumulate) {
        return { Saturate::Parameter {
            CSS::clampToRange<Saturate::Parameter::range, typename Saturate::Parameter::ResolvedValueType>(
                from.value.value + to.value.value - 1
            )
        } };
    }

    return { Style::blend(from.value, to.value, context) };
}

// MARK: - Evaluation

auto Evaluation<Saturate, Ref<FilterEffect>>::operator()(const Saturate& value) -> Ref<FilterEffect>
{
    auto inputParameters = Vector<float> { evaluate<float>(value) };
    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE, WTF::move(inputParameters));
}

// MARK: - Platform

auto ToPlatform<Saturate>::operator()(const Saturate& value) -> Ref<FilterOperation>
{
    return BasicColorMatrixFilterOperation::create(Style::evaluate<double>(value), filterFunctionOperationType<SaturateFunction::name>());
}

} // namespace Style
} // namespace WebCore
