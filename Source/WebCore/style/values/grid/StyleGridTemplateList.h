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

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleGridNamedLinesMap.h>
#include <WebCore/StyleGridOrderedNamedLinesMap.h>
#include <WebCore/StyleGridTrackSize.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace Style {

using RepeatEntry = Variant<GridTrackSize, Vector<String>>;
using RepeatTrackList = Vector<RepeatEntry>;

struct GridTrackEntryRepeat {
    unsigned repeats;
    RepeatTrackList list;

    bool operator==(const GridTrackEntryRepeat&) const = default;
};

struct GridTrackEntryAutoRepeat {
    AutoRepeatType type;
    RepeatTrackList list;

    bool operator==(const GridTrackEntryAutoRepeat&) const = default;
};

struct GridTrackEntrySubgrid { bool operator==(const GridTrackEntrySubgrid&) const = default; };

using GridTrackEntry = Variant<
    GridTrackSize,
    Vector<String>,
    GridTrackEntryRepeat,
    GridTrackEntryAutoRepeat,
    GridTrackEntrySubgrid
>;
using GridTrackList = Vector<GridTrackEntry>;

// <'grid-template-columns'/'grid-template-rows'> = none | <track-list> | <auto-track-list>
struct GridTemplateList {
    GridTemplateList(CSS::Keyword::None) { }
    GridTemplateList(GridTrackList&&);
    bool isNone() const { return list.isEmpty(); }

    GridTrackList list { };

    // Calculated from list.

    Vector<GridTrackSize> sizes { };
    GridNamedLinesMap namedLines { };
    GridOrderedNamedLinesMap orderedNamedLines { };

    Vector<GridTrackSize> autoRepeatSizes { };
    GridNamedLinesMap autoRepeatNamedLines { };
    GridOrderedNamedLinesMap autoRepeatOrderedNamedLines { };

    unsigned autoRepeatInsertionPoint { 0 };
    AutoRepeatType autoRepeatType { AutoRepeatType::None };

    bool subgrid { false };

    bool operator==(const GridTemplateList& other) const
    {
        // It is only necessary to compare the `list` member, as the other members
        // are purely cached derived values based on the value of `list`.
        return list == other.list;
    }
};

// MARK: - Conversion

template<> struct CSSValueConversion<GridTemplateList> { auto operator()(BuilderState&, const CSSValue&) -> GridTemplateList; };

// MARK: - Blending

template<> struct Blending<GridTemplateList> {
    auto canBlend(const GridTemplateList&, const GridTemplateList&) -> bool;
    auto blend(const GridTemplateList&, const GridTemplateList&, const BlendingContext&) -> GridTemplateList;
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const GridTemplateList&);
WTF::TextStream& operator<<(WTF::TextStream&, const GridTrackEntry&);
WTF::TextStream& operator<<(WTF::TextStream&, const RepeatEntry&);

} // namespace Style
} // namespace WebCore
