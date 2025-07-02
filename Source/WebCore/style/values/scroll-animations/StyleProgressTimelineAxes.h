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

#include "ScrollAxis.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// <single-progress-timeline-axis> = block | inline | x | y
using ProgressTimelineAxis = ScrollAxis;

// <progress-timeline-axis-list> = <single-progress-timeline-axis>#
using ProgressTimelineAxisList = CommaSeparatedFixedVector<ProgressTimelineAxis>;

// Default value for <'scroll-timeline-axis'> and <'view-timeline-axis'> is 'block'.
struct ProgressTimelineAxisDefaulter {
    auto operator()() const -> const ProgressTimelineAxis&
    {
        static constexpr auto value = ProgressTimelineAxis::Block;
        return value;
    }

    constexpr bool operator==(const ProgressTimelineAxisDefaulter&) const = default;
};

// <'scroll-timeline-axis'> = <progress-timeline-axis-list>
// https://drafts.csswg.org/scroll-animations-1/#propdef-scroll-timeline-axis
// <'view-timeline-axis'> = <progress-timeline-axis-list>
// https://drafts.csswg.org/scroll-animations-1/#propdef-view-timeline-axis
struct ProgressTimelineAxes : ListOrDefault<ProgressTimelineAxisList, ProgressTimelineAxisDefaulter> {
    using ListOrDefault<ProgressTimelineAxisList, ProgressTimelineAxisDefaulter>::ListOrDefault;

    // Special constructor for use constructing initial 'block' value.
    ProgressTimelineAxes(CSS::Keyword::Block)
        : ListOrDefault { DefaultValue }
    {
    }
};

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsRangeLike<WebCore::Style::ProgressTimelineAxes> = true;
template<> inline constexpr auto WebCore::SerializationSeparator<WebCore::Style::ProgressTimelineAxes> = WebCore::SerializationSeparator<typename WebCore::Style::ProgressTimelineAxes::List>;
