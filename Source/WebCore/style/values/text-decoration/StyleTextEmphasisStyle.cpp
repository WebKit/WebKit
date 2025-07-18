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
#include "StyleTextEmphasisStyle.h"

#include "CSSPrimitiveValueMappings.h"
#include "CSSValueList.h"
#include "RenderStyle.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveKeyword+Serialization.h"
#include "WritingMode.h"
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
namespace Style {

static TextEmphasisMark defaultTextEmphasisMark(WritingMode mode)
{
    // "If only filled or open is specified, the shape keyword computes to circle in horizontal typographic modes and sesame in vertical typographic modes." - https://drafts.csswg.org/css-text-decor/#propdef-text-emphasis-style

    // FIXME: The spec states we should use `circle`, not `dot` for horizontal typographic modes here - https://bugs.webkit.org/show_bug.cgi?id=295641.

    return mode.isVerticalTypographic() ? TextEmphasisMark::Sesame : TextEmphasisMark::Dot;
}

static TextEmphasisStyle::Shape defaultTextEmphasisShape(WritingMode writingMode, TextEmphasisFill fill)
{
    return { .fill = fill, .mark = defaultTextEmphasisMark(writingMode) };
}

static const AtomString& markStringFromShape(const TextEmphasisStyle::Shape& markShape)
{
    switch (markShape.mark) {
    case TextEmphasisMark::Dot: {
        static MainThreadNeverDestroyed<const AtomString> filledDotString(span(bullet));
        static MainThreadNeverDestroyed<const AtomString> openDotString(span(whiteBullet));
        return markShape.fill == TextEmphasisFill::Filled ? filledDotString : openDotString;
    }
    case TextEmphasisMark::Circle: {
        static MainThreadNeverDestroyed<const AtomString> filledCircleString(span(blackCircle));
        static MainThreadNeverDestroyed<const AtomString> openCircleString(span(whiteCircle));
        return markShape.fill == TextEmphasisFill::Filled ? filledCircleString : openCircleString;
    }
    case TextEmphasisMark::DoubleCircle: {
        static MainThreadNeverDestroyed<const AtomString> filledDoubleCircleString(span(fisheye));
        static MainThreadNeverDestroyed<const AtomString> openDoubleCircleString(span(bullseye));
        return markShape.fill == TextEmphasisFill::Filled ? filledDoubleCircleString : openDoubleCircleString;
    }
    case TextEmphasisMark::Triangle: {
        static MainThreadNeverDestroyed<const AtomString> filledTriangleString(span(blackUpPointingTriangle));
        static MainThreadNeverDestroyed<const AtomString> openTriangleString(span(whiteUpPointingTriangle));
        return markShape.fill == TextEmphasisFill::Filled ? filledTriangleString : openTriangleString;
    }
    case TextEmphasisMark::Sesame: {
        static MainThreadNeverDestroyed<const AtomString> filledSesameString(span(sesameDot));
        static MainThreadNeverDestroyed<const AtomString> openSesameString(span(whiteSesameDot));
        return markShape.fill == TextEmphasisFill::Filled ? filledSesameString : openSesameString;
    }
    }

    ASSERT_NOT_REACHED();
    return emptyAtom();
}

const AtomString& TextEmphasisStyle::markString() const
{
    return WTF::switchOn(value,
        [&](const CSS::Keyword::None&) -> const AtomString& {
            return nullAtom();
        },
        [&](const Shape& shape) -> const AtomString& {
            return markStringFromShape(shape);
        },
        [&](const AtomString& customMark) -> const AtomString& {
            return customMark;
        }
    );
}

// MARK: - Conversion

auto CSSValueConversion<TextEmphasisStyle>::operator()(BuilderState& state, const CSSValue& value) -> TextEmphasisStyle
{
    if (RefPtr list = dynamicDowncast<CSSValueList>(value)) {
        if (list->size() != 2) [[unlikely]] {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }

        std::optional<TextEmphasisFill> fill;
        std::optional<TextEmphasisMark> mark;
        for (Ref item : *list) {
            auto valueID = item->valueID();
            if (valueID == CSSValueFilled || valueID == CSSValueOpen)
                fill = fromCSSValueID<TextEmphasisFill>(valueID);
            else
                mark = fromCSSValueID<TextEmphasisMark>(valueID);
        }
        if (!fill || !mark) [[unlikely]] {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }
        return TextEmphasisStyle::Shape { .fill = *fill, .mark = *mark };
    }

    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::None { };

    switch (primitiveValue->valueID()) {
    case CSSValueInvalid:
        break;

    case CSSValueNone:
        return CSS::Keyword::None { };

    case CSSValueFilled:
    case CSSValueOpen:
        return defaultTextEmphasisShape(state.style().writingMode(), fromCSSValue<TextEmphasisFill>(*primitiveValue));

    default:
        return TextEmphasisStyle::Shape { .mark = fromCSSValue<TextEmphasisMark>(*primitiveValue) };
    }

    if (primitiveValue->isString())
        return AtomString { primitiveValue->stringValue() };

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::None { };
}

} // namespace Style
} // namespace WebCore
