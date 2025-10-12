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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleFontVariantEastAsian.h"

#include "CSSPropertyParserConsumer+Font.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<FontVariantEastAsian>::operator()(BuilderState& state, const CSSValue& value) -> FontVariantEastAsian
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, value);
    if (!list)
        return CSS::Keyword::Normal { };

    auto variant = FontVariantEastAsianVariant::Normal;
    auto width = FontVariantEastAsianWidth::Normal;
    auto ruby = FontVariantEastAsianRuby::Normal;

    for (Ref item : *list) {
        switch (item->valueID()) {
        case CSSValueJis78:
            variant = FontVariantEastAsianVariant::Jis78;
            break;
        case CSSValueJis83:
            variant = FontVariantEastAsianVariant::Jis83;
            break;
        case CSSValueJis90:
            variant = FontVariantEastAsianVariant::Jis90;
            break;
        case CSSValueJis04:
            variant = FontVariantEastAsianVariant::Jis04;
            break;
        case CSSValueSimplified:
            variant = FontVariantEastAsianVariant::Simplified;
            break;
        case CSSValueTraditional:
            variant = FontVariantEastAsianVariant::Traditional;
            break;
        case CSSValueFullWidth:
            width = FontVariantEastAsianWidth::Full;
            break;
        case CSSValueProportionalWidth:
            width = FontVariantEastAsianWidth::Proportional;
            break;
        case CSSValueRuby:
            ruby = FontVariantEastAsianRuby::Yes;
            break;
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    }

    return FontVariantEastAsian::Platform { variant, width, ruby };
}

} // namespace Style
} // namespace WebCore
