/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
#include <WebCore/StyleValueTypes.h>
#include <WebCore/WebAnimationTypes.h>

namespace WebCore {

class CSSPrimitiveValue;
class CSSValuePair;

namespace Style {

// <single-view-timeline-inset-item> = [ [ auto | <length-percentage> ]{1,2} ]
// https://drafts.csswg.org/scroll-animations-1/#propdef-view-timeline-inset
struct ViewTimelineInsetItem {
    struct Length : LengthWrapperBase<LengthPercentage<>, CSS::Keyword::Auto> {
        using Base::Base;

        bool isAuto() const { return holdsAlternative<CSS::Keyword::Auto>(); }
    };

    MinimallySerializingSpaceSeparatedPair<Length> value;

    ViewTimelineInsetItem(CSS::Keyword::Auto keyword)
        : value { Length { keyword }, Length { keyword } }
    {
    }

    ViewTimelineInsetItem(Length&& length)
        : value { length, length }
    {
    }

    ViewTimelineInsetItem(Length&& first, Length&& second)
        : value { WTF::move(first), WTF::move(second) }
    {
    }

    const Length& start() const { return value.first(); }
    const Length& end() const { return value.second(); }

    bool operator==(const ViewTimelineInsetItem&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(ViewTimelineInsetItem, value);

// MARK: - Conversion

template<> struct CSSValueConversion<ViewTimelineInsetItem> {
    auto operator()(BuilderState&, const CSSValue&) -> ViewTimelineInsetItem;
    auto operator()(BuilderState&, const CSSPrimitiveValue&) -> ViewTimelineInsetItem;
    auto operator()(BuilderState&, const CSSValuePair&) -> ViewTimelineInsetItem;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ViewTimelineInsetItem::Length)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::ViewTimelineInsetItem)
