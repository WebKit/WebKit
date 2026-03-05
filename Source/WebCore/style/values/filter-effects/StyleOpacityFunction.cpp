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
#include "StyleOpacityFunction.h"

#include "AnimationUtilities.h"
#include "CSSFilterFunctionDescriptor.h"
#include "CSSOpacityFunction.h"
#include "ColorMatrix.h"
#include "ColorUtilities.h"
#include "FEColorMatrix.h"
#include "FilterOperation.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

OpacityParameters OpacityParameters::passthroughForInterpolation()
{
    return { .value = CSSFilterFunctionDescriptor<CSSValueOpacity>::initialValueForInterpolation };
}

bool OpacityParameters::transformColor(SRGBA<float>& color) const
{
    color = colorWithOverriddenAlpha(color, std::clamp<float>(color.resolved().alpha * evaluate<float>(value), 0.0f, 1.0f));
    return true;
}

// MARK: - Conversion

auto ToCSS<OpacityParameters>::operator()(const OpacityParameters& value, const RenderStyle& style) -> CSS::Opacity
{
    return { .value = CSS::Opacity::Parameter { toCSS(value.value, style) } };
}

auto ToStyle<CSS::Opacity>::operator()(const CSS::Opacity& value, const BuilderState& state) -> OpacityParameters
{
    if (auto parameter = value.value) {
        return { .value = WTF::switchOn(*parameter,
            [&](const CSS::Opacity::Parameter::Number& number) -> OpacityParameters::Parameter {
                return { toStyle(number, state) };
            },
            [&](const CSS::Opacity::Parameter::Percentage& percentage) -> OpacityParameters::Parameter {
                return { toStyle(percentage, state).value / 100 };
            }
        ) };
    }
    return { .value = CSSFilterFunctionDescriptor<CSSValueOpacity>::defaultValue };
}

// MARK: - Blending

auto Blending<OpacityParameters>::blend(const OpacityParameters& from, const OpacityParameters& to, const BlendingContext& context) -> OpacityParameters
{
    // Accumulate needs to be special cased for filter functions with "initial values
    // for interpolation of 1" to use the formula "Vresult = Va + Vb - 1".
    // https://drafts.csswg.org/filter-effects/#accumulation
    static_assert(CSSFilterFunctionDescriptor<CSSValueOpacity>::initialValueForInterpolation == 1_css_number);

    if (context.compositeOperation == CompositeOperation::Accumulate) {
        return { OpacityParameters::Parameter {
            CSS::clampToRange<OpacityParameters::Parameter::range, typename OpacityParameters::Parameter::ResolvedValueType>(
                from.value.value + to.value.value - 1
            )
        } };
    }

    return { Style::blend(from.value, to.value, context) };
}

// MARK: - Evaluation

auto Evaluation<OpacityParameters, Ref<FilterEffect>>::operator()(const OpacityParameters& value) -> Ref<FilterEffect>
{
    ColorMatrix<5, 4> opacityMatrix = opacityColorMatrix(evaluate<float>(value));
    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX, opacityMatrix);
}

// MARK: - Platform

auto ToPlatform<OpacityParameters>::operator()(const OpacityParameters& value) -> Ref<FilterOperation>
{
    return BasicComponentTransferFilterOperation::create(Style::evaluate<double>(value), filterFunctionOperationType<OpacityFunction::name>());
}

} // namespace Style
} // namespace WebCore
