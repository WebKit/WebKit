/*
 * Copyright (C) 2017,2018 Apple Inc.  All rights reserved.
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
#include "RenderLinesClampSet.h"

#include "RenderBoxFragmentInfo.h"
#include "RenderLinesClampFlow.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderLinesClampSet);

RenderLinesClampSet::RenderLinesClampSet(RenderFragmentedFlow& fragmentedFlow, RenderStyle&& style)
    : RenderMultiColumnSet(fragmentedFlow, WTFMove(style))
{
}

bool RenderLinesClampSet::recalculateColumnHeight(bool initial)
{
    if (!initial && m_endPageHeight)
        return false;

    auto* fragmentedFlow = multiColumnFlow();
    if (!fragmentedFlow)
        return false;
    
    auto* blockFlow = multiColumnBlockFlow();
    auto endClamp = blockFlow->style().linesClamp().end();
    
    int maxLineCount = fragmentedFlow->lineCount();
    auto startClamp = blockFlow->style().linesClamp().start();
    int startLines = startClamp.isPercentage() ? std::max(1, maxLineCount * startClamp.value() / 100) : startClamp.value();
    
    auto bottom = logicalBottomInFragmentedFlow();
    
    if (initial) {
        if (startLines >= maxLineCount)
            m_startPageHeight = bottom;
        else
            m_startPageHeight = fragmentedFlow->logicalHeightForLineCount(startLines);
        m_endPageHeight = 0;
        m_middlePageHeight = bottom - m_startPageHeight;
        if (m_startPageHeight < bottom) {
            m_computedColumnHeight = m_startPageHeight;
            m_columnHeightComputed = true;
            updateLogicalWidth();
            return true;
        }
        return false;
    }
    
    if (!m_endPageHeight) {
        int endLines = endClamp.isPercentage() ? std::max(1, maxLineCount * endClamp.value() / 100) : endClamp.value();
        maxLineCount -= startLines;
        if (endClamp.isNone() || endLines >= maxLineCount)
            m_endPageHeight = bottom - m_startPageHeight;
        else
            m_endPageHeight = bottom - fragmentedFlow->logicalHeightExcludingLineCount(endLines);
        m_middlePageHeight = bottom - (m_endPageHeight + m_startPageHeight);
        if (m_endPageHeight > 0) {
            m_computedColumnHeight = m_startPageHeight;
            m_columnHeightComputed = true;
            updateLogicalWidth();
            return true;
        }
    }

    return false;
}

RenderBox::LogicalExtentComputedValues RenderLinesClampSet::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop) const
{
    return { m_startPageHeight + m_middleObjectHeight + m_endPageHeight, logicalTop, ComputedMarginValues() };
}

unsigned RenderLinesClampSet::columnCount() const
{
    if (m_endPageHeight)
        return 3;
    if (m_startPageHeight)
        return 2;
    return 1;
}

LayoutRect RenderLinesClampSet::columnRectAt(unsigned index) const
{
    LayoutUnit colLogicalWidth = computedColumnWidth();
    if (!index)
        return LayoutRect(columnLogicalLeft(0), columnLogicalTop(0), colLogicalWidth, m_startPageHeight);
    if (index == 1)
        return LayoutRect(columnLogicalLeft(0), columnLogicalTop(0), colLogicalWidth, 0);
    return LayoutRect(columnLogicalLeft(0), columnLogicalTop(0) + m_startPageHeight + m_middleObjectHeight, colLogicalWidth, m_endPageHeight);
}

unsigned RenderLinesClampSet::columnIndexAtOffset(LayoutUnit offset, ColumnIndexCalculationMode) const
{
    if (offset < m_startPageHeight)
        return 0;
    if (offset < m_startPageHeight + m_middlePageHeight)
        return 1;
    return 2;
}

LayoutUnit RenderLinesClampSet::pageLogicalTopForOffset(LayoutUnit offset) const
{
    unsigned colIndex = columnIndexAtOffset(offset);
    if (!colIndex)
        return 0;
    if (colIndex == 1)
        return m_startPageHeight;
    return m_startPageHeight + m_middlePageHeight;
}

LayoutUnit RenderLinesClampSet::pageLogicalHeightForOffset(LayoutUnit offset) const
{
    unsigned colIndex = columnIndexAtOffset(offset);
    if (!colIndex)
        return m_startPageHeight;
    if (colIndex == 1)
        return m_middlePageHeight;
    return m_endPageHeight;
}

LayoutRect RenderLinesClampSet::fragmentedFlowPortionRectAt(unsigned index) const
{
    LayoutUnit logicalTop;
    LayoutUnit logicalHeight;
    if (!index) {
        logicalTop = 0;
        logicalHeight = m_startPageHeight;
    } else if (index == 1) {
        logicalTop = m_startPageHeight;
        logicalHeight = m_middlePageHeight;
    } else {
        logicalTop = m_startPageHeight + m_middlePageHeight;
        logicalHeight = m_endPageHeight;
    }

    LayoutRect portionRect = fragmentedFlowPortionRect();
    if (isHorizontalWritingMode())
        portionRect = LayoutRect(portionRect.x(), portionRect.y() + logicalTop, portionRect.width(), logicalHeight);
    else
        portionRect = LayoutRect(portionRect.x() + logicalTop, portionRect.y(), logicalHeight, portionRect.height());
    return portionRect;
}

LayoutRect RenderLinesClampSet::fragmentedFlowPortionOverflowRect(const LayoutRect& portionRect, unsigned index, unsigned colCount, LayoutUnit /* colGap */)
{
    bool isFirstColumn = !index;
    bool isLastColumn = index == colCount - 1;
    
    LayoutRect overflowRect = overflowRectForFragmentedFlowPortion(portionRect, isFirstColumn, isLastColumn, VisualOverflow);
    
    if (isHorizontalWritingMode()) {
        if (!isFirstColumn)
            overflowRect.shiftYEdgeTo(portionRect.y());
        if (!isLastColumn)
            overflowRect.shiftMaxYEdgeTo(portionRect.maxY());
    } else {
        if (!isFirstColumn)
            overflowRect.shiftXEdgeTo(portionRect.x());
        if (!isLastColumn)
            overflowRect.shiftMaxXEdgeTo(portionRect.maxX());
    }
    return overflowRect;
}

LayoutUnit RenderLinesClampSet::customBlockProgressionAdjustmentForColumn(unsigned index) const
{
    if (index == 2)
        return m_middleObjectHeight - m_middlePageHeight;
    return 0;
}

const char* RenderLinesClampSet::renderName() const
{    
    return "RenderLinesClampSet";
}

}
