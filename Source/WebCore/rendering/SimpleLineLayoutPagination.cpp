/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SimpleLineLayoutPagination.h"

#include "RenderBlockFlow.h"
#include "SimpleLineLayout.h"
#include "SimpleLineLayoutFunctions.h"

namespace WebCore {
namespace SimpleLineLayout {

struct PaginatedLine {
    LayoutUnit top;
    LayoutUnit bottom;
    LayoutUnit height; // Same value for each lines atm.
};
using PaginatedLines = Vector<PaginatedLine, 20>;

static PaginatedLine computeLineTopAndBottomWithOverflow(const RenderBlockFlow& flow, unsigned lineIndex, Layout::SimpleLineStruts& struts)
{
    // FIXME: Add visualOverflowForDecorations.
    auto& fontMetrics = flow.style().fontCascade().fontMetrics();
    auto ascent = fontMetrics.floatAscent();
    auto descent = fontMetrics.floatDescent();
    auto lineHeight = lineHeightFromFlow(flow);
    LayoutUnit offset = flow.borderAndPaddingBefore();
    for (auto& strut : struts) {
        if (strut.lineBreak > lineIndex)
            break;
        offset += strut.offset;
    }
    if (ascent + descent <= lineHeight) {
        auto topPosition = lineIndex * lineHeight + offset;
        return { topPosition, topPosition + lineHeight, lineHeight };
    }
    auto baseline = baselineFromFlow(flow);
    auto topPosition = lineIndex * lineHeight + offset + baseline - ascent;
    auto bottomPosition = topPosition + ascent + descent;
    return { topPosition, bottomPosition, bottomPosition - topPosition };
}

static unsigned computeLineBreakIndex(unsigned breakCandidate, unsigned lineCount, unsigned widows, const Layout::SimpleLineStruts& struts)
{
    // First line does not fit the current page.
    if (!breakCandidate)
        return breakCandidate;
    
    auto remainingLineCount = lineCount - breakCandidate;
    if (widows <= remainingLineCount)
        return breakCandidate;
    
    // Only break after the first line with widows.
    auto lineBreak = std::max<int>(lineCount - widows, 1);
    // Break on current page only.
    if (struts.isEmpty())
        return lineBreak;
    ASSERT(struts.last().lineBreak + 1 < lineCount);
    return std::max<unsigned>(struts.last().lineBreak + 1, lineBreak);
}

static LayoutUnit computeOffsetAfterLineBreak(LayoutUnit lineBreakPosition, bool isFirstLine, bool atTheTopOfColumnOrPage, const RenderBlockFlow& flow)
{
    // No offset for top of the page lines unless widows pushed the line break.
    LayoutUnit offset = isFirstLine ? flow.borderAndPaddingBefore() : LayoutUnit();
    if (atTheTopOfColumnOrPage)
        return offset;
    return offset + flow.pageRemainingLogicalHeightForOffset(lineBreakPosition, RenderBlockFlow::ExcludePageBoundary);
}

static void setPageBreakForLine(unsigned lineBreakIndex, PaginatedLines& lines, RenderBlockFlow& flow, Layout::SimpleLineStruts& struts,
    bool atTheTopOfColumnOrPage)
{
    if (!lineBreakIndex) {
        // When the first line does not fit the current page, just add a page break in front and set the strut on the block.
        auto line = lines.first();
        auto remainingLogicalHeight = flow.pageRemainingLogicalHeightForOffset(line.top, RenderBlockFlow::ExcludePageBoundary);
        flow.setPageBreak(line.top, line.height - remainingLogicalHeight);
        flow.setPaginationStrut(remainingLogicalHeight);
        return;
    }
    auto beforeLineBreak = lines.at(lineBreakIndex - 1);
    auto spaceShortage = flow.pageRemainingLogicalHeightForOffset(beforeLineBreak.top, RenderBlockFlow::ExcludePageBoundary) - beforeLineBreak.height;
    flow.setPageBreak(beforeLineBreak.bottom, spaceShortage);
    struts.append({ lineBreakIndex, computeOffsetAfterLineBreak(lines[lineBreakIndex].top, !lineBreakIndex, atTheTopOfColumnOrPage, flow) });
}

static void updateMinimumPageHeight(RenderBlockFlow& flow, unsigned lineCount)
{
    auto& style = flow.style();
    auto widows = style.hasAutoWidows() ? 1 : std::max<int>(style.widows(), 1);
    auto orphans = style.hasAutoOrphans() ? 1 : std::max<int>(style.orphans(), 1);
    auto minimumLineCount = std::min<unsigned>(std::max(widows, orphans), lineCount);
    flow.updateMinimumPageHeight(0, minimumLineCount * lineHeightFromFlow(flow));
}

void adjustLinePositionsForPagination(SimpleLineLayout::Layout& simpleLines, RenderBlockFlow& flow)
{
    Layout::SimpleLineStruts struts;
    auto lineCount = simpleLines.lineCount();
    updateMinimumPageHeight(flow, lineCount);
    // First pass with no pagination offset?
    if (!flow.pageLogicalHeightForOffset(0))
        return;
    unsigned lineIndex = 0;
    auto widows = flow.style().hasAutoWidows() ? 1 : std::max<int>(flow.style().widows(), 1);
    PaginatedLines lines;
    for (unsigned runIndex = 0; runIndex < simpleLines.runCount(); ++runIndex) {
        auto& run = simpleLines.runAt(runIndex);
        if (!run.isEndOfLine)
            continue;

        auto line = computeLineTopAndBottomWithOverflow(flow, lineIndex, struts);
        lines.append(line);
        auto remainingHeight = flow.pageRemainingLogicalHeightForOffset(line.top, RenderBlockFlow::ExcludePageBoundary);
        auto atTheTopOfColumnOrPage = flow.pageLogicalHeightForOffset(line.top) == remainingHeight;
        if (line.height > remainingHeight || (atTheTopOfColumnOrPage && lineIndex)) {
            auto lineBreakIndex = computeLineBreakIndex(lineIndex, lineCount, widows, struts);
            // Are we still at the top of the column/page?
            atTheTopOfColumnOrPage = atTheTopOfColumnOrPage ? lineIndex == lineBreakIndex : false;
            setPageBreakForLine(lineBreakIndex, lines, flow, struts, atTheTopOfColumnOrPage);
            // Recompute line positions that we already visited but window break pushed them to a new page.
            for (auto i = lineBreakIndex; i < lines.size(); ++i)
                lines.at(i) = computeLineTopAndBottomWithOverflow(flow, i, struts);
        }
        ++lineIndex;
    }
    simpleLines.setLineStruts(WTFMove(struts));
}

}
}
