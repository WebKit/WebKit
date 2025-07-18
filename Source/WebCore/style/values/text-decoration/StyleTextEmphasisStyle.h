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

#pragma once

#include "RenderStyleConstants.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// <'text-emphasis-style'> = none | [ [ filled | open ] || [ dot | circle | double-circle | triangle | sesame ] ] | <string>
// https://drafts.csswg.org/css-text-decor/#propdef-text-emphasis-style
struct TextEmphasisStyle {
    struct Shape {
        TextEmphasisFill fill { TextEmphasisFill::Filled };
        TextEmphasisMark mark;

        template<typename... F> decltype(auto) switchOn(F&&... f) const
        {
            auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

            if (fill == TextEmphasisFill::Filled)
                return visitor(mark);
            return visitor(SpaceSeparatedTuple { fill, mark });
        }

        bool operator==(const Shape&) const = default;
    };

    Variant<CSS::Keyword::None, Shape, AtomString> value;

    TextEmphasisStyle(CSS::Keyword::None keyword)
        : value { keyword }
    {
    }

    TextEmphasisStyle(Shape shape)
        : value { shape }
    {
    }

    TextEmphasisStyle(AtomString&& customMark)
        : value { WTFMove(customMark) }
    {
    }

    bool isNone() const { return holdsAlternative<CSS::Keyword::None>(value); }

    // String representation of the mark.
    const AtomString& markString() const;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const TextEmphasisStyle&) const = default;
};

// MARK: - Conversion

template<> struct CSSValueConversion<TextEmphasisStyle> { auto operator()(BuilderState&, const CSSValue&) -> TextEmphasisStyle; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextEmphasisStyle::Shape);
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextEmphasisStyle);
