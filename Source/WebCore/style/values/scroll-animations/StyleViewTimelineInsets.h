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

#include "StyleValueTypes.h"
#include "WebAnimationTypes.h"

namespace WebCore {
namespace Style {

// <single-view-timeline-inset-item> = [ [ auto | <length-percentage> ]{1,2} ]
using ViewTimelineInsetItem = WebCore::ViewTimelineInsetItem;

// <view-timeline-inset-list> = <single-view-timeline-inset-item>#
using ViewTimelineInsetList = CommaSeparatedFixedVector<ViewTimelineInsetItem>;

// Default value for <'view-timeline-inset'> is 'auto'.
struct ViewTimelineInsetDefaulter {
    auto operator()() const -> const ViewTimelineInsetItem&;
    bool operator==(const ViewTimelineInsetDefaulter&) const = default;
};

// <'view-timeline-inset'> = <view-timeline-inset-list>
// https://drafts.csswg.org/scroll-animations-1/#propdef-view-timeline-inset
struct ViewTimelineInsets : ListOrDefault<ViewTimelineInsetList, ViewTimelineInsetDefaulter> {
    using ListOrDefault<ViewTimelineInsetList, ViewTimelineInsetDefaulter>::ListOrDefault;

    // Special constructor for use constructing initial 'auto' value.
    ViewTimelineInsets(CSS::Keyword::Auto)
        : ListOrDefault { DefaultValue }
    {
    }
};

// MARK: - Conversion

template<> struct CSSValueConversion<ViewTimelineInsetItem> { auto operator()(BuilderState&, const CSSValue&) -> ViewTimelineInsetItem; };
template<> struct CSSValueConversion<ViewTimelineInsets> { auto operator()(BuilderState&, const CSSValue&) -> ViewTimelineInsets; };

template<> struct CSSValueCreation<ViewTimelineInsetItem> { auto operator()(CSSValuePool&, const RenderStyle&, const ViewTimelineInsetItem&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<ViewTimelineInsetItem> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const ViewTimelineInsetItem&); };

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsRangeLike<WebCore::Style::ViewTimelineInsets> = true;
template<> inline constexpr auto WebCore::SerializationSeparator<WebCore::Style::ViewTimelineInsets> = WebCore::SerializationSeparator<typename WebCore::Style::ViewTimelineInsets::List>;
