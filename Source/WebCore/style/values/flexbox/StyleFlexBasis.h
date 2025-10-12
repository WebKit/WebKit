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

#include <WebCore/StyleLengthWrapper.h>

namespace WebCore {
namespace Style {

struct PreferredSize;

// <'flex-basis'> = content | <‘width’>
// https://drafts.csswg.org/css-flexbox/#propdef-flex-basis
struct FlexBasis : LengthWrapperBase<LengthPercentage<CSS::Nonnegative>, CSS::Keyword::Content, CSS::Keyword::Auto, CSS::Keyword::MinContent, CSS::Keyword::MaxContent, CSS::Keyword::FitContent, CSS::Keyword::WebkitFillAvailable, CSS::Keyword::Intrinsic, CSS::Keyword::MinIntrinsic> {
    using Base::Base;

    // `FlexBasis` is a superset of `PreferredSize` and therefore this conversion can fail when type is `content`.
    std::optional<PreferredSize> tryPreferredSize() const;

    ALWAYS_INLINE bool isContent() const { return holdsAlternative<CSS::Keyword::Content>(); }
    ALWAYS_INLINE bool isAuto() const { return holdsAlternative<CSS::Keyword::Auto>(); }
    ALWAYS_INLINE bool isMinContent() const { return holdsAlternative<CSS::Keyword::MinContent>(); }
    ALWAYS_INLINE bool isMaxContent() const { return holdsAlternative<CSS::Keyword::MaxContent>(); }
    ALWAYS_INLINE bool isFitContent() const { return holdsAlternative<CSS::Keyword::FitContent>(); }
    ALWAYS_INLINE bool isFillAvailable() const { return holdsAlternative<CSS::Keyword::WebkitFillAvailable>(); }
    ALWAYS_INLINE bool isIntrinsicKeyword() const { return holdsAlternative<CSS::Keyword::Intrinsic>(); }
    ALWAYS_INLINE bool isMinIntrinsic() const { return holdsAlternative<CSS::Keyword::MinIntrinsic>(); }

    ALWAYS_INLINE bool isIntrinsic() const
    {
        return holdsAlternative<CSS::Keyword::MinContent>()
            || holdsAlternative<CSS::Keyword::MaxContent>()
            || holdsAlternative<CSS::Keyword::WebkitFillAvailable>()
            || holdsAlternative<CSS::Keyword::FitContent>();
    }
    ALWAYS_INLINE bool isLegacyIntrinsic() const
    {
        return holdsAlternative<CSS::Keyword::Intrinsic>()
            || holdsAlternative<CSS::Keyword::MinIntrinsic>();
    }
    ALWAYS_INLINE bool isIntrinsicOrLegacyIntrinsicOrAuto() const
    {
        return holdsAlternative<CSS::Keyword::MinContent>()
            || holdsAlternative<CSS::Keyword::MaxContent>()
            || holdsAlternative<CSS::Keyword::WebkitFillAvailable>()
            || holdsAlternative<CSS::Keyword::FitContent>()
            || holdsAlternative<CSS::Keyword::Intrinsic>()
            || holdsAlternative<CSS::Keyword::MinIntrinsic>()
            || holdsAlternative<CSS::Keyword::Auto>();
    }
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::FlexBasis)
