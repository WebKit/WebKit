/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationInlineContentBuilder.h"

#include "InlineDamage.h"
#include "InlineDisplayBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutIntegrationBoxTree.h"
#include "LayoutIntegrationInlineContent.h"
#include "LayoutState.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBoxInlines.h"
#include "RenderStyleInlines.h"
#include "StringTruncator.h"

namespace WebCore {
namespace LayoutIntegration {

inline static float endPaddingQuirkValue(const RenderBlockFlow& flow)
{
    // FIXME: It's the copy of the lets-adjust-overflow-for-the-caret behavior from LegacyLineLayout::addOverflowFromInlineChildren.
    auto endPadding = flow.hasNonVisibleOverflow() ? flow.paddingEnd() : 0_lu;
    if (!endPadding)
        endPadding = flow.endPaddingWidthForCaret();
    if (flow.hasNonVisibleOverflow() && !endPadding && flow.element() && flow.element()->isRootEditableElement() && flow.style().isLeftToRightDirection())
        endPadding = 1;
    return endPadding;
}

InlineContentBuilder::InlineContentBuilder(const RenderBlockFlow& blockFlow, BoxTree& boxTree)
    : m_blockFlow(blockFlow)
    , m_boxTree(boxTree)
{
}

FloatRect InlineContentBuilder::build(Layout::InlineLayoutResult&& layoutResult, InlineContent& inlineContent, const Layout::InlineDamage* lineDamage) const
{
    auto firstDamagedLineIndex = [&]() -> std::optional<size_t> {
        auto& displayContentFromPreviousLayout = inlineContent.displayContent();
        if (!lineDamage || !lineDamage->start() || !displayContentFromPreviousLayout.lines.size())
            return { };
        auto canidateLineIndex = lineDamage->start()->lineIndex;
        if (canidateLineIndex >= displayContentFromPreviousLayout.lines.size()) {
            ASSERT_NOT_REACHED();
            return { };
        }
        if (layoutResult.displayContent.boxes.size() && canidateLineIndex > layoutResult.displayContent.boxes[0].lineIndex()) {
            // We should never generate lines _before_ the damaged line.
            ASSERT_NOT_REACHED();
            return { };
        }
        return { canidateLineIndex };
    }();

    auto firstDamagedBoxIndex = [&]() -> std::optional<size_t> {
        auto& displayContentFromPreviousLayout = inlineContent.displayContent();
        return firstDamagedLineIndex ? std::make_optional(displayContentFromPreviousLayout.lines[*firstDamagedLineIndex].firstBoxIndex()) : std::nullopt;
    }();

    auto numberOfDamagedLines = [&]() -> std::optional<size_t> {
        if (!firstDamagedLineIndex)
            return { };
        auto& displayContentFromPreviousLayout = inlineContent.displayContent();
        ASSERT(layoutResult.range != Layout::InlineLayoutResult::Range::Full);
        auto canidateLineCount = layoutResult.range == Layout::InlineLayoutResult::Range::FullFromDamage
            ? displayContentFromPreviousLayout.lines.size() - *firstDamagedLineIndex
            : layoutResult.displayContent.lines.size();

        if (*firstDamagedLineIndex + canidateLineCount > displayContentFromPreviousLayout.lines.size()) {
            ASSERT_NOT_REACHED();
            return { };
        }
        return { canidateLineCount };
    }();

    auto numberOfDamagedBoxes = [&]() -> std::optional<size_t> {
        if (!firstDamagedLineIndex || !numberOfDamagedLines || !firstDamagedBoxIndex)
            return { };
        auto& displayContentFromPreviousLayout = inlineContent.displayContent();
        ASSERT(*firstDamagedLineIndex + *numberOfDamagedLines <= displayContentFromPreviousLayout.lines.size());
        size_t boxCount = 0;
        for (size_t i = 0; i < *numberOfDamagedLines; ++i)
            boxCount += displayContentFromPreviousLayout.lines[*firstDamagedLineIndex + i].boxCount();
        ASSERT(boxCount);
        return { boxCount };
    }();
    auto numberOfNewLines = layoutResult.displayContent.lines.size();
    auto numberOfNewBoxes = layoutResult.displayContent.boxes.size();

    auto damagedRect = FloatRect { };
    auto adjustDamagedRectWithLineRange = [&](size_t firstLineIndex, size_t lineCount, auto& lines) {
        ASSERT(firstLineIndex + lineCount <= lines.size());
        for (size_t i = 0; i < lineCount; ++i)
            damagedRect.unite(lines[firstLineIndex + i].inkOverflow());
    };

    // Repaint the damaged content boundary.
    adjustDamagedRectWithLineRange(firstDamagedLineIndex.value_or(0), numberOfDamagedLines.value_or(inlineContent.displayContent().lines.size()), inlineContent.displayContent().lines);

    inlineContent.releaseCaches();

    switch (layoutResult.range) {
    case Layout::InlineLayoutResult::Range::Full:
        inlineContent.displayContent().set(WTFMove(layoutResult.displayContent));
        break;
    case Layout::InlineLayoutResult::Range::FullFromDamage: {
        if (!firstDamagedLineIndex || !numberOfDamagedLines || !firstDamagedBoxIndex || !numberOfDamagedBoxes) {
            // FIXME: Not sure if inlineContent::set or silent failing is what we should do here.
            break;
        }
        auto& displayContent = inlineContent.displayContent();
        displayContent.remove(*firstDamagedLineIndex, *numberOfDamagedLines, *firstDamagedBoxIndex, *numberOfDamagedBoxes);
        displayContent.append(WTFMove(layoutResult.displayContent));
        break;
    }
    case Layout::InlineLayoutResult::Range::PartialFromDamage: {
        if (!firstDamagedLineIndex || !numberOfDamagedLines || !firstDamagedBoxIndex || !numberOfDamagedBoxes) {
            // FIXME: Not sure if inlineContent::set or silent failing is what we should do here.
            break;
        }
        auto& displayContent = inlineContent.displayContent();
        displayContent.remove(*firstDamagedLineIndex, *numberOfDamagedLines, *firstDamagedBoxIndex, *numberOfDamagedBoxes);
        displayContent.insert(WTFMove(layoutResult.displayContent), *firstDamagedLineIndex, *firstDamagedBoxIndex);

        auto adjustCachedBoxIndexesIfNeeded = [&] {
            if (numberOfNewBoxes == *numberOfDamagedBoxes)
                return;
            auto firstCleanLineIndex = *firstDamagedLineIndex + *numberOfDamagedLines;
            auto offset = numberOfNewBoxes - *numberOfDamagedBoxes;
            auto& lines = displayContent.lines;
            for (size_t cleanLineIndex = firstCleanLineIndex; cleanLineIndex < lines.size(); ++cleanLineIndex) {
                ASSERT(lines[cleanLineIndex].firstBoxIndex() + offset > 0);
                auto adjustedFirstBoxIndex = std::max<size_t>(0, lines[cleanLineIndex].firstBoxIndex() + offset);
                lines[cleanLineIndex].setFirstBoxIndex(adjustedFirstBoxIndex);
            }
        };
        adjustCachedBoxIndexesIfNeeded();
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    auto updateIfTextRenderersNeedVisualReordering = [&] {
        // FIXME: We may want to have a global, "is this a bidi paragraph" flag to avoid this loop for non-rtl, non-bidi content. 
        for (auto& displayBox : inlineContent.displayContent().boxes) {
            auto& layoutBox = displayBox.layoutBox();
            if (!is<Layout::InlineTextBox>(layoutBox))
                continue;
            if (displayBox.bidiLevel() != UBIDI_DEFAULT_LTR) 
                downcast<RenderText>(m_boxTree.rendererForLayoutBox(layoutBox)).setNeedsVisualReordering();
        }
    };
    updateIfTextRenderersNeedVisualReordering();
    adjustDisplayLines(inlineContent);
    // Repaint the new content boundary.
    adjustDamagedRectWithLineRange(firstDamagedLineIndex.value_or(0), numberOfNewLines, inlineContent.displayContent().lines);
    computeIsFirstIsLastBoxForInlineContent(inlineContent);

    return damagedRect;
}

void InlineContentBuilder::updateLineOverflow(InlineContent& inlineContent) const
{
    adjustDisplayLines(inlineContent);
}

void InlineContentBuilder::adjustDisplayLines(InlineContent& inlineContent) const
{
    auto& lines = inlineContent.displayContent().lines;
    auto& boxes = inlineContent.displayContent().boxes;

    size_t boxIndex = 0;
    auto& rootBoxStyle = m_blockFlow.style();
    auto isLeftToRightInlineDirection = rootBoxStyle.isLeftToRightDirection();
    auto isHorizontalWritingMode = rootBoxStyle.isHorizontalWritingMode();

    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        auto& line = lines[lineIndex];
        auto scrollableOverflowRect = line.contentOverflow();
        auto adjustOverflowLogicalWidthWithBlockFlowQuirk = [&] {
            auto scrollableOverflowLogicalWidth = isHorizontalWritingMode ? scrollableOverflowRect.width() : scrollableOverflowRect.height();
            if (!isLeftToRightInlineDirection && line.contentLogicalWidth() > scrollableOverflowLogicalWidth) {
                // The only time when scrollable overflow here could be shorter than
                // the content width is when hanging RTL trailing content is applied (and ignored as scrollable overflow. See LineBoxBuilder::build.
                return;
            }
            auto adjustedOverflowLogicalWidth = line.contentLogicalWidth() + endPaddingQuirkValue(m_blockFlow);
            if (adjustedOverflowLogicalWidth > scrollableOverflowLogicalWidth) {
                auto overflowValue = adjustedOverflowLogicalWidth - scrollableOverflowLogicalWidth;
                if (isHorizontalWritingMode)
                    isLeftToRightInlineDirection ? scrollableOverflowRect.shiftMaxXEdgeBy(overflowValue) : scrollableOverflowRect.shiftXEdgeBy(-overflowValue);
                else
                    isLeftToRightInlineDirection ? scrollableOverflowRect.shiftMaxYEdgeBy(overflowValue) : scrollableOverflowRect.shiftYEdgeBy(-overflowValue);
            }
        };
        adjustOverflowLogicalWidthWithBlockFlowQuirk();

        auto firstBoxIndex = boxIndex;
        auto inkOverflowRect = scrollableOverflowRect;
        // Collect overflow from boxes.
        // Note while we compute ink overflow for all type of boxes including atomic inline level boxes (e.g. <iframe> <img>) as part of constructing
        // display boxes (see InlineDisplayContentBuilder) RenderBlockFlow expects visual overflow.
        // Visual overflow propagation is slightly different from ink overflow when it comes to renderers with self painting layers.
        // -and for now we consult atomic renderers for such visual overflow which is not how we are supposed to do in LFC.
        // (visual overflow is computed during their ::layout() call which we issue right before running inline layout in RenderBlockFlow::layoutModernLines)
        for (; boxIndex < boxes.size() && boxes[boxIndex].lineIndex() == lineIndex; ++boxIndex) {
            auto& box = boxes[boxIndex];
            if (box.isRootInlineBox() || box.isEllipsis() || box.isLineBreak())
                continue;

            if (box.isText()) {
                inkOverflowRect.unite(box.inkOverflow());
                continue;
            }

            if (box.isAtomicInlineLevelBox()) {
                auto& renderer = downcast<RenderBox>(m_boxTree.rendererForLayoutBox(box.layoutBox()));
                if (!renderer.hasSelfPaintingLayer()) {
                    auto childInkOverflow = renderer.logicalVisualOverflowRectForPropagation(&renderer.parent()->style());
                    childInkOverflow.move(box.left(), box.top());
                    inkOverflowRect.unite(childInkOverflow);
                }
                auto childScrollableOverflow = renderer.layoutOverflowRectForPropagation(&renderer.parent()->style());
                childScrollableOverflow.move(box.left(), box.top());
                scrollableOverflowRect.unite(childScrollableOverflow);
                continue;
            }

            if (box.isInlineBox()) {
                if (!downcast<RenderElement>(m_boxTree.rendererForLayoutBox(box.layoutBox())).hasSelfPaintingLayer())
                    inkOverflowRect.unite(box.inkOverflow());
            }
        }

        line.setScrollableOverflow(scrollableOverflowRect);
        line.setInkOverflow(inkOverflowRect);
        line.setFirstBoxIndex(firstBoxIndex);
        line.setBoxCount(boxIndex - firstBoxIndex);

        if (lineIndex) {
            auto& lastInkOverflow = lines[lineIndex - 1].inkOverflow();
            if (inkOverflowRect.y() <= lastInkOverflow.y() || lastInkOverflow.maxY() >= inkOverflowRect.maxY())
                inlineContent.hasMultilinePaintOverlap = true;
        }
        if (!inlineContent.hasVisualOverflow() && inkOverflowRect != scrollableOverflowRect)
            inlineContent.setHasVisualOverflow();
    }
}

void InlineContentBuilder::computeIsFirstIsLastBoxForInlineContent(InlineContent& inlineContent) const
{
    auto& boxes = inlineContent.displayContent().boxes;
    if (boxes.isEmpty()) {
        // Line clamp may produce a completely empty IFC.
        return;
    }

    HashMap<const Layout::Box*, size_t> lastDisplayBoxForLayoutBoxIndexes;
    lastDisplayBoxForLayoutBoxIndexes.reserveInitialCapacity(boxes.size() - 1);

    ASSERT(boxes[0].isRootInlineBox());
    boxes[0].setIsFirstForLayoutBox(true);
    size_t lastRootInlineBoxIndex = 0;

    for (size_t index = 1; index < boxes.size(); ++index) {
        auto& displayBox = boxes[index];
        if (displayBox.isRootInlineBox()) {
            lastRootInlineBoxIndex = index;
            continue;
        }
        auto& layoutBox = displayBox.layoutBox();
        if (lastDisplayBoxForLayoutBoxIndexes.set(&layoutBox, index).isNewEntry)
            displayBox.setIsFirstForLayoutBox(true);
    }
    for (auto index : lastDisplayBoxForLayoutBoxIndexes.values())
        boxes[index].setIsLastForLayoutBox(true);

    boxes[lastRootInlineBoxIndex].setIsLastForLayoutBox(true);
}

}
}

