/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StylePerspective.h"

#include "CSSKeywordValue.h"
#include "StyleBuilderChecking.h"

namespace WebCore::Style {

// MARK: - Conversion

auto CSSValueConversion<Perspective>::operator()(BuilderState& state, const CSSValue& value) -> Perspective
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueNone:
            return CSS::Keyword::None { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }
    }

    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::None { };

    auto& conversionData = state.cssToLengthConversionData();

    // NOTE: The isNumber() case below is only possible due to the `-webkit-perspective` legacy shorthand
    // which extends the grammar to `<'perspective'> | <number [0,inf]>`.

    float perspective = -1;
    if (primitiveValue->isLength())
        perspective = primitiveValue->resolveAsLength<float>(conversionData);
    else if (primitiveValue->isNumber())
        perspective = primitiveValue->resolveAsNumber<float>(conversionData) * conversionData.zoom();
    else
        ASSERT_NOT_REACHED();

    // FIXME: This should probably clamp to 0, like other numeric values would, rather than return CSS::Keyword::None.
    if (perspective < 0)
        return CSS::Keyword::None { };

    return Style::Perspective::Length { perspective };
}

} // namespace WebCore::Style
