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

#include "RenderStyle.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// <'text-emphasis-style'> = none | [ [ filled | open ] || [ dot | circle | double-circle | triangle | sesame ] ] | <string>
// https://drafts.csswg.org/css-text-decor/#propdef-text-emphasis-style

struct TextEmphasisStyle {
    TextEmphasisMark mark;
    TextEmphasisFill fill;
    AtomString customMark;

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        switch (mark) {
        case TextEmphasisMark::Auto:
            ASSERT_NOT_REACHED();
#if !ASSERT_ENABLED
            [[fallthrough]];
#endif
        case TextEmphasisMark::None:
            return functor(CSS::Keyword::None { });
        case TextEmphasisMark::Custom:
            return functor(customMark);
        case TextEmphasisMark::Dot:
            if (fill == TextEmphasisFill::Filled)
                return functor(CSS::Keyword::Dot { });
            return functor(SpaceSeparatedTuple { CSS::Keyword::Open { }, CSS::Keyword::Dot { } });
        case TextEmphasisMark::Circle:
            if (fill == TextEmphasisFill::Filled)
                return functor(CSS::Keyword::Circle { });
            return functor(SpaceSeparatedTuple { CSS::Keyword::Open { }, CSS::Keyword::Circle { } });
        case TextEmphasisMark::DoubleCircle:
            if (fill == TextEmphasisFill::Filled)
                return functor(CSS::Keyword::DoubleCircle { });
            return functor(SpaceSeparatedTuple { CSS::Keyword::Open { }, CSS::Keyword::DoubleCircle { } });
        case TextEmphasisMark::Triangle:
            if (fill == TextEmphasisFill::Filled)
                return functor(CSS::Keyword::Triangle { });
            return functor(SpaceSeparatedTuple { CSS::Keyword::Open { }, CSS::Keyword::Triangle { } });
        case TextEmphasisMark::Sesame:
            if (fill == TextEmphasisFill::Filled)
                return functor(CSS::Keyword::Sesame { });
            return functor(SpaceSeparatedTuple { CSS::Keyword::Open { }, CSS::Keyword::Sesame { } });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const TextEmphasisStyle&) const = default;
};

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::TextEmphasisStyle> = true;
