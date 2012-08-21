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
#include "PaintInfo.h"
#include "RenderMultiColumnFlowThread.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnBlock.h"

namespace WebCore {

RenderMultiColumnSet::RenderMultiColumnSet(Node* node, RenderFlowThread* flowThread)
    : RenderRegionSet(node, flowThread)
    , m_columnCount(1)
    , m_columnWidth(ZERO_LAYOUT_UNIT)
    , m_columnHeight(ZERO_LAYOUT_UNIT)
{
}

void RenderMultiColumnSet::computeLogicalWidth()
{
    // Our logical width starts off matching the column block itself.
    // This width will be fixed up after the flow thread lays out once it is determined exactly how many
    // columns we ended up holding.
    // FIXME: When we add regions support, we'll start it off at the width of the multi-column
    // block in that particular region.
    setLogicalWidth(parentBox()->contentLogicalWidth());
    
    RenderMultiColumnBlock* parentBlock = toRenderMultiColumnBlock(parent());
    setColumnWidthAndCount(parentBlock->columnWidth(), parentBlock->columnCount()); // FIXME: This will eventually vary if we are contained inside regions.
}

void RenderMultiColumnSet::computeLogicalHeight()
{
    // Make sure our column height is up to date.
    RenderMultiColumnBlock* parentBlock = toRenderMultiColumnBlock(parent());
    setColumnHeight(parentBlock->columnHeight()); // FIXME: Once we make more than one column set, this will become variable.
    
    // Our logical height is always just the height of our columns.
    setLogicalHeight(columnHeight());
}

LayoutUnit RenderMultiColumnSet::columnGap() const
{
    if (style()->hasNormalColumnGap())
        return style()->fontDescription().computedPixelSize(); // "1em" is recommended as the normal gap setting. Matches <p> margins.
    return static_cast<int>(style()->columnGap());
}

LayoutRect RenderMultiColumnSet::columnRectAt(unsigned index) const
{
    LayoutUnit colLogicalWidth = columnWidth();
    LayoutUnit colLogicalHeight = columnHeight();
    LayoutUnit colLogicalTop = borderBefore() + paddingBefore();
    LayoutUnit colLogicalLeft = borderAndPaddingLogicalLeft();
    int colGap = columnGap();
    if (style()->isLeftToRightDirection())
        colLogicalLeft += index * (colLogicalWidth + colGap);
    else
        colLogicalLeft += contentLogicalWidth() - colLogicalWidth - index * (colLogicalWidth + colGap);

    if (isHorizontalWritingMode())
        return LayoutRect(colLogicalLeft, colLogicalTop, colLogicalWidth, colLogicalHeight);
    return LayoutRect(colLogicalTop, colLogicalLeft, colLogicalHeight, colLogicalWidth);
}

void RenderMultiColumnSet::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // FIXME: RenderRegions are replaced elements right now and so they only paint in the foreground phase.
    // Columns should technically respect phases and allow for background/float/foreground overlap etc., just like
    // RenderBlocks do. We can't correct this, however, until RenderRegions are changed to actually be
    // RenderBlocks. Note this is a pretty minor issue, since the old column implementation clipped columns
    // anyway, thus making it impossible for them to overlap one another. It's also really unlikely that the columns
    // would overlap another block.
    setRegionObjectsRegionStyle();
    paintColumnRules(paintInfo, paintOffset);
    paintColumnContents(paintInfo, paintOffset);
    restoreRegionObjectsOriginalStyle();
}

void RenderMultiColumnSet::paintColumnRules(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.context->paintingDisabled())
        return;

    RenderStyle* blockStyle = toRenderMultiColumnBlock(parent())->style();
    const Color& ruleColor = blockStyle->visitedDependentColor(CSSPropertyWebkitColumnRuleColor);
    bool ruleTransparent = blockStyle->columnRuleIsTransparent();
    EBorderStyle ruleStyle = blockStyle->columnRuleStyle();
    LayoutUnit ruleThickness = blockStyle->columnRuleWidth();
    LayoutUnit colGap = columnGap();
    bool renderRule = ruleStyle > BHIDDEN && !ruleTransparent && ruleThickness <= colGap;
    if (!renderRule)
        return;

    unsigned colCount = columnCount();

    bool antialias = shouldAntialiasLines(paintInfo.context);

    bool leftToRight = style()->isLeftToRightDirection();
    LayoutUnit currLogicalLeftOffset = leftToRight ? ZERO_LAYOUT_UNIT : contentLogicalWidth();
    LayoutUnit ruleAdd = borderAndPaddingLogicalLeft();
    LayoutUnit ruleLogicalLeft = leftToRight ? ZERO_LAYOUT_UNIT : contentLogicalWidth();
    LayoutUnit inlineDirectionSize = columnWidth();
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
}

void RenderMultiColumnSet::paintColumnContents(PaintInfo& /* paintInfo */, const LayoutPoint& /* paintOffset */)
{
    // For each rectangle, set it as the region rectangle and then let flow thread painting do the rest.
    // We make multiple calls to paintIntoRegion, changing the rectangles each time.
    if (!columnCount())
        return;
        
    // FIXME: Implement.
}

const char* RenderMultiColumnSet::renderName() const
{    
    return "RenderMultiColumnSet";
}

}
