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
#include "StyleSepiaFunction.h"

#include "CSSFilterFunctionDescriptor.h"
#include "CSSSepiaFunction.h"
#include "ColorMatrix.h"
#include "FEColorMatrix.h"
#include "FilterOperation.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

Sepia Sepia::passthroughForInterpolation()
{
    return { .value = CSSFilterFunctionDescriptor<CSSValueSepia>::initialValueForInterpolation };
}

bool Sepia::transformColor(SRGBA<float>& color) const
{
    color = makeFromComponentsClamping<SRGBA<float>>(sepiaColorMatrix(evaluate<float>(value)).transformedColorComponents(asColorComponents(color.resolved())));
    return true;
}

// MARK: - Conversion

auto ToCSS<Sepia>::operator()(const Sepia& value, const RenderStyle& style) -> CSS::Sepia
{
    return { .value = CSS::Sepia::Parameter { toCSS(value.value, style) } };
}

auto ToStyle<CSS::Sepia>::operator()(const CSS::Sepia& value, const BuilderState& state) -> Sepia
{
    if (auto parameter = value.value) {
        return { .value = WTF::switchOn(*parameter,
            [&](const CSS::Sepia::Parameter::Number& number) -> Sepia::Parameter {
                return { toStyle(number, state) };
            },
            [&](const CSS::Sepia::Parameter::Percentage& percentage) -> Sepia::Parameter {
                return { toStyle(percentage, state).value / 100 };
            }
        ) };
    }
    return { .value = CSSFilterFunctionDescriptor<CSSValueSepia>::defaultValue };
}

// MARK: - Evaluation

auto Evaluation<Sepia, Ref<FilterEffect>>::operator()(const Sepia& value) -> Ref<FilterEffect>
{
    ColorMatrix<5, 4> sepiaMatrix = sepiaColorMatrix(evaluate<float>(value));
    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX, sepiaMatrix);
}

// MARK: - Platform

auto ToPlatform<Sepia>::operator()(const Sepia& value) -> Ref<FilterOperation>
{
    return BasicColorMatrixFilterOperation::create(Style::evaluate<double>(value), filterFunctionOperationType<CSS::SepiaFunction::name>());
}

} // namespace Style
} // namespace WebCore
