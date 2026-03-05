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
#include "StyleContrastFunction.h"

#include "AnimationUtilities.h"
#include "CSSContrastFunction.h"
#include "CSSFilterFunctionDescriptor.h"
#include "ColorMatrix.h"
#include "ColorUtilities.h"
#include "FEColorMatrix.h"
#include "FilterOperation.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

Contrast Contrast::passthroughForInterpolation()
{
    return { .value = CSSFilterFunctionDescriptor<CSSValueContrast>::initialValueForInterpolation };
}

bool Contrast::transformColor(SRGBA<float>& color) const
{
    auto amount = evaluate<float>(value);
    auto intercept = -(0.5f * amount) + 0.5f;
    color = colorByModifingEachNonAlphaComponent(color, [&](float component) {
        return std::clamp<float>(intercept + amount * component, 0.0f, 1.0f);
    });
    return true;
}

// MARK: - Conversion

auto ToCSS<Contrast>::operator()(const Contrast& value, const RenderStyle& style) -> CSS::Contrast
{
    return { .value = CSS::Contrast::Parameter { toCSS(value.value, style) } };
}

auto ToStyle<CSS::Contrast>::operator()(const CSS::Contrast& value, const BuilderState& state) -> Contrast
{
    if (auto parameter = value.value) {
        return { .value = WTF::switchOn(*parameter,
            [&](const CSS::Contrast::Parameter::Number& number) -> Contrast::Parameter {
                return { toStyle(number, state) };
            },
            [&](const CSS::Contrast::Parameter::Percentage& percentage) -> Contrast::Parameter {
                return { toStyle(percentage, state).value / 100 };
            }
        ) };
    }
    return { .value = CSSFilterFunctionDescriptor<CSSValueContrast>::defaultValue };
}

// MARK: - Blending

auto Blending<Contrast>::blend(const Contrast& from, const Contrast& to, const BlendingContext& context) -> Contrast
{
    // Accumulate needs to be special cased for filter functions with "initial values
    // for interpolation of 1" to use the formula "Vresult = Va + Vb - 1".
    // https://drafts.csswg.org/filter-effects/#accumulation
    static_assert(CSSFilterFunctionDescriptor<CSSValueContrast>::initialValueForInterpolation == 1_css_number);

    if (context.compositeOperation == CompositeOperation::Accumulate) {
        return { Contrast::Parameter {
            CSS::clampToRange<Contrast::Parameter::range, typename Contrast::Parameter::ResolvedValueType>(
                from.value.value + to.value.value - 1
            )
        } };
    }

    return { Style::blend(from.value, to.value, context) };
}

// MARK: - Evaluation

auto Evaluation<Contrast, Ref<FilterEffect>>::operator()(const Contrast& value) -> Ref<FilterEffect>
{
    ColorMatrix<5, 4> contrastMatrix = contrastColorMatrix(evaluate<float>(value));
    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX, contrastMatrix);
}

// MARK: - Platform

auto ToPlatform<Contrast>::operator()(const Contrast& value) -> Ref<FilterOperation>
{
    return BasicComponentTransferFilterOperation::create(Style::evaluate<double>(value), filterFunctionOperationType<ContrastFunction::name>());
}

} // namespace Style
} // namespace WebCore
