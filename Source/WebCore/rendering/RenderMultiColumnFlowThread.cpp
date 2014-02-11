/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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
 * PROFITS; OR BUSINESS IN..0TERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderMultiColumnFlowThread.h"

#include "LayoutState.h"
#include "RenderMultiColumnSet.h"

namespace WebCore {

RenderMultiColumnFlowThread::RenderMultiColumnFlowThread(Document& document, PassRef<RenderStyle> style)
    : RenderFlowThread(document, std::move(style))
    , m_columnCount(1)
    , m_columnWidth(0)
    , m_columnHeightAvailable(0)
    , m_inBalancingPass(false)
    , m_needsRebalancing(false)
    , m_progressionIsInline(false)
    , m_progressionIsReversed(false)
{
    setFlowThreadState(InsideInFlowThread);
}

RenderMultiColumnFlowThread::~RenderMultiColumnFlowThread()
{
}

const char* RenderMultiColumnFlowThread::renderName() const
{    
    return "RenderMultiColumnFlowThread";
}

void RenderMultiColumnFlowThread::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    // We simply remain at our intrinsic height.
    computedValues.m_extent = logicalHeight;
    computedValues.m_position = logicalTop;
}

LayoutUnit RenderMultiColumnFlowThread::initialLogicalWidth() const
{
    return columnWidth();
}

void RenderMultiColumnFlowThread::autoGenerateRegionsToBlockOffset(LayoutUnit /*offset*/)
{
    // This function ensures we have the correct column set information at all times.
    // For a simple multi-column layout in continuous media, only one column set child is required.
    // Once a column is nested inside an enclosing pagination context, the number of column sets
    // required becomes 2n-1, where n is the total number of nested pagination contexts. For example:
    //
    // Column layout with no enclosing pagination model = 2 * 1 - 1 = 1 column set.
    // Columns inside pages = 2 * 2 - 1 = 3 column sets (bottom of first page, all the subsequent pages, then the last page).
    // Columns inside columns inside pages = 2 * 3 - 1 = 5 column sets.
    //
    // In addition, column spans will force a column set to "split" into before/after sets around the spanning element.
    //
    // Finally, we will need to deal with columns inside regions. If regions have variable widths, then there will need
    // to be unique column sets created inside any region whose width is different from its surrounding regions. This is
    // actually pretty similar to the spanning case, in that we break up the column sets whenever the width varies.
    //
    // FIXME: For now just make one column set. This matches the old multi-column code.
    // Right now our goal is just feature parity with the old multi-column code so that we can switch over to the
    // new code as soon as possible.
    RenderMultiColumnSet* firstSet = toRenderMultiColumnSet(firstRegion());
    if (firstSet)
        return;
    
    invalidateRegions();

    RenderBlockFlow* parentBlock = toRenderBlockFlow(parent());
    firstSet = new RenderMultiColumnSet(*this, RenderStyle::createAnonymousStyleWithDisplay(&parentBlock->style(), BLOCK));
    firstSet->initializeStyle();
    parentBlock->RenderBlock::addChild(firstSet);

    // Even though we aren't placed yet, we can go ahead and set up our size. At this point we're
    // typically in the middle of laying out the thread, attempting to paginate, and we need to do
    // some rudimentary "layout" of the set now, so that pagination will work.
    firstSet->prepareForLayout();

    validateRegions();
}

void RenderMultiColumnFlowThread::setPageBreak(const RenderBlock* block, LayoutUnit offset, LayoutUnit spaceShortage)
{
    if (RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(regionAtBlockOffset(block, offset)))
        multicolSet->recordSpaceShortage(spaceShortage);
}

void RenderMultiColumnFlowThread::updateMinimumPageHeight(const RenderBlock* block, LayoutUnit offset, LayoutUnit minHeight)
{
    if (RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(regionAtBlockOffset(block, offset)))
        multicolSet->updateMinimumColumnHeight(minHeight);
}

bool RenderMultiColumnFlowThread::addForcedRegionBreak(const RenderBlock* block, LayoutUnit offset, RenderBox* /*breakChild*/, bool /*isBefore*/, LayoutUnit* offsetBreakAdjustment)
{
    if (RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(regionAtBlockOffset(block, offset))) {
        multicolSet->addForcedBreak(offset);
        if (offsetBreakAdjustment)
            *offsetBreakAdjustment = pageLogicalHeightForOffset(offset) ? pageRemainingLogicalHeightForOffset(offset, IncludePageBoundary) : LayoutUnit::fromPixel(0);
        return true;
    }
    return false;
}

void RenderMultiColumnFlowThread::computeLineGridPaginationOrigin(LayoutState& layoutState) const
{
    if (!progressionIsInline())
        return;
    
    // We need to cache a line grid pagination origin so that we understand how to reset the line grid
    // at the top of each column.
    // Get the current line grid and offset.
    const auto lineGrid = layoutState.lineGrid();
    if (!lineGrid)
        return;

    // Get the hypothetical line box used to establish the grid.
    auto lineGridBox = lineGrid->lineGridBox();
    if (!lineGridBox)
        return;
    
    bool isHorizontalWritingMode = lineGrid->isHorizontalWritingMode();

    LayoutUnit lineGridBlockOffset = isHorizontalWritingMode ? layoutState.lineGridOffset().height() : layoutState.lineGridOffset().width();

    // Now determine our position on the grid. Our baseline needs to be adjusted to the nearest baseline multiple
    // as established by the line box.
    // FIXME: Need to handle crazy line-box-contain values that cause the root line box to not be considered. I assume
    // the grid should honor line-box-contain.
    LayoutUnit gridLineHeight = lineGridBox->lineBottomWithLeading() - lineGridBox->lineTopWithLeading();
    if (!gridLineHeight)
        return;

    LayoutUnit firstLineTopWithLeading = lineGridBlockOffset + lineGridBox->lineTopWithLeading();
    
    if (layoutState.isPaginated() && layoutState.pageLogicalHeight()) {
        LayoutUnit pageLogicalTop = isHorizontalWritingMode ? layoutState.pageOffset().height() : layoutState.pageOffset().width();
        if (pageLogicalTop > firstLineTopWithLeading) {
            // Shift to the next highest line grid multiple past the page logical top. Cache the delta
            // between this new value and the page logical top as the pagination origin.
            LayoutUnit remainder = roundToInt(pageLogicalTop - firstLineTopWithLeading) % roundToInt(gridLineHeight);
            LayoutUnit paginationDelta = gridLineHeight - remainder;
            if (isHorizontalWritingMode)
                layoutState.setLineGridPaginationOrigin(LayoutSize(layoutState.lineGridPaginationOrigin().width(), paginationDelta));
            else
                layoutState.setLineGridPaginationOrigin(LayoutSize(paginationDelta, layoutState.lineGridPaginationOrigin().height()));
        }
    }
}

}
