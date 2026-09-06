/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleTextDecorationThickness.h"

#include "CSSKeywordValue.h"
#include "FontMetrics.h"
#include "StyleBuilderChecking.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

static constexpr float textDecorationBaseFontSize = 16;

float TextDecorationThickness::resolve(const Style::ComputedStyle& style) const
{
    if (isAuto())
        return style.usedFontSize() / textDecorationBaseFontSize;
    if (isFromFont())
        return style.metricsOfPrimaryFont().underlineThickness().value_or(0);
    return Style::evaluate<float>(*tryNumeric(), style.usedFontSize(), style.usedZoomForLength());
}

// MARK: - Conversion

auto CSSValueConversion<TextDecorationThickness>::operator()(BuilderState& state, const CSSValue& value) -> TextDecorationThickness
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueAuto:
            return CSS::Keyword::Auto { };
        case CSSValueFromFont:
            return CSS::Keyword::FromFont { };
        default:
            break;
        }

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Auto { };
    }

    return toStyleFromCSSValue<TextDecorationThickness::Numeric>(state, value);
}

} // namespace Style
} // namespace WebCore
