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

#include "BlockLayoutState.h"
#include "FontCascade.h"
#include "InlineIteratorLineBox.h"
#include "PlacedFloats.h"
#include "RenderBlockFlow.h"
#include "RenderStyleInlines.h"
#include "StyleOrphans.h"
#include "StyleWidows.h"

namespace WebCore {
namespace LayoutIntegration {

static LayoutUnit computeFirstLineSnapAdjustment(const InlineDisplay::Line& line, const Layout::BlockLayoutState::LineGrid& lineGrid)
{
    auto gridLineHeight = lineGrid.rowHeight;

    auto& gridFontMetrics = lineGrid.primaryFont->fontMetrics();
    auto lineGridFontAscent = gridFontMetrics.intAscent(line.baselineType());
    auto lineGridFontHeight = gridFontMetrics.intHeight();
    auto lineGridHalfLeading = (gridLineHeight - lineGridFontHeight) / 2;
    auto firstLineTop = lineGrid.topRowOffset;
    auto firstTextTop = firstLineTop + lineGridHalfLeading;
    auto firstBaselinePosition = firstTextTop + lineGridFontAscent;

    auto baseline =  LayoutUnit { line.baseline() };
    return lineGrid.paginationOrigin.value_or(LayoutSize { }).height() + firstBaselinePosition - baseline;
}

std::pair<Vector<LineAdjustment>, std::optional<LayoutRestartLine>> computeAdjustmentsForPagination(const InlineContent& inlineContent, const Layout::PlacedFloats& placedFloats, bool allowLayoutRestart, const Layout::BlockLayoutState& blockLayoutState, RenderBlockFlow& flow)
{
    auto lineCount = inlineContent.displayContent().lines.size();
    Vector<LineAdjustment> adjustments { lineCount };

    HashMap<size_t, LayoutUnit, DefaultHash<size_t>, WTF::UnsignedWithZeroKeyHashTraits<size_t>>  lineFloatBottomMap;
    for (auto& floatBox : placedFloats.list()) {
        if (!floatBox.layoutBox())
            continue;

        auto& renderer = downcast<RenderBox>(*floatBox.layoutBox()->rendererForIntegration());
        bool isUsplittable = renderer.isUnsplittableForPagination() || renderer.style().breakInside() == BreakInside::Avoid;

        auto placedByLine = floatBox.placedByLine();
        if (!placedByLine) {
            if (isUsplittable) {
                auto rect = floatBox.absoluteRectWithMargin();
                flow.updateMinimumPageHeight(rect.top(), rect.height());
            }
            continue;
        }

        auto floatMinimumBottom = [&] {
            if (isUsplittable)
                return floatBox.absoluteRectWithMargin().bottom();

            if (auto* block = dynamicDowncast<RenderBlockFlow>(renderer)) {
                if (auto firstLine = InlineIterator::firstLineBoxFor(*block))
                    return LayoutUnit { firstLine->logicalBottom() };
            }
            return 0_lu;
        }();

        auto result = lineFloatBottomMap.add(*placedByLine, floatMinimumBottom);
        if (!result.isNewEntry)
            result.iterator->value = std::max(floatMinimumBottom, result.iterator->value);
    }

    std::optional<size_t> previousPageBreakIndex;
    std::optional<LayoutRestartLine> layoutRestartLine;

    size_t widows = flow.style().widows().tryValue().value_or(0).value;
    size_t orphans = flow.style().orphans().tryValue().value_or(2).value;

    auto accumulatedOffset = 0_lu;
    for (size_t lineIndex = 0; lineIndex < lineCount;) {
        auto line = InlineIterator::lineBoxFor(inlineContent, lineIndex);
        auto floatMinimumBottom = lineFloatBottomMap.getOptional(lineIndex).value_or(0_lu);

        auto adjustment = flow.computeLineAdjustmentForPagination(line, accumulatedOffset, floatMinimumBottom);
        if (layoutRestartLine && layoutRestartLine->index == lineIndex) {
            ASSERT(!layoutRestartLine->offset);
            layoutRestartLine->offset = adjustment.strut;
        }

        if (adjustment.isFirstAfterPageBreak) {
            auto remainingLines = lineCount - lineIndex;
            // Ignore the last line if it is completely empty.
            if (inlineContent.displayContent().lines.last().lineBoxRect().isEmpty())
                remainingLines--;

            // See if there are enough lines left to meet the widow requirement.
            if (remainingLines < widows && allowLayoutRestart && !layoutRestartLine) {
                auto previousPageLineCount = lineIndex - previousPageBreakIndex.value_or(0);
                auto neededLines = widows - remainingLines;
                auto availableLines = previousPageLineCount > orphans ? previousPageLineCount - orphans : 0;
                auto linesToMove = std::min(neededLines, availableLines);
                if (linesToMove) {
                    auto breakIndex = lineIndex - linesToMove;
                    // Set the widow break and recompute the adjustments starting from that line.
                    flow.setBreakAtLineToAvoidWidow(breakIndex + 1);
                    lineIndex = breakIndex;
                    // We need to redo the layout starting from the break for things like intrusive floats.
                    layoutRestartLine = { breakIndex, { } };
                    continue;
                }
            }

            previousPageBreakIndex = lineIndex;
        }

        accumulatedOffset += adjustment.strut;

        if (adjustment.isFirstAfterPageBreak) {
            if (!lineIndex)
                accumulatedOffset += inlineContent.clearGapBeforeFirstLine();

            if (flow.style().lineSnap() != LineSnap::None && blockLayoutState.lineGrid())
                accumulatedOffset += computeFirstLineSnapAdjustment(inlineContent.displayContent().lines[lineIndex], *blockLayoutState.lineGrid());
        }

        adjustments[lineIndex] = LineAdjustment { accumulatedOffset, adjustment.isFirstAfterPageBreak };

        ++lineIndex;
    }

    flow.clearDidBreakAtLineToAvoidWidow();

    if (!previousPageBreakIndex)
        return { };

    return { adjustments, layoutRestartLine };
}

void adjustLinePositionsForPagination(InlineContent& inlineContent, const Vector<LineAdjustment>& adjustments)
{
    if (adjustments.isEmpty())
        return;

    auto writingMode = inlineContent.formattingContextRoot().style().writingMode();
    bool isHorizontalWritingMode = writingMode.isHorizontal();

    auto& displayContent = inlineContent.displayContent();
    for (size_t lineIndex = 0; lineIndex < displayContent.lines.size(); ++lineIndex) {
        auto& line = displayContent.lines[lineIndex];
        auto& adjustment = adjustments[lineIndex];
        line.moveInBlockDirection(adjustment.offset);
        if (adjustment.isFirstAfterPageBreak)
            line.setIsFirstAfterPageBreak();
    }
    for (auto& box : displayContent.boxes) {
        auto offset = adjustments[box.lineIndex()].offset;
        if (isHorizontalWritingMode)
            box.moveVertically(offset);
        else
            box.moveHorizontally(offset);
    }
}

}
}
