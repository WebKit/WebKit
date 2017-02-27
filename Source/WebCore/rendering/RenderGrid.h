/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013, 2014 Igalia S.L.
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

struct ContentAlignmentData;

enum GridAxisPosition {GridAxisStart, GridAxisEnd, GridAxisCenter};

class RenderGrid final : public RenderBlock {
public:
    RenderGrid(Element&, RenderStyle&&);
    virtual ~RenderGrid();

    Element& element() const { return downcast<Element>(nodeForNonAnonymous()); }

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) override;

    bool avoidsFloats() const override { return true; }
    bool canDropAnonymousBlockChild() const override { return false; }

    void dirtyGrid();
    Vector<LayoutUnit> trackSizesForComputedStyle(GridTrackSizingDirection) const;

    const Vector<LayoutUnit>& columnPositions() const { return m_columnPositions; }
    const Vector<LayoutUnit>& rowPositions() const { return m_rowPositions; }

    unsigned autoRepeatCountForDirection(GridTrackSizingDirection direction) const { return m_grid.autoRepeatTracks(direction); }

    // Required by GridTrackSizingAlgorithm. Keep them under control.
    bool isOrthogonalChild(const RenderBox&) const;
    LayoutUnit guttersSize(const Grid&, GridTrackSizingDirection, unsigned startLine, unsigned span) const;
private:
    const char* renderName() const override;
    bool isRenderGrid() const override { return true; }
    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;

    void addChild(RenderObject* newChild, RenderObject* beforeChild) final;
    void removeChild(RenderObject&) final;

    bool explicitGridDidResize(const RenderStyle&) const;
    bool namedGridLinesDefinitionDidChange(const RenderStyle&) const;

    std::optional<LayoutUnit> computeIntrinsicLogicalContentHeightUsing(Length logicalHeightLength, std::optional<LayoutUnit> intrinsicContentHeight, LayoutUnit borderAndPadding) const override;

    unsigned computeAutoRepeatTracksCount(GridTrackSizingDirection, SizingOperation) const;

    std::unique_ptr<OrderedTrackIndexSet> computeEmptyTracksForAutoRepeat(Grid&, GridTrackSizingDirection) const;

    void placeItemsOnGrid(Grid&, SizingOperation) const;
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
    void layoutPositionedObject(RenderBox&, bool relayoutChildren, bool fixedPositionObjectsOnly) override;
    void offsetAndBreadthForPositionedChild(const RenderBox&, GridTrackSizingDirection, LayoutUnit& offset, LayoutUnit& breadth);

    void computeTrackSizesForDefiniteSize(GridTrackSizingDirection, LayoutUnit availableSpace);
    void computeTrackSizesForIndefiniteSize(GridTrackSizingAlgorithm&, GridTrackSizingDirection, Grid&, LayoutUnit& minIntrinsicSize, LayoutUnit& maxIntrinsicSize) const;
    LayoutUnit computeTrackBasedLogicalHeight() const;

    void repeatTracksSizingIfNeeded(LayoutUnit availableSpaceForColumns, LayoutUnit availableSpaceForRows);

    void layoutGridItems();
    void populateGridPositionsForDirection(GridTrackSizingDirection);

    GridAxisPosition columnAxisPositionForChild(const RenderBox&) const;
    GridAxisPosition rowAxisPositionForChild(const RenderBox&) const;
    LayoutUnit columnAxisOffsetForChild(const RenderBox&) const;
    LayoutUnit rowAxisOffsetForChild(const RenderBox&) const;
    ContentAlignmentData computeContentPositionAndDistributionOffset(GridTrackSizingDirection, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks) const;
    LayoutPoint findChildLogicalPosition(const RenderBox&) const;
    GridArea cachedGridArea(const RenderBox&) const;
    GridSpan cachedGridSpan(const RenderBox&, GridTrackSizingDirection) const;

    LayoutUnit gridAreaBreadthForChildIncludingAlignmentOffsets(const RenderBox&, GridTrackSizingDirection) const;

    void applyStretchAlignmentToTracksIfNeeded(GridTrackSizingDirection);

    void paintChildren(PaintInfo& forSelf, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect) override;
    bool needToStretchChildLogicalHeight(const RenderBox&) const;
    LayoutUnit marginLogicalHeightForChild(const RenderBox&) const;
    LayoutUnit computeMarginLogicalSizeForChild(GridTrackSizingDirection, const RenderBox&) const;
    LayoutUnit availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const RenderBox&) const;
    StyleSelfAlignmentData justifySelfForChild(const RenderBox&) const;
    StyleSelfAlignmentData alignSelfForChild(const RenderBox&) const;
    void applyStretchAlignmentToChildIfNeeded(RenderBox&);
    bool hasAutoSizeInColumnAxis(const RenderBox& child) const { return isHorizontalWritingMode() ? child.style().height().isAuto() : child.style().width().isAuto(); }
    bool hasAutoSizeInRowAxis(const RenderBox& child) const { return isHorizontalWritingMode() ? child.style().width().isAuto() : child.style().height().isAuto(); }
    bool allowedToStretchChildAlongColumnAxis(const RenderBox& child) const { return alignSelfForChild(child).position() == ItemPositionStretch && hasAutoSizeInColumnAxis(child) && !hasAutoMarginsInColumnAxis(child); }
    bool allowedToStretchChildAlongRowAxis(const RenderBox& child) const { return justifySelfForChild(child).position() == ItemPositionStretch && hasAutoSizeInRowAxis(child) && !hasAutoMarginsInRowAxis(child); }
    bool hasAutoMarginsInColumnAxis(const RenderBox&) const;
    bool hasAutoMarginsInRowAxis(const RenderBox&) const;
    void resetAutoMarginsAndLogicalTopInColumnAxis(RenderBox& child);
    void updateAutoMarginsInColumnAxisIfNeeded(RenderBox&);
    void updateAutoMarginsInRowAxisIfNeeded(RenderBox&);

    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;
    std::optional<int> firstLineBaseline() const final;
    std::optional<int> inlineBlockBaseline(LineDirectionMode) const final;
    bool isInlineBaselineAlignedChild(const RenderBox&) const;

    LayoutUnit gridGapForDirection(GridTrackSizingDirection) const;

    unsigned numTracks(GridTrackSizingDirection, const Grid&) const;

    LayoutUnit translateRTLCoordinate(LayoutUnit) const;

    GridTrackSizingDirection flowAwareDirectionForChild(const RenderBox&, GridTrackSizingDirection) const;

    Grid m_grid;

    GridTrackSizingAlgorithm m_trackSizingAlgorithm;

    Vector<LayoutUnit> m_columnPositions;
    Vector<LayoutUnit> m_rowPositions;
    LayoutUnit m_offsetBetweenColumns;
    LayoutUnit m_offsetBetweenRows;

    std::optional<LayoutUnit> m_minContentHeight;
    std::optional<LayoutUnit> m_maxContentHeight;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderGrid, isRenderGrid())
