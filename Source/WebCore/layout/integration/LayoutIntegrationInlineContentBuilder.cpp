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

#include "InlineFormattingContext.h"
#include "InlineFormattingState.h"
#include "LayoutBoxGeometry.h"
#include "LayoutIntegrationInlineContent.h"
#include "LayoutIntegrationRun.h"
#include "LayoutReplacedBox.h"
#include "LayoutState.h"
#include "RenderBlockFlow.h"
#include "StringTruncator.h"

namespace WebCore {
namespace LayoutIntegration {

struct LineLevelVisualAdjustmentsForRuns {
    bool needsIntegralPosition { false };
    // It's only 'text-overflow: ellipsis' for now.
    bool needsTrailingContentReplacement { false };
};

inline static float lineOverflowWidth(const RenderBlockFlow& flow, InlineLayoutUnit lineLogicalWidth, InlineLayoutUnit lineBoxLogicalWidth)
{
    // FIXME: It's the copy of the lets-adjust-overflow-for-the-caret behavior from ComplexLineLayout::addOverflowFromInlineChildren.
    auto endPadding = flow.hasOverflowClip() ? flow.paddingEnd() : 0_lu;
    if (!endPadding)
        endPadding = flow.endPaddingWidthForCaret();
    if (flow.hasOverflowClip() && !endPadding && flow.element() && flow.element()->isRootEditableElement())
        endPadding = 1;
    lineBoxLogicalWidth += endPadding;
    return std::max(lineLogicalWidth, lineBoxLogicalWidth);
}

InlineContentBuilder::InlineContentBuilder(const Layout::LayoutState& layoutState, const RenderBlockFlow& blockFlow)
    : m_layoutState(layoutState)
    , m_blockFlow(blockFlow)
{
}

void InlineContentBuilder::build(InlineContent& inlineContent, const Layout::InlineFormattingState& inlineFormattingState) const
{
    auto lineLevelVisualAdjustmentsForRuns = computeLineLevelVisualAdjustmentsForRuns(inlineFormattingState);
    constructDisplayLineRuns(inlineContent, inlineFormattingState, lineLevelVisualAdjustmentsForRuns);
    constructDisplayLines(inlineContent, inlineFormattingState, lineLevelVisualAdjustmentsForRuns);
}

InlineContentBuilder::LineLevelVisualAdjustmentsForRunsList InlineContentBuilder::computeLineLevelVisualAdjustmentsForRuns(const Layout::InlineFormattingState& inlineFormattingState) const
{
    auto& lines = inlineFormattingState.lines();
    auto& rootStyle = m_layoutState.root().style();
    auto shouldCheckHorizontalOverflowForContentReplacement = rootStyle.overflowX() == Overflow::Hidden && rootStyle.textOverflow() != TextOverflow::Clip;

    LineLevelVisualAdjustmentsForRunsList lineLevelVisualAdjustmentsForRuns(lines.size());
    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        auto lineNeedsLegacyIntegralVerticalPosition = [&] {
            // InlineTree rounds y position to integral value for certain content (see InlineFlowBox::placeBoxesInBlockDirection).
            auto inlineLevelBoxList = inlineFormattingState.lineBoxes()[lineIndex].inlineLevelBoxList();
            if (inlineLevelBoxList.size() == 1) {
                // This is text content only with root inline box.
                return true;
            }
            // Text + <br> (or just <br> or text<span></span><br>) behaves like text.
            for (auto& inlineLevelBox : inlineLevelBoxList) {
                if (inlineLevelBox->isAtomicInlineLevelBox()) {
                    // Content like text<img> prevents legacy snapping.
                    return false;
                }
            }
            return true;
        };
        lineLevelVisualAdjustmentsForRuns[lineIndex].needsIntegralPosition = lineNeedsLegacyIntegralVerticalPosition();
        if (shouldCheckHorizontalOverflowForContentReplacement) {
            auto& line = lines[lineIndex];
            auto overflowWidth = lineOverflowWidth(m_blockFlow, line.logicalWidth(), line.lineBoxLogicalSize().width());
            lineLevelVisualAdjustmentsForRuns[lineIndex].needsTrailingContentReplacement = overflowWidth > line.logicalWidth();
        }
    }
    return lineLevelVisualAdjustmentsForRuns;
}

void InlineContentBuilder::constructDisplayLineRuns(InlineContent& inlineContent, const Layout::InlineFormattingState& inlineFormattingState, const LineLevelVisualAdjustmentsForRunsList& lineLevelVisualAdjustmentsForRuns) const
{
    auto& lines = inlineFormattingState.lines();
    auto initialContaingBlockSize = m_layoutState.viewportSize();
    Vector<bool> hasAdjustedTrailingLineList(lines.size(), false);
    for (auto& lineRun : inlineFormattingState.lineRuns()) {
        auto& layoutBox = lineRun.layoutBox();
        auto& style = layoutBox.style();
        auto computedInkOverflow = [&] (auto runRect) {
            // FIXME: Add support for non-text ink overflow.
            if (!lineRun.text())
                return runRect;
            auto inkOverflow = runRect;
            auto strokeOverflow = std::ceil(style.computedStrokeWidth(ceiledIntSize(initialContaingBlockSize)));
            inkOverflow.inflate(strokeOverflow);

            auto letterSpacing = style.fontCascade().letterSpacing();
            if (letterSpacing < 0) {
                // Last letter's negative spacing shrinks logical rect. Push it to ink overflow.
                inkOverflow.expand(-letterSpacing, { });
            }
            return inkOverflow;
        };
        auto runRect = FloatRect { lineRun.logicalRect() };
        // Inline boxes are relative to the line box while final Runs need to be relative to the parent Box
        // FIXME: Shouldn't we just leave them be relative to the line box?
        auto lineIndex = lineRun.lineIndex();
        auto& line = lines[lineIndex];
        runRect.moveBy({ line.logicalLeft(), line.logicalTop() });
        if (lineLevelVisualAdjustmentsForRuns[lineIndex].needsIntegralPosition)
            runRect.setY(roundToInt(runRect.y()));

        WTF::Optional<Run::TextContent> textContent;
        if (auto text = lineRun.text()) {
            auto adjustedContentToRenderer = [&] {
                auto originalContent = text->content().substring(text->start(), text->length());
                if (text->needsHyphen())
                    return makeString(originalContent, style.hyphenString());
                if (lineLevelVisualAdjustmentsForRuns[lineIndex].needsTrailingContentReplacement) {
                    // Currently it's ellipsis replacement only, but adding support for "text-overflow: string" should be relatively simple.
                    if (hasAdjustedTrailingLineList[lineIndex]) {
                        // This line already has adjusted trailing. Any runs after the ellipsis should render blank.
                        return emptyString();
                    }
                    auto runLogicalRect = lineRun.logicalRect();
                    auto lineLogicalRight = line.logicalRight();
                    auto ellipsisWidth = style.fontCascade().width(WebCore::TextRun { &horizontalEllipsis });
                    if (runLogicalRect.right() + ellipsisWidth > lineLogicalRight) {
                        // The next run with ellipsis would surely overflow. So let's just add it to this run even if
                        // it makes the run wider than it originally was.
                        hasAdjustedTrailingLineList[lineIndex] = true;
                        float resultWidth = 0;
                        auto maxWidth = line.logicalWidth() - runLogicalRect.left();
                        return StringTruncator::rightTruncate(originalContent, maxWidth, style.fontCascade(), resultWidth, true);
                    }
                }
                return String();
            };
            textContent = Run::TextContent { text->start(), text->length(), text->content(), adjustedContentToRenderer(), text->needsHyphen() };
        }
        auto expansion = Run::Expansion { lineRun.expansion().behavior, lineRun.expansion().horizontalExpansion };
        auto displayRun = Run { lineIndex, layoutBox, runRect, computedInkOverflow(runRect), expansion, textContent };
        inlineContent.runs.append(displayRun);
    }
}

void InlineContentBuilder::constructDisplayLines(InlineContent& inlineContent, const Layout::InlineFormattingState& inlineFormattingState, const LineLevelVisualAdjustmentsForRunsList& lineLevelVisualAdjustmentsForRuns) const
{
    auto& lines = inlineFormattingState.lines();
    auto& lineBoxes = inlineFormattingState.lineBoxes();
    auto& runs = inlineContent.runs;
    size_t runIndex = 0;
    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        auto& line = lines[lineIndex];
        auto lineBoxLogicalSize = line.lineBoxLogicalSize();
        // FIXME: This is where the logical to physical translate should happen.
        auto scrollableOverflowRect = FloatRect { line.logicalLeft(), line.logicalTop(), lineOverflowWidth(m_blockFlow, line.logicalWidth(), lineBoxLogicalSize.width()), line.logicalHeight() };

        auto firstRunIndex = runIndex;
        auto lineInkOverflowRect = scrollableOverflowRect;
        while (runIndex < runs.size() && runs[runIndex].lineIndex() == lineIndex)
            lineInkOverflowRect.unite(runs[runIndex++].inkOverflow());
        auto runCount = runIndex - firstRunIndex;
        auto lineRect = FloatRect { line.logicalRect() };
        auto enclosingContentRect = [&] {
            // Let's (vertically)enclose all the inline level boxes.
            // This mostly matches 'lineRect', unless line-height triggers line box overflow (not to be confused with ink or scroll overflow).
            auto enclosingRect = FloatRect { lineRect.location(), lineBoxLogicalSize };
            auto& lineBox = lineBoxes[lineIndex];
            for (auto& inlineLevelBox : lineBox.inlineLevelBoxList()) {
                auto inlineLevelBoxLogicalRect = lineBox.logicalRectForInlineLevelBox(inlineLevelBox->layoutBox());
                // inlineLevelBoxLogicalRect is relative to the line.
                inlineLevelBoxLogicalRect.moveBy(lineRect.location());
                enclosingRect.setY(std::min(enclosingRect.y(), inlineLevelBoxLogicalRect.top()));
                enclosingRect.shiftMaxYEdgeTo(std::max(enclosingRect.maxY(), inlineLevelBoxLogicalRect.bottom()));
            }
            return enclosingRect;
        }();
        if (lineLevelVisualAdjustmentsForRuns[lineIndex].needsIntegralPosition) {
            lineRect.setY(roundToInt(lineRect.y()));
            enclosingContentRect.setY(roundToInt(enclosingContentRect.y()));
        }
        inlineContent.lines.append({ firstRunIndex, runCount, lineRect, lineBoxLogicalSize.width(), enclosingContentRect, scrollableOverflowRect, lineInkOverflowRect, line.baseline(), line.horizontalAlignmentOffset() });
    }
}

}
}

#endif
