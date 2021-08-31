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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "InlineFormattingState.h"
#include "LayoutBoxGeometry.h"
#include "LayoutIntegrationBoxTree.h"
#include "LayoutIntegrationInlineContent.h"
#include "LayoutIntegrationRun.h"
#include "LayoutReplacedBox.h"
#include "LayoutState.h"
#include "RenderBlockFlow.h"
#include "StringTruncator.h"

namespace WebCore {
namespace LayoutIntegration {

inline Layout::LineGeometry::EnclosingTopAndBottom operator+(const Layout::LineGeometry::EnclosingTopAndBottom enclosingTopAndBottom, float offset)
{
    return { enclosingTopAndBottom.top + offset, enclosingTopAndBottom.bottom + offset };
}

inline static float lineOverflowWidth(const RenderBlockFlow& flow, Layout::InlineLayoutUnit lineBoxLogicalWidth, Layout::InlineLayoutUnit lineContentLogicalWidth)
{
    // FIXME: It's the copy of the lets-adjust-overflow-for-the-caret behavior from LegacyLineLayout::addOverflowFromInlineChildren.
    auto endPadding = flow.hasNonVisibleOverflow() ? flow.paddingEnd() : 0_lu;
    if (!endPadding)
        endPadding = flow.endPaddingWidthForCaret();
    if (flow.hasNonVisibleOverflow() && !endPadding && flow.element() && flow.element()->isRootEditableElement())
        endPadding = 1;
    lineContentLogicalWidth += endPadding;
    return std::max(lineBoxLogicalWidth, lineContentLogicalWidth);
}

InlineContentBuilder::InlineContentBuilder(const Layout::LayoutState& layoutState, const RenderBlockFlow& blockFlow, const BoxTree& boxTree)
    : m_layoutState(layoutState)
    , m_blockFlow(blockFlow)
    , m_boxTree(boxTree)
{
}

void InlineContentBuilder::build(const Layout::InlineFormattingState& inlineFormattingState, InlineContent& inlineContent) const
{
    createDisplayRuns(inlineFormattingState.runs(), inlineContent);
    createDisplayLines(inlineFormattingState.lines(), inlineContent);
}

void InlineContentBuilder::createDisplayRuns(const Layout::InlineRuns& runs, InlineContent& inlineContent) const
{
    if (runs.isEmpty())
        return;

    inlineContent.runs.reserveInitialCapacity(runs.size());
    for (auto& run : runs) {
        if (auto text = run.text()) {
            inlineContent.runs.append({ run.lineIndex(), run.layoutBox(), run.logicalRect(), run.inkOverflow(), Run::Expansion { run.expansion().behavior, run.expansion().horizontalExpansion }
                , Run::Text { text->start(), text->length(), text->originalContent(), text->renderedContent(), text->hasHyphen() } });
        } else if (!run.isRootInlineBox()) {
            // FIXME: Teach the run iterators to ignore the root inline box runs.
            auto& geometry = m_layoutState.geometryForBox(run.layoutBox());
            if (!run.isInlineBox() || !run.isLineSpanning()) {
                // FIXME: Run iterators with (text)runs spanning over multiple lines expect no "in-between" runs (e.g. line spanning or root inline boxes).
                inlineContent.runs.append({ run.lineIndex(), run.layoutBox(), run.logicalRect(), run.inkOverflow(), { }, { } });
            }
            if (run.isInlineBox()) {
                auto hasScrollableContent = [&] {
                    // In standards mode, inline boxes always start with an imaginary strut.
                    return m_layoutState.inStandardsMode() || run.hasContent() || geometry.horizontalBorder() || (geometry.horizontalPadding() && geometry.horizontalPadding().value());
                };
                inlineContent.nonRootInlineBoxes.append({ run.lineIndex(), run.layoutBox(), run.logicalRect(), hasScrollableContent() });
            }
        }
    }
}

void InlineContentBuilder::createDisplayLines(const Layout::InlineLines& lines, InlineContent& inlineContent) const
{
    auto& runs = inlineContent.runs;
    auto& nonRootInlineBoxes = inlineContent.nonRootInlineBoxes;
    size_t runIndex = 0;
    size_t inlineBoxIndex = 0;
    inlineContent.lines.reserveInitialCapacity(lines.size());
    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        auto& line = lines[lineIndex];
        auto& lineBoxLogicalRect = line.lineBoxLogicalRect();
        // FIXME: This is where the logical to physical translate should happen.
        auto scrollableOverflowRect = FloatRect { lineBoxLogicalRect.left(), lineBoxLogicalRect.top(), lineOverflowWidth(m_blockFlow, lineBoxLogicalRect.width(), line.contentLogicalWidth()), lineBoxLogicalRect.height() };

        auto firstRunIndex = runIndex;
        auto lineInkOverflowRect = scrollableOverflowRect;
        // Collect overflow from runs.
        for (; runIndex < runs.size() && runs[runIndex].lineIndex() == lineIndex; ++runIndex) {
            auto& run = runs[runIndex];
            if (line.needsIntegralPosition())
                run.setVerticalPositionIntegral();

            lineInkOverflowRect.unite(run.inkOverflow());

            auto& layoutBox = run.layoutBox();
            if (!layoutBox.isReplacedBox())
                continue;

            // Similar to LegacyInlineFlowBox::addReplacedChildOverflow.
            auto& box = downcast<RenderBox>(m_boxTree.rendererForLayoutBox(layoutBox));
            auto runLogicalRect = run.logicalRect();
            if (!box.hasSelfPaintingLayer()) {
                auto childInkOverflow = box.logicalVisualOverflowRectForPropagation(&box.parent()->style());
                childInkOverflow.move(runLogicalRect.x(), runLogicalRect.y());
                lineInkOverflowRect.unite(childInkOverflow);
            }
            auto childScrollableOverflow = box.logicalLayoutOverflowRectForPropagation(&box.parent()->style());
            childScrollableOverflow.move(runLogicalRect.x(), runLogicalRect.y());
            scrollableOverflowRect.unite(childScrollableOverflow);
        }
        // Collect scrollable overflow from inline boxes. All other inline level boxes (e.g atomic inline level boxes) stretch the line.
        while (inlineBoxIndex < nonRootInlineBoxes.size() && nonRootInlineBoxes[inlineBoxIndex].lineIndex() == lineIndex) {
            auto& inlineBox = nonRootInlineBoxes[inlineBoxIndex++];

            if (line.needsIntegralPosition())
                inlineBox.setVerticalPositionIntegral();

            if (inlineBox.hasScrollableContent())
                scrollableOverflowRect.unite(inlineBox.rect());
        }

        auto adjustedLineBoxRect = FloatRect { lineBoxLogicalRect };
        // Final enclosing top and bottom values are in the same coordinate space as the line itself.
        auto enclosingTopAndBottom = line.enclosingTopAndBottom() + lineBoxLogicalRect.top();
        if (line.needsIntegralPosition()) {
            adjustedLineBoxRect.setY(roundToInt(adjustedLineBoxRect.y()));
            enclosingTopAndBottom.top = roundToInt(enclosingTopAndBottom.top);
            enclosingTopAndBottom.bottom = roundToInt(enclosingTopAndBottom.bottom);
        }
        auto runCount = runIndex - firstRunIndex;
        inlineContent.lines.append({ firstRunIndex, runCount, adjustedLineBoxRect, enclosingTopAndBottom.top, enclosingTopAndBottom.bottom, scrollableOverflowRect, lineInkOverflowRect, line.baseline(), line.contentLogicalLeft(), line.contentLogicalWidth() });
    }
}

}
}

#endif
