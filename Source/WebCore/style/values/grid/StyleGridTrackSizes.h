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

#include "StyleGridTrackSize.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

using GridTrackSizeList = SpaceSeparatedFixedVector<GridTrackSize>;

// Default values for <'grid-auto-columns'>/<'grid-auto-rows'> is 'auto'.
struct GridTrackSizeDefaulter {
    auto operator()() const -> const GridTrackSize&;
    bool operator==(const GridTrackSizeDefaulter&) const = default;
};

// <'grid-auto-columns'>/<'grid-auto-rows'> = <track-size>+
// https://www.w3.org/TR/css-grid-2/#propdef-grid-auto-rows
struct GridTrackSizes : ListOrDefault<GridTrackSizeList, GridTrackSizeDefaulter> {
    using ListOrDefault<GridTrackSizeList, GridTrackSizeDefaulter>::ListOrDefault;

    // Special constructor for use constructing initial 'auto' value.
    GridTrackSizes(CSS::Keyword::Auto)
        : ListOrDefault { DefaultValue }
    {
    }
};

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsRangeLike<WebCore::Style::GridTrackSizes> = true;
template<> inline constexpr auto WebCore::SerializationSeparator<WebCore::Style::GridTrackSizes> = WebCore::SerializationSeparator<typename WebCore::Style::GridTrackSizes::List>;
