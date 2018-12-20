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

#include "FrameView.h"
#include "HitTestResult.h"
#include "PaintInfo.h"
#include "RenderBoxFragmentInfo.h"
#include "RenderLayer.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderView.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMultiColumnSet);

RenderMultiColumnSet::RenderMultiColumnSet(RenderFragmentedFlow& fragmentedFlow, RenderStyle&& style)
    : RenderFragmentContainerSet(fragmentedFlow.document(), WTFMove(style), fragmentedFlow)
    , m_computedColumnCount(1)
    , m_computedColumnWidth(0)
    , m_computedColumnHeight(0)
    , m_availableColumnHeight(0)
    , m_columnHeightComputed(false)
    , m_maxColumnHeight(RenderFragmentedFlow::maxLogicalHeight())
    , m_minSpaceShortage(RenderFragmentedFlow::maxLogicalHeight())
    , m_minimumColumnHeight(0)
{
}

RenderMultiColumnSet* RenderMultiColumnSet::nextSiblingMultiColumnSet() const
{
    for (RenderObject* sibling = nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (is<RenderMultiColumnSet>(*sibling))
            return downcast<RenderMultiColumnSet>(sibling);
    }
    return nullptr;
}

RenderMultiColumnSet* RenderMultiColumnSet::previousSiblingMultiColumnSet() const
{
    for (RenderObject* sibling = previousSibling(); sibling; sibling = sibling->previousSibling()) {
        if (is<RenderMultiColumnSet>(*sibling))
            return downcast<RenderMultiColumnSet>(sibling);
    }
    return nullptr;
}

RenderObject* RenderMultiColumnSet::firstRendererInFragmentedFlow() const
{
    if (RenderBox* sibling = RenderMultiColumnFlow::previousColumnSetOrSpannerSiblingOf(this)) {
        // Adjacent sets should not occur. Currently we would have no way of figuring out what each
        // of them contains then.
        ASSERT(!sibling->isRenderMultiColumnSet());
        if (RenderMultiColumnSpannerPlaceholder* placeholder = multiColumnFlow()->findColumnSpannerPlaceholder(sibling))
            return placeholder->nextInPreOrderAfterChildren();
        ASSERT_NOT_REACHED();
    }
    return fragmentedFlow()->firstChild();
}

RenderObject* RenderMultiColumnSet::lastRendererInFragmentedFlow() const
{
    if (RenderBox* sibling = RenderMultiColumnFlow::nextColumnSetOrSpannerSiblingOf(this)) {
        // Adjacent sets should not occur. Currently we would have no way of figuring out what each
        // of them contains then.
        ASSERT(!sibling->isRenderMultiColumnSet());
        if (RenderMultiColumnSpannerPlaceholder* placeholder = multiColumnFlow()->findColumnSpannerPlaceholder(sibling))
            return placeholder->previousInPreOrder();
        ASSERT_NOT_REACHED();
    }
    return fragmentedFlow()->lastLeafChild();
}

static bool precedesRenderer(const RenderObject* renderer, const RenderObject* boundary)
{
    for (; renderer; renderer = renderer->nextInPreOrder()) {
        if (renderer == boundary)
            return true;
    }
    return false;
}

bool RenderMultiColumnSet::containsRendererInFragmentedFlow(const RenderObject& renderer) const
{
    if (!previousSiblingMultiColumnSet() && !nextSiblingMultiColumnSet()) {
        // There is only one set. This is easy, then.
        return renderer.isDescendantOf(m_fragmentedFlow);
    }

    RenderObject* firstRenderer = firstRendererInFragmentedFlow();
    RenderObject* lastRenderer = lastRendererInFragmentedFlow();
    ASSERT(firstRenderer);
    ASSERT(lastRenderer);

    // This is SLOW! But luckily very uncommon.
    return precedesRenderer(firstRenderer, &renderer) && precedesRenderer(&renderer, lastRenderer);
}

void RenderMultiColumnSet::setLogicalTopInFragmentedFlow(LayoutUnit logicalTop)
{
    LayoutRect rect = fragmentedFlowPortionRect();
    if (isHorizontalWritingMode())
        rect.setY(logicalTop);
    else
        rect.setX(logicalTop);
    setFragmentedFlowPortionRect(rect);
}

void RenderMultiColumnSet::setLogicalBottomInFragmentedFlow(LayoutUnit logicalBottom)
{
    LayoutRect rect = fragmentedFlowPortionRect();
    if (isHorizontalWritingMode())
        rect.shiftMaxYEdgeTo(logicalBottom);
    else
        rect.shiftMaxXEdgeTo(logicalBottom);
    setFragmentedFlowPortionRect(rect);
}

LayoutUnit RenderMultiColumnSet::heightAdjustedForSetOffset(LayoutUnit height) const
{
    RenderBlockFlow& multicolBlock = downcast<RenderBlockFlow>(*parent());
    LayoutUnit contentLogicalTop = logicalTop() - multicolBlock.borderAndPaddingBefore();

    height -= contentLogicalTop;
    return std::max(height, 1_lu); // Let's avoid zero height, as that would probably cause an infinite amount of columns to be created.
}

LayoutUnit RenderMultiColumnSet::pageLogicalTopForOffset(LayoutUnit offset) const
{
    unsigned columnIndex = columnIndexAtOffset(offset, AssumeNewColumns);
    return logicalTopInFragmentedFlow() + columnIndex * computedColumnHeight();
}

void RenderMultiColumnSet::setAndConstrainColumnHeight(LayoutUnit newHeight)
{
    m_computedColumnHeight = newHeight;
    if (m_computedColumnHeight > m_maxColumnHeight)
        m_computedColumnHeight = m_maxColumnHeight;
    
    // FIXME: The available column height is not the same as the constrained height specified
    // by the pagination API. The column set in this case is allowed to be bigger than the
    // height of a single column. We cache available column height in order to use it
    // in computeLogicalHeight later. This is pretty gross, and maybe there's a better way
    // to formalize the idea of clamped column heights without having a view dependency
    // here.
    m_availableColumnHeight = m_computedColumnHeight;
    if (multiColumnFlow() && !multiColumnFlow()->progressionIsInline() && parent()->isRenderView()) {
        int pageLength = view().frameView().pagination().pageLength;
        if (pageLength)
            m_computedColumnHeight = pageLength;
    }
    
    m_columnHeightComputed = true;

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
#ifndef NDEBUG
    // There should be no implicit breaks assumed at this point.
    for (unsigned i = 0; i < forcedBreaksCount(); i++)
        ASSERT(!m_contentRuns[i].assumedImplicitBreaks());
#endif // NDEBUG

    // Insert a final content run to encompass all content. This will include overflow if this is
    // the last set.
    addForcedBreak(logicalBottomInFragmentedFlow());
    unsigned breakCount = forcedBreaksCount();

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
        LayoutUnit startOffset = index > 0 ? m_contentRuns[index - 1].breakOffset() : logicalTopInFragmentedFlow();
        return std::max<LayoutUnit>(m_contentRuns[index].columnLogicalHeight(startOffset), m_minimumColumnHeight);
    }

    if (columnCount() <= computedColumnCount()) {
        // With the current column height, the content fits without creating overflowing columns. We're done.
        return m_computedColumnHeight;
    }

    if (forcedBreaksCount() >= computedColumnCount()) {
        // Too many forced breaks to allow any implicit breaks. Initial balancing should already
        // have set a good height. There's nothing more we should do.
        return m_computedColumnHeight;
    }

    // If the initial guessed column height wasn't enough, stretch it now. Stretch by the lowest
    // amount of space shortage found during layout.

    ASSERT(m_minSpaceShortage > 0); // We should never _shrink_ the height!
    // ASSERT(m_minSpaceShortage != RenderFragmentedFlow::maxLogicalHeight()); // If this happens, we probably have a bug.
    if (m_minSpaceShortage == RenderFragmentedFlow::maxLogicalHeight())
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

    m_minSpaceShortage = RenderFragmentedFlow::maxLogicalHeight();
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
}

void RenderMultiColumnSet::updateLogicalWidth()
{
    setComputedColumnWidthAndCount(multiColumnFlow()->columnWidth(), multiColumnFlow()->columnCount()); // FIXME: This will eventually vary if we are contained inside fragments.
    
    // FIXME: When we add fragments support, we'll start it off at the width of the multi-column
    // block in that particular fragment.
    setLogicalWidth(multiColumnBlockFlow()->contentLogicalWidth());
}

bool RenderMultiColumnSet::requiresBalancing() const
{
    if (!multiColumnFlow()->progressionIsInline())
        return false;

    if (RenderBox* next = RenderMultiColumnFlow::nextColumnSetOrSpannerSiblingOf(this)) {
        if (!next->isRenderMultiColumnSet() && !next->isLegend()) {
            // If we're followed by a spanner, we need to balance.
            ASSERT(multiColumnFlow()->findColumnSpannerPlaceholder(next));
            return true;
        }
    }
    RenderBlockFlow* container = multiColumnBlockFlow();
    if (container->style().columnFill() == ColumnFill::Balance)
        return true;
    return !multiColumnFlow()->columnHeightAvailable();
}

void RenderMultiColumnSet::prepareForLayout(bool initial)
{
    // Guess box logical top. This might eliminate the need for another layout pass.
    if (RenderBox* previous = RenderMultiColumnFlow::previousColumnSetOrSpannerSiblingOf(this))
        setLogicalTop(previous->logicalBottom() + previous->marginAfter());
    else
        setLogicalTop(multiColumnBlockFlow()->borderAndPaddingBefore());

    if (initial)
        m_maxColumnHeight = calculateMaxColumnHeight();
    if (requiresBalancing()) {
        if (initial) {
            m_computedColumnHeight = 0;
            m_availableColumnHeight = 0;
            m_columnHeightComputed = false;
        }
    } else
        setAndConstrainColumnHeight(heightAdjustedForSetOffset(multiColumnFlow()->columnHeightAvailable()));

    // Set box width.
    updateLogicalWidth();

    // Any breaks will be re-inserted during layout, so get rid of what we already have.
    clearForcedBreaks();

    // Nuke previously stored minimum column height. Contents may have changed for all we know.
    m_minimumColumnHeight = 0;

    // Start with "infinite" flow thread portion height until height is known.
    setLogicalBottomInFragmentedFlow(RenderFragmentedFlow::maxLogicalHeight());

    setNeedsLayout(MarkOnlyThis);
}

void RenderMultiColumnSet::beginFlow(RenderBlock* container)
{
    RenderMultiColumnFlow* fragmentedFlow = multiColumnFlow();

    // At this point layout is exactly at the beginning of this set. Store block offset from flow
    // thread start.
    LayoutUnit logicalTopInFragmentedFlow = fragmentedFlow->offsetFromLogicalTopOfFirstFragment(container) + container->logicalHeight();
    setLogicalTopInFragmentedFlow(logicalTopInFragmentedFlow);
}

void RenderMultiColumnSet::endFlow(RenderBlock* container, LayoutUnit bottomInContainer)
{
    RenderMultiColumnFlow* fragmentedFlow = multiColumnFlow();

    // At this point layout is exactly at the end of this set. Store block offset from flow thread
    // start. Also note that a new column height may have affected the height used in the flow
    // thread (because of struts), which may affect the number of columns. So we also need to update
    // the flow thread portion height in order to be able to calculate actual column-count.
    LayoutUnit logicalBottomInFragmentedFlow = fragmentedFlow->offsetFromLogicalTopOfFirstFragment(container) + bottomInContainer;
    setLogicalBottomInFragmentedFlow(logicalBottomInFragmentedFlow);
    container->setLogicalHeight(bottomInContainer);
}

void RenderMultiColumnSet::layout()
{
    RenderBlockFlow::layout();

    // At this point the logical top and bottom of the column set are known. Update maximum column
    // height (multicol height may be constrained).
    m_maxColumnHeight = calculateMaxColumnHeight();

    if (!nextSiblingMultiColumnSet()) {
        // This is the last set, i.e. the last fragment. Seize the opportunity to validate them.
        multiColumnFlow()->validateFragments();
    }
}

RenderBox::LogicalExtentComputedValues RenderMultiColumnSet::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop) const
{
    return { m_availableColumnHeight, logicalTop, ComputedMarginValues() };
}

LayoutUnit RenderMultiColumnSet::calculateMaxColumnHeight() const
{
    RenderBlockFlow* multicolBlock = multiColumnBlockFlow();
    const RenderStyle& multicolStyle = multicolBlock->style();
    LayoutUnit availableHeight = multiColumnFlow()->columnHeightAvailable();
    LayoutUnit maxColumnHeight = availableHeight ? availableHeight : RenderFragmentedFlow::maxLogicalHeight();
    if (!multicolStyle.logicalMaxHeight().isUndefined())
        maxColumnHeight = std::min(maxColumnHeight, multicolBlock->computeContentLogicalHeight(MaxSize, multicolStyle.logicalMaxHeight(), WTF::nullopt).value_or(maxColumnHeight));
    return heightAdjustedForSetOffset(maxColumnHeight);
}

LayoutUnit RenderMultiColumnSet::columnGap() const
{
    // FIXME: Eventually we will cache the column gap when the widths of columns start varying, but for now we just
    // go to the parent block to get the gap.
    RenderBlockFlow& parentBlock = downcast<RenderBlockFlow>(*parent());
    if (parentBlock.style().columnGap().isNormal())
        return parentBlock.style().fontDescription().computedPixelSize(); // "1em" is recommended as the normal gap setting. Matches <p> margins.
    return valueForLength(parentBlock.style().columnGap().length(), parentBlock.availableLogicalWidth());
}

unsigned RenderMultiColumnSet::columnCount() const
{
    // We must always return a value of 1 or greater. Column count = 0 is a meaningless situation,
    // and will confuse and cause problems in other parts of the code.
    if (!computedColumnHeight())
        return 1;

    // Our portion rect determines our column count. We have as many columns as needed to fit all the content.
    LayoutUnit logicalHeightInColumns = fragmentedFlow()->isHorizontalWritingMode() ? fragmentedFlowPortionRect().height() : fragmentedFlowPortionRect().width();
    if (!logicalHeightInColumns)
        return 1;
    
    unsigned count = ceil(static_cast<float>(logicalHeightInColumns) / computedColumnHeight());
    ASSERT(count >= 1);
    return count;
}

LayoutUnit RenderMultiColumnSet::columnLogicalLeft(unsigned index) const
{
    LayoutUnit colLogicalWidth = computedColumnWidth();
    LayoutUnit colLogicalLeft = borderAndPaddingLogicalLeft();
    LayoutUnit colGap = columnGap();

    bool progressionReversed = multiColumnFlow()->progressionIsReversed();
    bool progressionInline = multiColumnFlow()->progressionIsInline();

    if (progressionInline) {
        if (style().isLeftToRightDirection() ^ progressionReversed)
            colLogicalLeft += index * (colLogicalWidth + colGap);
        else
            colLogicalLeft += contentLogicalWidth() - colLogicalWidth - index * (colLogicalWidth + colGap);
    }

    return colLogicalLeft;
}

LayoutUnit RenderMultiColumnSet::columnLogicalTop(unsigned index) const
{
    LayoutUnit colLogicalHeight = computedColumnHeight();
    LayoutUnit colLogicalTop = borderAndPaddingBefore();
    LayoutUnit colGap = columnGap();

    bool progressionReversed = multiColumnFlow()->progressionIsReversed();
    bool progressionInline = multiColumnFlow()->progressionIsInline();

    if (!progressionInline) {
        if (!progressionReversed)
            colLogicalTop += index * (colLogicalHeight + colGap);
        else
            colLogicalTop += contentLogicalHeight() - colLogicalHeight - index * (colLogicalHeight + colGap);
    }

    return colLogicalTop;
}

LayoutRect RenderMultiColumnSet::columnRectAt(unsigned index) const
{
    LayoutUnit colLogicalWidth = computedColumnWidth();
    LayoutUnit colLogicalHeight = computedColumnHeight();

    if (isHorizontalWritingMode())
        return LayoutRect(columnLogicalLeft(index), columnLogicalTop(index), colLogicalWidth, colLogicalHeight);
    return LayoutRect(columnLogicalTop(index), columnLogicalLeft(index), colLogicalHeight, colLogicalWidth);
}

unsigned RenderMultiColumnSet::columnIndexAtOffset(LayoutUnit offset, ColumnIndexCalculationMode mode) const
{
    LayoutRect portionRect(fragmentedFlowPortionRect());

    // Handle the offset being out of range.
    LayoutUnit fragmentedFlowLogicalTop = isHorizontalWritingMode() ? portionRect.y() : portionRect.x();
    if (offset < fragmentedFlowLogicalTop)
        return 0;
    // If we're laying out right now, we cannot constrain against some logical bottom, since it
    // isn't known yet. Otherwise, just return the last column if we're past the logical bottom.
    if (mode == ClampToExistingColumns) {
        LayoutUnit fragmentedFlowLogicalBottom = isHorizontalWritingMode() ? portionRect.maxY() : portionRect.maxX();
        if (offset >= fragmentedFlowLogicalBottom)
            return columnCount() - 1;
    }

    // Sometimes computedColumnHeight() is 0 here: see https://bugs.webkit.org/show_bug.cgi?id=132884
    if (!computedColumnHeight())
        return 0;

    // Just divide by the column height to determine the correct column.
    return static_cast<float>(offset - fragmentedFlowLogicalTop) / computedColumnHeight();
}

LayoutRect RenderMultiColumnSet::fragmentedFlowPortionRectAt(unsigned index) const
{
    LayoutRect portionRect = fragmentedFlowPortionRect();
    if (isHorizontalWritingMode())
        portionRect = LayoutRect(portionRect.x(), portionRect.y() + index * computedColumnHeight(), portionRect.width(), computedColumnHeight());
    else
        portionRect = LayoutRect(portionRect.x() + index * computedColumnHeight(), portionRect.y(), computedColumnHeight(), portionRect.height());
    return portionRect;
}

LayoutRect RenderMultiColumnSet::fragmentedFlowPortionOverflowRect(const LayoutRect& portionRect, unsigned index, unsigned colCount, LayoutUnit colGap)
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
    // This problem applies to fragments and pages as well and is not unique to columns.

    bool progressionReversed = multiColumnFlow()->progressionIsReversed();

    bool isFirstColumn = !index;
    bool isLastColumn = index == colCount - 1;
    bool isLeftmostColumn = style().isLeftToRightDirection() ^ progressionReversed ? isFirstColumn : isLastColumn;
    bool isRightmostColumn = style().isLeftToRightDirection() ^ progressionReversed ? isLastColumn : isFirstColumn;

    // Calculate the overflow rectangle, based on the flow thread's, clipped at column logical
    // top/bottom unless it's the first/last column.
    LayoutRect overflowRect = overflowRectForFragmentedFlowPortion(portionRect, isFirstColumn && isFirstFragment(), isLastColumn && isLastFragment(), VisualOverflow);

    // For RenderViews only (i.e., iBooks), avoid overflowing into neighboring columns, by clipping in the middle of adjacent column gaps. Also make sure that we avoid rounding errors.
    if (&view() == parent()) {
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
    }
    return overflowRect;
}

void RenderMultiColumnSet::paintColumnRules(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.context().paintingDisabled())
        return;

    RenderMultiColumnFlow* fragmentedFlow = multiColumnFlow();
    const RenderStyle& blockStyle = parent()->style();
    const Color& ruleColor = blockStyle.visitedDependentColorWithColorFilter(CSSPropertyColumnRuleColor);
    bool ruleTransparent = blockStyle.columnRuleIsTransparent();
    BorderStyle ruleStyle = collapsedBorderStyle(blockStyle.columnRuleStyle());
    LayoutUnit ruleThickness = blockStyle.columnRuleWidth();
    LayoutUnit colGap = columnGap();
    bool renderRule = ruleStyle > BorderStyle::Hidden && !ruleTransparent;
    if (!renderRule)
        return;

    unsigned colCount = columnCount();
    if (colCount <= 1)
        return;

    bool antialias = shouldAntialiasLines(paintInfo.context());

    if (fragmentedFlow->progressionIsInline()) {
        bool leftToRight = style().isLeftToRightDirection() ^ fragmentedFlow->progressionIsReversed();
        LayoutUnit currLogicalLeftOffset = leftToRight ? 0_lu : contentLogicalWidth();
        LayoutUnit ruleAdd = logicalLeftOffsetForContent();
        LayoutUnit ruleLogicalLeft = leftToRight ? 0_lu : contentLogicalWidth();
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
                IntRect pixelSnappedRuleRect = snappedIntRect(ruleLeft, ruleTop, ruleRight - ruleLeft, ruleBottom - ruleTop);
                drawLineForBoxSide(paintInfo.context(), pixelSnappedRuleRect, boxSide, ruleColor, ruleStyle, 0, 0, antialias);
            }
            
            ruleLogicalLeft = currLogicalLeftOffset;
        }
    } else {
        bool topToBottom = !style().isFlippedBlocksWritingMode() ^ fragmentedFlow->progressionIsReversed();
        LayoutUnit ruleLeft = isHorizontalWritingMode() ? 0_lu : colGap / 2 - colGap - ruleThickness / 2;
        LayoutUnit ruleWidth = isHorizontalWritingMode() ? contentWidth() : ruleThickness;
        LayoutUnit ruleTop = isHorizontalWritingMode() ? colGap / 2 - colGap - ruleThickness / 2 : 0_lu;
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

        LayoutSize step(0_lu, topToBottom ? computedColumnHeight() + colGap : -(computedColumnHeight() + colGap));
        if (!isHorizontalWritingMode())
            step = step.transposedSize();

        for (unsigned i = 1; i < colCount; i++) {
            ruleRect.move(step);
            IntRect pixelSnappedRuleRect = snappedIntRect(ruleRect);
            drawLineForBoxSide(paintInfo.context(), pixelSnappedRuleRect, boxSide, ruleColor, ruleStyle, 0, 0, antialias);
        }
    }
}

void RenderMultiColumnSet::repaintFragmentedFlowContent(const LayoutRect& repaintRect)
{
    // Figure out the start and end columns and only check within that range so that we don't walk the
    // entire column set. Put the repaint rect into flow thread coordinates by flipping it first.
    LayoutRect fragmentedFlowRepaintRect(repaintRect);
    fragmentedFlow()->flipForWritingMode(fragmentedFlowRepaintRect);
    
    // Now we can compare this rect with the flow thread portions owned by each column. First let's
    // just see if the repaint rect intersects our flow thread portion at all.
    LayoutRect clippedRect(fragmentedFlowRepaintRect);
    clippedRect.intersect(RenderFragmentContainer::fragmentedFlowPortionOverflowRect());
    if (clippedRect.isEmpty())
        return;
    
    // Now we know we intersect at least one column. Let's figure out the logical top and logical
    // bottom of the area we're repainting.
    LayoutUnit repaintLogicalTop = isHorizontalWritingMode() ? fragmentedFlowRepaintRect.y() : fragmentedFlowRepaintRect.x();
    LayoutUnit repaintLogicalBottom = (isHorizontalWritingMode() ? fragmentedFlowRepaintRect.maxY() : fragmentedFlowRepaintRect.maxX()) - 1;
    
    unsigned startColumn = columnIndexAtOffset(repaintLogicalTop);
    unsigned endColumn = columnIndexAtOffset(repaintLogicalBottom);
    
    LayoutUnit colGap = columnGap();
    unsigned colCount = columnCount();
    for (unsigned i = startColumn; i <= endColumn; i++) {
        LayoutRect colRect = columnRectAt(i);
        
        // Get the portion of the flow thread that corresponds to this column.
        LayoutRect fragmentedFlowPortion = fragmentedFlowPortionRectAt(i);

        // Now get the overflow rect that corresponds to the column.
        LayoutRect fragmentedFlowOverflowPortion = fragmentedFlowPortionOverflowRect(fragmentedFlowPortion, i, colCount, colGap);

        // Do a repaint for this specific column.
        flipForWritingMode(colRect);
        repaintFragmentedFlowContentRectangle(repaintRect, fragmentedFlowPortion, colRect.location(), &fragmentedFlowOverflowPortion);
    }
}

LayoutUnit RenderMultiColumnSet::initialBlockOffsetForPainting() const
{
    bool progressionReversed = multiColumnFlow()->progressionIsReversed();
    bool progressionIsInline = multiColumnFlow()->progressionIsInline();
    
    LayoutUnit result;
    if (!progressionIsInline && progressionReversed) {
        LayoutRect colRect = columnRectAt(0);
        result = isHorizontalWritingMode() ? colRect.y() : colRect.x();
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
    LayoutRect layerBoundsInFragmentedFlow(layerBoundingBox);
    fragmentedFlow()->flipForWritingMode(layerBoundsInFragmentedFlow);

    // Now we can compare with the flow thread portions owned by each column. First let's
    // see if the rect intersects our flow thread portion at all.
    LayoutRect clippedRect(layerBoundsInFragmentedFlow);
    clippedRect.intersect(RenderFragmentContainer::fragmentedFlowPortionOverflowRect());
    if (clippedRect.isEmpty())
        return;
    
    // Now we know we intersect at least one column. Let's figure out the logical top and logical
    // bottom of the area we're checking.
    LayoutUnit layerLogicalTop = isHorizontalWritingMode() ? layerBoundsInFragmentedFlow.y() : layerBoundsInFragmentedFlow.x();
    LayoutUnit layerLogicalBottom = (isHorizontalWritingMode() ? layerBoundsInFragmentedFlow.maxY() : layerBoundsInFragmentedFlow.maxX()) - 1;
    
    // Figure out the start and end columns and only check within that range so that we don't walk the
    // entire column set.
    unsigned startColumn = columnIndexAtOffset(layerLogicalTop);
    unsigned endColumn = columnIndexAtOffset(layerLogicalBottom);
    
    LayoutUnit colLogicalWidth = computedColumnWidth();
    LayoutUnit colGap = columnGap();
    unsigned colCount = columnCount();

    bool progressionReversed = multiColumnFlow()->progressionIsReversed();
    bool progressionIsInline = multiColumnFlow()->progressionIsInline();

    LayoutUnit initialBlockOffset = initialBlockOffsetForPainting();
    
    for (unsigned i = startColumn; i <= endColumn; i++) {
        // Get the portion of the flow thread that corresponds to this column.
        LayoutRect fragmentedFlowPortion = fragmentedFlowPortionRectAt(i);
        
        // Now get the overflow rect that corresponds to the column.
        LayoutRect fragmentedFlowOverflowPortion = fragmentedFlowPortionOverflowRect(fragmentedFlowPortion, i, colCount, colGap);

        // In order to create a fragment we must intersect the portion painted by this column.
        LayoutRect clippedRect(layerBoundsInFragmentedFlow);
        clippedRect.intersect(fragmentedFlowOverflowPortion);
        if (clippedRect.isEmpty())
            continue;
        
        // We also need to intersect the dirty rect. We have to apply a translation and shift based off
        // our column index.
        LayoutSize translationOffset;
        LayoutUnit inlineOffset = progressionIsInline ? i * (colLogicalWidth + colGap) : 0_lu;
        
        bool leftToRight = style().isLeftToRightDirection() ^ progressionReversed;
        if (!leftToRight) {
            inlineOffset = -inlineOffset;
            if (progressionReversed)
                inlineOffset += contentLogicalWidth() - colLogicalWidth;
        }
        translationOffset.setWidth(inlineOffset);

        LayoutUnit blockOffset = initialBlockOffset + logicalTop() - fragmentedFlow()->logicalTop() + (isHorizontalWritingMode() ? -fragmentedFlowPortion.y() : -fragmentedFlowPortion.x());
        if (!progressionIsInline) {
            if (!progressionReversed)
                blockOffset = i * colGap;
            else
                blockOffset -= i * (computedColumnHeight() + colGap);
        }
        if (isFlippedWritingMode(style().writingMode()))
            blockOffset = -blockOffset;
        translationOffset.setHeight(blockOffset);
        if (!isHorizontalWritingMode())
            translationOffset = translationOffset.transposedSize();
        
        // Shift the dirty rect to be in flow thread coordinates with this translation applied.
        LayoutRect translatedDirtyRect(dirtyRect);
        translatedDirtyRect.move(-translationOffset);
        
        // See if we intersect the dirty rect.
        clippedRect = layerBoundingBox;
        clippedRect.intersect(translatedDirtyRect);
        if (clippedRect.isEmpty())
            continue;
        
        // Something does need to paint in this column. Make a fragment now and supply the physical translation
        // offset and the clip rect for the column with that offset applied.
        LayerFragment fragment;
        fragment.paginationOffset = translationOffset;

        LayoutRect flippedFragmentedFlowOverflowPortion(fragmentedFlowOverflowPortion);
        // Flip it into more a physical (RenderLayer-style) rectangle.
        fragmentedFlow()->flipForWritingMode(flippedFragmentedFlowOverflowPortion);
        fragment.paginationClip = flippedFragmentedFlowOverflowPortion;
        fragments.append(fragment);
    }
}

LayoutPoint RenderMultiColumnSet::columnTranslationForOffset(const LayoutUnit& offset) const
{
    unsigned startColumn = columnIndexAtOffset(offset);
    
    LayoutUnit colGap = columnGap();
    
    LayoutRect fragmentedFlowPortion = fragmentedFlowPortionRectAt(startColumn);
    LayoutPoint translationOffset;
    
    bool progressionReversed = multiColumnFlow()->progressionIsReversed();
    bool progressionIsInline = multiColumnFlow()->progressionIsInline();

    LayoutUnit initialBlockOffset = initialBlockOffsetForPainting();
    
    translationOffset.setX(columnLogicalLeft(startColumn));

    LayoutUnit blockOffset = initialBlockOffset - (isHorizontalWritingMode() ? fragmentedFlowPortion.y() : fragmentedFlowPortion.x());
    if (!progressionIsInline) {
        if (!progressionReversed)
            blockOffset = startColumn * colGap;
        else
            blockOffset -= startColumn * (computedColumnHeight() + colGap);
    }
    if (isFlippedWritingMode(style().writingMode()))
        blockOffset = -blockOffset;
    translationOffset.setY(blockOffset);
    
    if (!isHorizontalWritingMode())
        translationOffset = translationOffset.transposedPoint();
    
    return translationOffset;
}

void RenderMultiColumnSet::adjustFragmentBoundsFromFragmentedFlowPortionRect(LayoutRect&) const
{
    // This only fires for named flow thread compositing code, so let's make sure to ASSERT if this ever gets invoked.
    ASSERT_NOT_REACHED();
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

VisiblePosition RenderMultiColumnSet::positionForPoint(const LayoutPoint& logicalPoint, const RenderFragmentContainer*)
{
    return multiColumnFlow()->positionForPoint(translateFragmentPointToFragmentedFlow(logicalPoint, ClampHitTestTranslationToColumns), this);
}

LayoutPoint RenderMultiColumnSet::translateFragmentPointToFragmentedFlow(const LayoutPoint & logicalPoint, ColumnHitTestTranslationMode clampMode) const
{
    // Determine which columns we intersect.
    LayoutUnit colGap = columnGap();
    LayoutUnit halfColGap = colGap / 2;

    bool progressionIsInline = multiColumnFlow()->progressionIsInline();

    LayoutPoint point = logicalPoint;
    
    for (unsigned i = 0; i < columnCount(); i++) {
        // Add in half the column gap to the left and right of the rect.
        LayoutRect colRect = columnRectAt(i);
        if (isHorizontalWritingMode() == progressionIsInline) {
            LayoutRect gapAndColumnRect(colRect.x() - halfColGap, colRect.y(), colRect.width() + colGap, colRect.height());
            if (point.x() >= gapAndColumnRect.x() && point.x() < gapAndColumnRect.maxX()) {
                if (clampMode == ClampHitTestTranslationToColumns) {
                    if (progressionIsInline) {
                        // FIXME: The clamping that follows is not completely right for right-to-left
                        // content.
                        // Clamp everything above the column to its top left.
                        if (point.y() < gapAndColumnRect.y())
                            point = gapAndColumnRect.location();
                        // Clamp everything below the column to the next column's top left. If there is
                        // no next column, this still maps to just after this column.
                        else if (point.y() >= gapAndColumnRect.maxY()) {
                            point = gapAndColumnRect.location();
                            point.move(0_lu, gapAndColumnRect.height());
                        }
                    } else {
                        if (point.x() < colRect.x())
                            point.setX(colRect.x());
                        else if (point.x() >= colRect.maxX())
                            point.setX(colRect.maxX() - 1);
                    }
                }
                
                LayoutSize offsetInColumn = point - colRect.location();
                LayoutRect fragmentedFlowPortion = fragmentedFlowPortionRectAt(i);
                
                return fragmentedFlowPortion.location() + offsetInColumn;
            }
        } else {
            LayoutRect gapAndColumnRect(colRect.x(), colRect.y() - halfColGap, colRect.width(), colRect.height() + colGap);
            if (point.y() >= gapAndColumnRect.y() && point.y() < gapAndColumnRect.maxY()) {
                if (clampMode == ClampHitTestTranslationToColumns) {
                    if (progressionIsInline) {
                        // FIXME: The clamping that follows is not completely right for right-to-left
                        // content.
                        // Clamp everything above the column to its top left.
                        if (point.x() < gapAndColumnRect.x())
                            point = gapAndColumnRect.location();
                        // Clamp everything below the column to the next column's top left. If there is
                        // no next column, this still maps to just after this column.
                        else if (point.x() >= gapAndColumnRect.maxX()) {
                            point = gapAndColumnRect.location();
                            point.move(gapAndColumnRect.width(), 0_lu);
                        }
                    } else {
                        if (point.y() < colRect.y())
                            point.setY(colRect.y());
                        else if (point.y() >= colRect.maxY())
                            point.setY(colRect.maxY() - 1);
                    }
                }
                
                LayoutSize offsetInColumn = point - colRect.location();
                LayoutRect fragmentedFlowPortion = fragmentedFlowPortionRectAt(i);
                return fragmentedFlowPortion.location() + offsetInColumn;
            }
        }
    }

    return logicalPoint;
}

void RenderMultiColumnSet::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode() || !parent()->isRenderView())
        return;
    
    // Note this does not work with column spans, but once we implement RenderPageSet, we can move this code
    // over there instead (and spans of course won't be allowed on pages).
    Node* node = document().documentElement();
    if (node) {
        result.setInnerNode(node);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(node);
        LayoutPoint adjustedPoint = translateFragmentPointToFragmentedFlow(point);
        view().offsetForContents(adjustedPoint);
        result.setLocalPoint(adjustedPoint);
    }
}

const char* RenderMultiColumnSet::renderName() const
{    
    return "RenderMultiColumnSet";
}

}
