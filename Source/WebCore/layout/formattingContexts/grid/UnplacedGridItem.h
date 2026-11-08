/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "StyleGridPosition.h"
#include <wtf/Range.h>

namespace WebCore {
namespace Layout {

class ElementBox;
class GridLayout;
class NamedGridLines;

// Grid item placement is written in terms of grid-auto-flow instead of columns and rows. From
// https://drafts.csswg.org/css-grid-1/#grid-auto-flow-property:
// "row: The auto-placement algorithm places items by filling each row in turn, adding new rows as necessary."
// The auto-flow tracks are the tracks that grid-auto-flow names (rows for row, columns for column): the implicit
// grid grows by adding them. The non-auto-flow tracks are the others. GridItemPlacer is the only place that maps
// between these and the columns and rows of the style and of GridAreaLines.
//
// A grid area in the flow-relative terms: the lines of the non-auto-flow tracks and of the auto-flow tracks it spans.
struct FlowRelativeAreaLines {
    size_t nonAutoFlowStartLine { 0 };
    size_t nonAutoFlowEndLine { 0 };
    size_t autoFlowStartLine { 0 };
    size_t autoFlowEndLine { 0 };
};

class UnplacedGridItem {
private:
    // https://drafts.csswg.org/css-grid-1/#placement
    // A definite value for any two of Start, End, and Span in a given dimension implies
    // a definite value for the third.
    //
    // A "definite" axis (at least one edge references an explicit line) resolves to a concrete
    // 0-based line range. Everything else (auto/auto, span/auto, auto/span) is auto-positioned
    // and only carries the span size that the placement algorithm resolves into a position later.
    struct DefinitePosition {
        // 0-based grid line indices; endLine is exclusive.
        size_t startLine { 0 };
        size_t endLine { 0 };
    };

    struct AutoPosition {
        // Number of tracks the item spans; resolved to a position during auto-placement.
        size_t span { 1 };
    };

    class GridPosition {
    public:
        // explicitTrackCount is the number of explicit tracks in the axis, used to resolve negative
        // lines against the end of the explicit grid, and the NamedGridLines are the axis's, used to
        // resolve lines given with a name. The leading implicit track count then shifts negative
        // resolved lines forward so the range maps to non-negative matrix indices. See
        // UnplacedGridItem::resolveDefinitePosition().
        static GridPosition create(const Style::GridPosition& start, const Style::GridPosition& end, size_t explicitTrackCount, const NamedGridLines&, size_t leadingImplicitTracksCount, size_t maxTracks);

        bool isDefinite() const { return std::holds_alternative<DefinitePosition>(m_position); }
        bool isAuto() const { return std::holds_alternative<AutoPosition>(m_position); }

        const DefinitePosition& definitePosition() const { return std::get<DefinitePosition>(m_position); }

        size_t span() const;

    private:
        using Value = Variant<DefinitePosition, AutoPosition>;

        GridPosition(DefinitePosition position)
            : m_position(position) { }
        GridPosition(AutoPosition position)
            : m_position(position) { }

        Value m_position;
    };

public:
    UnplacedGridItem(const ElementBox&, Style::GridPosition nonAutoFlowStart, Style::GridPosition nonAutoFlowEnd, Style::GridPosition autoFlowStart, Style::GridPosition autoFlowEnd, size_t explicitNonAutoFlowTracksCount, size_t explicitAutoFlowTracksCount, const NamedGridLines& namedNonAutoFlowLines, const NamedGridLines& namedAutoFlowLines, size_t leadingImplicitNonAutoFlowTracksCount, size_t leadingImplicitAutoFlowTracksCount, size_t maxTracks);

    NODELETE bool hasDefiniteAutoFlowPosition() const;
    NODELETE bool hasDefiniteNonAutoFlowPosition() const;
    size_t nonAutoFlowSpanSize() const;
    size_t autoFlowSpanSize() const;

    WTF::Range<size_t> definiteAutoFlowRange() const;
    WTF::Range<size_t> definiteNonAutoFlowRange() const;

    // Resolves the 0-based explicit line range [start, end) for an axis, or std::nullopt when the
    // axis is auto-positioned (neither edge is a line, which is an <integer> or a lone <custom-ident>).
    // explicitTrackCount is used to resolve negative lines against the end of the explicit grid, and
    // the NamedGridLines to resolve lines given with a name; the resolved lines may still be negative
    // for negative line placements that count past the start edge, and the leading implicit track
    // count is applied later in GridPosition::create(). maxTracks limits the resolved lines, see
    // https://drafts.csswg.org/css-grid-1/#overlarge-grids.
    static std::optional<std::pair<int, int>> resolveDefinitePosition(const Style::GridPosition& start, const Style::GridPosition& end, size_t explicitTrackCount, const NamedGridLines&, size_t maxTracks);

private:
    CheckedRef<const ElementBox> m_layoutBox;

    // https://drafts.csswg.org/css-grid-1/#typedef-grid-row-start-grid-line
    // The position in the non-auto-flow tracks and in the auto-flow tracks.
    GridPosition m_nonAutoFlowPosition;
    GridPosition m_autoFlowPosition;

    friend class GridLayoutContextBuilder;
};

// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
struct UnplacedGridItems {
    // 1. Position anything that’s not auto-positioned.
    Vector<UnplacedGridItem> nonAutoPositionedItems;
    // 2. Process the items locked to a given auto-flow track (a row for grid-auto-flow: row).
    Vector<UnplacedGridItem> definiteAutoFlowPositionedItems;
    // 4. Position the remaining grid items.
    Vector<UnplacedGridItem> autoPositionedItems;
};

}
}
