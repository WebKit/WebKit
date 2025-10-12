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
#include "StyleFontWeight.h"

#include "CSSPropertyParserConsumer+Font.h"
#include "FontCascadeDescription.h"
#include "RenderStyle.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "SystemFontDatabase.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<FontWeight>::operator()(BuilderState& state, const CSSValue& value) -> FontWeight
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Normal { };

    switch (auto valueID = primitiveValue->valueID(); valueID) {
    case CSSValueInvalid:
        return toStyleFromCSSValue<FontWeight::Number>(state, *primitiveValue);
    case CSSValueNormal:
        return CSS::Keyword::Normal { };
    case CSSValueBold:
        return CSS::Keyword::Bold { };
    case CSSValueBolder:
        return FontCascadeDescription::bolderWeight(state.parentStyle().fontDescription().weight());
    case CSSValueLighter:
        return FontCascadeDescription::lighterWeight(state.parentStyle().fontDescription().weight());
    default:
        if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
            return SystemFontDatabase::singleton().systemFontShorthandWeight(CSSPropertyParserHelpers::lowerFontShorthand(valueID));

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Normal { };
    }
}

// MARK: Blending

auto Blending<FontWeight>::blend(const FontWeight& a, const FontWeight& b, const BlendingContext& context) -> FontWeight
{
    return Style::blend(a.number(), b.number(), context);
}

} // namespace Style
} // namespace WebCore
