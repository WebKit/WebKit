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
#include "StyleFontVariantNumeric.h"

#include "CSSPropertyParserConsumer+Font.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<FontVariantNumeric>::operator()(BuilderState& state, const CSSValue& value) -> FontVariantNumeric
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

    auto figure = FontVariantNumericFigure::Normal;
    auto spacing = FontVariantNumericSpacing::Normal;
    auto fraction = FontVariantNumericFraction::Normal;
    auto ordinal = FontVariantNumericOrdinal::Normal;
    auto slashedZero = FontVariantNumericSlashedZero::Normal;

    for (Ref item : *list) {
        switch (item->valueID()) {
        case CSSValueLiningNums:
            figure = FontVariantNumericFigure::LiningNumbers;
            break;
        case CSSValueOldstyleNums:
            figure = FontVariantNumericFigure::OldStyleNumbers;
            break;
        case CSSValueProportionalNums:
            spacing = FontVariantNumericSpacing::ProportionalNumbers;
            break;
        case CSSValueTabularNums:
            spacing = FontVariantNumericSpacing::TabularNumbers;
            break;
        case CSSValueDiagonalFractions:
            fraction = FontVariantNumericFraction::DiagonalFractions;
            break;
        case CSSValueStackedFractions:
            fraction = FontVariantNumericFraction::StackedFractions;
            break;
        case CSSValueOrdinal:
            ordinal = FontVariantNumericOrdinal::Yes;
            break;
        case CSSValueSlashedZero:
            slashedZero = FontVariantNumericSlashedZero::Yes;
            break;
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    }

    return FontVariantNumeric::Platform { figure, spacing, fraction, ordinal, slashedZero };
}

} // namespace Style
} // namespace WebCore
