/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013-2017 Igalia S.L.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Grid.h"
#include "GridTrackSizingAlgorithm.h"
#include "RenderBlock.h"

namespace WebCore {

class GridArea;
class GridSpan;

struct ContentAlignmentData {
    WTF_MAKE_NONCOPYABLE(ContentAlignmentData); WTF_MAKE_FAST_ALLOCATED;
public:
    ContentAlignmentData() = default;
    bool isValid() const { return positionOffset >= 0 && distributionOffset >= 0; }

    LayoutUnit positionOffset;
    LayoutUnit distributionOffset;
};

enum GridAxisPosition {GridAxisStart, GridAxisEnd, GridAxisCenter};

class RenderGrid final : public RenderBlock {
    WTF_MAKE_ISO_ALLOCATED(RenderGrid);
public:
    RenderGrid(Element&, RenderStyle&&);
    virtual ~RenderGrid();

    Element& element() const { return downcast<Element>(nodeForNonAnonymous()); }

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) override;

    bool avoidsFloats() const override { return true; }
    bool canDropAnonymousBlockChild() const override { return false; }

    void dirtyGrid();
    Vector<LayoutUnit> trackSizesForComputedStyle(GridTrackSizingDirection) const;

    const Vector<LayoutUnit>& columnPositions() const { return m_columnPositions; }
    const Vector<LayoutUnit>& rowPositions() const { return m_rowPositions; }

    unsigned autoRepeatCountForDirection(GridTrackSizingDirection direction) const { return m_grid.autoRepeatTracks(direction); }

    // Required by GridTrackSizingAlgorithm. Keep them under control.
    LayoutUnit guttersSize(const Grid&, GridTrackSizingDirection, unsigned startLine, unsigned span, Optional<LayoutUnit> availableSize) const;
    LayoutUnit gridItemOffset(GridTrackSizingDirection) const;

    void updateGridAreaLogicalSize(RenderBox&, LayoutSize) const;
    bool isBaselineAlignmentForChild(const RenderBox&) const;
    bool isBaselineAlignmentForChild(const RenderBox&, GridAxis) const;

    StyleSelfAlignmentData selfAlignmentForChild(GridAxis, const RenderBox&, const RenderStyle* = nullptr) const;

    StyleContentAlignmentData contentAlignment(GridTrackSizingDirection) const;

protected:
    ItemPosition selfAlignmentNormalBehavior(const RenderBox* child = nullptr) const override
    {
        ASSERT(child);
        return child->isRenderReplaced() ? ItemPosition::Start : ItemPosition::Stretch;
    }

private:
    const char* renderName() const override;
    bool isRenderGrid() const override { return true; }
    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;

    bool selfAlignmentChangedToStretch(GridAxis, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox&) const;
    bool selfAlignmentChangedFromStretch(GridAxis, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox&) const;

    Optional<LayoutUnit> availableSpaceForGutters(GridTrackSizingDirection) const;

    bool explicitGridDidResize(const RenderStyle&) const;
    bool namedGridLinesDefinitionDidChange(const RenderStyle&) const;

    unsigned computeAutoRepeatTracksCount(GridTrackSizingDirection, Optional<LayoutUnit> availableSize) const;

    unsigned clampAutoRepeatTracks(GridTrackSizingDirection, unsigned autoRepeatTracks) const;

    std::unique_ptr<OrderedTrackIndexSet> computeEmptyTracksForAutoRepeat(Grid&, GridTrackSizingDirection) const;

    void performGridItemsPreLayout(const GridTrackSizingAlgorithm&) const;

    void placeItemsOnGrid(GridTrackSizingAlgorithm&, Optional<LayoutUnit> availableLogicalWidth) const;
    void populateExplicitGridAndOrderIterator(Grid&) const;
    std::unique_ptr<GridArea> createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(Grid&, const RenderBox&, GridTrackSizingDirection, const GridSpan&) const;
    void placeSpecifiedMajorAxisItemsOnGrid(Grid&, const Vector<RenderBox*>&) const;
    void placeAutoMajorAxisItemsOnGrid(Grid&, const Vector<RenderBox*>&) const;
    typedef std::pair<unsigned, unsigned> AutoPlacementCursor;
    void placeAutoMajorAxisItemOnGrid(Grid&, RenderBox&, AutoPlacementCursor&) const;
    GridTrackSizingDirection autoPlacementMajorAxisDirection() const;
    GridTrackSizingDirection autoPlacementMinorAxisDirection() const;

    bool canPerformSimplifiedLayout() const final;
    void prepareChildForPositionedLayout(RenderBox&);
    bool hasStaticPositionForChild(const RenderBox&, GridTrackSizingDirection) const;
    void layoutPositionedObject(RenderBox&, bool relayoutChildren, bool fixedPositionObjectsOnly) override;

    void computeTrackSizesForDefiniteSize(GridTrackSizingDirection, LayoutUnit availableSpace);
    void computeTrackSizesForIndefiniteSize(GridTrackSizingAlgorithm&, GridTrackSizingDirection, LayoutUnit* minIntrinsicSize = nullptr, LayoutUnit* maxIntrinsicSize = nullptr) const;
    LayoutUnit computeTrackBasedLogicalHeight() const;

    void repeatTracksSizingIfNeeded(LayoutUnit availableSpaceForColumns, LayoutUnit availableSpaceForRows);

    void layoutGridItems();
    void populateGridPositionsForDirection(GridTrackSizingDirection);

    LayoutUnit resolveAutoStartGridPosition(GridTrackSizingDirection) const;
    LayoutUnit resolveAutoEndGridPosition(GridTrackSizingDirection) const;
    LayoutUnit gridAreaBreadthForOutOfFlowChild(const RenderBox&, GridTrackSizingDirection);
    LayoutUnit logicalOffsetForChild(const RenderBox&, GridTrackSizingDirection, LayoutUnit) const;
    void gridAreaPositionForOutOfFlowChild(const RenderBox&, GridTrackSizingDirection, LayoutUnit& start, LayoutUnit& end) const;
    void gridAreaPositionForInFlowChild(const RenderBox&, GridTrackSizingDirection, LayoutUnit& start, LayoutUnit& end) const;
    void gridAreaPositionForChild(const RenderBox&, GridTrackSizingDirection, LayoutUnit& start, LayoutUnit& end) const;

    GridAxisPosition columnAxisPositionForChild(const RenderBox&) const;
    GridAxisPosition rowAxisPositionForChild(const RenderBox&) const;
    LayoutUnit columnAxisOffsetForChild(const RenderBox&) const;
    LayoutUnit rowAxisOffsetForChild(const RenderBox&) const;
    void computeContentPositionAndDistributionOffset(GridTrackSizingDirection, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks);
    LayoutPoint findChildLogicalPosition(const RenderBox&) const;
    GridArea cachedGridArea(const RenderBox&) const;
    GridSpan cachedGridSpan(const RenderBox&, GridTrackSizingDirection) const;

    LayoutUnit gridAreaBreadthForChildIncludingAlignmentOffsets(const RenderBox&, GridTrackSizingDirection) const;

    void paintChildren(PaintInfo& forSelf, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect) override;
    LayoutUnit availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const RenderBox&) const;
    StyleSelfAlignmentData justifySelfForChild(const RenderBox&, const RenderStyle* = nullptr) const;
    StyleSelfAlignmentData alignSelfForChild(const RenderBox&, const RenderStyle* = nullptr) const;
    void applyStretchAlignmentToChildIfNeeded(RenderBox&);
    bool hasAutoSizeInColumnAxis(const RenderBox& child) const { return isHorizontalWritingMode() ? child.style().height().isAuto() : child.style().width().isAuto(); }
    bool hasAutoSizeInRowAxis(const RenderBox& child) const { return isHorizontalWritingMode() ? child.style().width().isAuto() : child.style().height().isAuto(); }
    bool allowedToStretchChildAlongColumnAxis(const RenderBox& child) const { return alignSelfForChild(child).position() == ItemPosition::Stretch && hasAutoSizeInColumnAxis(child) && !hasAutoMarginsInColumnAxis(child); }
    bool allowedToStretchChildAlongRowAxis(const RenderBox& child) const { return justifySelfForChild(child).position() == ItemPosition::Stretch && hasAutoSizeInRowAxis(child) && !hasAutoMarginsInRowAxis(child); }
    bool hasAutoMarginsInColumnAxis(const RenderBox&) const;
    bool hasAutoMarginsInRowAxis(const RenderBox&) const;
    void resetAutoMarginsAndLogicalTopInColumnAxis(RenderBox& child);
    void updateAutoMarginsInColumnAxisIfNeeded(RenderBox&);
    void updateAutoMarginsInRowAxisIfNeeded(RenderBox&);

    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;
    Optional<int> firstLineBaseline() const final;
    Optional<int> inlineBlockBaseline(LineDirectionMode) const final;
    bool isInlineBaselineAlignedChild(const RenderBox&) const;

    LayoutUnit columnAxisBaselineOffsetForChild(const RenderBox&) const;
    LayoutUnit rowAxisBaselineOffsetForChild(const RenderBox&) const;

    LayoutUnit gridGap(GridTrackSizingDirection) const;
    LayoutUnit gridGap(GridTrackSizingDirection, Optional<LayoutUnit> availableSize) const;

    unsigned nonCollapsedTracks(GridTrackSizingDirection) const;
    unsigned numTracks(GridTrackSizingDirection, const Grid&) const;

    LayoutUnit translateOutOfFlowRTLCoordinate(const RenderBox&, LayoutUnit) const;
    LayoutUnit translateRTLCoordinate(LayoutUnit) const;

    Grid m_grid;

    GridTrackSizingAlgorithm m_trackSizingAlgorithm;

    Vector<LayoutUnit> m_columnPositions;
    Vector<LayoutUnit> m_rowPositions;
    ContentAlignmentData m_offsetBetweenColumns;
    ContentAlignmentData m_offsetBetweenRows;

    typedef HashMap<const RenderBox*, Optional<size_t>> OutOfFlowPositionsMap;
    OutOfFlowPositionsMap m_outOfFlowItemColumn;
    OutOfFlowPositionsMap m_outOfFlowItemRow;

    bool m_hasAnyOrthogonalItem {false};
    bool m_baselineItemsCached {false};
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderGrid, isRenderGrid())
