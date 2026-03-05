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
#include "StyleBlurFunction.h"

#include "CSSBlurFunction.h"
#include "CSSFilterFunctionDescriptor.h"
#include "FEGaussianBlur.h"
#include "FilterOperation.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

Blur Blur::passthroughForInterpolation()
{
    return { .value = CSSFilterFunctionDescriptor<CSSValueBlur>::initialValueForInterpolation };
}

IntOutsets Blur::calculateOutsets(ZoomFactor) const
{
    auto stdDeviation = evaluate<float>(value, ZoomNeeded { });
    return FEGaussianBlur::calculateOutsets({ stdDeviation, stdDeviation });
}

// MARK: - Conversion

auto ToCSS<Blur>::operator()(const Blur& value, const RenderStyle& style) -> CSS::Blur
{
    return { .value = toCSS(value.value, style) };
}

auto ToStyle<CSS::Blur>::operator()(const CSS::Blur& value, const BuilderState& state) -> Blur
{
    if (auto parameter = value.value)
        return { .value = toStyle(*parameter, state) };
    return { .value = CSSFilterFunctionDescriptor<CSSValueBlur>::defaultValue };
}

// MARK: - Evaluation

auto Evaluation<Blur, Ref<FilterEffect>>::operator()(const Blur& value, const RenderStyle&) -> Ref<FilterEffect>
{
    auto stdDeviation = evaluate<float>(value, ZoomNeeded { });
    return FEGaussianBlur::create(stdDeviation, stdDeviation, EdgeModeType::None);
}

// MARK: - Platform

auto ToPlatform<Blur>::operator()(const Blur& value, const RenderStyle&) -> Ref<FilterOperation>
{
    return BlurFilterOperation::create(Style::evaluate<float>(value, ZoomNeeded { }));
}

} // namespace Style
} // namespace WebCore
