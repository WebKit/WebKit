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
#include <cstdlib>

namespace WebCore {

static inline bool isColumnSide(GridPositionSide side)
{
    return side == ColumnStartSide || side == ColumnEndSide;
}

static inline bool isStartSide(GridPositionSide side)
{
    return side == ColumnStartSide || side == RowStartSide;
}

static inline GridPositionSide initialPositionSide(GridTrackSizingDirection direction)
{
    return direction == ForColumns ? ColumnStartSide : RowStartSide;
}

static inline GridPositionSide finalPositionSide(GridTrackSizingDirection direction)
{
    return direction == ForColumns ? ColumnEndSide : RowEndSide;
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

static void adjustGridPositionsFromStyle(const RenderStyle& gridContainerStyle, const RenderBox& gridItem, GridTrackSizingDirection direction, GridPosition& initialPosition, GridPosition& finalPosition)
{
    bool isForColumns = direction == ForColumns;
    initialPosition = isForColumns ? gridItem.style().gridItemColumnStart() : gridItem.style().gridItemRowStart();
    finalPosition = isForColumns ? gridItem.style().gridItemColumnEnd() : gridItem.style().gridItemRowEnd();

    // We must handle the placement error handling code here instead of in the StyleAdjuster because we don't want to
    // overwrite the specified values.
    if (initialPosition.isSpan() && finalPosition.isSpan())
        finalPosition.setAutoPosition();

    // Try to early detect the case of non existing named grid lines. This way we could assume later that
    // GridResolvedPosition::resolveGrisPositionFromStyle() won't require the autoplacement to run, i.e., it'll always return a
    // valid resolved position.
    if (initialPosition.isNamedGridArea() && GridResolvedPosition::isNonExistentNamedLineOrArea(initialPosition.namedGridLine(), gridContainerStyle, initialPositionSide(direction)))
        initialPosition.setAutoPosition();

    if (finalPosition.isNamedGridArea() && GridResolvedPosition::isNonExistentNamedLineOrArea(finalPosition.namedGridLine(), gridContainerStyle, finalPositionSide(direction)))
        finalPosition.setAutoPosition();

    // If the grid item has an automatic position and a grid span for a named line in a given dimension, instead treat the grid span as one.
    if (initialPosition.isAuto() && finalPosition.isSpan() && !finalPosition.namedGridLine().isNull())
        finalPosition.setSpanPosition(1, String());
    if (finalPosition.isAuto() && initialPosition.isSpan() && !initialPosition.namedGridLine().isNull())
        initialPosition.setSpanPosition(1, String());
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

static unsigned resolveNamedGridLinePositionFromStyle(const RenderStyle& gridContainerStyle, const GridPosition& position, GridPositionSide side)
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

static GridSpan resolveRowStartColumnStartNamedGridLinePositionAgainstOppositePosition(unsigned resolvedOppositePosition, const GridPosition& position, const Vector<unsigned>& gridLines)
{
    if (!resolvedOppositePosition)
        return GridSpan::untranslatedDefiniteGridSpan(resolvedOppositePosition, resolvedOppositePosition + 1);

    unsigned firstLineBeforePositionIndex = 0;
    auto firstLineBeforePosition = std::lower_bound(gridLines.begin(), gridLines.end(), resolvedOppositePosition);
    if (firstLineBeforePosition != gridLines.end())
        firstLineBeforePositionIndex = firstLineBeforePosition - gridLines.begin();

    unsigned gridLineIndex = std::max<int>(0, firstLineBeforePositionIndex - position.spanPosition());

    unsigned resolvedGridLinePosition = gridLines[gridLineIndex];
    if (resolvedGridLinePosition >= resolvedOppositePosition)
        resolvedGridLinePosition = resolvedOppositePosition - 1;
    return GridSpan::untranslatedDefiniteGridSpan(std::min(resolvedGridLinePosition, resolvedOppositePosition), resolvedOppositePosition);
}

static GridSpan resolveRowEndColumnEndNamedGridLinePositionAgainstOppositePosition(unsigned resolvedOppositePosition, const GridPosition& position, const Vector<unsigned>& gridLines)
{
    ASSERT(gridLines.size());
    unsigned firstLineAfterOppositePositionIndex = gridLines.size() - 1;
    const unsigned* firstLineAfterOppositePosition = std::upper_bound(gridLines.begin(), gridLines.end(), resolvedOppositePosition);
    if (firstLineAfterOppositePosition != gridLines.end())
        firstLineAfterOppositePositionIndex = firstLineAfterOppositePosition - gridLines.begin();

    unsigned gridLineIndex = std::min<unsigned>(gridLines.size() - 1, firstLineAfterOppositePositionIndex + position.spanPosition() - 1);
    unsigned resolvedGridLinePosition = gridLines[gridLineIndex];
    if (resolvedGridLinePosition <= resolvedOppositePosition)
        resolvedGridLinePosition = resolvedOppositePosition + 1;
    return GridSpan::untranslatedDefiniteGridSpan(resolvedOppositePosition, resolvedGridLinePosition);
}

static GridSpan resolveNamedGridLinePositionAgainstOppositePosition(const RenderStyle& gridContainerStyle, unsigned resolvedOppositePosition, const GridPosition& position, GridPositionSide side)
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
        if (isStartSide(side))
            return GridSpan::untranslatedDefiniteGridSpan(resolvedOppositePosition - 1, resolvedOppositePosition);
        return GridSpan::untranslatedDefiniteGridSpan(resolvedOppositePosition, resolvedOppositePosition + 1);
    }

    if (side == RowStartSide || side == ColumnStartSide)
        return resolveRowStartColumnStartNamedGridLinePositionAgainstOppositePosition(resolvedOppositePosition, position, it->value);

    return resolveRowEndColumnEndNamedGridLinePositionAgainstOppositePosition(resolvedOppositePosition, position, it->value);
}

static GridSpan resolveGridPositionAgainstOppositePosition(const RenderStyle& gridContainerStyle, unsigned resolvedOppositePosition, const GridPosition& position, GridPositionSide side)
{
    if (position.isAuto()) {
        if (isStartSide(side))
            return GridSpan::untranslatedDefiniteGridSpan(resolvedOppositePosition - 1, resolvedOppositePosition);
        return GridSpan::untranslatedDefiniteGridSpan(resolvedOppositePosition, resolvedOppositePosition + 1);
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
    if (isStartSide(side))
        return GridSpan::untranslatedDefiniteGridSpan(resolvedOppositePosition - positionOffset, resolvedOppositePosition);

    return GridSpan::untranslatedDefiniteGridSpan(resolvedOppositePosition, resolvedOppositePosition + positionOffset);
}

unsigned GridResolvedPosition::spanSizeForAutoPlacedItem(const RenderStyle& gridContainerStyle, const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    GridPosition initialPosition, finalPosition;
    adjustGridPositionsFromStyle(gridContainerStyle, gridItem, direction, initialPosition, finalPosition);

    // This method will only be used when both positions need to be resolved against the opposite one.
    ASSERT(initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition());

    if (initialPosition.isAuto() && finalPosition.isAuto())
        return 1;

    GridPosition position = initialPosition.isSpan() ? initialPosition : finalPosition;
    ASSERT(position.isSpan());

    ASSERT(position.spanPosition());
    return position.spanPosition();
}

static int resolveGridPositionFromStyle(const RenderStyle& gridContainerStyle, const GridPosition& position, GridPositionSide side)
{
    switch (position.type()) {
    case ExplicitPosition: {
        ASSERT(position.integerPosition());

        if (!position.namedGridLine().isNull())
            return resolveNamedGridLinePositionFromStyle(gridContainerStyle, position, side);

        // Handle <integer> explicit position.
        if (position.isPositive())
            return position.integerPosition() - 1;

        unsigned resolvedPosition = std::abs(position.integerPosition()) - 1;
        const unsigned endOfTrack = explicitGridSizeForSide(gridContainerStyle, side);

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
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

GridSpan GridResolvedPosition::resolveGridPositionsFromStyle(const RenderStyle& gridContainerStyle, const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    GridPosition initialPosition, finalPosition;
    adjustGridPositionsFromStyle(gridContainerStyle, gridItem, direction, initialPosition, finalPosition);

    GridPositionSide initialSide = initialPositionSide(direction);
    GridPositionSide finalSide = finalPositionSide(direction);

    // We can't get our grid positions without running the auto placement algorithm.
    if (initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition())
        return GridSpan::indefiniteGridSpan();

    if (initialPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer the position from the final position ('auto / 1' or 'span 2 / 3' case).
        auto finalResolvedPosition = resolveGridPositionFromStyle(gridContainerStyle, finalPosition, finalSide);
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, finalResolvedPosition, initialPosition, initialSide);
    }

    if (finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer our position from the initial position ('1 / auto' or '3 / span 2' case).
        auto initialResolvedPosition = resolveGridPositionFromStyle(gridContainerStyle, initialPosition, initialSide);
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, initialResolvedPosition, finalPosition, finalSide);
    }

    int resolvedInitialPosition = resolveGridPositionFromStyle(gridContainerStyle, initialPosition, initialSide);
    int resolvedFinalPosition = resolveGridPositionFromStyle(gridContainerStyle, finalPosition, finalSide);

    if (resolvedInitialPosition > resolvedFinalPosition)
        std::swap(resolvedInitialPosition, resolvedFinalPosition);
    else if (resolvedInitialPosition == resolvedFinalPosition)
        resolvedFinalPosition = resolvedInitialPosition + 1;

    return GridSpan::untranslatedDefiniteGridSpan(resolvedInitialPosition, std::max(resolvedInitialPosition, resolvedFinalPosition));
}

} // namespace WebCore

#endif // ENABLE(CSS_GRID_LAYOUT)
