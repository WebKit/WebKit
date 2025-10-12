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
#include "StylePageSize.h"

#include "CSSValuePair.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

static PageSize pageSizeFromName(BuilderState& state, const CSSPrimitiveValue& pageSizeName, RefPtr<const CSSPrimitiveValue> pageOrientation)
{
    auto mmLength = [](double mm) {
        return Length<CSS::Nonnegative>(CSS::pixelsPerMm * mm);
    };

    auto inchLength = [](double inch) {
        return Length<CSS::Nonnegative>(CSS::pixelsPerInch * inch);
    };

    static constexpr Length<CSS::Nonnegative> a5Width(mmLength(148));
    static constexpr Length<CSS::Nonnegative> a5Height(mmLength(210));
    static constexpr Length<CSS::Nonnegative> a4Width(mmLength(210));
    static constexpr Length<CSS::Nonnegative> a4Height(mmLength(297));
    static constexpr Length<CSS::Nonnegative> a3Width(mmLength(297));
    static constexpr Length<CSS::Nonnegative> a3Height(mmLength(420));
    static constexpr Length<CSS::Nonnegative> b5Width(mmLength(176));
    static constexpr Length<CSS::Nonnegative> b5Height(mmLength(250));
    static constexpr Length<CSS::Nonnegative> b4Width(mmLength(250));
    static constexpr Length<CSS::Nonnegative> b4Height(mmLength(353));
    static constexpr Length<CSS::Nonnegative> jisB5Width(mmLength(182));
    static constexpr Length<CSS::Nonnegative> jisB5Height(mmLength(257));
    static constexpr Length<CSS::Nonnegative> jisB4Width(mmLength(257));
    static constexpr Length<CSS::Nonnegative> jisB4Height(mmLength(364));
    static constexpr Length<CSS::Nonnegative> letterWidth(inchLength(8.5));
    static constexpr Length<CSS::Nonnegative> letterHeight(inchLength(11));
    static constexpr Length<CSS::Nonnegative> legalWidth(inchLength(8.5));
    static constexpr Length<CSS::Nonnegative> legalHeight(inchLength(14));
    static constexpr Length<CSS::Nonnegative> ledgerWidth(inchLength(11));
    static constexpr Length<CSS::Nonnegative> ledgerHeight(inchLength(17));

    Style::Length<CSS::Nonnegative> width { 0 };
    Style::Length<CSS::Nonnegative> height { 0 };

    switch (pageSizeName.valueID()) {
    case CSSValueA5:
        width = a5Width;
        height = a5Height;
        break;
    case CSSValueA4:
        width = a4Width;
        height = a4Height;
        break;
    case CSSValueA3:
        width = a3Width;
        height = a3Height;
        break;
    case CSSValueB5:
        width = b5Width;
        height = b5Height;
        break;
    case CSSValueB4:
        width = b4Width;
        height = b4Height;
        break;
    case CSSValueJisB5:
        width = jisB5Width;
        height = jisB5Height;
        break;
    case CSSValueJisB4:
        width = jisB4Width;
        height = jisB4Height;
        break;
    case CSSValueLetter:
        width = letterWidth;
        height = letterHeight;
        break;
    case CSSValueLegal:
        width = legalWidth;
        height = legalHeight;
        break;
    case CSSValueLedger:
        width = ledgerWidth;
        height = ledgerHeight;
        break;
    default:
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Auto { };
    }

    if (pageOrientation) {
        switch (pageOrientation->valueID()) {
        case CSSValueLandscape:
            std::swap(width, height);
            break;
        case CSSValuePortrait:
            // Nothing to do.
            break;
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    }

    return PageSize::Lengths { width, height };
}

auto CSSValueConversion<PageSize>::operator()(BuilderState& state, const CSSValue& value) -> PageSize
{
    if (RefPtr pair = dynamicDowncast<CSSValuePair>(value)) {
        // <length [0,∞]>{2} | [ <page-size> [ portrait | landscape ] ]
        RefPtr first = requiredDowncast<CSSPrimitiveValue>(state, pair->first());
        if (!first)
            return CSS::Keyword::Auto { };
        RefPtr second = requiredDowncast<CSSPrimitiveValue>(state, pair->second());
        if (!second)
            return CSS::Keyword::Auto { };

        if (first->isLength()) {
            // <length [0,∞]>{2}
            if (!second->isLength()) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Auto { };
            }

            auto conversionData = state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f);
            return PageSize::Lengths {
                toStyleFromCSSValue<Length<CSS::Nonnegative>>(conversionData, *first),
                toStyleFromCSSValue<Length<CSS::Nonnegative>>(conversionData, *second),
            };
        }

        // [ <page-size> [ portrait | landscape ] ]
        // The value order is guaranteed. See CSSParser::parseSizeParameter.
        return pageSizeFromName(state, *first, second);
    }

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        // <length [0,∞]> | auto | <page-size> | [ portrait | landscape]
        if (primitiveValue->isLength()) {
            // <length [0,∞]>
            auto conversionData = state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f);
            auto length = toStyleFromCSSValue<Length<CSS::Nonnegative>>(conversionData, *primitiveValue);
            return PageSize::Lengths { length, length };
        }

        switch (primitiveValue->valueID()) {
        case CSSValueAuto:
            return CSS::Keyword::Auto { };
        case CSSValuePortrait:
            return CSS::Keyword::Portrait { };
        case CSSValueLandscape:
            return CSS::Keyword::Landscape { };
        default:
            return pageSizeFromName(state, *primitiveValue, nullptr);
        }
    }

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::Auto { };
}

} // namespace Style
} // namespace WebCore
