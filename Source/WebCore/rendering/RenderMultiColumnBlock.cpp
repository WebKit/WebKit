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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderMultiColumnBlock.h"
#include "RenderMultiColumnFlowThread.h"
#include "RenderMultiColumnSet.h"
#include "StyleInheritedData.h"

using namespace std;

namespace WebCore {

RenderMultiColumnBlock::RenderMultiColumnBlock(Node* node)
    : RenderBlock(node)
    , m_flowThread(0)
    , m_columnCount(1)
    , m_columnWidth(0)
    , m_columnHeight(0)
    , m_requiresBalancing(false)
{
}

void RenderMultiColumnBlock::computeColumnCountAndWidth()
{
    // Calculate our column width and column count.
    // FIXME: Can overflow on fast/block/float/float-not-removed-from-next-sibling4.html, see https://bugs.webkit.org/show_bug.cgi?id=68744
    m_columnCount = 1;
    m_columnWidth = contentLogicalWidth();
    
    ASSERT(!style()->hasAutoColumnCount() || !style()->hasAutoColumnWidth());

    LayoutUnit availWidth = m_columnWidth;
    LayoutUnit colGap = columnGap();
    LayoutUnit colWidth = max<LayoutUnit>(1, LayoutUnit(style()->columnWidth()));
    int colCount = max<int>(1, style()->columnCount());

    if (style()->hasAutoColumnWidth() && !style()->hasAutoColumnCount()) {
        m_columnCount = colCount;
        m_columnWidth = max<LayoutUnit>(0, (availWidth - ((m_columnCount - 1) * colGap)) / m_columnCount);
    } else if (!style()->hasAutoColumnWidth() && style()->hasAutoColumnCount()) {
        m_columnCount = max<LayoutUnit>(1, (availWidth + colGap) / (colWidth + colGap));
        m_columnWidth = ((availWidth + colGap) / m_columnCount) - colGap;
    } else {
        m_columnCount = max<LayoutUnit>(min<LayoutUnit>(colCount, (availWidth + colGap) / (colWidth + colGap)), 1);
        m_columnWidth = ((availWidth + colGap) / m_columnCount) - colGap;
    }
}

bool RenderMultiColumnBlock::updateLogicalWidthAndColumnWidth()
{
    bool relayoutChildren = RenderBlock::updateLogicalWidthAndColumnWidth();
    LayoutUnit oldColumnWidth = m_columnWidth;
    computeColumnCountAndWidth();
    if (m_columnWidth != oldColumnWidth)
        relayoutChildren = true;
    return relayoutChildren;
}

void RenderMultiColumnBlock::checkForPaginationLogicalHeightChange(LayoutUnit& /*pageLogicalHeight*/, bool& /*pageLogicalHeightChanged*/, bool& /*hasSpecifiedPageLogicalHeight*/)
{
    // We don't actually update any of the variables. We just subclassed to adjust our column height.
    updateLogicalHeight();
    LayoutUnit newContentLogicalHeight = contentLogicalHeight();
    m_requiresBalancing = !newContentLogicalHeight;
    if (!m_requiresBalancing) {
        // The regions will be invalidated when we lay them out and they change size to
        // the new column height.
        if (columnHeight() != newContentLogicalHeight)
            setColumnHeight(newContentLogicalHeight);
    }
    setLogicalHeight(0);

    // Set up our column sets.
    ensureColumnSets();
}

bool RenderMultiColumnBlock::relayoutForPagination(bool, LayoutUnit, LayoutStateMaintainer&)
{
    // FIXME: Implement.
    return false;
}

void RenderMultiColumnBlock::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (!m_flowThread) {
        m_flowThread = new (renderArena()) RenderMultiColumnFlowThread(document());
        m_flowThread->setStyle(RenderStyle::createAnonymousStyleWithDisplay(style(), BLOCK));
        RenderBlock::addChild(m_flowThread); // Always put the flow thread at the end.
    }

    // Column sets are siblings of the flow thread. All children designed to be in the columns, however, are part
    // of the flow thread itself.
    if (newChild->isRenderMultiColumnSet())
        RenderBlock::addChild(newChild, beforeChild);
    else
        m_flowThread->addChild(newChild, beforeChild);
}

void RenderMultiColumnBlock::ensureColumnSets()
{
    // This function ensures we have the correct column set information before we get into layout.
    // For a simple multi-column layout in continuous media, only one column set child is required.
    // Once a column is nested inside an enclosing pagination context, the number of column sets
    // required becomes 2n-1, where n is the total number of nested pagination contexts. For example:
    //
    // Column layout with no enclosing pagination model = 2 * 1 - 1 = 1 column set.
    // Columns inside pages = 2 * 2 - 1 = 3 column sets (bottom of first page, all the subsequent pages, then the last page).
    // Columns inside columns inside pages = 2 * 3 - 1 = 5 column sets.
    //
    // In addition, column spans will force a column set to "split" into before/after sets around the spanning region.
    //
    // Finally, we will need to deal with columns inside regions. If regions have variable widths, then there will need
    // to be unique column sets created inside any region whose width is different from its surrounding regions. This is
    // actually pretty similar to the spanning case, in that we break up the column sets whenever the width varies.
    //
    // FIXME: For now just make one column set. This matches the old multi-column code.
    // Right now our goal is just feature parity with the old multi-column code so that we can switch over to the
    // new code as soon as possible.
    if (!flowThread())
        return;

    RenderMultiColumnSet* columnSet = firstChild()->isRenderMultiColumnSet() ? toRenderMultiColumnSet(firstChild()) : 0;
    if (!columnSet) {
        columnSet = new (renderArena()) RenderMultiColumnSet(document(), flowThread());
        columnSet->setStyle(RenderStyle::createAnonymousStyleWithDisplay(style(), BLOCK));
        RenderBlock::addChild(columnSet, firstChild());
    }
    columnSet->setRequiresBalancing(requiresBalancing());
}

const char* RenderMultiColumnBlock::renderName() const
{
    if (isFloating())
        return "RenderMultiColumnBlock (floating)";
    if (isOutOfFlowPositioned())
        return "RenderMultiColumnBlock (positioned)";
    if (isAnonymousBlock())
        return "RenderMultiColumnBlock (anonymous)";
    // FIXME: Temporary hack while the new generated content system is being implemented.
    if (isPseudoElement())
        return "RenderMultiColumnBlock (generated)";
    if (isAnonymous())
        return "RenderMultiColumnBlock (generated)";
    if (isRelPositioned())
        return "RenderMultiColumnBlock (relative positioned)";
    return "RenderMultiColumnBlock";
}

}
