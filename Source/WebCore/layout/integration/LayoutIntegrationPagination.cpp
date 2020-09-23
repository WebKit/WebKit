/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationPagination.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FontCascade.h"
#include "RenderBlockFlow.h"
#include "SimpleLineLayout.h"
#include "SimpleLineLayoutFunctions.h"

namespace WebCore {
namespace LayoutIntegration {

struct Strut {
    unsigned lineBreak;
    float offset;
};

struct PaginatedLine {
    LayoutUnit top;
    LayoutUnit height;
};
using PaginatedLines = Vector<PaginatedLine, 20>;

static PaginatedLine computeLineTopAndBottomWithOverflow(const RenderBlockFlow&, Display::InlineContent::Lines& lines, unsigned lineIndex, Vector<Strut>& struts)
{
    LayoutUnit offset = 0;
    for (auto& strut : struts) {
        if (strut.lineBreak > lineIndex)
            break;
        offset += strut.offset;
    }
    auto overflowRect = LayoutRect(lines[lineIndex].inkOverflow());
    return { overflowRect.y() + offset, overflowRect.height() };
}

static unsigned computeLineBreakIndex(unsigned breakCandidate, unsigned lineCount, int orphansNeeded, int widowsNeeded,
    const Vector<Strut>& struts)
{
    // First line does not fit the current page.
    if (!breakCandidate)
        return breakCandidate;
    
    int widowsOnTheNextPage = lineCount - breakCandidate;
    if (widowsNeeded <= widowsOnTheNextPage)
        return breakCandidate;
    // Only break after the first line with widows.
    auto lineBreak = std::max<int>(lineCount - widowsNeeded, 1);
    if (orphansNeeded > lineBreak)
        return breakCandidate;
    // Break on current page only.
    if (struts.isEmpty())
        return lineBreak;
    ASSERT(struts.last().lineBreak + 1 < lineCount);
    return std::max<unsigned>(struts.last().lineBreak + 1, lineBreak);
}

static LayoutUnit computeOffsetAfterLineBreak(LayoutUnit lineBreakPosition, bool isFirstLine, bool atTheTopOfColumnOrPage, const RenderBlockFlow& flow)
{
    // No offset for top of the page lines unless widows pushed the line break.
    LayoutUnit offset = isFirstLine ? flow.borderAndPaddingBefore() : 0_lu;
    if (atTheTopOfColumnOrPage)
        return offset;
    return offset + flow.pageRemainingLogicalHeightForOffset(lineBreakPosition, RenderBlockFlow::ExcludePageBoundary);
}

static void setPageBreakForLine(unsigned lineBreakIndex, PaginatedLines& lines, RenderBlockFlow& flow, Vector<Strut>& struts,
    bool atTheTopOfColumnOrPage, bool lineDoesNotFit)
{
    auto line = lines.at(lineBreakIndex);
    auto remainingLogicalHeight = flow.pageRemainingLogicalHeightForOffset(line.top, RenderBlockFlow::ExcludePageBoundary);
    auto& style = flow.style();
    auto firstLineDoesNotFit = !lineBreakIndex && line.height < flow.pageLogicalHeightForOffset(line.top);
    auto moveOrphanToNextColumn = lineDoesNotFit && !style.hasAutoOrphans() && style.orphans() > (short)lineBreakIndex;
    if (firstLineDoesNotFit || moveOrphanToNextColumn) {
        auto firstLine = lines.first();
        auto firstLineUpperOverhang = std::max(LayoutUnit(-firstLine.top), 0_lu);
        flow.setPaginationStrut(line.top + remainingLogicalHeight + firstLineUpperOverhang);
        return;
    }
    if (atTheTopOfColumnOrPage)
        flow.setPageBreak(line.top, line.height);
    else
        flow.setPageBreak(line.top, line.height - remainingLogicalHeight);
    struts.append({ lineBreakIndex, computeOffsetAfterLineBreak(lines[lineBreakIndex].top, !lineBreakIndex, atTheTopOfColumnOrPage, flow) });
}

static void updateMinimumPageHeight(RenderBlockFlow& flow, const Display::InlineContent& inlineContent, unsigned lineCount)
{
    auto& style = flow.style();
    auto widows = style.hasAutoWidows() ? 1 : std::max<int>(style.widows(), 1);
    auto orphans = style.hasAutoOrphans() ? 1 : std::max<int>(style.orphans(), 1);
    auto minimumLineCount = std::min<unsigned>(std::max(widows, orphans), lineCount);
    flow.updateMinimumPageHeight(0, LayoutUnit(inlineContent.lines[minimumLineCount - 1].rect().maxY()));
}

static Ref<Display::InlineContent> makeAdjustedContent(const Display::InlineContent& inlineContent, Vector<float> adjustments)
{
    auto moveVertically = [](FloatRect rect, float offset) {
        rect.move(FloatSize(0, offset));
        return rect;
    };

    auto adjustedLine = [&](const Display::Line& line, float offset)
    {
        return Display::Line {
            moveVertically(line.rect(), offset),
            moveVertically(line.scrollableOverflow(), offset),
            moveVertically(line.inkOverflow(), offset),
            line.baseline(),
            line.horizontalAlignmentOffset()
        };
    };

    auto adjustedRun = [&](const Display::Run& run, float offset)
    {
        return Display::Run {
            run.lineIndex(),
            run.layoutBox(),
            moveVertically(run.rect(), offset),
            moveVertically(run.inkOverflow(), offset),
            run.expansion(),
            run.textContent()
        };
    };

    auto adjustedContent = Display::InlineContent::create();

    for (size_t lineIndex = 0; lineIndex < inlineContent.lines.size(); ++lineIndex)
        adjustedContent->lines.append(adjustedLine(inlineContent.lines[lineIndex], adjustments[lineIndex]));

    for (auto& run : inlineContent.runs)
        adjustedContent->runs.append(adjustedRun(run, adjustments[run.lineIndex()]));

    return adjustedContent;
}

Ref<Display::InlineContent> adjustLinePositionsForPagination(Display::InlineContent& inlineContent, RenderBlockFlow& flow)
{
    Vector<Strut> struts;
    auto lineCount = inlineContent.lines.size();
    updateMinimumPageHeight(flow, inlineContent, lineCount);
    // First pass with no pagination offset?
    if (!flow.pageLogicalHeightForOffset(0))
        return inlineContent;

    auto widows = flow.style().hasAutoWidows() ? 1 : std::max<int>(flow.style().widows(), 1);
    auto orphans = flow.style().hasAutoOrphans() ? 1 : std::max<int>(flow.style().orphans(), 1);
    PaginatedLines lines;
    for (unsigned lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
        auto line = computeLineTopAndBottomWithOverflow(flow, inlineContent.lines, lineIndex, struts);
        lines.append(line);
        auto remainingHeight = flow.pageRemainingLogicalHeightForOffset(line.top, RenderBlockFlow::ExcludePageBoundary);
        auto atTheTopOfColumnOrPage = flow.pageLogicalHeightForOffset(line.top) == remainingHeight;
        auto lineDoesNotFit = line.height > remainingHeight;
        if (lineDoesNotFit || (atTheTopOfColumnOrPage && lineIndex)) {
            auto lineBreakIndex = computeLineBreakIndex(lineIndex, lineCount, orphans, widows, struts);
            // Are we still at the top of the column/page?
            atTheTopOfColumnOrPage = atTheTopOfColumnOrPage ? lineIndex == lineBreakIndex : false;
            setPageBreakForLine(lineBreakIndex, lines, flow, struts, atTheTopOfColumnOrPage, lineDoesNotFit);
            // Recompute line positions that we already visited but widow break pushed them to a new page.
            for (auto i = lineBreakIndex; i < lines.size(); ++i)
                lines.at(i) = computeLineTopAndBottomWithOverflow(flow, inlineContent.lines, i, struts);
        }
    }

    if (struts.isEmpty())
        return inlineContent;

    auto adjustments = Vector<float>(lineCount, 0);
    for (auto& strut : struts) {
        for (auto lineIndex = strut.lineBreak; lineIndex < lineCount; ++lineIndex)
            adjustments[lineIndex] += strut.offset;
    }

    return makeAdjustedContent(inlineContent, adjustments);
}

}
}

#endif
