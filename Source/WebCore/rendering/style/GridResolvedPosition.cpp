/*
 * Copyright (C) 2014 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GridResolvedPosition.h"

#if ENABLE(CSS_GRID_LAYOUT)

#include "GridCoordinate.h"
#include "RenderBox.h"

namespace WebCore {

static inline bool isColumnSide(GridPositionSide side)
{
    return side == ColumnStartSide || side == ColumnEndSide;
}

static inline bool isStartSide(GridPositionSide side)
{
    return side == ColumnStartSide || side == RowStartSide;
}

static const NamedGridLinesMap& gridLinesForSide(const RenderStyle& style, GridPositionSide side)
{
    return isColumnSide(side) ? style.namedGridColumnLines() : style.namedGridRowLines();
}

static const String implicitNamedGridLineForSide(const String& lineName, GridPositionSide side)
{
    return lineName + (isStartSide(side) ? "-start" : "-end");
}

bool GridResolvedPosition::isNonExistentNamedLineOrArea(const String& lineName, const RenderStyle& style, GridPositionSide side)
{
    const NamedGridLinesMap& gridLineNames = gridLinesForSide(style, side);
    return !gridLineNames.contains(implicitNamedGridLineForSide(lineName, side)) && !gridLineNames.contains(lineName);
}

bool GridUnresolvedSpan::requiresAutoPlacement() const
{
    return m_initialPosition.shouldBeResolvedAgainstOppositePosition() && m_finalPosition.shouldBeResolvedAgainstOppositePosition();
}

void GridUnresolvedSpan::adjustGridPositionsFromStyle(const RenderStyle& gridContainerStyle)
{
    ASSERT(isColumnSide(m_initialPositionSide) == isColumnSide(m_finalPositionSide));

    // We must handle the placement error handling code here instead of in the StyleAdjuster because we don't want to
    // overwrite the specified values.
    if (m_initialPosition.isSpan() && m_finalPosition.isSpan())
        m_finalPosition.setAutoPosition();

    // Try to early detect the case of non existing named grid lines. This way we could assume later that
    // GridResolvedPosition::resolveGrisPositionFromStyle() won't require the autoplacement to run, i.e., it'll always return a
    // valid resolved position.
    if (m_initialPosition.isNamedGridArea() && GridResolvedPosition::isNonExistentNamedLineOrArea(m_initialPosition.namedGridLine(), gridContainerStyle, m_initialPositionSide))
        m_initialPosition.setAutoPosition();

    if (m_finalPosition.isNamedGridArea() && GridResolvedPosition::isNonExistentNamedLineOrArea(m_finalPosition.namedGridLine(), gridContainerStyle, m_finalPositionSide))
        m_finalPosition.setAutoPosition();

    // If the grid item has an automatic position and a grid span for a named line in a given dimension, instead treat the grid span as one.
    if (m_initialPosition.isAuto() && m_finalPosition.isSpan() && !m_finalPosition.namedGridLine().isNull())
        m_finalPosition.setSpanPosition(1, String());
    if (m_finalPosition.isAuto() && m_initialPosition.isSpan() && !m_initialPosition.namedGridLine().isNull())
        m_initialPosition.setSpanPosition(1, String());
}

unsigned GridResolvedPosition::explicitGridColumnCount(const RenderStyle& gridContainerStyle)
{
    return std::min<unsigned>(gridContainerStyle.gridColumns().size(), kGridMaxTracks);
}

unsigned GridResolvedPosition::explicitGridRowCount(const RenderStyle& gridContainerStyle)
{
    return std::min<unsigned>(gridContainerStyle.gridRows().size(), kGridMaxTracks);
}

static unsigned explicitGridSizeForSide(const RenderStyle& gridContainerStyle, GridPositionSide side)
{
    return isColumnSide(side) ? GridResolvedPosition::explicitGridColumnCount(gridContainerStyle) : GridResolvedPosition::explicitGridRowCount(gridContainerStyle);
}

static GridResolvedPosition resolveNamedGridLinePositionFromStyle(const RenderStyle& gridContainerStyle, const GridPosition& position, GridPositionSide side)
{
    ASSERT(!position.namedGridLine().isNull());

    const NamedGridLinesMap& gridLinesNames = isColumnSide(side) ? gridContainerStyle.namedGridColumnLines() : gridContainerStyle.namedGridRowLines();
    NamedGridLinesMap::const_iterator it = gridLinesNames.find(position.namedGridLine());
    if (it == gridLinesNames.end()) {
        if (position.isPositive())
            return 0;
        const unsigned lastLine = explicitGridSizeForSide(gridContainerStyle, side);
        return lastLine;
    }

    unsigned namedGridLineIndex;
    if (position.isPositive())
        namedGridLineIndex = std::min<unsigned>(position.integerPosition(), it->value.size()) - 1;
    else
        namedGridLineIndex = std::max<int>(0, it->value.size() - abs(position.integerPosition()));
    return it->value[namedGridLineIndex];
}

static GridSpan resolveRowStartColumnStartNamedGridLinePositionAgainstOppositePosition(const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, const Vector<unsigned>& gridLines)
{
    if (!resolvedOppositePosition.toInt())
        return GridSpan(resolvedOppositePosition, resolvedOppositePosition.next());

    unsigned firstLineBeforePositionIndex = 0;
    auto firstLineBeforePosition = std::lower_bound(gridLines.begin(), gridLines.end(), resolvedOppositePosition.toInt());
    if (firstLineBeforePosition != gridLines.end())
        firstLineBeforePositionIndex = firstLineBeforePosition - gridLines.begin();

    unsigned gridLineIndex = std::max<int>(0, firstLineBeforePositionIndex - position.spanPosition());

    GridResolvedPosition resolvedGridLinePosition = GridResolvedPosition(gridLines[gridLineIndex]);
    if (resolvedGridLinePosition >= resolvedOppositePosition)
        resolvedGridLinePosition = resolvedOppositePosition.prev();
    return GridSpan(std::min<GridResolvedPosition>(resolvedGridLinePosition, resolvedOppositePosition), resolvedOppositePosition);
}

static GridSpan resolveRowEndColumnEndNamedGridLinePositionAgainstOppositePosition(const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, const Vector<unsigned>& gridLines)
{
    ASSERT(gridLines.size());
    unsigned firstLineAfterOppositePositionIndex = gridLines.size() - 1;
    const unsigned* firstLineAfterOppositePosition = std::upper_bound(gridLines.begin(), gridLines.end(), resolvedOppositePosition);
    if (firstLineAfterOppositePosition != gridLines.end())
        firstLineAfterOppositePositionIndex = firstLineAfterOppositePosition - gridLines.begin();

    unsigned gridLineIndex = std::min<unsigned>(gridLines.size() - 1, firstLineAfterOppositePositionIndex + position.spanPosition() - 1);
    GridResolvedPosition resolvedGridLinePosition = gridLines[gridLineIndex];
    if (resolvedGridLinePosition <= resolvedOppositePosition)
        resolvedGridLinePosition = resolvedOppositePosition.next();
    return GridSpan(resolvedOppositePosition, resolvedGridLinePosition);
}

static GridSpan resolveNamedGridLinePositionAgainstOppositePosition(const RenderStyle& gridContainerStyle, const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, GridPositionSide side)
{
    ASSERT(position.isSpan());
    ASSERT(!position.namedGridLine().isNull());
    // Negative positions are not allowed per the specification and should have been handled during parsing.
    ASSERT(position.spanPosition() > 0);

    const NamedGridLinesMap& gridLinesNames = isColumnSide(side) ? gridContainerStyle.namedGridColumnLines() : gridContainerStyle.namedGridRowLines();
    NamedGridLinesMap::const_iterator it = gridLinesNames.find(position.namedGridLine());

    // If there is no named grid line of that name, we resolve the position to 'auto' (which is equivalent to 'span 1' in this case).
    // See http://lists.w3.org/Archives/Public/www-style/2013Jun/0394.html.
    if (it == gridLinesNames.end()) {
        if (isStartSide(side) && resolvedOppositePosition.toInt())
            return GridSpan(resolvedOppositePosition.prev(), resolvedOppositePosition);
        return GridSpan(resolvedOppositePosition, resolvedOppositePosition.next());
    }

    if (side == RowStartSide || side == ColumnStartSide)
        return resolveRowStartColumnStartNamedGridLinePositionAgainstOppositePosition(resolvedOppositePosition, position, it->value);

    return resolveRowEndColumnEndNamedGridLinePositionAgainstOppositePosition(resolvedOppositePosition, position, it->value);
}

static GridSpan resolveGridPositionAgainstOppositePosition(const RenderStyle& gridContainerStyle, const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, GridPositionSide side)
{
    if (position.isAuto()) {
        if (isStartSide(side) && resolvedOppositePosition.toInt())
            return GridSpan(resolvedOppositePosition.prev(), resolvedOppositePosition);
        return GridSpan(resolvedOppositePosition, resolvedOppositePosition.next());
    }

    ASSERT(position.isSpan());
    ASSERT(position.spanPosition() > 0);

    if (!position.namedGridLine().isNull()) {
        // span 2 'c' -> we need to find the appropriate grid line before / after our opposite position.
        return resolveNamedGridLinePositionAgainstOppositePosition(gridContainerStyle, resolvedOppositePosition, position, side);
    }

    // 'span 1' is contained inside a single grid track regardless of the direction.
    // That's why the CSS span value is one more than the offset we apply.
    unsigned positionOffset = position.spanPosition();
    if (isStartSide(side)) {
        if (!resolvedOppositePosition.toInt())
            return GridSpan(resolvedOppositePosition, resolvedOppositePosition.next());

        unsigned initialResolvedPosition = std::max<int>(0, resolvedOppositePosition.toInt() - positionOffset);
        return GridSpan(initialResolvedPosition, resolvedOppositePosition);
    }

    return GridSpan(resolvedOppositePosition, resolvedOppositePosition.toInt() + positionOffset);
}

GridSpan GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(const RenderStyle& gridContainerStyle, const RenderBox& gridItem, GridTrackSizingDirection direction, const GridResolvedPosition& resolvedInitialPosition)
{
    GridUnresolvedSpan unresolvedSpan = unresolvedSpanFromStyle(gridContainerStyle, gridItem, direction);

    // This method will only be used when both positions need to be resolved against the opposite one.
    ASSERT(unresolvedSpan.requiresAutoPlacement());

    GridResolvedPosition resolvedFinalPosition = resolvedInitialPosition.next();

    if (unresolvedSpan.initialPosition().isSpan())
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, resolvedInitialPosition, unresolvedSpan.initialPosition(), unresolvedSpan.finalPositionSide());
    if (unresolvedSpan.finalPosition().isSpan())
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, resolvedInitialPosition, unresolvedSpan.finalPosition(), unresolvedSpan.finalPositionSide());

    return GridSpan(resolvedInitialPosition, resolvedFinalPosition);
}

static GridResolvedPosition resolveGridPositionFromStyle(const RenderStyle& gridContainerStyle, const GridPosition& position, GridPositionSide side)
{
    switch (position.type()) {
    case ExplicitPosition: {
        ASSERT(position.integerPosition());

        if (!position.namedGridLine().isNull())
            return resolveNamedGridLinePositionFromStyle(gridContainerStyle, position, side);

        // Handle <integer> explicit position.
        if (position.isPositive())
            return position.integerPosition() - 1;

        unsigned resolvedPosition = abs(position.integerPosition()) - 1;
        const unsigned endOfTrack = explicitGridSizeForSide(gridContainerStyle, side);

        // Per http://lists.w3.org/Archives/Public/www-style/2013Mar/0589.html, we clamp negative value to the first line.
        if (endOfTrack < resolvedPosition)
            return 0;

        return endOfTrack - resolvedPosition;
    }
    case NamedGridAreaPosition:
    {
        // First attempt to match the grid area's edge to a named grid area: if there is a named line with the name
        // ''<custom-ident>-start (for grid-*-start) / <custom-ident>-end'' (for grid-*-end), contributes the first such
        // line to the grid item's placement.
        String namedGridLine = position.namedGridLine();
        ASSERT(!GridResolvedPosition::isNonExistentNamedLineOrArea(namedGridLine, gridContainerStyle, side));

        const NamedGridLinesMap& gridLineNames = gridLinesForSide(gridContainerStyle, side);
        auto implicitLine = gridLineNames.find(implicitNamedGridLineForSide(namedGridLine, side));
        if (implicitLine != gridLineNames.end())
            return implicitLine->value[0];

        // Otherwise, if there is a named line with the specified name, contributes the first such line to the grid
        // item's placement.
        auto explicitLine = gridLineNames.find(namedGridLine);
        if (explicitLine != gridLineNames.end())
            return explicitLine->value[0];

        // If none of the above works specs mandate us to treat it as auto BUT we should have detected it before calling
        // this function in resolveGridPositionsFromStyle(). We should be covered anyway by the ASSERT at the beginning
        // of this case block.
        ASSERT_NOT_REACHED();
        return 0;
    }
    case AutoPosition:
    case SpanPosition:
        // 'auto' and span depend on the opposite position for resolution (e.g. grid-row: auto / 1 or grid-column: span 3 / "myHeader").
        ASSERT_NOT_REACHED();
        return GridResolvedPosition(0);
    }
    ASSERT_NOT_REACHED();
    return GridResolvedPosition(0);
}

GridUnresolvedSpan GridResolvedPosition::unresolvedSpanFromStyle(const RenderStyle& gridContainerStyle, const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    GridPosition initialPosition = (direction == ForColumns) ? gridItem.style().gridItemColumnStart() : gridItem.style().gridItemRowStart();
    auto initialPositionSide = (direction == ForColumns) ? ColumnStartSide : RowStartSide;
    GridPosition finalPosition = (direction == ForColumns) ? gridItem.style().gridItemColumnEnd() : gridItem.style().gridItemRowEnd();
    auto finalPositionSide = (direction == ForColumns) ? ColumnEndSide : RowEndSide;

    GridUnresolvedSpan unresolvedSpan(initialPosition, initialPositionSide, finalPosition, finalPositionSide);
    unresolvedSpan.adjustGridPositionsFromStyle(gridContainerStyle);

    return unresolvedSpan;
}

GridSpan GridResolvedPosition::resolveGridPositionsFromStyle(const GridUnresolvedSpan& unresolvedSpan, const RenderStyle& gridContainerStyle)
{
    ASSERT(!unresolvedSpan.requiresAutoPlacement());

    // We must create the GridSpan using finalPosition.prev() because GridSpan stores cell indexes and
    // an item finishing on the N-th line belongs to the N-1-th cell.

    if (unresolvedSpan.initialPosition().shouldBeResolvedAgainstOppositePosition()) {
        // Infer the position from the final position ('auto / 1' or 'span 2 / 3' case).
        auto finalResolvedPosition = resolveGridPositionFromStyle(gridContainerStyle, unresolvedSpan.finalPosition(), unresolvedSpan.finalPositionSide());
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, finalResolvedPosition, unresolvedSpan.initialPosition(), unresolvedSpan.initialPositionSide());
    }

    if (unresolvedSpan.finalPosition().shouldBeResolvedAgainstOppositePosition()) {
        // Infer our position from the initial position ('1 / auto' or '3 / span 2' case).
        auto initialResolvedPosition = resolveGridPositionFromStyle(gridContainerStyle, unresolvedSpan.initialPosition(), unresolvedSpan.initialPositionSide());
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, initialResolvedPosition, unresolvedSpan.finalPosition(), unresolvedSpan.finalPositionSide());
    }

    GridResolvedPosition resolvedInitialPosition = resolveGridPositionFromStyle(gridContainerStyle, unresolvedSpan.initialPosition(), unresolvedSpan.initialPositionSide());
    GridResolvedPosition resolvedFinalPosition = resolveGridPositionFromStyle(gridContainerStyle, unresolvedSpan.finalPosition(), unresolvedSpan.finalPositionSide());

    if (resolvedInitialPosition > resolvedFinalPosition)
        std::swap(resolvedInitialPosition, resolvedFinalPosition);
    else if (resolvedInitialPosition == resolvedFinalPosition)
        resolvedFinalPosition = resolvedInitialPosition.next();

    return GridSpan(resolvedInitialPosition, std::max(resolvedInitialPosition, resolvedFinalPosition));
}

} // namespace WebCore

#endif // ENABLE(CSS_GRID_LAYOUT)
