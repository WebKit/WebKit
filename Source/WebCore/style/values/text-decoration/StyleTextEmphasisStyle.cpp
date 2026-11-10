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

#include "CSSValueList.h"
#include "StyleBuilderChecking.h"
#include "StyleComputedStyle.h"
#include "StyleKeyword+CSSValueConversion.h"
#include "StyleKeyword+CSSValueCreation.h"
#include "StyleKeyword+Serialization.h"
#include "WritingMode.h"
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
namespace Style {

NODELETE static TextEmphasisMark defaultTextEmphasisMark(WritingMode mode)
{
    // "If only filled or open is specified, the shape keyword computes to circle in horizontal typographic modes and sesame in vertical typographic modes." - https://drafts.csswg.org/css-text-decor/#propdef-text-emphasis-style

    return mode.isVerticalTypographic() ? TextEmphasisMark::Sesame : TextEmphasisMark::Circle;
}

NODELETE static TextEmphasisStyle::Shape defaultTextEmphasisShape(WritingMode writingMode, TextEmphasisFill fill)
{
    return { .fill = fill, .mark = defaultTextEmphasisMark(writingMode) };
}

static const RefPtr<AtomStringImpl>& markStringFromShape(const TextEmphasisStyle::Shape& markShape)
{
    switch (markShape.mark) {
    case TextEmphasisMark::Dot: {
        static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> filledDotString(AtomStringImpl::add(span(bullet)));
        static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> openDotString(AtomStringImpl::add(span(whiteBullet)));
        return markShape.fill == TextEmphasisFill::Filled ? filledDotString : openDotString;
    }
    case TextEmphasisMark::Circle: {
        static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> filledCircleString(AtomStringImpl::add(span(blackCircle)));
        static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> openCircleString(AtomStringImpl::add(span(whiteCircle)));
        return markShape.fill == TextEmphasisFill::Filled ? filledCircleString : openCircleString;
    }
    case TextEmphasisMark::DoubleCircle: {
        static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> filledDoubleCircleString(AtomStringImpl::add(span(fisheye)));
        static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> openDoubleCircleString(AtomStringImpl::add(span(bullseye)));
        return markShape.fill == TextEmphasisFill::Filled ? filledDoubleCircleString : openDoubleCircleString;
    }
    case TextEmphasisMark::Triangle: {
        static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> filledTriangleString(AtomStringImpl::add(span(blackUpPointingTriangle)));
        static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> openTriangleString(AtomStringImpl::add(span(whiteUpPointingTriangle)));
        return markShape.fill == TextEmphasisFill::Filled ? filledTriangleString : openTriangleString;
    }
    case TextEmphasisMark::Sesame: {
        static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> filledSesameString(AtomStringImpl::add(span(sesameDot)));
        static MainThreadNeverDestroyed<const RefPtr<AtomStringImpl>> openSesameString(AtomStringImpl::add(span(whiteSesameDot)));
        return markShape.fill == TextEmphasisFill::Filled ? filledSesameString : openSesameString;
    }
    }

    ASSERT_NOT_REACHED();
    return emptyAtom();
}

const RefPtr<AtomStringImpl>& TextEmphasisStyle::markString() const
{
    return WTF::switchOn(value,
        [&](const CSS::Keyword::None&) -> const RefPtr<AtomStringImpl>& {
            return nullAtom();
        },
        [&](const Shape& shape) -> const RefPtr<AtomStringImpl>& {
            return markStringFromShape(shape);
        },
        [&](const CustomMark& customMark) -> const RefPtr<AtomStringImpl>& {
            return customMark.value;
        }
    );
}

// MARK: - Conversion

auto CSSValueConversion<TextEmphasisStyle>::operator()(BuilderState& state, const CSSValue& value) -> TextEmphasisStyle
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (auto valueID = keywordValue->valueID(); valueID) {
        case CSSValueID::None:
            return CSS::Keyword::None { };

        case CSSValueID::Filled:
        case CSSValueID::Open:
            return defaultTextEmphasisShape(state.style().writingMode(), fromCSSValueID<TextEmphasisFill>(valueID));

        default:
            return TextEmphasisStyle::Shape { .mark = fromCSSValueID<TextEmphasisMark>(valueID) };
        }
    }

    if (RefPtr list = dynamicDowncast<CSSValueList>(value)) {
        if (list->size() != 2) [[unlikely]] {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }

        std::optional<TextEmphasisFill> fill;
        std::optional<TextEmphasisMark> mark;
        for (Ref item : *list) {
            RefPtr keywordValue = requiredDowncast<CSSKeywordValue>(state, item);
            if (!keywordValue)
                return CSS::Keyword::None { };

            switch (auto valueID = keywordValue->valueID(); valueID) {
            case CSSValueID::Filled:
            case CSSValueID::Open:
                fill = fromCSSValueID<TextEmphasisFill>(valueID);
                break;
            default:
                mark = fromCSSValueID<TextEmphasisMark>(valueID);
                break;
            }
        }
        if (!fill || !mark) [[unlikely]] {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }
        return TextEmphasisStyle::Shape { .fill = *fill, .mark = *mark };
    }

    return TextEmphasisStyle::CustomMark { AtomStringImpl::add(toStyleFromCSSValue<String>(state, value).value) };
}

} // namespace Style
} // namespace WebCore
