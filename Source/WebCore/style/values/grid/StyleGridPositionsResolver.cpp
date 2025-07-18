/*
 * Copyright (C) 2014-2017 Igalia S.L.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleGridPositionsResolver.h"

#include "AncestorSubgridIterator.h"
#include "GridArea.h"
#include "RenderBox.h"
#include "RenderGrid.h"
#include "RenderStyleInlines.h"
#include "StyleGridData.h"
#include "StyleGridPositionSide.h"
#include "StyleGridTrackSizingDirection.h"
#include <cstdlib>
#include <ranges>
#include <wtf/text/MakeString.h>

namespace WebCore {
namespace Style {

namespace {

class NamedLineCollectionBase {
    WTF_MAKE_NONCOPYABLE(NamedLineCollectionBase);
public:
    NamedLineCollectionBase(const RenderGrid&, const String& name, GridPositionSide, bool nameIsAreaName);

    bool hasNamedLines() const;
    bool hasExplicitNamedLines() const;
    bool contains(unsigned line) const;
protected:

    void ensureInheritedNamedIndices();

    const Vector<unsigned>* m_namedLinesIndices { nullptr };
    const Vector<unsigned>* m_autoRepeatNamedLinesIndices { nullptr };
    const Vector<unsigned>* m_implicitNamedLinesIndices { nullptr };

    Vector<unsigned> m_inheritedNamedLinesIndices;

    unsigned m_insertionPoint { 0 };
    unsigned m_lastLine { 0 };
    unsigned m_autoRepeatTotalTracks { 0 };
    unsigned m_autoRepeatLines { 0 };
    unsigned m_autoRepeatTrackListLength { 0 };
    bool m_isSubgrid { false };
};

class NamedLineCollection : public NamedLineCollectionBase {
    WTF_MAKE_NONCOPYABLE(NamedLineCollection);
public:
    NamedLineCollection(const RenderGrid&, const String& name, GridPositionSide, bool nameIsAreaName = false);

    int firstPosition() const;

    unsigned lastLine() const;

private:
    int firstExplicitPosition() const;
};

} // namespace (anonymous)

static inline bool isColumnSide(GridPositionSide side)
{
    return side == GridPositionSide::ColumnStartSide || side == GridPositionSide::ColumnEndSide;
}

static inline bool isStartSide(GridPositionSide side)
{
    return side == GridPositionSide::ColumnStartSide || side == GridPositionSide::RowStartSide;
}

static inline GridTrackSizingDirection directionFromSide(GridPositionSide side)
{
    return side == GridPositionSide::ColumnStartSide || side == GridPositionSide::ColumnEndSide ? GridTrackSizingDirection::Columns : GridTrackSizingDirection::Rows;
}

static String implicitNamedGridLineForSide(const String& lineName, GridPositionSide side)
{
    return makeString(lineName, isStartSide(side) ? "-start"_s : "-end"_s);
}

static unsigned explicitGridSizeForSide(const RenderGrid& gridContainer, GridPositionSide side)
{
    return GridPositionsResolver::explicitGridCount(gridContainer, directionFromSide(side));
}

static inline GridPositionSide transposedSide(GridPositionSide side)
{
    switch (side) {
    case GridPositionSide::ColumnStartSide: return GridPositionSide::RowStartSide;
    case GridPositionSide::ColumnEndSide: return GridPositionSide::RowEndSide;
    case GridPositionSide::RowStartSide: return GridPositionSide::ColumnStartSide;
    default: return GridPositionSide::ColumnEndSide;
    }
}

static std::optional<int> clampedImplicitLineForArea(const RenderStyle& style, const String& name, int min, int max, GridTrackSizingDirection direction, bool isStartSide)
{
    auto& areas = style.gridTemplateAreas().map.map;
    auto gridAreaIt = areas.find(name);
    if (gridAreaIt != areas.end()) {
        auto& gridArea = gridAreaIt->value;
        auto gridSpan = gridArea.span(direction);
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

    m_lastLine = explicitGridSizeForSide(*grid, side);

    auto& tracks = gridContainerStyle->gridTemplateList(direction);
    auto& gridLineNames = tracks.namedLines.map;
    auto& autoRepeatGridLineNames = tracks.autoRepeatNamedLines.map;
    auto& implicitGridLineNames = gridContainerStyle->gridTemplateAreas().implicitNamedGridLines(direction).map;

    auto linesIterator = gridLineNames.find(lineName);
    m_namedLinesIndices = linesIterator == gridLineNames.end() ? nullptr : &linesIterator->value;

    auto autoRepeatLinesIterator = autoRepeatGridLineNames.find(lineName);
    m_autoRepeatNamedLinesIndices = autoRepeatLinesIterator == autoRepeatGridLineNames.end() ? nullptr : &autoRepeatLinesIterator->value;

    auto implicitGridLinesIterator = implicitGridLineNames.find(lineName);
    m_implicitNamedLinesIndices = implicitGridLinesIterator == implicitGridLineNames.end() ? nullptr : &implicitGridLinesIterator->value;
    m_isSubgrid = grid->isSubgrid(direction);

    m_autoRepeatTotalTracks = grid->autoRepeatCountForDirection(direction);
    m_autoRepeatTrackListLength = tracks.autoRepeatSizes.size();
    m_autoRepeatLines = 0;
    m_insertionPoint = tracks.autoRepeatInsertionPoint;

    if (!m_isSubgrid) {
        if (tracks.subgrid) {
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
        auto implicitLine = clampedImplicitLineForArea(*gridContainerStyle, areaName, 0, m_lastLine, direction, startSide);
        if (implicitLine)
            m_inheritedNamedLinesIndices.append(*implicitLine);
    }

    ASSERT(!m_autoRepeatTotalTracks);
    m_autoRepeatTrackListLength = tracks.autoRepeatOrderedNamedLines.map.size();
    if (m_autoRepeatTrackListLength) {
        unsigned namedLines = tracks.orderedNamedLines.map.size();
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
}

bool NamedLineCollectionBase::contains(unsigned line) const
{
    ASSERT(hasNamedLines());

    if (line > m_lastLine)
        return false;

    auto contains = [](const Vector<unsigned>* Indices, unsigned line) {
        return Indices && Indices->find(line) != notFound;
    };

    if (contains(m_implicitNamedLinesIndices, line))
        return true;

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

    // If we're a subgrid, we want to inherit the line names from any ancestor grids.
    if (initialGrid.isSubgrid(direction)) {
        for (auto& currentAncestorSubgrid : AncestorSubgridIterator(initialGrid, direction)) {
            auto* currentAncestorSubgridParent = downcast<RenderGrid>(currentAncestorSubgrid.parent());
            auto& currentAncestorStyle = currentAncestorSubgrid.style();

            // auto-placed subgrids inside a masonry grid do not inherit any line names
            if ((currentAncestorSubgridParent->areMasonryRows() && (currentAncestorStyle.gridItemColumnStart().isAuto() || currentAncestorStyle.gridItemColumnStart().isSpan()))
                || (currentAncestorSubgridParent->areMasonryColumns() && (currentAncestorStyle.gridItemRowStart().isAuto() || currentAncestorStyle.gridItemRowStart().isSpan())))
                return;
            // Translate our explicit grid set of lines into the coordinate space of the
            // parent grid, adjusting direction/side as needed.
            if (currentAncestorSubgrid.isHorizontalWritingMode() != currentAncestorSubgridParent->isHorizontalWritingMode())
                currentSide = transposedSide(currentSide);
            direction = directionFromSide(currentSide);

            auto span = currentAncestorSubgridParent->gridSpanForGridItem(currentAncestorSubgrid, direction);
            search.translateTo(span, GridLayoutFunctions::isSubgridReversedDirection(*currentAncestorSubgridParent, direction, currentAncestorSubgrid));

            auto convertToInitialSpace = [&](unsigned i) {
                ASSERT(i >= search.startLine());
                i -= search.startLine();
                if (GridLayoutFunctions::isFlippedDirection(*currentAncestorSubgridParent, direction) != initialFlipped) {
                    ASSERT(m_lastLine >= i);
                    i = m_lastLine - i;
                }
                return i;
            };

            // Create a line collection for the parent grid, and check to see if any of our lines
            // are present. If we find any, add them to a locally stored line name list (with numbering
            // relative to our grid).
            bool appended = false;
            NamedLineCollectionBase parentCollection(*currentAncestorSubgridParent, name, currentSide, nameIsAreaName);
            if (parentCollection.hasNamedLines()) {
                for (unsigned i = search.startLine(); i <= search.endLine(); i++) {
                    if (parentCollection.contains(i)) {
                        ensureInheritedNamedIndices();
                        appended = true;
                        m_inheritedNamedLinesIndices.append(convertToInitialSpace(i));
                    }
                }
            }

            if (nameIsAreaName) {
                // We now need to look at the grid areas for the parent (not the implicit
                // lines for the parent!), and insert the ones that intersect as implicit
                // lines (but in our single combined list).
                auto implicitLine = clampedImplicitLineForArea(currentAncestorSubgridParent->style(), name, search.startLine(), search.endLine(), direction, isStartSide(side));
                if (implicitLine) {
                    ensureInheritedNamedIndices();
                    appended = true;
                    m_inheritedNamedLinesIndices.append(convertToInitialSpace(*implicitLine));
                }
            }

            if (appended) {
                // Re-sort m_inheritedNamedLinesIndices
                std::ranges::sort(m_inheritedNamedLinesIndices);
            }
        }
    }
}

bool NamedLineCollectionBase::hasExplicitNamedLines() const
{
    if (m_namedLinesIndices)
        return true;
    return m_autoRepeatNamedLinesIndices && (!m_isSubgrid || m_autoRepeatLines);
}

bool NamedLineCollectionBase::hasNamedLines() const
{
    return hasExplicitNamedLines() || (m_implicitNamedLinesIndices && !m_implicitNamedLinesIndices->isEmpty());
}

unsigned NamedLineCollection::lastLine() const
{
    return m_lastLine;
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
static bool isIndefiniteSpan(const GridPosition& initialPosition, const GridPosition& finalPosition)
{
    if (initialPosition.isAuto())
        return !finalPosition.isSpan();
    if (finalPosition.isAuto())
        return !initialPosition.isSpan();
    return false;
}

static std::pair<GridPosition, GridPosition> adjustGridPositionsFromStyle(const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    auto initialPosition = gridItem.style().gridItemStart(direction);
    auto finalPosition = gridItem.style().gridItemEnd(direction);

    // We must handle the placement error handling code here instead of in the StyleAdjuster because we don't want to
    // overwrite the specified values.
    if (initialPosition.isSpan() && finalPosition.isSpan())
        finalPosition = CSS::Keyword::Auto { };

    // If the grid item has an automatic position and a grid span for a named line in a given dimension, instead treat the grid span as one.
    if (initialPosition.isAuto() && finalPosition.isSpan() && !finalPosition.namedGridLine().isNull())
        finalPosition = GridPosition::Span { { 1 } };
    if (finalPosition.isAuto() && initialPosition.isSpan() && !initialPosition.namedGridLine().isNull())
        initialPosition = GridPosition::Span { { 1 } };

    if (isIndefiniteSpan(initialPosition, finalPosition)) {
        auto* renderGrid = dynamicDowncast<RenderGrid>(gridItem);
        if (renderGrid && renderGrid->isSubgrid(direction)) {
            // Indefinite span for an item that is subgridded in this axis.
            int lineCount = gridItem.style().gridTemplateList(direction).orderedNamedLines.map.size();

            if (initialPosition.isAuto()) {
                // Set initial position to span <line names - 1>
                initialPosition = GridPosition::Span { { std::max(1, lineCount - 1) } };
            } else {
                // Set final position to span <line names - 1>
                finalPosition = GridPosition::Span { { std::max(1, lineCount - 1) } };
            }
        }
    }

    return { WTFMove(initialPosition), WTFMove(finalPosition) };
}

unsigned GridPositionsResolver::explicitGridCount(const RenderGrid& gridContainer, GridTrackSizingDirection direction)
{
    if (gridContainer.isSubgrid(direction)) {
        auto& parent = *downcast<RenderGrid>(gridContainer.parent());
        auto flowAwareDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(parent, gridContainer, direction);
        return parent.gridSpanForGridItem(gridContainer, flowAwareDirection).integerSpan();
    }
    auto& style = gridContainer.style();
    return std::min<unsigned>(
        std::max(
            style.gridTemplateList(direction).sizes.size() + gridContainer.autoRepeatCountForDirection(direction),
            style.gridTemplateAreas().count(direction)
        ),
        GridPosition::max()
    );
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

static int resolveNamedGridLinePositionFromStyle(const RenderGrid& gridContainer, const CustomIdentifier& name, const GridPosition::Explicit::Position& explicitPosition, GridPositionSide side)
{
    NamedLineCollection linesCollection(gridContainer, name.value, side);

    if (explicitPosition.value > 0)
        return lookAheadForNamedGridLine(0, std::abs(explicitPosition.value), linesCollection);
    return lookBackForNamedGridLine(linesCollection.lastLine(), std::abs(explicitPosition.value), linesCollection);
}

static GridSpan definiteGridSpanWithNamedLineSpanAgainstOpposite(int oppositeLine, const GridPosition::Span::Position& spanPosition, GridPositionSide side, NamedLineCollection& linesCollection)
{
    int start, end;
    if (side == GridPositionSide::RowStartSide || side == GridPositionSide::ColumnStartSide) {
        start = lookBackForNamedGridLine(oppositeLine - 1, spanPosition.value, linesCollection);
        end = oppositeLine;
    } else {
        start = oppositeLine;
        end = lookAheadForNamedGridLine(oppositeLine + 1, spanPosition.value, linesCollection);
    }

    return GridSpan::untranslatedDefiniteGridSpan(start, end);
}

static GridSpan resolveNamedGridLinePositionAgainstOppositePosition(const RenderGrid& gridContainer, int oppositeLine, const CustomIdentifier& name, const GridPosition::Span::Position& spanPosition, GridPositionSide side)
{
    NamedLineCollection linesCollection(gridContainer, name.value, side);

    return definiteGridSpanWithNamedLineSpanAgainstOpposite(oppositeLine, spanPosition, side, linesCollection);
}

static GridSpan resolveGridPositionAgainstOppositePosition(const RenderGrid& gridContainer, int oppositeLine, const GridPosition& position, GridPositionSide side)
{
    ASSERT(position.shouldBeResolvedAgainstOppositePosition());

    return WTF::switchOn(position,
        [&](const CSS::Keyword::Auto&) -> GridSpan {
            if (isStartSide(side))
                return GridSpan::untranslatedDefiniteGridSpan(oppositeLine - 1, oppositeLine);
            return GridSpan::untranslatedDefiniteGridSpan(oppositeLine, oppositeLine + 1);
        },
        [&](const GridPosition::Span& spanPosition) -> GridSpan {
            if (!spanPosition.name.value.isNull()) {
                // span 2 'c' -> we need to find the appropriate grid line before / after our opposite position.
                return resolveNamedGridLinePositionAgainstOppositePosition(gridContainer, oppositeLine, spanPosition.name, spanPosition.position, side);
            }

            // 'span 1' is contained inside a single grid track regardless of the direction.
            // That's why the CSS span value is one more than the offset we apply.
            auto positionOffset = static_cast<unsigned>(spanPosition.position.value);
            if (isStartSide(side))
                return GridSpan::untranslatedDefiniteGridSpan(oppositeLine - positionOffset, oppositeLine);
            return GridSpan::untranslatedDefiniteGridSpan(oppositeLine, oppositeLine + positionOffset);
        },
        [&](const GridPosition::Explicit&) -> GridSpan {
            ASSERT_NOT_REACHED();
            return GridSpan::indefiniteGridSpan();
        },
        [&](const CustomIdentifier&) -> GridSpan {
            ASSERT_NOT_REACHED();
            return GridSpan::indefiniteGridSpan();
        }
    );
}

GridPositionSide GridPositionsResolver::initialPositionSide(GridTrackSizingDirection direction)
{
    return direction == GridTrackSizingDirection::Columns ? GridPositionSide::ColumnStartSide : GridPositionSide::RowStartSide;
}

GridPositionSide GridPositionsResolver::finalPositionSide(GridTrackSizingDirection direction)
{
    return direction == GridTrackSizingDirection::Columns ? GridPositionSide::ColumnEndSide : GridPositionSide::RowEndSide;
}

unsigned GridPositionsResolver::spanSizeForAutoPlacedItem(const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    auto [initialPosition, finalPosition] = adjustGridPositionsFromStyle(gridItem, direction);

    // This method will only be used when both positions need to be resolved against the opposite one.
    ASSERT(initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition());

    if (initialPosition.isAuto() && finalPosition.isAuto())
        return 1;

    auto position = initialPosition.isSpan() ? initialPosition : finalPosition;
    ASSERT(position.isSpan());

    ASSERT(position.spanPosition());
    return position.spanPosition();
}

static int resolveGridPositionFromStyle(const RenderGrid& gridContainer, const GridPosition& position, GridPositionSide side)
{
    return WTF::switchOn(position,
        [&](const GridPosition::Explicit& explicitPosition) -> int {
            ASSERT(explicitPosition.position != 0);

            if (!explicitPosition.name.value.isNull())
                return resolveNamedGridLinePositionFromStyle(gridContainer, explicitPosition.name, explicitPosition.position, side);

            // Handle <integer> explicit position.
            if (explicitPosition.position.value > 0)
                return explicitPosition.position.value - 1;

            unsigned resolvedPosition = std::abs(explicitPosition.position.value) - 1;
            unsigned endOfTrack = explicitGridSizeForSide(gridContainer, side);

            return endOfTrack - resolvedPosition;
        },
        [&](const CustomIdentifier& namedGridAreaPosition) -> int {
            // First attempt to match the grid area's edge to a named grid area: if there is a named line with the name
            // ''<custom-ident>-start (for grid-*-start) / <custom-ident>-end'' (for grid-*-end), contributes the first such
            // line to the grid item's placement.
            auto namedGridLine = namedGridAreaPosition.value;
            ASSERT(!namedGridLine.isNull());

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
        },
        [&](const CSS::Keyword::Auto&) -> int {
            // 'auto' depends on the opposite position for resolution (e.g. `grid-row: auto / 1`).
            ASSERT_NOT_REACHED();
            return 0;
        },
        [&](const GridPosition::Span&) -> int {
            // `span` depends on the opposite position for resolution (e.g. `grid-column: span 3 / "myHeader"`).
            ASSERT_NOT_REACHED();
            return 0;
        }
    );
}

GridSpan GridPositionsResolver::resolveGridPositionsFromStyle(const RenderGrid& gridContainer, const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    auto [initialPosition, finalPosition] = adjustGridPositionsFromStyle(gridItem, direction);

    auto initialSide = initialPositionSide(direction);
    auto finalSide = finalPositionSide(direction);

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

} // namespace Style
} // namespace WebCore
