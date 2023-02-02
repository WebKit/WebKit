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

#include "InlineDisplayBox.h"
#include "InlineFormattingState.h"
#include "LayoutBoxGeometry.h"
#include "LayoutIntegrationBoxTree.h"
#include "LayoutIntegrationInlineContent.h"
#include "LayoutState.h"
#include "RenderBlockFlow.h"
#include "StringTruncator.h"

namespace WebCore {
namespace LayoutIntegration {

inline static float lineOverflowLogicalWidth(const RenderBlockFlow& flow, Layout::InlineLayoutUnit lineContentLogicalWidth)
{
    // FIXME: It's the copy of the lets-adjust-overflow-for-the-caret behavior from LegacyLineLayout::addOverflowFromInlineChildren.
    auto endPadding = flow.hasNonVisibleOverflow() ? flow.paddingEnd() : 0_lu;
    if (!endPadding)
        endPadding = flow.endPaddingWidthForCaret();
    if (flow.hasNonVisibleOverflow() && !endPadding && flow.element() && flow.element()->isRootEditableElement() && flow.style().isLeftToRightDirection())
        endPadding = 1;
    return lineContentLogicalWidth + endPadding;
}

InlineContentBuilder::InlineContentBuilder(const RenderBlockFlow& blockFlow, BoxTree& boxTree)
    : m_blockFlow(blockFlow)
    , m_boxTree(boxTree)
{
}

void InlineContentBuilder::build(Layout::InlineFormattingState& inlineFormattingState, InlineContent& inlineContent) const
{
    // FIXME: This might need a different approach with partial layout where the layout code needs to know about the boxes.
    inlineContent.boxes = WTFMove(inlineFormattingState.boxes());

    auto updateIfTextRenderersNeedVisualReordering = [&] {
        // FIXME: We may want to have a global, "is this a bidi paragraph" flag to avoid this loop for non-rtl, non-bidi content. 
        for (auto& displayBox : inlineContent.boxes) {
            auto& layoutBox = displayBox.layoutBox();
            if (!is<Layout::InlineTextBox>(layoutBox))
                continue;
            if (displayBox.bidiLevel() != UBIDI_DEFAULT_LTR) 
                downcast<RenderText>(m_boxTree.rendererForLayoutBox(layoutBox)).setNeedsVisualReordering();
        }
    };
    updateIfTextRenderersNeedVisualReordering();
    createDisplayLines(inlineFormattingState, inlineContent);
}

void InlineContentBuilder::updateLineOverflow(Layout::InlineFormattingState& inlineFormattingState, InlineContent& inlineContent) const
{
    inlineContent.lines.clear();
    createDisplayLines(inlineFormattingState, inlineContent);
}

void InlineContentBuilder::createDisplayLines(Layout::InlineFormattingState& inlineFormattingState, InlineContent& inlineContent) const
{
    auto& lines = inlineFormattingState.lines();
    auto& boxes = inlineContent.boxes;
    size_t boxIndex = 0;
    inlineContent.lines.reserveInitialCapacity(lines.size());

    auto& rootBoxStyle = m_blockFlow.style();
    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        auto& line = lines[lineIndex];
        auto scrollableOverflowRect = line.scrollableOverflow();

        auto adjustOverflowLogicalWidthWithBlockFlowQuirk = [&] {
            auto isHorizontalWritingMode = rootBoxStyle.isHorizontalWritingMode();
            auto adjustedOverflowLogicalWidth = lineOverflowLogicalWidth(m_blockFlow, line.contentLogicalWidth());
            auto scrollableOverflowLogicalWidth = isHorizontalWritingMode ? scrollableOverflowRect.width() : scrollableOverflowRect.height();
            if (adjustedOverflowLogicalWidth > scrollableOverflowLogicalWidth) {
                auto overflowValue = adjustedOverflowLogicalWidth - scrollableOverflowLogicalWidth;
                if (isHorizontalWritingMode)
                    rootBoxStyle.isLeftToRightDirection() ? scrollableOverflowRect.shiftMaxXEdgeBy(overflowValue) : scrollableOverflowRect.shiftXEdgeBy(-overflowValue);
                else
                    rootBoxStyle.isLeftToRightDirection() ? scrollableOverflowRect.shiftMaxYEdgeBy(overflowValue) : scrollableOverflowRect.shiftYEdgeBy(-overflowValue);
            }
        };
        adjustOverflowLogicalWidthWithBlockFlowQuirk();

        auto firstBoxIndex = boxIndex;
        auto lineInkOverflowRect = scrollableOverflowRect;
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
                lineInkOverflowRect.unite(box.inkOverflow());
                continue;
            }

            if (box.isAtomicInlineLevelBox()) {
                auto& renderer = downcast<RenderBox>(m_boxTree.rendererForLayoutBox(box.layoutBox()));
                if (!renderer.hasSelfPaintingLayer()) {
                    auto childInkOverflow = renderer.logicalVisualOverflowRectForPropagation(&renderer.parent()->style());
                    childInkOverflow.move(box.left(), box.top());
                    lineInkOverflowRect.unite(childInkOverflow);
                }
                auto childScrollableOverflow = renderer.layoutOverflowRectForPropagation(&renderer.parent()->style());
                childScrollableOverflow.move(box.left(), box.top());
                scrollableOverflowRect.unite(childScrollableOverflow);
                continue;
            }

            if (box.isInlineBox()) {
                if (!downcast<RenderElement>(m_boxTree.rendererForLayoutBox(box.layoutBox())).hasSelfPaintingLayer())
                    lineInkOverflowRect.unite(box.inkOverflow());
            }
        }

        if (!inlineContent.lines.isEmpty()) {
            auto& lastInkOverflow = inlineContent.lines.last().inkOverflow();
            if (lineInkOverflowRect.y() <= lastInkOverflow.y() || lastInkOverflow.maxY() >= lineInkOverflowRect.maxY())
                inlineContent.hasMultilinePaintOverlap = true;
        }

        auto boxCount = boxIndex - firstBoxIndex;
        if (!inlineContent.hasVisualOverflow() && lineInkOverflowRect != line.scrollableOverflow())
            inlineContent.setHasVisualOverflow();
        auto ellipsis = std::optional<Line::Ellipsis> { };
        if (auto ellipsisVisualRect = line.ellipsisVisualRect())
            ellipsis = { *ellipsisVisualRect, rootBoxStyle.isLeftToRightDirection() };
        inlineContent.lines.append({ firstBoxIndex, boxCount, line.lineBoxLogicalRect(), line.lineBoxRect(), line.enclosingLogicalTopAndBottom().top, line.enclosingLogicalTopAndBottom().bottom, scrollableOverflowRect, lineInkOverflowRect, line.baseline(), line.baselineType(), line.contentLogicalLeft(), line.contentLogicalLeftIgnoringInlineDirection(), line.contentLogicalWidth(), line.isHorizontal(), ellipsis });
    }
}

}
}

