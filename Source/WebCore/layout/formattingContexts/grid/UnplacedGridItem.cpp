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

#include "config.h"
#include "UnplacedGridItem.h"

#include "LayoutElementBox.h"

namespace WebCore {
namespace Layout {

// Convert a 1-indexed explicit CSS grid line into a 0-indexed grid line.
// https://www.w3.org/TR/css-grid-1/#line-placement
//
// Positive lines count forward from the start of the explicit grid: line 1 maps to 0.
// Negative lines count backward from the *end* of the explicit grid: line -1 maps to
// the last line (explicitTrackCount). The mapping is a single linear formula that stays correct as
// a negative line counts past the start edge into the leading implicit grid, producing a negative
// index. For example, with 3 explicit tracks (lines 0..3):
//     -1 -> 3 (last line), -4 -> 0 (first line), -5 -> -1, -6 -> -2 (two lines before the start).
// A negative index is not a valid matrix position on its own; GridFormattingContext computes the
// most-negative line across all items and shifts every line forward by that magnitude (the number
// of leading implicit tracks) so, e.g., -2 above maps to matrix index 0. See GridPosition::create().
NODELETE static int explicitLineToIndex(const Style::GridPosition& position, size_t explicitTrackCount)
{
    ASSERT(position.isExplicit());
    auto line = position.explicitPosition();
    // A value of zero makes the declaration invalid.
    ASSERT(line);
    return line > 0 ? line - 1 : static_cast<int>(explicitTrackCount) + 1 + line;
}

UnplacedGridItem::GridPosition UnplacedGridItem::GridPosition::create(const Style::GridPosition& start, const Style::GridPosition& end, size_t explicitTrackCount, size_t leadingImplicitTracksCount, size_t maxTracks)
{
    auto explicitRange = UnplacedGridItem::resolveDefinitePosition(start, end, explicitTrackCount, maxTracks);
    if (!explicitRange) {
        // An axis with no explicit line is auto-positioned; carry the span forward for the
        // placement algorithm to resolve.
        size_t span = 1;
        if (start.isSpan())
            span = start.spanPosition();
        else if (end.isSpan())
            span = end.spanPosition();
        return AutoPosition { span };
    }

    // Shift the resolved range forward by the number of leading implicit tracks. That count is the
    // magnitude of the most-negative line across all items, so applying it maps every line to a
    // non-negative matrix index.
    auto [rawStartLine, rawEndLine] = *explicitRange;
    int startLine = rawStartLine + static_cast<int>(leadingImplicitTracksCount);
    int endLine = rawEndLine + static_cast<int>(leadingImplicitTracksCount);

    ASSERT(startLine >= 0 && endLine >= 0);
    ASSERT(startLine < endLine);
    return DefinitePosition { static_cast<size_t>(startLine), static_cast<size_t>(endLine) };
}

std::optional<std::pair<int, int>> UnplacedGridItem::resolveDefinitePosition(const Style::GridPosition& start, const Style::GridPosition& end, size_t explicitTrackCount, size_t maxTracks)
{
    // An axis is only definite when one of its edges references an explicit line. Anything
    // else (auto/auto, span/auto, auto/span) is auto-positioned and has no explicit line range.
    if (!start.isExplicit() && !end.isExplicit())
        return { };

    int startLine = 0;
    int endLine = 0;
    if (start.isExplicit() && end.isExplicit()) {
        startLine = explicitLineToIndex(start, explicitTrackCount);
        endLine = explicitLineToIndex(end, explicitTrackCount);

        // https://drafts.csswg.org/css-grid-1/#grid-placement-errors
        // "If the placement for a grid item contains two lines, and the start line is further
        // end-ward than the end line, swap the two lines."
        if (startLine > endLine)
            std::swap(startLine, endLine);
        // "If the start line is equal to the end line, remove the end line." An item with only a
        // start line occupies the one track after it.
        else if (startLine == endLine)
            endLine = startLine + 1;
    } else if (start.isExplicit() && end.isSpan()) {
        startLine = explicitLineToIndex(start, explicitTrackCount);
        endLine = startLine + end.spanPosition();
    } else if (start.isSpan() && end.isExplicit()) {
        endLine = explicitLineToIndex(end, explicitTrackCount);
        startLine = endLine - static_cast<int>(start.spanPosition());
    } else if (start.isExplicit() && end.isAuto()) {
        startLine = explicitLineToIndex(start, explicitTrackCount);
        endLine = startLine + 1;
    } else {
        ASSERT(start.isAuto() && end.isExplicit());
        endLine = explicitLineToIndex(end, explicitTrackCount);
        startLine = endLine - 1;
    }

    // https://drafts.csswg.org/css-grid-1/#overlarge-grids
    // "If a grid item is placed outside this limit, its grid area must be clamped to within this limited grid."
    // The spec states the limit as a range of lines around the start of the explicit grid ("lines -10000 to 10000"). The
    // limited grid here is the lines [1 - maxTracks, maxTracks] around the explicit grid: up to maxTracks - 1 leading implicit
    // tracks and, counting the explicit grid, up to maxTracks tracks after its start, so the trailing implicit tracks are at
    // most maxTracks minus the explicit tracks. The leading and trailing implicit tracks together stay below 2 * maxTracks,
    // which is what the render tree's Grid allows. Chrome allows maxTracks leading tracks, so an item placed before the
    // explicit grid can get one track fewer here than in Chrome.
    // Style has already clamped line numbers to +-maxTracks. That makes a line number alone reach one line less than the spec's
    // example (500 / 1500 with 1000 tracks gives 500 / 1000 here, 500 / 1001 in the spec), because Style counts line numbers
    // where the spec counts tracks. A span can reach the last line.
    auto firstLine = 1 - static_cast<int>(maxTracks);
    auto lastLine = static_cast<int>(maxTracks);
    if (endLine <= firstLine) {
        // "If the grid area would be placed completely outside the limited grid, its span must be truncated to 1 and the
        // area repositioned into the last grid track on that side of the grid."
        startLine = firstLine;
        endLine = firstLine + 1;
    } else if (startLine >= lastLine) {
        // Same rule, on the trailing side.
        startLine = lastLine - 1;
        endLine = lastLine;
    } else {
        // "If the grid area would span outside the limited grid, its span is clamped to the last line of the limited grid."
        startLine = std::max(startLine, firstLine);
        endLine = std::min(endLine, lastLine);
    }

    return std::pair { startLine, endLine };
}

size_t UnplacedGridItem::GridPosition::span() const
{
    if (auto* definitePosition = std::get_if<DefinitePosition>(&m_position))
        return definitePosition->endLine - definitePosition->startLine;
    return std::get<AutoPosition>(m_position).span;
}

UnplacedGridItem::UnplacedGridItem(const ElementBox& layoutBox, Style::GridPosition nonAutoFlowStart, Style::GridPosition nonAutoFlowEnd,
    Style::GridPosition autoFlowStart, Style::GridPosition autoFlowEnd, size_t explicitNonAutoFlowTracksCount, size_t explicitAutoFlowTracksCount,
    size_t leadingImplicitNonAutoFlowTracksCount, size_t leadingImplicitAutoFlowTracksCount, size_t maxTracks)
    : m_layoutBox(layoutBox)
    , m_nonAutoFlowPosition(GridPosition::create(nonAutoFlowStart, nonAutoFlowEnd, explicitNonAutoFlowTracksCount, leadingImplicitNonAutoFlowTracksCount, maxTracks))
    , m_autoFlowPosition(GridPosition::create(autoFlowStart, autoFlowEnd, explicitAutoFlowTracksCount, leadingImplicitAutoFlowTracksCount, maxTracks))
{
}

bool UnplacedGridItem::hasDefiniteAutoFlowPosition() const
{
    return m_autoFlowPosition.isDefinite();
}

bool UnplacedGridItem::hasDefiniteNonAutoFlowPosition() const
{
    return m_nonAutoFlowPosition.isDefinite();
}

size_t UnplacedGridItem::nonAutoFlowSpanSize() const
{
    return m_nonAutoFlowPosition.span();
}

size_t UnplacedGridItem::autoFlowSpanSize() const
{
    return m_autoFlowPosition.span();
}

WTF::Range<size_t> UnplacedGridItem::definiteAutoFlowRange() const
{
    auto& definitePosition = m_autoFlowPosition.definitePosition();
    return { definitePosition.startLine, definitePosition.endLine };
}

WTF::Range<size_t> UnplacedGridItem::definiteNonAutoFlowRange() const
{
    auto& definitePosition = m_nonAutoFlowPosition.definitePosition();
    return { definitePosition.startLine, definitePosition.endLine };
}

} // namespace Layout

} // namespace WebCore
