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

static size_t explicitGridSizeForSide(const RenderStyle& gridContainerStyle, GridPositionSide side)
{
    return isColumnSide(side) ? gridContainerStyle.gridColumns().size() : gridContainerStyle.gridRows().size();
}

GridSpan GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(const RenderStyle& gridContainerStyle, const RenderBox& gridItem, GridTrackSizingDirection direction, const GridResolvedPosition& resolvedInitialPosition)
{
    GridPosition initialPosition = (direction == ForColumns) ? gridItem.style().gridItemColumnStart() : gridItem.style().gridItemRowStart();
    const GridPositionSide initialPositionSide = (direction == ForColumns) ? ColumnStartSide : RowStartSide;
    GridPosition finalPosition = (direction == ForColumns) ? gridItem.style().gridItemColumnEnd() : gridItem.style().gridItemRowEnd();
    const GridPositionSide finalPositionSide = (direction == ForColumns) ? ColumnEndSide : RowEndSide;

    adjustGridPositionsFromStyle(gridContainerStyle, initialPosition, finalPosition, initialPositionSide, finalPositionSide);

    // This method will only be used when both positions need to be resolved against the opposite one.
    ASSERT(initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition());

    GridResolvedPosition resolvedFinalPosition = resolvedInitialPosition;

    if (initialPosition.isSpan())
        resolvedFinalPosition = resolveGridPositionAgainstOppositePosition(gridContainerStyle, resolvedInitialPosition, initialPosition, finalPositionSide)->resolvedFinalPosition;
    else if (finalPosition.isSpan())
        resolvedFinalPosition = resolveGridPositionAgainstOppositePosition(gridContainerStyle, resolvedInitialPosition, finalPosition, finalPositionSide)->resolvedFinalPosition;

    return GridSpan(resolvedInitialPosition, resolvedFinalPosition);
}

static inline const NamedGridLinesMap& gridLinesForSide(const RenderStyle& style, GridPositionSide side)
{
    return isColumnSide(side) ? style.namedGridColumnLines() : style.namedGridRowLines();
}

static inline const String implicitNamedGridLineForSide(const String& lineName, GridPositionSide side)
{
    return lineName + (isStartSide(side) ? "-start" : "-end");
}

static bool isNonExistentNamedLineOrArea(const String& lineName, const RenderStyle& style, GridPositionSide side)
{
    const NamedGridLinesMap& gridLineNames = gridLinesForSide(style, side);
    return !gridLineNames.contains(implicitNamedGridLineForSide(lineName, side)) && !gridLineNames.contains(lineName);
}

void GridResolvedPosition::adjustGridPositionsFromStyle(const RenderStyle& gridContainerStyle, GridPosition& initialPosition, GridPosition& finalPosition, GridPositionSide initialPositionSide, GridPositionSide finalPositionSide)
{
    ASSERT(isColumnSide(initialPositionSide) == isColumnSide(finalPositionSide));

    // We must handle the placement error handling code here instead of in the StyleAdjuster because we don't want to
    // overwrite the specified values.
    if (initialPosition.isSpan() && finalPosition.isSpan())
        finalPosition.setAutoPosition();

    // Try to early detect the case of non existing named grid lines. This way we could assume later that
    // GridResolvedPosition::resolveGrisPositionFromStyle() won't require the autoplacement to run, i.e., it'll always return a
    // valid resolved position.
    if (initialPosition.isNamedGridArea() && isNonExistentNamedLineOrArea(initialPosition.namedGridLine(), gridContainerStyle, initialPositionSide))
        initialPosition.setAutoPosition();

    if (finalPosition.isNamedGridArea() && isNonExistentNamedLineOrArea(finalPosition.namedGridLine(), gridContainerStyle, finalPositionSide))
        finalPosition.setAutoPosition();

    // If the grid item has an automatic position and a grid span for a named line in a given dimension, instead treat the grid span as one.
    if (initialPosition.isAuto() && finalPosition.isSpan() && !finalPosition.namedGridLine().isNull())
        finalPosition.setSpanPosition(1, String());
    if (finalPosition.isAuto() && initialPosition.isSpan() && !initialPosition.namedGridLine().isNull())
        initialPosition.setSpanPosition(1, String());
}

std::unique_ptr<GridSpan> GridResolvedPosition::resolveGridPositionsFromStyle(const RenderStyle& gridContainerStyle, const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    GridPosition initialPosition = (direction == ForColumns) ? gridItem.style().gridItemColumnStart() : gridItem.style().gridItemRowStart();
    const GridPositionSide initialPositionSide = (direction == ForColumns) ? ColumnStartSide : RowStartSide;
    GridPosition finalPosition = (direction == ForColumns) ? gridItem.style().gridItemColumnEnd() : gridItem.style().gridItemRowEnd();
    const GridPositionSide finalPositionSide = (direction == ForColumns) ? ColumnEndSide : RowEndSide;

    adjustGridPositionsFromStyle(gridContainerStyle, initialPosition, finalPosition, initialPositionSide, finalPositionSide);

    if (initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        // FIXME: Implement properly "stack" value in auto-placement algorithm.
        if (gridContainerStyle.isGridAutoFlowAlgorithmStack())
            return std::make_unique<GridSpan>(0, 0);

        // We can't get our grid positions without running the auto placement algorithm.
        return nullptr;
    }

    if (initialPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer the position from the final position ('auto / 1' or 'span 2 / 3' case).
        const GridResolvedPosition finalResolvedPosition = resolveGridPositionFromStyle(gridContainerStyle, finalPosition, finalPositionSide);
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, finalResolvedPosition, initialPosition, initialPositionSide);
    }

    if (finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer our position from the initial position ('1 / auto' or '3 / span 2' case).
        const GridResolvedPosition initialResolvedPosition = resolveGridPositionFromStyle(gridContainerStyle, initialPosition, initialPositionSide);
        return resolveGridPositionAgainstOppositePosition(gridContainerStyle, initialResolvedPosition, finalPosition, finalPositionSide);
    }

    GridResolvedPosition resolvedInitialPosition = resolveGridPositionFromStyle(gridContainerStyle, initialPosition, initialPositionSide);
    GridResolvedPosition resolvedFinalPosition = resolveGridPositionFromStyle(gridContainerStyle, finalPosition, finalPositionSide);

    // If 'grid-row-end' specifies a line at or before that specified by 'grid-row-start', it computes to 'span 1'.
    if (resolvedFinalPosition < resolvedInitialPosition)
        resolvedFinalPosition = resolvedInitialPosition;

    return std::make_unique<GridSpan>(resolvedInitialPosition, resolvedFinalPosition);
}

GridResolvedPosition GridResolvedPosition::resolveNamedGridLinePositionFromStyle(const RenderStyle& gridContainerStyle, const GridPosition& position, GridPositionSide side)
{
    ASSERT(!position.namedGridLine().isNull());

    const NamedGridLinesMap& gridLinesNames = isColumnSide(side) ? gridContainerStyle.namedGridColumnLines() : gridContainerStyle.namedGridRowLines();
    NamedGridLinesMap::const_iterator it = gridLinesNames.find(position.namedGridLine());
    if (it == gridLinesNames.end()) {
        if (position.isPositive())
            return 0;
        const size_t lastLine = explicitGridSizeForSide(gridContainerStyle, side);
        return GridResolvedPosition::adjustGridPositionForSide(lastLine, side);
    }

    size_t namedGridLineIndex;
    if (position.isPositive())
        namedGridLineIndex = std::min<size_t>(position.integerPosition(), it->value.size()) - 1;
    else
        namedGridLineIndex = std::max<int>(it->value.size() - abs(position.integerPosition()), 0);
    return GridResolvedPosition::adjustGridPositionForSide(it->value[namedGridLineIndex], side);
}

GridResolvedPosition GridResolvedPosition::resolveGridPositionFromStyle(const RenderStyle& gridContainerStyle, const GridPosition& position, GridPositionSide side)
{
    switch (position.type()) {
    case ExplicitPosition: {
        ASSERT(position.integerPosition());

        if (!position.namedGridLine().isNull())
            return resolveNamedGridLinePositionFromStyle(gridContainerStyle, position, side);

        // Handle <integer> explicit position.
        if (position.isPositive())
            return GridResolvedPosition::adjustGridPositionForSide(position.integerPosition() - 1, side);

        size_t resolvedPosition = abs(position.integerPosition()) - 1;
        const size_t endOfTrack = explicitGridSizeForSide(gridContainerStyle, side);

        // Per http://lists.w3.org/Archives/Public/www-style/2013Mar/0589.html, we clamp negative value to the first line.
        if (endOfTrack < resolvedPosition)
            return 0;

        return GridResolvedPosition::adjustGridPositionForSide(endOfTrack - resolvedPosition, side);
    }
    case NamedGridAreaPosition:
    {
        // First attempt to match the grid area's edge to a named grid area: if there is a named line with the name
        // ''<custom-ident>-start (for grid-*-start) / <custom-ident>-end'' (for grid-*-end), contributes the first such
        // line to the grid item's placement.
        String namedGridLine = position.namedGridLine();
        ASSERT(!isNonExistentNamedLineOrArea(namedGridLine, gridContainerStyle, side));

        const NamedGridLinesMap& gridLineNames = gridLinesForSide(gridContainerStyle, side);
        auto implicitLine = gridLineNames.find(implicitNamedGridLineForSide(namedGridLine, side));
        if (implicitLine != gridLineNames.end())
            return GridResolvedPosition::adjustGridPositionForSide(implicitLine->value[0], side);

        // Otherwise, if there is a named line with the specified name, contributes the first such line to the grid
        // item's placement.
        auto explicitLine = gridLineNames.find(namedGridLine);
        if (explicitLine != gridLineNames.end())
            return GridResolvedPosition::adjustGridPositionForSide(explicitLine->value[0], side);

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

std::unique_ptr<GridSpan> GridResolvedPosition::resolveGridPositionAgainstOppositePosition(const RenderStyle& gridContainerStyle, const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, GridPositionSide side)
{
    if (position.isAuto())
        return std::make_unique<GridSpan>(resolvedOppositePosition, resolvedOppositePosition);

    ASSERT(position.isSpan());
    ASSERT(position.spanPosition() > 0);

    if (!position.namedGridLine().isNull()) {
        // span 2 'c' -> we need to find the appropriate grid line before / after our opposite position.
        return resolveNamedGridLinePositionAgainstOppositePosition(gridContainerStyle, resolvedOppositePosition, position, side);
    }

    // 'span 1' is contained inside a single grid track regardless of the direction.
    // That's why the CSS span value is one more than the offset we apply.
    size_t positionOffset = position.spanPosition() - 1;
    if (isStartSide(side)) {
        size_t initialResolvedPosition = std::max<int>(0, resolvedOppositePosition.toInt() - positionOffset);
        return std::make_unique<GridSpan>(initialResolvedPosition, resolvedOppositePosition);
    }

    return std::make_unique<GridSpan>(resolvedOppositePosition, resolvedOppositePosition.toInt() + positionOffset);
}

std::unique_ptr<GridSpan> GridResolvedPosition::resolveNamedGridLinePositionAgainstOppositePosition(const RenderStyle& gridContainerStyle, const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, GridPositionSide side)
{
    ASSERT(position.isSpan());
    ASSERT(!position.namedGridLine().isNull());
    // Negative positions are not allowed per the specification and should have been handled during parsing.
    ASSERT(position.spanPosition() > 0);

    const NamedGridLinesMap& gridLinesNames = isColumnSide(side) ? gridContainerStyle.namedGridColumnLines() : gridContainerStyle.namedGridRowLines();
    NamedGridLinesMap::const_iterator it = gridLinesNames.find(position.namedGridLine());

    // If there is no named grid line of that name, we resolve the position to 'auto' (which is equivalent to 'span 1' in this case).
    // See http://lists.w3.org/Archives/Public/www-style/2013Jun/0394.html.
    if (it == gridLinesNames.end())
        return std::make_unique<GridSpan>(resolvedOppositePosition, resolvedOppositePosition);

    if (side == RowStartSide || side == ColumnStartSide)
        return resolveRowStartColumnStartNamedGridLinePositionAgainstOppositePosition(resolvedOppositePosition, position, it->value);

    return resolveRowEndColumnEndNamedGridLinePositionAgainstOppositePosition(resolvedOppositePosition, position, it->value);
}

static inline size_t firstNamedGridLineBeforePosition(size_t position, const Vector<size_t>& gridLines)
{
    // The grid line inequality needs to be strict (which doesn't match the after / end case) because |position| is
    // already converted to an index in our grid representation (ie one was removed from the grid line to account for
    // the side).
    size_t firstLineBeforePositionIndex = 0;
    const size_t* firstLineBeforePosition = std::lower_bound(gridLines.begin(), gridLines.end(), position);
    if (firstLineBeforePosition != gridLines.end()) {
        if (*firstLineBeforePosition > position && firstLineBeforePosition != gridLines.begin())
            --firstLineBeforePosition;

        firstLineBeforePositionIndex = firstLineBeforePosition - gridLines.begin();
    }
    return firstLineBeforePositionIndex;
}

std::unique_ptr<GridSpan> GridResolvedPosition::resolveRowStartColumnStartNamedGridLinePositionAgainstOppositePosition(const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, const Vector<size_t>& gridLines)
{
    size_t gridLineIndex = std::max<int>(0, firstNamedGridLineBeforePosition(resolvedOppositePosition.toInt(), gridLines) - position.spanPosition() + 1);
    GridResolvedPosition resolvedGridLinePosition = GridResolvedPosition(gridLines[gridLineIndex]);
    if (resolvedGridLinePosition > resolvedOppositePosition)
        resolvedGridLinePosition = resolvedOppositePosition;
    return std::make_unique<GridSpan>(resolvedGridLinePosition, resolvedOppositePosition);
}

std::unique_ptr<GridSpan> GridResolvedPosition::resolveRowEndColumnEndNamedGridLinePositionAgainstOppositePosition(const GridResolvedPosition& resolvedOppositePosition, const GridPosition& position, const Vector<size_t>& gridLines)
{
    size_t firstLineAfterOppositePositionIndex = gridLines.size() - 1;
    const size_t* firstLineAfterOppositePosition = std::upper_bound(gridLines.begin(), gridLines.end(), resolvedOppositePosition);
    if (firstLineAfterOppositePosition != gridLines.end())
        firstLineAfterOppositePositionIndex = firstLineAfterOppositePosition - gridLines.begin();

    size_t gridLineIndex = std::min(gridLines.size() - 1, firstLineAfterOppositePositionIndex + position.spanPosition() - 1);
    GridResolvedPosition resolvedGridLinePosition = GridResolvedPosition::adjustGridPositionForRowEndColumnEndSide(gridLines[gridLineIndex]);
    if (resolvedGridLinePosition < resolvedOppositePosition)
        resolvedGridLinePosition = resolvedOppositePosition;
    return std::make_unique<GridSpan>(resolvedOppositePosition, resolvedGridLinePosition);
}

} // namespace WebCore

#endif // ENABLE(CSS_GRID_LAYOUT)
