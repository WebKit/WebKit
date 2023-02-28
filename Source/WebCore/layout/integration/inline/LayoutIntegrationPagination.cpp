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

#include "FontCascade.h"
#include "InlineIteratorLineBox.h"
#include "RenderBlockFlow.h"
#include "RenderTableCell.h"

namespace WebCore {
namespace LayoutIntegration {

struct PaginatedLine {
    LayoutUnit top;
    LayoutUnit height;
};
using PaginatedLines = Vector<PaginatedLine, 20>;

static PaginatedLine computeLineTopAndBottomWithOverflow(const RenderBlockFlow&, const InlineContent::Lines& lines, unsigned lineIndex, const Vector<LineAdjustment>& adjustments)
{
    auto offset = adjustments[lineIndex].offset;
    auto overflowRect = LayoutRect(lines[lineIndex].inkOverflow());
    return { overflowRect.y() + offset, overflowRect.height() };
}

static unsigned computeLineBreakIndex(unsigned breakCandidate, unsigned lineCount, int orphansNeeded, int widowsNeeded,
    std::optional<unsigned> lastLineBreakIndex)
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
    if (!lastLineBreakIndex)
        return lineBreak;
    ASSERT(*lastLineBreakIndex + 1 < lineCount);
    return std::max<unsigned>(*lastLineBreakIndex + 1, lineBreak);
}

static LayoutUnit computeOffsetAfterLineBreak(LayoutUnit lineBreakPosition, bool isFirstLine, bool atTheTopOfColumnOrPage, const RenderBlockFlow& flow)
{
    // No offset for top of the page lines unless widows pushed the line break.
    LayoutUnit offset = isFirstLine ? flow.borderAndPaddingBefore() : 0_lu;
    if (atTheTopOfColumnOrPage)
        return offset;
    return offset + flow.pageRemainingLogicalHeightForOffset(lineBreakPosition, RenderBlockFlow::ExcludePageBoundary);
}

static bool setPageBreakForLine(unsigned lineBreakIndex, PaginatedLines& lines, RenderBlockFlow& flow, Vector<LineAdjustment>& adjustments, bool atTheTopOfColumnOrPage, bool lineDoesNotFit)
{
    auto line = lines.at(lineBreakIndex);
    auto remainingLogicalHeight = flow.pageRemainingLogicalHeightForOffset(line.top, RenderBlockFlow::ExcludePageBoundary);

    if (atTheTopOfColumnOrPage)
        flow.setPageBreak(line.top, line.height);
    else
        flow.setPageBreak(line.top, line.height - remainingLogicalHeight);

    auto& style = flow.style();
    auto firstLineDoesNotFit = !lineBreakIndex && line.height < flow.pageLogicalHeightForOffset(line.top);
    auto moveOrphanToNextColumn = lineDoesNotFit && !style.hasAutoOrphans() && style.orphans() > (short)lineBreakIndex;
    // Special table cell handling. See RenderBlockFlow::adjustLinePositionForPagination for details.
    if ((firstLineDoesNotFit || moveOrphanToNextColumn) && !is<RenderTableCell>(flow)) {
        auto firstLine = lines.first();
        auto firstLineUpperOverhang = std::max(LayoutUnit(-firstLine.top), 0_lu);
        flow.setPaginationStrut(line.top + remainingLogicalHeight + firstLineUpperOverhang);
        return false;
    }

    auto offset = computeOffsetAfterLineBreak(lines[lineBreakIndex].top, !lineBreakIndex, atTheTopOfColumnOrPage, flow);

    adjustments[lineBreakIndex].isFirstAfterPageBreak = true;
    for (auto i = lineBreakIndex; i < adjustments.size(); ++i)
        adjustments[i].offset += offset;
    return true;
}

static void updateMinimumPageHeight(RenderBlockFlow& flow, const InlineContent& inlineContent, unsigned lineCount)
{
    auto& style = flow.style();
    auto widows = style.hasAutoWidows() ? 1 : std::max<int>(style.widows(), 1);
    auto orphans = style.hasAutoOrphans() ? 1 : std::max<int>(style.orphans(), 1);
    auto minimumLineCount = std::min<unsigned>(std::max(widows, orphans), lineCount);
    flow.updateMinimumPageHeight(0, LayoutUnit(inlineContent.lines[minimumLineCount - 1].lineBoxBottom()));
}

Vector<LineAdjustment> computeAdjustmentsForPagination(const InlineContent& inlineContent, RenderBlockFlow& flow)
{
    auto lineCount = inlineContent.lines.size();
    updateMinimumPageHeight(flow, inlineContent, lineCount);
    // First pass with no pagination offset?
    if (!flow.pageLogicalHeightForOffset(0))
        return { };

    auto widows = flow.style().hasAutoWidows() ? 1 : std::max<int>(flow.style().widows(), 1);
    auto orphans = flow.style().hasAutoOrphans() ? 1 : std::max<int>(flow.style().orphans(), 1);
    PaginatedLines lines;

    Vector<LineAdjustment> adjustments { lineCount };
    std::optional<unsigned> lastLineBreakIndex;
    for (size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
        auto line = computeLineTopAndBottomWithOverflow(flow, inlineContent.lines, lineIndex, adjustments);
        lines.append(line);
        auto remainingHeight = flow.pageRemainingLogicalHeightForOffset(line.top, RenderBlockFlow::ExcludePageBoundary);
        auto atTheTopOfColumnOrPage = flow.pageLogicalHeightForOffset(line.top) == remainingHeight;
        auto lineDoesNotFit = line.height > remainingHeight;
        if (lineDoesNotFit || (atTheTopOfColumnOrPage && lineIndex)) {
            auto lineBreakIndex = computeLineBreakIndex(lineIndex, lineCount, orphans, widows, lastLineBreakIndex);
            // Are we still at the top of the column/page?
            atTheTopOfColumnOrPage = atTheTopOfColumnOrPage ? lineIndex == lineBreakIndex : false;

            if (setPageBreakForLine(lineBreakIndex, lines, flow, adjustments, atTheTopOfColumnOrPage, lineDoesNotFit))
                lastLineBreakIndex = lineBreakIndex;

            // Recompute line positions that we already visited but widow break pushed them to a new page.
            for (auto i = lineBreakIndex; i < lines.size(); ++i)
                lines.at(i) = computeLineTopAndBottomWithOverflow(flow, inlineContent.lines, i, adjustments);
        }
    }

    if (!lastLineBreakIndex)
        return { };

    return adjustments;
}

void adjustLinePositionsForPagination(InlineContent& inlineContent, const Vector<LineAdjustment>& adjustments)
{
    if (adjustments.isEmpty())
        return;

    for (size_t lineIndex = 0; lineIndex < inlineContent.lines.size(); ++lineIndex) {
        auto& line = inlineContent.lines[lineIndex];
        auto& adjustment = adjustments[lineIndex];
        line.moveVertically(adjustment.offset);
        if (adjustment.isFirstAfterPageBreak)
            line.setIsFirstAfterPageBreak();
    }
    for (auto& box : inlineContent.boxes)
        box.moveVertically(adjustments[box.lineIndex()].offset);
}

}
}

