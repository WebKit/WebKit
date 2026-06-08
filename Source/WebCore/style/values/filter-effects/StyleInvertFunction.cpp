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
#include "StyleInvertFunction.h"

#include "CSSFilterFunctionDescriptor.h"
#include "CSSInvertFunction.h"
#include "ColorMatrix.h"
#include "ColorUtilities.h"
#include "FEColorMatrix.h"
#include "FilterOperation.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

Invert Invert::passthroughForInterpolation()
{
    return { .value = CSSFilterFunctionDescriptor<CSSValueInvert>::initialValueForInterpolation };
}

bool Invert::transformColor(SRGBA<float>& color) const
{
    auto amount = evaluate<float>(value);
    auto oneMinusAmount = 1.0f - amount;
    color = colorByModifingEachNonAlphaComponent(color, [&](float component) {
        return 1.0f - (oneMinusAmount + component * (amount - oneMinusAmount));
    });
    return true;
}

// MARK: - Conversion

auto ToCSS<Invert>::operator()(const Invert& value, const RenderStyle& style) -> CSS::Invert
{
    return { .value = CSS::Invert::Parameter { toCSS(value.value, style) } };
}

auto ToStyle<CSS::Invert>::operator()(const CSS::Invert& value, const BuilderState& state) -> Invert
{
    if (auto parameter = value.value) {
        return { .value = WTF::switchOn(*parameter,
            [&](const CSS::Invert::Parameter::Number& number) -> Invert::Parameter {
                return { toStyle(number, state) };
            },
            [&](const CSS::Invert::Parameter::Percentage& percentage) -> Invert::Parameter {
                return { toStyle(percentage, state).value / 100 };
            }
        ) };
    }
    return { .value = CSSFilterFunctionDescriptor<CSSValueInvert>::defaultValue };
}

// MARK: - Evaluation

auto Evaluation<Invert, Ref<FilterEffect>>::operator()(const Invert& value) -> Ref<FilterEffect>
{
    ColorMatrix<5, 4> invertMatrix = invertColorMatrix(evaluate<float>(value));
    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX, invertMatrix);
}

// MARK: - Platform

auto ToPlatform<Invert>::operator()(const Invert& value) -> Ref<FilterOperation>
{
    return BasicComponentTransferFilterOperation::create(Style::evaluate<double>(value), filterFunctionOperationType<InvertFunction::name>());
}

} // namespace Style
} // namespace WebCore
