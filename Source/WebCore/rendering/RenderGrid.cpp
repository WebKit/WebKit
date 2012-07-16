/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderGrid.h"

#include "LayoutRepainter.h"
#include "NotImplemented.h"
#include "RenderLayer.h"
#include "RenderView.h"

namespace WebCore {

class RenderGrid::GridTrack {
public:
    GridTrack()
        : m_usedBreadth(0)
    {
    }

    LayoutUnit m_usedBreadth;
};

RenderGrid::RenderGrid(Node* node)
    : RenderBlock(node)
{
    // All of our children must be block level.
    setChildrenInline(false);
}

RenderGrid::~RenderGrid()
{
}

void RenderGrid::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    // FIXME: Much of this method is boiler plate that matches RenderBox::layoutBlock and Render*FlexibleBox::layoutBlock.
    // It would be nice to refactor some of the duplicate code.
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    LayoutStateMaintainer statePusher(view(), this, locationOffset(), hasTransform() || hasReflection() || style()->isFlippedBlocksWritingMode());

    if (inRenderFlowThread()) {
        // Regions changing widths can force us to relayout our children.
        if (logicalWidthChangedInRegions())
            relayoutChildren = true;
    }
    computeInitialRegionRangeForBlock();

    LayoutSize previousSize = size();

    setLogicalHeight(0);
    computeLogicalWidth();

    m_overflow.clear();

    if (scrollsOverflow()) {
        if (style()->overflowX() == OSCROLL)
            layer()->setHasHorizontalScrollbar(true);
        if (style()->overflowY() == OSCROLL)
            layer()->setHasVerticalScrollbar(true);
    }

    layoutGridItems();

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    computeLogicalHeight();

    if (size() != previousSize)
        relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isRoot());

    computeRegionRangeForBlock();

    computeOverflow(oldClientAfterEdge);
    statePusher.pop();

    updateLayerTransform();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    if (hasOverflowClip())
        layer()->updateScrollInfoAfterLayout();

    repainter.repaintAfterLayout();

    setNeedsLayout(false);
}

void RenderGrid::computedUsedBreadthOfGridTracks(TrackSizingDirection direction, Vector<GridTrack>& tracks)
{
    const Vector<Length>& trackStyles = (direction == ForColumns) ? style()->gridColumns() : style()->gridRows();
    for (size_t i = 0; i < trackStyles.size(); ++i) {
        GridTrack track;
        if (trackStyles[i].isFixed())
            track.m_usedBreadth = trackStyles[i].getFloatValue();
        else
            notImplemented();

        tracks.append(track);
    }
}

void RenderGrid::layoutGridItems()
{
    Vector<GridTrack> columnTracks, rowTracks;
    computedUsedBreadthOfGridTracks(ForColumns, columnTracks);
    // FIXME: The logical width of Grid Columns from the prior step is used in
    // the formatting of Grid items in content-sized Grid Rows to determine
    // their required height. We will probably need to pass columns through.
    computedUsedBreadthOfGridTracks(ForRows, rowTracks);

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        LayoutPoint childPosition = findChildLogicalPosition(child, columnTracks, rowTracks);
        // FIXME: Grid items should stretch to fill their cells. Once we
        // implement grid-{column,row}-align, we can also shrink to fit. For
        // now, just size as if we were a regular child.
        child->layoutIfNeeded();

        // FIXME: Handle border & padding on the grid element.
        child->setLogicalLocation(childPosition);
    }

    // FIXME: Handle border & padding on the grid element.
    for (size_t i = 0; i < rowTracks.size(); ++i)
        setLogicalHeight(logicalHeight() + rowTracks[i].m_usedBreadth);
}

LayoutPoint RenderGrid::findChildLogicalPosition(RenderBox* child, const Vector<GridTrack>& columnTracks, const Vector<GridTrack>& rowTracks)
{
    Length column = child->style()->gridItemColumn();
    Length row = child->style()->gridItemRow();

    // FIXME: What does a non-positive integer mean for a column/row?
    if (!column.isPositive() || !row.isPositive())
        return LayoutPoint();

    // FIXME: Handle other values for grid-{row,column} like ranges or line names.
    if (!column.isFixed() || !row.isFixed())
        return LayoutPoint();

    size_t columnTrack = static_cast<size_t>(column.intValue()) - 1;
    size_t rowTrack = static_cast<size_t>(row.intValue()) - 1;

    LayoutPoint offset;
    for (size_t i = 0; i < columnTrack && i < columnTracks.size(); ++i)
        offset.setX(offset.x() + columnTracks[i].m_usedBreadth);
    for (size_t i = 0; i < rowTrack && i < rowTracks.size(); ++i)
        offset.setY(offset.y() + rowTracks[i].m_usedBreadth);

    // FIXME: Handle margins on the grid item.
    return offset;
}

const char* RenderGrid::renderName() const
{
    if (isFloating())
        return "RenderGrid (floating)";
    if (isOutOfFlowPositioned())
        return "RenderGrid (positioned)";
    if (isAnonymous())
        return "RenderGrid (generated)";
    if (isRelPositioned())
        return "RenderGrid (relative positioned)";
    return "RenderGrid";
}

} // namespace WebCore
