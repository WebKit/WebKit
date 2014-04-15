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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderMultiColumnSet.h"

#include "PaintInfo.h"
#include "RenderLayer.h"
#include "RenderMultiColumnFlowThread.h"
#include "RenderMultiColumnSpannerPlaceholder.h"

namespace WebCore {

RenderMultiColumnSet::RenderMultiColumnSet(RenderFlowThread& flowThread, PassRef<RenderStyle> style)
    : RenderRegionSet(flowThread.document(), std::move(style), flowThread)
    , m_computedColumnCount(1)
    , m_computedColumnWidth(0)
    , m_computedColumnHeight(0)
    , m_maxColumnHeight(RenderFlowThread::maxLogicalHeight())
    , m_minSpaceShortage(RenderFlowThread::maxLogicalHeight())
    , m_minimumColumnHeight(0)
{
}

RenderMultiColumnSet* RenderMultiColumnSet::nextSiblingMultiColumnSet() const
{
    for (RenderObject* sibling = nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (sibling->isRenderMultiColumnSet())
            return toRenderMultiColumnSet(sibling);
    }
    return nullptr;
}

RenderMultiColumnSet* RenderMultiColumnSet::previousSiblingMultiColumnSet() const
{
    for (RenderObject* sibling = previousSibling(); sibling; sibling = sibling->previousSibling()) {
        if (sibling->isRenderMultiColumnSet())
            return toRenderMultiColumnSet(sibling);
    }
    return nullptr;
}

RenderObject* RenderMultiColumnSet::firstRendererInFlowThread() const
{
    if (RenderBox* sibling = RenderMultiColumnFlowThread::previousColumnSetOrSpannerSiblingOf(this)) {
        // Adjacent sets should not occur. Currently we would have no way of figuring out what each
        // of them contains then.
        ASSERT(!sibling->isRenderMultiColumnSet());
        RenderMultiColumnSpannerPlaceholder* placeholder = multiColumnFlowThread()->findColumnSpannerPlaceholder(sibling);
        return placeholder->nextInPreOrderAfterChildren();
    }
    return flowThread()->firstChild();
}

RenderObject* RenderMultiColumnSet::lastRendererInFlowThread() const
{
    if (RenderBox* sibling = RenderMultiColumnFlowThread::nextColumnSetOrSpannerSiblingOf(this)) {
        // Adjacent sets should not occur. Currently we would have no way of figuring out what each
        // of them contains then.
        ASSERT(!sibling->isRenderMultiColumnSet());
        RenderMultiColumnSpannerPlaceholder* placeholder = multiColumnFlowThread()->findColumnSpannerPlaceholder(sibling);
        return placeholder->previousInPreOrder();
    }
    return flowThread()->lastLeafChild();
}

static bool precedesRenderer(RenderObject* renderer, RenderObject* boundary)
{
    for (; renderer; renderer = renderer->nextInPreOrder()) {
        if (renderer == boundary)
            return true;
    }
    return false;
}

bool RenderMultiColumnSet::containsRendererInFlowThread(RenderObject* renderer) const
{
    if (!previousSiblingMultiColumnSet() && !nextSiblingMultiColumnSet()) {
        // There is only one set. This is easy, then.
        return renderer->isDescendantOf(m_flowThread);
    }

    RenderObject* firstRenderer = firstRendererInFlowThread();
    RenderObject* lastRenderer = lastRendererInFlowThread();
    ASSERT(firstRenderer);
    ASSERT(lastRenderer);

    // This is SLOW! But luckily very uncommon.
    return precedesRenderer(firstRenderer, renderer) && precedesRenderer(renderer, lastRenderer);
}

void RenderMultiColumnSet::setLogicalTopInFlowThread(LayoutUnit logicalTop)
{
    LayoutRect rect = flowThreadPortionRect();
    if (isHorizontalWritingMode())
        rect.setY(logicalTop);
    else
        rect.setX(logicalTop);
    setFlowThreadPortionRect(rect);
}

void RenderMultiColumnSet::setLogicalBottomInFlowThread(LayoutUnit logicalBottom)
{
    LayoutRect rect = flowThreadPortionRect();
    if (isHorizontalWritingMode())
        rect.shiftMaxYEdgeTo(logicalBottom);
    else
        rect.shiftMaxXEdgeTo(logicalBottom);
    setFlowThreadPortionRect(rect);
}

LayoutUnit RenderMultiColumnSet::heightAdjustedForSetOffset(LayoutUnit height) const
{
    RenderBlockFlow* multicolBlock = toRenderBlockFlow(parent());
    LayoutUnit contentLogicalTop = logicalTop() - multicolBlock->borderAndPaddingBefore();

    height -= contentLogicalTop;
    return std::max(height, LayoutUnit::fromPixel(1)); // Let's avoid zero height, as that would probably cause an infinite amount of columns to be created.
}

LayoutUnit RenderMultiColumnSet::pageLogicalTopForOffset(LayoutUnit offset) const
{
    unsigned columnIndex = columnIndexAtOffset(offset, AssumeNewColumns);
    return logicalTopInFlowThread() + columnIndex * computedColumnHeight();
}

void RenderMultiColumnSet::setAndConstrainColumnHeight(LayoutUnit newHeight)
{
    m_computedColumnHeight = newHeight;
    if (m_computedColumnHeight > m_maxColumnHeight)
        m_computedColumnHeight = m_maxColumnHeight;
    // FIXME: the height may also be affected by the enclosing pagination context, if any.
}

unsigned RenderMultiColumnSet::findRunWithTallestColumns() const
{
    unsigned indexWithLargestHeight = 0;
    LayoutUnit largestHeight;
    LayoutUnit previousOffset;
    size_t runCount = m_contentRuns.size();
    ASSERT(runCount);
    for (size_t i = 0; i < runCount; i++) {
        const ContentRun& run = m_contentRuns[i];
        LayoutUnit height = run.columnLogicalHeight(previousOffset);
        if (largestHeight < height) {
            largestHeight = height;
            indexWithLargestHeight = i;
        }
        previousOffset = run.breakOffset();
    }
    return indexWithLargestHeight;
}

void RenderMultiColumnSet::distributeImplicitBreaks()
{
    unsigned breakCount = forcedBreaksCount();

#ifndef NDEBUG
    // There should be no implicit breaks assumed at this point.
    for (unsigned i = 0; i < breakCount; i++)
        ASSERT(!m_contentRuns[i].assumedImplicitBreaks());
#endif // NDEBUG

    if (!breakCount) {
        // The flow thread would normally insert a forced break at end of content, but if this set
        // isn't last in the multicol container, we have to do it ourselves.
        addForcedBreak(logicalBottomInFlowThread());
        breakCount = 1;
    }

    // If there is room for more breaks (to reach the used value of column-count), imagine that we
    // insert implicit breaks at suitable locations. At any given time, the content run with the
    // currently tallest columns will get another implicit break "inserted", which will increase its
    // column count by one and shrink its columns' height. Repeat until we have the desired total
    // number of breaks. The largest column height among the runs will then be the initial column
    // height for the balancer to use.
    while (breakCount < m_computedColumnCount) {
        unsigned index = findRunWithTallestColumns();
        m_contentRuns[index].assumeAnotherImplicitBreak();
        breakCount++;
    }
}

LayoutUnit RenderMultiColumnSet::calculateBalancedHeight(bool initial) const
{
    if (initial) {
        // Start with the lowest imaginable column height.
        unsigned index = findRunWithTallestColumns();
        LayoutUnit startOffset = index > 0 ? m_contentRuns[index - 1].breakOffset() : logicalTopInFlowThread();
        return std::max<LayoutUnit>(m_contentRuns[index].columnLogicalHeight(startOffset), m_minimumColumnHeight);
    }

    if (columnCount() <= computedColumnCount()) {
        // With the current column height, the content fits without creating overflowing columns. We're done.
        return m_computedColumnHeight;
    }

    if (forcedBreaksCount() > 1 && forcedBreaksCount() >= computedColumnCount()) {
        // Too many forced breaks to allow any implicit breaks. Initial balancing should already
        // have set a good height. There's nothing more we should do.
        return m_computedColumnHeight;
    }

    // If the initial guessed column height wasn't enough, stretch it now. Stretch by the lowest
    // amount of space shortage found during layout.

    ASSERT(m_minSpaceShortage > 0); // We should never _shrink_ the height!
    ASSERT(m_minSpaceShortage != RenderFlowThread::maxLogicalHeight()); // If this happens, we probably have a bug.
    if (m_minSpaceShortage == RenderFlowThread::maxLogicalHeight())
        return m_computedColumnHeight; // So bail out rather than looping infinitely.

    return m_computedColumnHeight + m_minSpaceShortage;
}

void RenderMultiColumnSet::clearForcedBreaks()
{
    m_contentRuns.clear();
}

void RenderMultiColumnSet::addForcedBreak(LayoutUnit offsetFromFirstPage)
{
    if (!requiresBalancing())
        return;
    if (!m_contentRuns.isEmpty() && offsetFromFirstPage <= m_contentRuns.last().breakOffset())
        return;
    // Append another item as long as we haven't exceeded used column count. What ends up in the
    // overflow area shouldn't affect column balancing.
    if (m_contentRuns.size() < m_computedColumnCount)
        m_contentRuns.append(ContentRun(offsetFromFirstPage));
}

bool RenderMultiColumnSet::recalculateColumnHeight(bool initial)
{
    LayoutUnit oldColumnHeight = m_computedColumnHeight;
    if (requiresBalancing()) {
        if (initial)
            distributeImplicitBreaks();
        LayoutUnit newColumnHeight = calculateBalancedHeight(initial);
        setAndConstrainColumnHeight(newColumnHeight);
        // After having calculated an initial column height, the multicol container typically needs at
        // least one more layout pass with a new column height, but if a height was specified, we only
        // need to do this if we think that we need less space than specified. Conversely, if we
        // determined that the columns need to be as tall as the specified height of the container, we
        // have already laid it out correctly, and there's no need for another pass.
    } else {
        // The position of the column set may have changed, in which case height available for
        // columns may have changed as well.
        setAndConstrainColumnHeight(m_computedColumnHeight);
    }
    if (m_computedColumnHeight == oldColumnHeight)
        return false; // No change. We're done.

    m_minSpaceShortage = RenderFlowThread::maxLogicalHeight();
    return true; // Need another pass.
}

void RenderMultiColumnSet::recordSpaceShortage(LayoutUnit spaceShortage)
{
    if (spaceShortage >= m_minSpaceShortage)
        return;

    // The space shortage is what we use as our stretch amount. We need a positive number here in
    // order to get anywhere. Some lines actually have zero height. Ignore them.
    if (spaceShortage > 0)
        m_minSpaceShortage = spaceShortage;

    m_minSpaceShortage = spaceShortage;
}

void RenderMultiColumnSet::updateLogicalWidth()
{
    setComputedColumnWidthAndCount(multiColumnFlowThread()->columnWidth(), multiColumnFlowThread()->columnCount()); // FIXME: This will eventually vary if we are contained inside regions.
    
    // FIXME: When we add regions support, we'll start it off at the width of the multi-column
    // block in that particular region.
    setLogicalWidth(parentBox()->contentLogicalWidth());
}

bool RenderMultiColumnSet::requiresBalancing() const
{
    if (!multiColumnFlowThread()->progressionIsInline())
        return false;

    if (RenderBox* next = RenderMultiColumnFlowThread::nextColumnSetOrSpannerSiblingOf(this)) {
        if (!next->isRenderMultiColumnSet()) {
            // If we're followed by a spanner, we need to balance.
            ASSERT(multiColumnFlowThread()->findColumnSpannerPlaceholder(next));
            return true;
        }
    }
    RenderBlockFlow* container = multiColumnBlockFlow();
    if (container->style().columnFill() == ColumnFillBalance)
        return true;
    return !multiColumnFlowThread()->columnHeightAvailable();
}

void RenderMultiColumnSet::prepareForLayout(bool initial)
{
    // Guess box logical top. This might eliminate the need for another layout pass.
    if (RenderBox* previous = RenderMultiColumnFlowThread::previousColumnSetOrSpannerSiblingOf(this))
        setLogicalTop(previous->logicalBottom() + previous->marginAfter());
    else
        setLogicalTop(multiColumnBlockFlow()->borderAndPaddingBefore());

    if (initial)
        m_maxColumnHeight = calculateMaxColumnHeight();
    if (requiresBalancing()) {
        if (initial)
            m_computedColumnHeight = 0;
    } else
        setAndConstrainColumnHeight(heightAdjustedForSetOffset(multiColumnFlowThread()->columnHeightAvailable()));

    // Set box width.
    updateLogicalWidth();

    // Any breaks will be re-inserted during layout, so get rid of what we already have.
    clearForcedBreaks();

    // Nuke previously stored minimum column height. Contents may have changed for all we know.
    m_minimumColumnHeight = 0;

    // Start with "infinite" flow thread portion height until height is known.
    setLogicalBottomInFlowThread(RenderFlowThread::maxLogicalHeight());

    setNeedsLayout();
}

void RenderMultiColumnSet::beginFlow(RenderBlock* container)
{
    RenderMultiColumnFlowThread* flowThread = multiColumnFlowThread();

    // At this point layout is exactly at the beginning of this set. Store block offset from flow
    // thread start.
    LayoutUnit logicalTopInFlowThread = flowThread->offsetFromLogicalTopOfFirstRegion(container) + container->logicalHeight();
    setLogicalTopInFlowThread(logicalTopInFlowThread);
}

void RenderMultiColumnSet::endFlow(RenderBlock* container, LayoutUnit bottomInContainer)
{
    RenderMultiColumnFlowThread* flowThread = multiColumnFlowThread();

    // At this point layout is exactly at the end of this set. Store block offset from flow thread
    // start. Also note that a new column height may have affected the height used in the flow
    // thread (because of struts), which may affect the number of columns. So we also need to update
    // the flow thread portion height in order to be able to calculate actual column-count.
    LayoutUnit logicalBottomInFlowThread = flowThread->offsetFromLogicalTopOfFirstRegion(container) + bottomInContainer;
    setLogicalBottomInFlowThread(logicalBottomInFlowThread);
    container->setLogicalHeight(bottomInContainer);
    unsigned colCount = columnCount();
    if (colCount > 1) {
        LayoutUnit paddedLogicalBottomInFlowThread = logicalTopInFlowThread() + computedColumnHeight() * colCount;
        if (logicalBottomInFlowThread != paddedLogicalBottomInFlowThread) {
            // Stretch the container to fully contain all columns, so that later content doesn't
            // start at the wrong position and end up bleeding into the columns of this set.
            container->setLogicalHeight(container->logicalHeight() + paddedLogicalBottomInFlowThread - logicalBottomInFlowThread);
            setLogicalBottomInFlowThread(paddedLogicalBottomInFlowThread);
        }
    }
}

void RenderMultiColumnSet::layout()
{
    RenderBlockFlow::layout();

    // At this point the logical top and bottom of the column set are known. Update maximum column
    // height (multicol height may be constrained).
    m_maxColumnHeight = calculateMaxColumnHeight();

    if (!nextSiblingMultiColumnSet()) {
        // This is the last set, i.e. the last region. Seize the opportunity to validate them.
        multiColumnFlowThread()->validateRegions();
    }
}

void RenderMultiColumnSet::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    computedValues.m_extent = m_computedColumnHeight;
    computedValues.m_position = logicalTop;
}

LayoutUnit RenderMultiColumnSet::calculateMaxColumnHeight() const
{
    RenderBlockFlow* multicolBlock = multiColumnBlockFlow();
    const RenderStyle& multicolStyle = multicolBlock->style();
    LayoutUnit availableHeight = multiColumnFlowThread()->columnHeightAvailable();
    LayoutUnit maxColumnHeight = availableHeight ? availableHeight : RenderFlowThread::maxLogicalHeight();
    if (!multicolStyle.logicalMaxHeight().isUndefined()) {
        LayoutUnit logicalMaxHeight = multicolBlock->computeContentLogicalHeight(multicolStyle.logicalMaxHeight());
        if (logicalMaxHeight != -1 && maxColumnHeight > logicalMaxHeight)
            maxColumnHeight = logicalMaxHeight;
    }
    return heightAdjustedForSetOffset(maxColumnHeight);
}

LayoutUnit RenderMultiColumnSet::columnGap() const
{
    // FIXME: Eventually we will cache the column gap when the widths of columns start varying, but for now we just
    // go to the parent block to get the gap.
    RenderBlockFlow* parentBlock = toRenderBlockFlow(parent());
    if (parentBlock->style().hasNormalColumnGap())
        return parentBlock->style().fontDescription().computedPixelSize(); // "1em" is recommended as the normal gap setting. Matches <p> margins.
    return parentBlock->style().columnGap();
}

unsigned RenderMultiColumnSet::columnCount() const
{
    // We must always return a value of 1 or greater. Column count = 0 is a meaningless situation,
    // and will confuse and cause problems in other parts of the code.
    if (!computedColumnHeight())
        return 1;

    // Our portion rect determines our column count. We have as many columns as needed to fit all the content.
    LayoutUnit logicalHeightInColumns = flowThread()->isHorizontalWritingMode() ? flowThreadPortionRect().height() : flowThreadPortionRect().width();
    if (!logicalHeightInColumns)
        return 1;
    
    unsigned count = ceil(static_cast<float>(logicalHeightInColumns) / computedColumnHeight());
    ASSERT(count >= 1);
    return count;
}

LayoutRect RenderMultiColumnSet::columnRectAt(unsigned index) const
{
    LayoutUnit colLogicalWidth = computedColumnWidth();
    LayoutUnit colLogicalHeight = computedColumnHeight();
    LayoutUnit colLogicalTop = borderAndPaddingBefore();
    LayoutUnit colLogicalLeft = borderAndPaddingLogicalLeft();
    LayoutUnit colGap = columnGap();
    
    bool progressionReversed = multiColumnFlowThread()->progressionIsReversed();
    bool progressionInline = multiColumnFlowThread()->progressionIsInline();
    
    if (progressionInline) {
        if (style().isLeftToRightDirection() ^ progressionReversed)
            colLogicalLeft += index * (colLogicalWidth + colGap);
        else
            colLogicalLeft += contentLogicalWidth() - colLogicalWidth - index * (colLogicalWidth + colGap);
    } else {
        if (!progressionReversed)
            colLogicalTop += index * (colLogicalHeight + colGap);
        else
            colLogicalTop += contentLogicalHeight() - colLogicalHeight - index * (colLogicalHeight + colGap);
    }
    
    if (isHorizontalWritingMode())
        return LayoutRect(colLogicalLeft, colLogicalTop, colLogicalWidth, colLogicalHeight);
    return LayoutRect(colLogicalTop, colLogicalLeft, colLogicalHeight, colLogicalWidth);
}

unsigned RenderMultiColumnSet::columnIndexAtOffset(LayoutUnit offset, ColumnIndexCalculationMode mode) const
{
    LayoutRect portionRect(flowThreadPortionRect());

    // Handle the offset being out of range.
    LayoutUnit flowThreadLogicalTop = isHorizontalWritingMode() ? portionRect.y() : portionRect.x();
    if (offset < flowThreadLogicalTop)
        return 0;
    // If we're laying out right now, we cannot constrain against some logical bottom, since it
    // isn't known yet. Otherwise, just return the last column if we're past the logical bottom.
    if (mode == ClampToExistingColumns) {
        LayoutUnit flowThreadLogicalBottom = isHorizontalWritingMode() ? portionRect.maxY() : portionRect.maxX();
        if (offset >= flowThreadLogicalBottom)
            return columnCount() - 1;
    }

    // Just divide by the column height to determine the correct column.
    return static_cast<float>(offset - flowThreadLogicalTop) / computedColumnHeight();
}

LayoutRect RenderMultiColumnSet::flowThreadPortionRectAt(unsigned index) const
{
    LayoutRect portionRect = flowThreadPortionRect();
    if (isHorizontalWritingMode())
        portionRect = LayoutRect(portionRect.x(), portionRect.y() + index * computedColumnHeight(), portionRect.width(), computedColumnHeight());
    else
        portionRect = LayoutRect(portionRect.x() + index * computedColumnHeight(), portionRect.y(), computedColumnHeight(), portionRect.height());
    return portionRect;
}

LayoutRect RenderMultiColumnSet::flowThreadPortionOverflowRect(const LayoutRect& portionRect, unsigned index, unsigned colCount, LayoutUnit colGap)
{
    // This function determines the portion of the flow thread that paints for the column. Along the inline axis, columns are
    // unclipped at outside edges (i.e., the first and last column in the set), and they clip to half the column
    // gap along interior edges.
    //
    // In the block direction, we will not clip overflow out of the top of the first column, or out of the bottom of
    // the last column. This applies only to the true first column and last column across all column sets.
    //
    // FIXME: Eventually we will know overflow on a per-column basis, but we can't do this until we have a painting
    // mode that understands not to paint contents from a previous column in the overflow area of a following column.
    // This problem applies to regions and pages as well and is not unique to columns.

    bool progressionReversed = multiColumnFlowThread()->progressionIsReversed();

    bool isFirstColumn = !index;
    bool isLastColumn = index == colCount - 1;
    bool isLeftmostColumn = style().isLeftToRightDirection() ^ progressionReversed ? isFirstColumn : isLastColumn;
    bool isRightmostColumn = style().isLeftToRightDirection() ^ progressionReversed ? isLastColumn : isFirstColumn;

    // Calculate the overflow rectangle, based on the flow thread's, clipped at column logical
    // top/bottom unless it's the first/last column.
    LayoutRect overflowRect = overflowRectForFlowThreadPortion(portionRect, isFirstColumn && isFirstRegion(), isLastColumn && isLastRegion(), VisualOverflow);

    // Avoid overflowing into neighboring columns, by clipping in the middle of adjacent column
    // gaps. Also make sure that we avoid rounding errors.
    if (isHorizontalWritingMode()) {
        if (!isLeftmostColumn)
            overflowRect.shiftXEdgeTo(portionRect.x() - colGap / 2);
        if (!isRightmostColumn)
            overflowRect.shiftMaxXEdgeTo(portionRect.maxX() + colGap - colGap / 2);
    } else {
        if (!isLeftmostColumn)
            overflowRect.shiftYEdgeTo(portionRect.y() - colGap / 2);
        if (!isRightmostColumn)
            overflowRect.shiftMaxYEdgeTo(portionRect.maxY() + colGap - colGap / 2);
    }
    return overflowRect;
}

void RenderMultiColumnSet::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (style().visibility() != VISIBLE)
        return;

    RenderBlock::paintObject(paintInfo, paintOffset);

    // FIXME: Right now we're only painting in the foreground phase.
    // Columns should technically respect phases and allow for background/float/foreground overlap etc., just like
    // RenderBlocks do. Note this is a pretty minor issue, since the old column implementation clipped columns
    // anyway, thus making it impossible for them to overlap one another. It's also really unlikely that the columns
    // would overlap another block.
    if (!m_flowThread || !isValid() || (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection))
        return;

    paintColumnRules(paintInfo, paintOffset);
}

void RenderMultiColumnSet::paintColumnRules(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.context->paintingDisabled())
        return;

    RenderMultiColumnFlowThread* flowThread = multiColumnFlowThread();
    const RenderStyle& blockStyle = parent()->style();
    const Color& ruleColor = blockStyle.visitedDependentColor(CSSPropertyWebkitColumnRuleColor);
    bool ruleTransparent = blockStyle.columnRuleIsTransparent();
    EBorderStyle ruleStyle = blockStyle.columnRuleStyle();
    LayoutUnit ruleThickness = blockStyle.columnRuleWidth();
    LayoutUnit colGap = columnGap();
    bool renderRule = ruleStyle > BHIDDEN && !ruleTransparent;
    if (!renderRule)
        return;

    unsigned colCount = columnCount();
    if (colCount <= 1)
        return;

    bool antialias = shouldAntialiasLines(paintInfo.context);

    if (flowThread->progressionIsInline()) {
        bool leftToRight = style().isLeftToRightDirection() ^ flowThread->progressionIsReversed();
        LayoutUnit currLogicalLeftOffset = leftToRight ? LayoutUnit() : contentLogicalWidth();
        LayoutUnit ruleAdd = logicalLeftOffsetForContent();
        LayoutUnit ruleLogicalLeft = leftToRight ? LayoutUnit() : contentLogicalWidth();
        LayoutUnit inlineDirectionSize = computedColumnWidth();
        BoxSide boxSide = isHorizontalWritingMode()
            ? leftToRight ? BSLeft : BSRight
            : leftToRight ? BSTop : BSBottom;

        for (unsigned i = 0; i < colCount; i++) {
            // Move to the next position.
            if (leftToRight) {
                ruleLogicalLeft += inlineDirectionSize + colGap / 2;
                currLogicalLeftOffset += inlineDirectionSize + colGap;
            } else {
                ruleLogicalLeft -= (inlineDirectionSize + colGap / 2);
                currLogicalLeftOffset -= (inlineDirectionSize + colGap);
            }

            // Now paint the column rule.
            if (i < colCount - 1) {
                LayoutUnit ruleLeft = isHorizontalWritingMode() ? paintOffset.x() + ruleLogicalLeft - ruleThickness / 2 + ruleAdd : paintOffset.x() + borderLeft() + paddingLeft();
                LayoutUnit ruleRight = isHorizontalWritingMode() ? ruleLeft + ruleThickness : ruleLeft + contentWidth();
                LayoutUnit ruleTop = isHorizontalWritingMode() ? paintOffset.y() + borderTop() + paddingTop() : paintOffset.y() + ruleLogicalLeft - ruleThickness / 2 + ruleAdd;
                LayoutUnit ruleBottom = isHorizontalWritingMode() ? ruleTop + contentHeight() : ruleTop + ruleThickness;
                IntRect pixelSnappedRuleRect = pixelSnappedIntRectFromEdges(ruleLeft, ruleTop, ruleRight, ruleBottom);
                drawLineForBoxSide(paintInfo.context, pixelSnappedRuleRect.x(), pixelSnappedRuleRect.y(), pixelSnappedRuleRect.maxX(), pixelSnappedRuleRect.maxY(), boxSide, ruleColor, ruleStyle, 0, 0, antialias);
            }
            
            ruleLogicalLeft = currLogicalLeftOffset;
        }
    } else {
        bool topToBottom = !style().isFlippedBlocksWritingMode() ^ flowThread->progressionIsReversed();
        LayoutUnit ruleLeft = isHorizontalWritingMode() ? LayoutUnit() : colGap / 2 - colGap - ruleThickness / 2;
        LayoutUnit ruleWidth = isHorizontalWritingMode() ? contentWidth() : ruleThickness;
        LayoutUnit ruleTop = isHorizontalWritingMode() ? colGap / 2 - colGap - ruleThickness / 2 : LayoutUnit();
        LayoutUnit ruleHeight = isHorizontalWritingMode() ? ruleThickness : contentHeight();
        LayoutRect ruleRect(ruleLeft, ruleTop, ruleWidth, ruleHeight);

        if (!topToBottom) {
            if (isHorizontalWritingMode())
                ruleRect.setY(height() - ruleRect.maxY());
            else
                ruleRect.setX(width() - ruleRect.maxX());
        }

        ruleRect.moveBy(paintOffset);

        BoxSide boxSide = isHorizontalWritingMode() ? topToBottom ? BSTop : BSBottom : topToBottom ? BSLeft : BSRight;

        LayoutSize step(0, topToBottom ? computedColumnHeight() + colGap : -(computedColumnHeight() + colGap));
        if (!isHorizontalWritingMode())
            step = step.transposedSize();

        for (unsigned i = 1; i < colCount; i++) {
            ruleRect.move(step);
            IntRect pixelSnappedRuleRect = pixelSnappedIntRect(ruleRect);
            drawLineForBoxSide(paintInfo.context, pixelSnappedRuleRect.x(), pixelSnappedRuleRect.y(), pixelSnappedRuleRect.maxX(), pixelSnappedRuleRect.maxY(), boxSide, ruleColor, ruleStyle, 0, 0, antialias);
        }
    }
}

void RenderMultiColumnSet::repaintFlowThreadContent(const LayoutRect& repaintRect)
{
    // Figure out the start and end columns and only check within that range so that we don't walk the
    // entire column set. Put the repaint rect into flow thread coordinates by flipping it first.
    LayoutRect flowThreadRepaintRect(repaintRect);
    flowThread()->flipForWritingMode(flowThreadRepaintRect);
    
    // Now we can compare this rect with the flow thread portions owned by each column. First let's
    // just see if the repaint rect intersects our flow thread portion at all.
    LayoutRect clippedRect(flowThreadRepaintRect);
    clippedRect.intersect(RenderRegion::flowThreadPortionOverflowRect());
    if (clippedRect.isEmpty())
        return;
    
    // Now we know we intersect at least one column. Let's figure out the logical top and logical
    // bottom of the area we're repainting.
    LayoutUnit repaintLogicalTop = isHorizontalWritingMode() ? flowThreadRepaintRect.y() : flowThreadRepaintRect.x();
    LayoutUnit repaintLogicalBottom = (isHorizontalWritingMode() ? flowThreadRepaintRect.maxY() : flowThreadRepaintRect.maxX()) - 1;
    
    unsigned startColumn = columnIndexAtOffset(repaintLogicalTop);
    unsigned endColumn = columnIndexAtOffset(repaintLogicalBottom);
    
    LayoutUnit colGap = columnGap();
    unsigned colCount = columnCount();
    for (unsigned i = startColumn; i <= endColumn; i++) {
        LayoutRect colRect = columnRectAt(i);
        
        // Get the portion of the flow thread that corresponds to this column.
        LayoutRect flowThreadPortion = flowThreadPortionRectAt(i);

        // Now get the overflow rect that corresponds to the column.
        LayoutRect flowThreadOverflowPortion = flowThreadPortionOverflowRect(flowThreadPortion, i, colCount, colGap);

        // Do a repaint for this specific column.
        repaintFlowThreadContentRectangle(repaintRect, flowThreadPortion, colRect.location(), &flowThreadOverflowPortion);
    }
}

LayoutUnit RenderMultiColumnSet::initialBlockOffsetForPainting() const
{
    bool progressionReversed = multiColumnFlowThread()->progressionIsReversed();
    bool progressionIsInline = multiColumnFlowThread()->progressionIsInline();
    
    LayoutUnit result = 0;
    if (!progressionIsInline && progressionReversed) {
        LayoutRect colRect = columnRectAt(0);
        result = isHorizontalWritingMode() ? colRect.y() : colRect.x();
        if (style().isFlippedBlocksWritingMode())
            result = -result;
    }
    return result;
}

void RenderMultiColumnSet::collectLayerFragments(LayerFragments& fragments, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect)
{
    // Let's start by introducing the different coordinate systems involved here. They are different
    // in how they deal with writing modes and columns. RenderLayer rectangles tend to be more
    // physical than the rectangles used in RenderObject & co.
    //
    // The two rectangles passed to this method are physical, except that we pretend that there's
    // only one long column (that's the flow thread). They are relative to the top left corner of
    // the flow thread. All rectangles being compared to the dirty rect also need to be in this
    // coordinate system.
    //
    // Then there's the output from this method - the stuff we put into the list of fragments. The
    // translationOffset point is the actual physical translation required to get from a location in
    // the flow thread to a location in some column. The paginationClip rectangle is in the same
    // coordinate system as the two rectangles passed to this method (i.e. physical, in flow thread
    // coordinates, pretending that there's only one long column).
    //
    // All other rectangles in this method are slightly less physical, when it comes to how they are
    // used with different writing modes, but they aren't really logical either. They are just like
    // RenderBox::frameRect(). More precisely, the sizes are physical, and the inline direction
    // coordinate is too, but the block direction coordinate is always "logical top". These
    // rectangles also pretend that there's only one long column, i.e. they are for the flow thread.
    //
    // To sum up: input and output from this method are "physical" RenderLayer-style rectangles and
    // points, while inside this method we mostly use the RenderObject-style rectangles (with the
    // block direction coordinate always being logical top).

    // Put the layer bounds into flow thread-local coordinates by flipping it first. Since we're in
    // a renderer, most rectangles are represented this way.
    LayoutRect layerBoundsInFlowThread(layerBoundingBox);
    flowThread()->flipForWritingMode(layerBoundsInFlowThread);

    // Now we can compare with the flow thread portions owned by each column. First let's
    // see if the rect intersects our flow thread portion at all.
    LayoutRect clippedRect(layerBoundsInFlowThread);
    clippedRect.intersect(RenderRegion::flowThreadPortionOverflowRect());
    if (clippedRect.isEmpty())
        return;
    
    // Now we know we intersect at least one column. Let's figure out the logical top and logical
    // bottom of the area we're checking.
    LayoutUnit layerLogicalTop = isHorizontalWritingMode() ? layerBoundsInFlowThread.y() : layerBoundsInFlowThread.x();
    LayoutUnit layerLogicalBottom = (isHorizontalWritingMode() ? layerBoundsInFlowThread.maxY() : layerBoundsInFlowThread.maxX()) - 1;
    
    // Figure out the start and end columns and only check within that range so that we don't walk the
    // entire column set.
    unsigned startColumn = columnIndexAtOffset(layerLogicalTop);
    unsigned endColumn = columnIndexAtOffset(layerLogicalBottom);
    
    LayoutUnit colLogicalWidth = computedColumnWidth();
    LayoutUnit colGap = columnGap();
    unsigned colCount = columnCount();

    bool progressionReversed = multiColumnFlowThread()->progressionIsReversed();
    bool progressionIsInline = multiColumnFlowThread()->progressionIsInline();

    LayoutUnit initialBlockOffset = initialBlockOffsetForPainting();
    
    for (unsigned i = startColumn; i <= endColumn; i++) {
        // Get the portion of the flow thread that corresponds to this column.
        LayoutRect flowThreadPortion = flowThreadPortionRectAt(i);
        
        // Now get the overflow rect that corresponds to the column.
        LayoutRect flowThreadOverflowPortion = flowThreadPortionOverflowRect(flowThreadPortion, i, colCount, colGap);

        // In order to create a fragment we must intersect the portion painted by this column.
        LayoutRect clippedRect(layerBoundsInFlowThread);
        clippedRect.intersect(flowThreadOverflowPortion);
        if (clippedRect.isEmpty())
            continue;
        
        // We also need to intersect the dirty rect. We have to apply a translation and shift based off
        // our column index.
        LayoutPoint translationOffset;
        LayoutUnit inlineOffset = progressionIsInline ? i * (colLogicalWidth + colGap) : LayoutUnit();
        
        bool leftToRight = style().isLeftToRightDirection() ^ progressionReversed;
        if (!leftToRight) {
            inlineOffset = -inlineOffset;
            if (progressionReversed)
                inlineOffset += contentLogicalWidth() - colLogicalWidth;
        }
        translationOffset.setX(inlineOffset);
        LayoutUnit blockOffset = initialBlockOffset + logicalTop() - flowThread()->logicalTop() + (isHorizontalWritingMode() ? -flowThreadPortion.y() : -flowThreadPortion.x());
        if (!progressionIsInline) {
            if (!progressionReversed)
                blockOffset = i * colGap;
            else
                blockOffset -= i * (computedColumnHeight() + colGap);
        }
        if (isFlippedBlocksWritingMode(style().writingMode()))
            blockOffset = -blockOffset;
        translationOffset.setY(blockOffset);
        if (!isHorizontalWritingMode())
            translationOffset = translationOffset.transposedPoint();
        // FIXME: The translation needs to include the multicolumn set's content offset within the
        // multicolumn block as well. This won't be an issue until we start creating multiple multicolumn sets.

        // Shift the dirty rect to be in flow thread coordinates with this translation applied.
        LayoutRect translatedDirtyRect(dirtyRect);
        translatedDirtyRect.moveBy(-translationOffset);
        
        // See if we intersect the dirty rect.
        clippedRect = layerBoundingBox;
        clippedRect.intersect(translatedDirtyRect);
        if (clippedRect.isEmpty())
            continue;
        
        // Something does need to paint in this column. Make a fragment now and supply the physical translation
        // offset and the clip rect for the column with that offset applied.
        LayerFragment fragment;
        fragment.paginationOffset = translationOffset;

        LayoutRect flippedFlowThreadOverflowPortion(flowThreadOverflowPortion);
        // Flip it into more a physical (RenderLayer-style) rectangle.
        flowThread()->flipForWritingMode(flippedFlowThreadOverflowPortion);
        fragment.paginationClip = flippedFlowThreadOverflowPortion;
        fragments.append(fragment);
    }
}

LayoutPoint RenderMultiColumnSet::columnTranslationForOffset(const LayoutUnit& offset) const
{
    unsigned startColumn = columnIndexAtOffset(offset);
    
    LayoutUnit colGap = columnGap();
    LayoutUnit colLogicalWidth = computedColumnWidth();
    
    LayoutRect flowThreadPortion = flowThreadPortionRectAt(startColumn);
    LayoutPoint translationOffset;

    bool progressionReversed = multiColumnFlowThread()->progressionIsReversed();
    bool progressionIsInline = multiColumnFlowThread()->progressionIsInline();

    LayoutUnit initialBlockOffset = initialBlockOffsetForPainting();
    
    LayoutUnit inlineOffset = progressionIsInline ? startColumn * (colLogicalWidth + colGap) : LayoutUnit();
    
    bool leftToRight = style().isLeftToRightDirection() ^ progressionReversed;
    if (!leftToRight) {
        inlineOffset = -inlineOffset;
        if (progressionReversed)
            inlineOffset += contentLogicalWidth() - colLogicalWidth;
    }
    translationOffset.setX(inlineOffset);
    LayoutUnit blockOffset = initialBlockOffset + (isHorizontalWritingMode() ? -flowThreadPortion.y() : -flowThreadPortion.x());
    if (!progressionIsInline) {
        if (!progressionReversed)
            blockOffset = startColumn * colGap;
        else
            blockOffset -= startColumn * (computedColumnHeight() + colGap);
    }
    if (isFlippedBlocksWritingMode(style().writingMode()))
        blockOffset = -blockOffset;
    translationOffset.setY(blockOffset);
    
    if (!isHorizontalWritingMode())
        translationOffset = translationOffset.transposedPoint();
    
    // FIXME: The translation needs to include the multicolumn set's content offset within the
    // multicolumn block as well. This won't be an issue until we start creating multiple multicolumn sets.
    return translationOffset;
}

void RenderMultiColumnSet::adjustRegionBoundsFromFlowThreadPortionRect(const LayoutPoint& layerOffset, LayoutRect& regionBounds)
{
    regionBounds.moveBy(roundedIntPoint(-columnTranslationForOffset(isHorizontalWritingMode() ? layerOffset.y() : layerOffset.x())));
}

void RenderMultiColumnSet::addOverflowFromChildren()
{
    // FIXME: Need to do much better here.
    unsigned colCount = columnCount();
    if (!colCount)
        return;
    
    LayoutRect lastRect = columnRectAt(colCount - 1);
    addLayoutOverflow(lastRect);
    if (!hasOverflowClip())
        addVisualOverflow(lastRect);
}

const char* RenderMultiColumnSet::renderName() const
{    
    return "RenderMultiColumnSet";
}

}
