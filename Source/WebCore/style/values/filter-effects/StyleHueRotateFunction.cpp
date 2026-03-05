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
#include "StyleHueRotateFunction.h"

#include "CSSFilterFunctionDescriptor.h"
#include "CSSHueRotateFunction.h"
#include "FEColorMatrix.h"
#include "FilterOperation.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

HueRotate HueRotate::passthroughForInterpolation()
{
    return { .value = CSSFilterFunctionDescriptor<CSSValueHueRotate>::initialValueForInterpolation };
}

bool HueRotate::transformColor(SRGBA<float>& color) const
{
    color = makeFromComponentsClamping<SRGBA<float>>(hueRotateColorMatrix(evaluate<float>(value)).transformedColorComponents(asColorComponents(color.resolved())));
    return true;
}

// MARK: - Conversion

auto ToCSS<HueRotate>::operator()(const HueRotate& value, const RenderStyle& style) -> CSS::HueRotate
{
    return { .value = toCSS(value.value, style) };
}

auto ToStyle<CSS::HueRotate>::operator()(const CSS::HueRotate& value, const BuilderState& state) -> HueRotate
{
    if (auto parameter = value.value)
        return { .value = toStyle(*parameter, state) };
    return { .value = CSSFilterFunctionDescriptor<CSSValueHueRotate>::defaultValue };
}

// MARK: - Evaluation

auto Evaluation<HueRotate, Ref<FilterEffect>>::operator()(const HueRotate& value) -> Ref<FilterEffect>
{
    auto inputParameters = Vector<float> { evaluate<float>(value) };
    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE, WTF::move(inputParameters));
}

// MARK: - Platform

auto ToPlatform<HueRotate>::operator()(const HueRotate& value) -> Ref<FilterOperation>
{
    return BasicColorMatrixFilterOperation::create(Style::evaluate<double>(value), filterFunctionOperationType<HueRotateFunction::name>());
}

} // namespace Style
} // namespace WebCore
