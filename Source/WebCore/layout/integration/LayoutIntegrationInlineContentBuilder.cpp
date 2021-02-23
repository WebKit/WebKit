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

#include "BidiResolver.h"
#include "InlineFormattingContext.h"
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

#define PROCESS_BIDI_CONTENT 0

struct LineLevelVisualAdjustmentsForRuns {
    bool needsIntegralPosition { false };
    // It's only 'text-overflow: ellipsis' for now.
    bool needsTrailingContentReplacement { false };
};

inline Layout::InlineLineGeometry::EnclosingTopAndBottom operator+(const Layout::InlineLineGeometry::EnclosingTopAndBottom enclosingTopAndBottom, float offset)
{
    return { enclosingTopAndBottom.top + offset, enclosingTopAndBottom.bottom + offset };
}

inline static float lineOverflowWidth(const RenderBlockFlow& flow, InlineLayoutUnit lineBoxLogicalWidth, InlineLayoutUnit lineContentLogicalWidth)
{
    // FIXME: It's the copy of the lets-adjust-overflow-for-the-caret behavior from ComplexLineLayout::addOverflowFromInlineChildren.
    auto endPadding = flow.hasOverflowClip() ? flow.paddingEnd() : 0_lu;
    if (!endPadding)
        endPadding = flow.endPaddingWidthForCaret();
    if (flow.hasOverflowClip() && !endPadding && flow.element() && flow.element()->isRootEditableElement())
        endPadding = 1;
    lineContentLogicalWidth += endPadding;
    return std::max(lineBoxLogicalWidth, lineContentLogicalWidth);
}

#if PROCESS_BIDI_CONTENT
class Iterator {
public:
    Iterator() = default;
    Iterator(const Layout::InlineLineRuns* runList, size_t currentRunIndex);

    void increment();
    unsigned offset() const { return m_offset; }
    UCharDirection direction() const;

    bool operator==(const Iterator& other) const { return offset() == other.offset(); }
    bool operator!=(const Iterator& other) const { return offset() != other.offset(); };
    bool atEnd() const { return !m_runList || m_runIndex == m_runList->size(); };

private:
    const Layout::LineRun& currentRun() const { return m_runList->at(m_runIndex); }

    const Layout::InlineLineRuns* m_runList { nullptr };
    size_t m_offset { 0 };
    size_t m_runIndex { 0 };
    size_t m_runOffset { 0 };
};

Iterator::Iterator(const Layout::InlineLineRuns* runList, size_t runIndex)
    : m_runList(runList)
    , m_runIndex(runIndex)
{
}

UCharDirection Iterator::direction() const
{
    ASSERT(m_runList);
    ASSERT(!atEnd());
    auto& textContent = currentRun().text();
    if (!textContent)
        return U_OTHER_NEUTRAL;
    return u_charDirection(textContent->content()[textContent->start() + m_runOffset]);
}

void Iterator::increment()
{
    ASSERT(m_runList);
    ASSERT(!atEnd());
    ++m_offset;
    auto& currentRun = this->currentRun();
    if (auto& textContent = currentRun.text()) {
        if (++m_runOffset < textContent->length())
            return;
    }
    ++m_runIndex;
    m_runOffset = 0;
}

class BidiRun {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BidiRun(unsigned start, unsigned end, BidiContext*, UCharDirection);

    size_t start() const { return m_start; }
    size_t end() const { return m_end; }
    unsigned char level() const { return m_level; }

    BidiRun* next() const { return m_next.get(); }
    void setNext(std::unique_ptr<BidiRun>&& next) { m_next = WTFMove(next); }
    std::unique_ptr<BidiRun> takeNext() { return WTFMove(m_next); }

private:
    std::unique_ptr<BidiRun> m_next;
    size_t m_start { 0 };
    size_t m_end { 0 };
    unsigned char m_level { 0 };
};

BidiRun::BidiRun(unsigned start, unsigned end, BidiContext* context, UCharDirection direction)
    : m_start(start)
    , m_end(end)
    , m_level(context->level())
{
    ASSERT(context);
    if (direction == U_OTHER_NEUTRAL)
        direction = context->dir();
    if (m_level % 2)
        m_level = (direction == U_LEFT_TO_RIGHT || direction == U_ARABIC_NUMBER || direction == U_EUROPEAN_NUMBER) ? m_level + 1 : m_level;
    else
        m_level = (direction == U_RIGHT_TO_LEFT) ? m_level + 1 : (direction == U_ARABIC_NUMBER || direction == U_EUROPEAN_NUMBER) ? m_level + 2 : m_level;
}
#endif

InlineContentBuilder::InlineContentBuilder(const Layout::LayoutState& layoutState, const RenderBlockFlow& blockFlow, const BoxTree& boxTree)
    : m_layoutState(layoutState)
    , m_blockFlow(blockFlow)
    , m_boxTree(boxTree)
{
}

void InlineContentBuilder::build(const Layout::InlineFormattingContext& inlineFormattingContext, InlineContent& inlineContent) const
{
    auto& inlineFormattingState = inlineFormattingContext.formattingState();
    auto lineLevelVisualAdjustmentsForRuns = computeLineLevelVisualAdjustmentsForRuns(inlineFormattingState);
    createDisplayLineRuns(inlineFormattingState, inlineContent, lineLevelVisualAdjustmentsForRuns);
    createDisplayNonRootInlineBoxes(inlineFormattingContext, inlineContent);
    createDisplayLines(inlineFormattingState, inlineContent, lineLevelVisualAdjustmentsForRuns);
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
            auto& nonRootInlineLevelBoxList = inlineFormattingState.lineBoxes()[lineIndex].nonRootInlineLevelBoxes();
            if (nonRootInlineLevelBoxList.isEmpty()) {
                // This is text content only with root inline box.
                return true;
            }
            // Text + <br> (or just <br> or text<span></span><br>) behaves like text.
            for (auto& inlineLevelBox : nonRootInlineLevelBoxList) {
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
            auto lineBoxLogicalWidth = line.lineBoxLogicalRect().width();
            auto overflowWidth = lineOverflowWidth(m_blockFlow, lineBoxLogicalWidth, line.contentLogicalWidth());
            lineLevelVisualAdjustmentsForRuns[lineIndex].needsTrailingContentReplacement = overflowWidth > lineBoxLogicalWidth;
        }
    }
    return lineLevelVisualAdjustmentsForRuns;
}

void InlineContentBuilder::createDisplayLineRuns(const Layout::InlineFormattingState& inlineFormattingState, InlineContent& inlineContent, const LineLevelVisualAdjustmentsForRunsList& lineLevelVisualAdjustmentsForRuns) const
{
    auto& runList = inlineFormattingState.lineRuns();
    if (runList.isEmpty())
        return;
    auto& lines = inlineFormattingState.lines();

#if PROCESS_BIDI_CONTENT
    BidiResolver<Iterator, BidiRun> bidiResolver;
    // FIXME: Add support for override.
    bidiResolver.setStatus(BidiStatus(m_layoutState.root().style().direction(), false));
    // FIXME: Grab the nested isolates from the previous line.
    bidiResolver.setPosition(Iterator(&runList, 0), 0);
    bidiResolver.createBidiRunsForLine(Iterator(&runList, runList.size()));
#endif

    Vector<bool> hasAdjustedTrailingLineList(lines.size(), false);

    auto createDisplayBoxRun = [&](auto& lineRun) {
        auto& layoutBox = lineRun.layoutBox();
        auto lineIndex = lineRun.lineIndex();
        auto& lineBoxLogicalRect = lines[lineIndex].lineBoxLogicalRect();
        // Inline boxes are relative to the line box while final runs need to be relative to the parent box
        // FIXME: Shouldn't we just leave them be relative to the line box?
        auto runRect = FloatRect { lineRun.logicalRect() };
        auto& geometry = m_layoutState.geometryForBox(layoutBox);
        runRect.moveBy({ lineBoxLogicalRect.left(), lineBoxLogicalRect.top() });
        runRect.setSize({ geometry.borderBoxWidth(), geometry.borderBoxHeight() });
        if (lineLevelVisualAdjustmentsForRuns[lineIndex].needsIntegralPosition)
            runRect.setY(roundToInt(runRect.y()));
        // FIXME: Add support for non-text ink overflow.
        // FIXME: Add support for cases when the run is after ellipsis.
        inlineContent.runs.append({ lineIndex, layoutBox, runRect, runRect, { }, { } });
    };

    auto createDisplayTextRunForRange = [&](auto& lineRun, auto startOffset, auto endOffset) {
        RELEASE_ASSERT(startOffset < endOffset);
        auto& layoutBox = lineRun.layoutBox();
        auto lineIndex = lineRun.lineIndex();
        auto& lineBoxLogicalRect = lines[lineIndex].lineBoxLogicalRect();
        auto runRect = FloatRect { lineRun.logicalRect() };
        runRect.moveBy({ lineBoxLogicalRect.left(), lineBoxLogicalRect.top() });
        if (lineLevelVisualAdjustmentsForRuns[lineIndex].needsIntegralPosition)
            runRect.setY(roundToInt(runRect.y()));

        auto& style = layoutBox.style();
        auto text = lineRun.text();
        auto adjustedContentToRender = [&] {
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
                auto ellipsisWidth = style.fontCascade().width(WebCore::TextRun { &horizontalEllipsis });
                if (runLogicalRect.right() + ellipsisWidth > lineBoxLogicalRect.right()) {
                    // The next run with ellipsis would surely overflow. So let's just add it to this run even if
                    // it makes the run wider than it originally was.
                    hasAdjustedTrailingLineList[lineIndex] = true;
                    float resultWidth = 0;
                    auto maxWidth = lineBoxLogicalRect.width() - runLogicalRect.left();
                    return StringTruncator::rightTruncate(originalContent, maxWidth, style.fontCascade(), resultWidth, true);
                }
            }
            return String();
        };

        auto computedInkOverflow = [&] (auto runRect) {
            auto inkOverflow = runRect;
            auto initialContaingBlockSize = m_layoutState.viewportSize();
            auto strokeOverflow = std::ceil(style.computedStrokeWidth(ceiledIntSize(initialContaingBlockSize)));
            inkOverflow.inflate(strokeOverflow);
            auto letterSpacing = style.fontCascade().letterSpacing();
            if (letterSpacing < 0) {
                // Last letter's negative spacing shrinks logical rect. Push it to ink overflow.
                inkOverflow.expand(-letterSpacing, { });
            }
            return inkOverflow;
        };
        RELEASE_ASSERT(startOffset >= text->start() && startOffset < text->end());
        RELEASE_ASSERT(endOffset > text->start() && endOffset <= text->end());
        auto textContent = Run::TextContent { startOffset, endOffset - startOffset, text->content(), adjustedContentToRender(), text->needsHyphen() };
        auto expansion = Run::Expansion { lineRun.expansion().behavior, lineRun.expansion().horizontalExpansion };
        auto displayRun = Run { lineIndex, layoutBox, runRect, computedInkOverflow(runRect), expansion, textContent };
        inlineContent.runs.append(displayRun);
    };

    inlineContent.runs.reserveInitialCapacity(inlineFormattingState.lineRuns().size());
    for (auto& lineRun : inlineFormattingState.lineRuns()) {
        if (auto& text = lineRun.text())
            createDisplayTextRunForRange(lineRun, text->start(), text->end());
        else
            createDisplayBoxRun(lineRun);
    }
}

void InlineContentBuilder::createDisplayLines(const Layout::InlineFormattingState& inlineFormattingState, InlineContent& inlineContent, const LineLevelVisualAdjustmentsForRunsList& lineLevelVisualAdjustmentsForRuns) const
{
    auto& lines = inlineFormattingState.lines();
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
            lineInkOverflowRect.unite(run.inkOverflow());

            auto& layoutBox = run.layoutBox();
            if (!layoutBox.isReplacedBox())
                continue;

            // Similar to InlineFlowBox::addReplacedChildOverflow.
            auto& box = downcast<RenderBox>(m_boxTree.rendererForLayoutBox(layoutBox));
            if (!box.hasSelfPaintingLayer()) {
                auto childInkOverflow = box.logicalVisualOverflowRectForPropagation(&box.parent()->style());
                childInkOverflow.move(run.rect().x(), run.rect().y());
                lineInkOverflowRect.unite(childInkOverflow);
            }
            auto childScrollableOverflow = box.logicalLayoutOverflowRectForPropagation(&box.parent()->style());
            childScrollableOverflow.move(run.rect().x(), run.rect().y());
            scrollableOverflowRect.unite(childScrollableOverflow);
        }
        // Collect scrollable overflow from inline boxes. All other inline level boxes (e.g atomic inline level boxes) stretch the line.
        while (inlineBoxIndex < nonRootInlineBoxes.size() && nonRootInlineBoxes[inlineBoxIndex].lineIndex() == lineIndex) {
            auto& inlineBox = nonRootInlineBoxes[inlineBoxIndex++];
            if (inlineBox.canContributeToLineOverflow())
                scrollableOverflowRect.unite(inlineBox.rect());
        }

        auto adjustedLineBoxRect = FloatRect { lineBoxLogicalRect };
        // Final enclosing top and bottom values are in the same coordinate space as the line itself.
        auto enclosingTopAndBottom = line.enclosingTopAndBottom() + lineBoxLogicalRect.top();
        if (lineLevelVisualAdjustmentsForRuns[lineIndex].needsIntegralPosition) {
            adjustedLineBoxRect.setY(roundToInt(adjustedLineBoxRect.y()));
            enclosingTopAndBottom.top = roundToInt(enclosingTopAndBottom.top);
            enclosingTopAndBottom.bottom = roundToInt(enclosingTopAndBottom.bottom);
        }
        auto runCount = runIndex - firstRunIndex;
        inlineContent.lines.append({ firstRunIndex, runCount, adjustedLineBoxRect, enclosingTopAndBottom.top, enclosingTopAndBottom.bottom, scrollableOverflowRect, lineInkOverflowRect, line.baseline(), line.contentLogicalLeftOffset(), line.contentLogicalWidth() });
    }
}

void InlineContentBuilder::createDisplayNonRootInlineBoxes(const Layout::InlineFormattingContext& inlineFormattingContext, InlineContent& inlineContent) const
{
    auto& inlineFormattingState = inlineFormattingContext.formattingState();
    auto inlineQuirks = inlineFormattingContext.quirks();
    for (size_t lineIndex = 0; lineIndex < inlineFormattingState.lineBoxes().size(); ++lineIndex) {
        auto& lineBox = inlineFormattingState.lineBoxes()[lineIndex];
        auto& lineBoxLogicalRect = lineBox.logicalRect();

        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            if (!inlineLevelBox->isInlineBox())
                continue;
            auto& layoutBox = inlineLevelBox->layoutBox();
            auto& boxGeometry = m_layoutState.geometryForBox(layoutBox);
            auto inlineBoxRect = lineBox.logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
            inlineBoxRect.moveBy(lineBoxLogicalRect.topLeft());

            inlineContent.nonRootInlineBoxes.append({ lineIndex, layoutBox, inlineBoxRect, inlineQuirks.inlineLevelBoxAffectsLineBox(*inlineLevelBox, lineBox) });
        }
    }
}

}
}

#endif
