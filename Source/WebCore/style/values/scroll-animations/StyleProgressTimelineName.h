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

namespace WebCore {
namespace Style {

// <single-progress-timeline-name> = none | <dashed-ident>
// FIXME: This should actually model the union of CSS::Keyword::None and CustomIdentifier to match the spec - https://bugs.webkit.org/show_bug.cgi?id=295467.
struct ProgressTimelineName {
    CustomIdentifier value;

    bool operator==(const ProgressTimelineName&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(ProgressTimelineName, value);

// <progress-timeline-name-list> = <single-progress-timeline-name>#
using ProgressTimelineNameList = CommaSeparatedFixedVector<ProgressTimelineName>;

// <'scroll-timeline-name'> = <progress-timeline-name-list>
// https://drafts.csswg.org/scroll-animations-1/#propdef-scroll-timeline-name
// <'view-timeline-name'> = <progress-timeline-name-list>
// https://drafts.csswg.org/scroll-animations-1/#propdef-view-timeline-name
struct ProgressTimelineNames : ListOrNone<ProgressTimelineNameList> { using ListOrNone<ProgressTimelineNameList>::ListOrNone; };

// MARK: - Conversion

template<> struct CSSValueConversion<ProgressTimelineName> { auto operator()(BuilderState&, const CSSValue&) -> ProgressTimelineName; };

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::ProgressTimelineName, 1)
template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::ProgressTimelineNames> = true;
