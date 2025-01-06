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
#include "StyleBlurFunction.h"

#include "CSSBlurFunction.h"
#include "CSSFilterFunctionDescriptor.h"
#include "FilterOperation.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

CSS::Blur toCSSBlur(Ref<BlurFilterOperation> operation, const RenderStyle& style)
{
    return { CSS::Blur::Parameter { toCSS(Length<CSS::Nonnegative> { operation->stdDeviation().value() }, style) } };
}

Ref<FilterOperation> createFilterOperation(const CSS::Blur& filter, const Document&, RenderStyle&, const CSSToLengthConversionData& conversionData)
{
    WebCore::Length stdDeviation;
    if (auto parameter = filter.value)
        stdDeviation = WebCore::Length { toStyle(*parameter, conversionData).value, LengthType::Fixed };
    else
        stdDeviation = WebCore::Length { filterFunctionDefaultValue<CSS::BlurFunction::name>().value, LengthType::Fixed };

    return BlurFilterOperation::create(WTFMove(stdDeviation));
}

} // namespace Style
} // namespace WebCore
