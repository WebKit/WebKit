/*
 * Copyright (C) 2014-2017 Igalia S.L.
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
#include "GridPositionsResolver.h"

#include "GridArea.h"
#include "RenderBox.h"
#include "RenderGrid.h"
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

static inline GridTrackSizingDirection directionFromSide(GridPositionSide side)
{
    return side == ColumnStartSide || side == ColumnEndSide ? ForColumns : ForRows;
}

static const String implicitNamedGridLineForSide(const String& lineName, GridPositionSide side)
{
    return lineName + (isStartSide(side) ? "-start" : "-end");
}

static unsigned explicitGridSizeForSide(const RenderGrid& gridContainer, GridPositionSide side)
{
    return isColumnSide(side) ? GridPositionsResolver::explicitGridColumnCount(gridContainer) : GridPositionsResolver::explicitGridRowCount(gridContainer);
}

static inline GridPositionSide transposedSide(GridPositionSide side)
{
    switch (side) {
    case ColumnStartSide: return RowStartSide;
    case ColumnEndSide: return RowEndSide;
    case RowStartSide: return ColumnStartSide;
    default: return ColumnEndSide;
    }
}

static std::optional<int> clampedImplicitLineForArea(const RenderStyle& style, const String& name, int min, int max, bool isRowAxis, bool isStartSide)
{
    const NamedGridAreaMap& areas = style.namedGridArea();
    auto gridAreaIt = areas.find(name);
    if (gridAreaIt != areas.end()) {
        const GridArea& gridArea = gridAreaIt->value;
        auto gridSpan = isRowAxis ? gridArea.columns : gridArea.rows;
        if (gridSpan.clamp(min, max))
            return isStartSide ? gridSpan.startLine() : gridSpan.endLine();
    }
    return std::nullopt;
}

NamedLineCollectionBase::NamedLineCollectionBase(const RenderGrid& initialGrid, const String& name, GridPositionSide side, bool nameIsAreaName)
{
    String lineName = nameIsAreaName ? implicitNamedGridLineForSide(name, side) : name;
    auto direction = directionFromSide(side);
    const auto* grid = &initialGrid;
    const auto* gridContainerStyle = &grid->style();
    bool isRowAxis = direction == ForColumns;

    m_lastLine = explicitGridSizeForSide(*grid, side);

    const auto& gridLineNames = isRowAxis ? gridContainerStyle->namedGridColumnLines() : gridContainerStyle->namedGridRowLines();
    const auto& autoRepeatGridLineNames = isRowAxis ? gridContainerStyle->autoRepeatNamedGridColumnLines() : gridContainerStyle->autoRepeatNamedGridRowLines();
    const auto& implicitGridLineNames = isRowAxis ? gridContainerStyle->implicitNamedGridColumnLines() : gridContainerStyle->implicitNamedGridRowLines();

    auto linesIterator = gridLineNames.find(lineName);
    m_namedLinesIndices = linesIterator == gridLineNames.end() ? nullptr : &linesIterator->value;

    auto autoRepeatLinesIterator = autoRepeatGridLineNames.find(lineName);
    m_autoRepeatNamedLinesIndices = autoRepeatLinesIterator == autoRepeatGridLineNames.end() ? nullptr : &autoRepeatLinesIterator->value;

    auto implicitGridLinesIterator = implicitGridLineNames.find(lineName);
    m_implicitNamedLinesIndices = implicitGridLinesIterator == implicitGridLineNames.end() ? nullptr : &implicitGridLinesIterator->value;
    m_isSubgrid = grid->isSubgrid(direction);

    m_autoRepeatTotalTracks = grid->autoRepeatCountForDirection(direction);
    m_autoRepeatTrackListLength = isRowAxis ? gridContainerStyle->gridAutoRepeatColumns().size() : gridContainerStyle->gridAutoRepeatRows().size();
    m_autoRepeatLines = 0;
    m_insertionPoint = isRowAxis ? gridContainerStyle->gridAutoRepeatColumnsInsertionPoint() : gridContainerStyle->gridAutoRepeatRowsInsertionPoint();

    if (!m_isSubgrid) {
        if (isRowAxis ? gridContainerStyle->gridSubgridColumns() : gridContainerStyle->gridSubgridRows()) {
            // If subgrid was specified, but the grid wasn't able to actually become a subgrid, the used
            // value of the style should be the initial 'none' value.
            m_namedLinesIndices = nullptr;
            m_autoRepeatNamedLinesIndices = nullptr;
        }
        return;
    }

    if (m_implicitNamedLinesIndices) {
        // The implicit lines list was created based on the areas specified for the grid areas property, but the
        // subgrid might have inherited fewer tracks than needed to cover the specified area. We want to clamp
        // the specified area down to explicit grid we actually have, and then generate implicit -start/-end
        // lines for the new area.
        ASSERT(m_implicitNamedLinesIndices->size() == 1);
        m_implicitNamedLinesIndices = &m_inheritedNamedLinesIndices;

        // Find the area name that creates the implicit line we're looking for. If the input was an area name,
        // then we can use that, otherwise we need to choose the substring and infer which side the input specified.
        // It's possible for authors to manually name a *-start implicit line name for the end line search, and vice-versa,
        // so we need to remember which side we inferred from the name, separately from the side we're searching for.
        String areaName = name;
        bool startSide = isStartSide(side);
        if (!nameIsAreaName) {
            size_t suffix = name.find("-start"_s);
            if (suffix == notFound) {
                suffix = name.find("-end"_s);
                ASSERT(suffix != notFound);
                startSide = false;
            } else
                startSide = true;
            areaName = name.left(suffix);
        }
        auto implicitLine = clampedImplicitLineForArea(*gridContainerStyle, areaName, 0, m_lastLine, isRowAxis, startSide);
        if (implicitLine)
            m_inheritedNamedLinesIndices.append(*implicitLine);
    }

    ASSERT(!m_autoRepeatTotalTracks);
    m_autoRepeatTrackListLength = (isRowAxis ? gridContainerStyle->autoRepeatOrderedNamedGridColumnLines() : gridContainerStyle->autoRepeatOrderedNamedGridRowLines()).size();
    if (m_autoRepeatTrackListLength) {
        unsigned namedLines = (isRowAxis ? gridContainerStyle->orderedNamedGridColumnLines() : gridContainerStyle->orderedNamedGridRowLines()).size();
        unsigned totalLines = m_lastLine + 1;
        if (namedLines < totalLines) {
            // auto repeat in a subgrid specifies the line names that should be repeated, not
            // the tracks.
            m_autoRepeatLines = (totalLines - namedLines) / m_autoRepeatTrackListLength;
            m_autoRepeatLines *= m_autoRepeatTrackListLength;
        }
    }
}

void NamedLineCollectionBase::ensureInheritedNamedIndices()
{
    if (m_implicitNamedLinesIndices != &m_inheritedNamedLinesIndices) {
        if (m_implicitNamedLinesIndices)
            m_inheritedNamedLinesIndices.appendVector(*m_implicitNamedLinesIndices);
        m_implicitNamedLinesIndices = &m_inheritedNamedLinesIndices;
    }
};

bool NamedLineCollectionBase::contains(unsigned line) const
{
    if (line > m_lastLine)
        return false;

    auto contains = [](const Vector<unsigned>* Indices, unsigned line) {
        return Indices && Indices->find(line) != notFound;
    };

    if (!m_autoRepeatTrackListLength || line < m_insertionPoint)
        return contains(m_namedLinesIndices, line);

    if (m_isSubgrid) {
        if (line >= m_insertionPoint + m_autoRepeatLines)
            return contains(m_namedLinesIndices, line - m_autoRepeatLines);

        if (!m_autoRepeatLines)
            return contains(m_namedLinesIndices, line);

        unsigned autoRepeatIndexInFirstRepetition = (line - m_insertionPoint) % m_autoRepeatTrackListLength;
        return contains(m_autoRepeatNamedLinesIndices, autoRepeatIndexInFirstRepetition);
    }

    ASSERT(m_autoRepeatTotalTracks);

    if (line > m_insertionPoint + m_autoRepeatTotalTracks)
        return contains(m_namedLinesIndices, line - (m_autoRepeatTotalTracks - 1));

    if (line == m_insertionPoint)
        return contains(m_namedLinesIndices, line) || contains(m_autoRepeatNamedLinesIndices, 0);

    if (line == m_insertionPoint + m_autoRepeatTotalTracks)
        return contains(m_autoRepeatNamedLinesIndices, m_autoRepeatTrackListLength) || contains(m_namedLinesIndices, m_insertionPoint + 1);

    size_t autoRepeatIndexInFirstRepetition = (line - m_insertionPoint) % m_autoRepeatTrackListLength;
    if (!autoRepeatIndexInFirstRepetition && contains(m_autoRepeatNamedLinesIndices, m_autoRepeatTrackListLength))
        return true;
    return contains(m_autoRepeatNamedLinesIndices, autoRepeatIndexInFirstRepetition);
}

NamedLineCollection::NamedLineCollection(const RenderGrid& initialGrid, const String& name, GridPositionSide side, bool nameIsAreaName)
    : NamedLineCollectionBase(initialGrid, name, side, nameIsAreaName)
{
    if (!m_lastLine)
        return;
    auto search = GridSpan::translatedDefiniteGridSpan(0, m_lastLine);
    auto currentSide = side;
    auto direction = directionFromSide(currentSide);
    bool initialFlipped = GridLayoutFunctions::isFlippedDirection(initialGrid, direction);
    const RenderGrid* grid = &initialGrid;
    bool isRowAxis = direction == ForColumns;

    // If we're a subgrid, we want to inherit the line names from any ancestor grids.
    while (isRowAxis ? grid->isSubgridColumns() : grid->isSubgridRows()) {
        const auto* parent = downcast<RenderGrid>(grid->parent());

        // Translate our explicit grid set of lines into the coordinate space of the
        // parent grid, adjusting direction/side as needed.
        if (grid->isHorizontalWritingMode() != parent->isHorizontalWritingMode()) {
            isRowAxis = !isRowAxis;
            currentSide = transposedSide(currentSide);
        }
        direction = directionFromSide(currentSide);

        auto span = parent->gridSpanForChild(*grid, direction);
        search.translateTo(span, GridLayoutFunctions::isSubgridReversedDirection(*parent, direction, *grid));

        auto convertToInitialSpace = [&](unsigned i) {
            ASSERT(i >= search.startLine());
            i -= search.startLine();
            if (GridLayoutFunctions::isFlippedDirection(*parent, direction) != initialFlipped) {
                ASSERT(m_lastLine >= i);
                i = m_lastLine - i;
            }
            return i;
        };

        // Create a line collection for the parent grid, and check to see if any of our lines
        // are present. If we find any, add them to a locally stored line name list (with numbering
        // relative to our grid).
        bool appended = false;
        NamedLineCollectionBase parentCollection(*parent, name, currentSide, nameIsAreaName);
        for (unsigned i = search.startLine(); i <= search.endLine(); i++) {
            if (parentCollection.contains(i)) {
                ensureInheritedNamedIndices();
                appended = true;
                m_inheritedNamedLinesIndices.append(convertToInitialSpace(i));
            }
        }

        if (nameIsAreaName) {
            // We now need to look at the grid areas for the parent (not the implicit
            // lines for the parent!), and insert the ones that intersect as implicit
            // lines (but in our single combined list).
            auto implicitLine = clampedImplicitLineForArea(parent->style(), name, search.startLine(), search.endLine(), isRowAxis, isStartSide(side));
            if (implicitLine) {
                ensureInheritedNamedIndices();
                appended = true;
                m_inheritedNamedLinesIndices.append(convertToInitialSpace(*implicitLine));
            }
        }

        if (appended) {
            // Re-sort m_inheritedNamedLinesIndices
            std::sort(m_inheritedNamedLinesIndices.begin(), m_inheritedNamedLinesIndices.end());
        }

        grid = parent;
    }
}

bool NamedLineCollection::hasExplicitNamedLines() const
{
    if (m_namedLinesIndices)
        return true;
    return m_autoRepeatNamedLinesIndices && (!m_isSubgrid || m_autoRepeatLines);
}

bool NamedLineCollection::hasNamedLines() const
{
    return hasExplicitNamedLines() || m_implicitNamedLinesIndices;
}

unsigned NamedLineCollection::lastLine() const
{
    return m_lastLine;
}

bool NamedLineCollection::contains(unsigned line) const
{
    ASSERT(hasNamedLines());

    if (line > m_lastLine)
        return false;

    auto contains = [](const Vector<unsigned>* Indices, unsigned line) {
        return Indices && Indices->find(line) != notFound;
    };

    if (contains(m_implicitNamedLinesIndices, line))
        return true;

    return NamedLineCollectionBase::contains(line);
}

int NamedLineCollection::firstExplicitPosition() const
{
    ASSERT(hasExplicitNamedLines());
    unsigned firstLine = 0;
    unsigned autoRepeats = m_isSubgrid ? m_autoRepeatLines : m_autoRepeatTotalTracks;

    // If there is no auto repeat(), there must be some named line outside, return the 1st one. Also return it if it precedes the auto-repeat().
    if (!autoRepeats || (m_namedLinesIndices && m_namedLinesIndices->at(firstLine) <= m_insertionPoint))
        return m_namedLinesIndices->at(firstLine);

    // Return the 1st named line inside the auto repeat(), if any.
    if (m_autoRepeatNamedLinesIndices)
        return m_autoRepeatNamedLinesIndices->at(firstLine) + m_insertionPoint;

    // The 1st named line must be after the auto repeat().
    if (m_isSubgrid)
        return m_namedLinesIndices->at(firstLine) + autoRepeats;
    return m_namedLinesIndices->at(firstLine) + autoRepeats - 1;
}

int NamedLineCollection::firstPosition() const
{
    ASSERT(hasNamedLines());
    unsigned firstLine = 0;
    if (!m_implicitNamedLinesIndices)
        return firstExplicitPosition();
    if (!hasExplicitNamedLines())
        return m_implicitNamedLinesIndices->at(firstLine);
    return std::min<int>(firstExplicitPosition(), m_implicitNamedLinesIndices->at(firstLine));
}

// https://drafts.csswg.org/css-grid-2/#indefinite-grid-span
static bool isIndefiniteSpan(GridPosition& initialPosition, GridPosition& finalPosition)
{
    if (initialPosition.isAuto())
        return !finalPosition.isSpan();
    if (finalPosition.isAuto())
        return !initialPosition.isSpan();
    return false;
}

static void adjustGridPositionsFromStyle(const RenderBox& gridItem, GridTrackSizingDirection direction, GridPosition& initialPosition, GridPosition& finalPosition)
{
    bool isForColumns = direction == ForColumns;
    initialPosition = isForColumns ? gridItem.style().gridItemColumnStart() : gridItem.style().gridItemRowStart();
    finalPosition = isForColumns ? gridItem.style().gridItemColumnEnd() : gridItem.style().gridItemRowEnd();

    // We must handle the placement error handling code here instead of in the StyleAdjuster because we don't want to
    // overwrite the specified values.
    if (initialPosition.isSpan() && finalPosition.isSpan())
        finalPosition.setAutoPosition();

    // If the grid item has an automatic position and a grid span for a named line in a given dimension, instead treat the grid span as one.
    if (initialPosition.isAuto() && finalPosition.isSpan() && !finalPosition.namedGridLine().isNull())
        finalPosition.setSpanPosition(1, String());
    if (finalPosition.isAuto() && initialPosition.isSpan() && !initialPosition.namedGridLine().isNull())
        initialPosition.setSpanPosition(1, String());

    if (isIndefiniteSpan(initialPosition, finalPosition) && is<RenderGrid>(gridItem) && downcast<RenderGrid>(gridItem).isSubgrid(direction)) {
        // Indefinite span for an item that is subgridded in this axis.
        int lineCount = (isForColumns ? gridItem.style().orderedNamedGridColumnLines() : gridItem.style().orderedNamedGridRowLines()).size();

        if (initialPosition.isAuto()) {
            // Set initial position to span <line names - 1>
            initialPosition.setSpanPosition(std::max(1, lineCount - 1), emptyString());
        } else {
            // Set final position to span <line names - 1>
            finalPosition.setSpanPosition(std::max(1, lineCount - 1), emptyString());
        }
    }
}

unsigned GridPositionsResolver::explicitGridColumnCount(const RenderGrid& gridContainer)
{
    if (gridContainer.isSubgridColumns()) {
        const RenderGrid& parent = *downcast<RenderGrid>(gridContainer.parent());
        GridTrackSizingDirection direction = GridLayoutFunctions::flowAwareDirectionForChild(parent, gridContainer, ForColumns);
        return parent.gridSpanForChild(gridContainer, direction).integerSpan();
    }
    return std::min<unsigned>(std::max(gridContainer.style().gridColumnTrackSizes().size() + gridContainer.autoRepeatCountForDirection(ForColumns), gridContainer.style().namedGridAreaColumnCount()), GridPosition::max());
}

unsigned GridPositionsResolver::explicitGridRowCount(const RenderGrid& gridContainer)
{
    if (gridContainer.isSubgridRows()) {
        const RenderGrid& parent = *downcast<RenderGrid>(gridContainer.parent());
        GridTrackSizingDirection direction = GridLayoutFunctions::flowAwareDirectionForChild(parent, gridContainer, ForRows);
        return parent.gridSpanForChild(gridContainer, direction).integerSpan();
    }
    return std::min<unsigned>(std::max(gridContainer.style().gridRowTrackSizes().size() + gridContainer.autoRepeatCountForDirection(ForRows), gridContainer.style().namedGridAreaRowCount()), GridPosition::max());
}

static unsigned lookAheadForNamedGridLine(int start, unsigned numberOfLines, NamedLineCollection& linesCollection)
{
    ASSERT(numberOfLines);

    // Only implicit lines on the search direction are assumed to have the given name, so we can start to look from first line.
    // See: https://drafts.csswg.org/css-grid/#grid-placement-span-int
    unsigned end = std::max(start, 0);

    if (!linesCollection.hasNamedLines())
        return std::max(end, linesCollection.lastLine() + 1) + numberOfLines - 1;

    for (; numberOfLines; ++end) {
        if (end > linesCollection.lastLine() || linesCollection.contains(end))
            numberOfLines--;
    }

    ASSERT(end);
    return end - 1;
}

static int lookBackForNamedGridLine(int end, unsigned numberOfLines, NamedLineCollection& linesCollection)
{
    ASSERT(numberOfLines);


    // Only implicit lines on the search direction are assumed to have the given name, so we can start to look from last line.
    // See: https://drafts.csswg.org/css-grid/#grid-placement-span-int
    int start = std::min<int>(end, linesCollection.lastLine());

    if (!linesCollection.hasNamedLines())
        return std::min(start, -1) - numberOfLines + 1;

    for (; numberOfLines; --start) {
        if (start < 0 || linesCollection.contains(start))
            numberOfLines--;
    }

    return start + 1;
}

static int resolveNamedGridLinePositionFromStyle(const RenderGrid& gridContainer, const GridPosition& position, GridPositionSide side)
{
    ASSERT(!position.namedGridLine().isNull());

    NamedLineCollection linesCollection(gridContainer, position.namedGridLine(), side);

    if (position.isPositive())
        return lookAheadForNamedGridLine(0, std::abs(position.integerPosition()), linesCollection);
    return lookBackForNamedGridLine(linesCollection.lastLine(), std::abs(position.integerPosition()), linesCollection);
}

static GridSpan definiteGridSpanWithNamedLineSpanAgainstOpposite(int oppositeLine, const GridPosition& position, GridPositionSide side, NamedLineCollection& linesCollection)
{
    int start, end;
    if (side == RowStartSide || side == ColumnStartSide) {
        start = lookBackForNamedGridLine(oppositeLine - 1, position.spanPosition(), linesCollection);
        end = oppositeLine;
    } else {
        start = oppositeLine;
        end = lookAheadForNamedGridLine(oppositeLine + 1, position.spanPosition(), linesCollection);
    }

    return GridSpan::untranslatedDefiniteGridSpan(start, end);
}

static GridSpan resolveNamedGridLinePositionAgainstOppositePosition(const RenderGrid& gridContainer, int oppositeLine, const GridPosition& position, GridPositionSide side)
{
    ASSERT(position.isSpan());
    ASSERT(!position.namedGridLine().isNull());
    // Negative positions are not allowed per the specification and should have been handled during parsing.
    ASSERT(position.spanPosition() > 0);

    NamedLineCollection linesCollection(gridContainer, position.namedGridLine(), side);
    return definiteGridSpanWithNamedLineSpanAgainstOpposite(oppositeLine, position, side, linesCollection);
}

static GridSpan resolveGridPositionAgainstOppositePosition(const RenderGrid& gridContainer, int oppositeLine, const GridPosition& position, GridPositionSide side)
{
    if (position.isAuto()) {
        if (isStartSide(side))
            return GridSpan::untranslatedDefiniteGridSpan(oppositeLine - 1, oppositeLine);
        return GridSpan::untranslatedDefiniteGridSpan(oppositeLine, oppositeLine + 1);
    }

    ASSERT(position.isSpan());
    ASSERT(position.spanPosition() > 0);

    if (!position.namedGridLine().isNull()) {
        // span 2 'c' -> we need to find the appropriate grid line before / after our opposite position.
        return resolveNamedGridLinePositionAgainstOppositePosition(gridContainer, oppositeLine, position, side);
    }

    // 'span 1' is contained inside a single grid track regardless of the direction.
    // That's why the CSS span value is one more than the offset we apply.
    unsigned positionOffset = position.spanPosition();
    if (isStartSide(side))
        return GridSpan::untranslatedDefiniteGridSpan(oppositeLine - positionOffset, oppositeLine);

    return GridSpan::untranslatedDefiniteGridSpan(oppositeLine, oppositeLine + positionOffset);
}

GridPositionSide GridPositionsResolver::initialPositionSide(GridTrackSizingDirection direction)
{
    return direction == ForColumns ? ColumnStartSide : RowStartSide;
}

GridPositionSide GridPositionsResolver::finalPositionSide(GridTrackSizingDirection direction)
{
    return direction == ForColumns ? ColumnEndSide : RowEndSide;
}

unsigned GridPositionsResolver::spanSizeForAutoPlacedItem(const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    GridPosition initialPosition, finalPosition;
    adjustGridPositionsFromStyle(gridItem, direction, initialPosition, finalPosition);

    // This method will only be used when both positions need to be resolved against the opposite one.
    ASSERT(initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition());

    if (initialPosition.isAuto() && finalPosition.isAuto())
        return 1;

    GridPosition position = initialPosition.isSpan() ? initialPosition : finalPosition;
    ASSERT(position.isSpan());

    ASSERT(position.spanPosition());
    return position.spanPosition();
}

static int resolveGridPositionFromStyle(const RenderGrid& gridContainer, const GridPosition& position, GridPositionSide side)
{
    switch (position.type()) {
    case ExplicitPosition: {
        ASSERT(position.integerPosition());

        if (!position.namedGridLine().isNull())
            return resolveNamedGridLinePositionFromStyle(gridContainer, position, side);

        // Handle <integer> explicit position.
        if (position.isPositive())
            return position.integerPosition() - 1;

        unsigned resolvedPosition = std::abs(position.integerPosition()) - 1;
        const unsigned endOfTrack = explicitGridSizeForSide(gridContainer, side);

        return endOfTrack - resolvedPosition;
    }
    case NamedGridAreaPosition:
    {
        // First attempt to match the grid area's edge to a named grid area: if there is a named line with the name
        // ''<custom-ident>-start (for grid-*-start) / <custom-ident>-end'' (for grid-*-end), contributes the first such
        // line to the grid item's placement.
        String namedGridLine = position.namedGridLine();
        ASSERT(!position.namedGridLine().isNull());

        NamedLineCollection implicitLines(gridContainer, namedGridLine, side, true);
        if (implicitLines.hasNamedLines())
            return implicitLines.firstPosition();

        // Otherwise, if there is a named line with the specified name, contributes the first such line to the grid
        // item's placement.
        NamedLineCollection explicitLines(gridContainer, namedGridLine, side);
        if (explicitLines.hasNamedLines())
            return explicitLines.firstPosition();

        // If none of the above works specs mandate to assume that all the lines in the implicit grid have this name.
        return explicitGridSizeForSide(gridContainer, side) + 1;
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

GridSpan GridPositionsResolver::resolveGridPositionsFromStyle(const RenderGrid& gridContainer, const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    GridPosition initialPosition, finalPosition;
    adjustGridPositionsFromStyle(gridItem, direction, initialPosition, finalPosition);

    GridPositionSide initialSide = initialPositionSide(direction);
    GridPositionSide finalSide = finalPositionSide(direction);

    // We can't get our grid positions without running the auto placement algorithm.
    if (initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition())
        return GridSpan::indefiniteGridSpan();

    if (initialPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer the position from the final position ('auto / 1' or 'span 2 / 3' case).
        auto endLine = resolveGridPositionFromStyle(gridContainer, finalPosition, finalSide);
        return resolveGridPositionAgainstOppositePosition(gridContainer, endLine, initialPosition, initialSide);
    }

    if (finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer our position from the initial position ('1 / auto' or '3 / span 2' case).
        auto startLine = resolveGridPositionFromStyle(gridContainer, initialPosition, initialSide);
        return resolveGridPositionAgainstOppositePosition(gridContainer, startLine, finalPosition, finalSide);
    }

    int startLine = resolveGridPositionFromStyle(gridContainer, initialPosition, initialSide);
    int endLine = resolveGridPositionFromStyle(gridContainer, finalPosition, finalSide);

    if (startLine > endLine)
        std::swap(startLine, endLine);
    else if (startLine == endLine)
        endLine = startLine + 1;

    return GridSpan::untranslatedDefiniteGridSpan(startLine, std::max(startLine, endLine));
}

} // namespace WebCore
