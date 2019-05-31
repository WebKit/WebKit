/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "InlineFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FloatingContext.h"
#include "FloatingState.h"
#include "InlineFormattingState.h"
#include "InlineLine.h"
#include "InlineLineBreaker.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutState.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

struct UncommittedContent {
    void add(InlineItem&);
    void reset();

    Vector<InlineItem*> inlineItems() { return m_inlineItems; }
    bool isEmpty() const { return m_inlineItems.isEmpty(); }
    unsigned size() const { return m_inlineItems.size(); }
    LayoutUnit width() const { return m_width; }

private:
    Vector<InlineItem*> m_inlineItems;
    LayoutUnit m_width;
};

void UncommittedContent::add(InlineItem& inlineItem)
{
    m_inlineItems.append(&inlineItem);
    m_width += inlineItem.width();
}

void UncommittedContent::reset()
{
    m_inlineItems.clear();
    m_width = 0;
}

InlineFormattingContext::LineLayout::LineInput::LineInput(LayoutUnit logicalTop, LayoutUnit availableLogicalWidth, unsigned firstInlineItemIndex, const InlineItems& inlineItems)
    : logicalTop(logicalTop)
    , availableLogicalWidth(availableLogicalWidth)
    , firstInlineItemIndex(firstInlineItemIndex)
    , inlineItems(inlineItems)
{
}

InlineFormattingContext::LineLayout::LineLayout(const InlineFormattingContext& inlineFormattingContext)
    : m_formattingContext(inlineFormattingContext)
    , m_formattingState(m_formattingContext.formattingState())
    , m_floatingState(m_formattingState.floatingState())
    , m_formattingRoot(downcast<Container>(m_formattingContext.root()))
{
}

static std::unique_ptr<Line> constructLine(const LayoutState& layoutState, const FloatingState& floatingState, const Box& formattingRoot,
    LayoutUnit lineLogicalTop, LayoutUnit availableWidth)
{
    auto& formattingRootDisplayBox = layoutState.displayBoxForLayoutBox(formattingRoot);
    auto lineLogicalLeft = formattingRootDisplayBox.contentBoxLeft();

    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    if (!floatingState.isEmpty()) {
        auto floatConstraints = floatingState.constraints({ lineLogicalTop }, formattingRoot);
        // Check if these constraints actually put limitation on the line.
        if (floatConstraints.left && *floatConstraints.left <= formattingRootDisplayBox.contentBoxLeft())
            floatConstraints.left = { };

        if (floatConstraints.right && *floatConstraints.right >= formattingRootDisplayBox.contentBoxRight())
            floatConstraints.right = { };

        if (floatConstraints.left && floatConstraints.right) {
            ASSERT(*floatConstraints.left < *floatConstraints.right);
            availableWidth = *floatConstraints.right - *floatConstraints.left;
            lineLogicalLeft = *floatConstraints.left;
        } else if (floatConstraints.left) {
            ASSERT(*floatConstraints.left > lineLogicalLeft);
            availableWidth -= (*floatConstraints.left - lineLogicalLeft);
            lineLogicalLeft = *floatConstraints.left;
        } else if (floatConstraints.right) {
            ASSERT(*floatConstraints.right > lineLogicalLeft);
            availableWidth = *floatConstraints.right - lineLogicalLeft;
        }
    }

    auto& formattingRootStyle = formattingRoot.style();
    auto mimimumLineHeight = formattingRootStyle.computedLineHeight();
    auto baselineOffset = Line::halfLeadingMetrics(formattingRootStyle.fontMetrics(), mimimumLineHeight).height;
    return std::make_unique<Line>(layoutState, LayoutPoint { lineLogicalLeft, lineLogicalTop }, availableWidth, mimimumLineHeight, baselineOffset);
}

InlineFormattingContext::LineLayout::LineContent InlineFormattingContext::LineLayout::placeInlineItems(const LineInput& lineInput) const
{
    auto line = constructLine(layoutState(), m_floatingState, m_formattingRoot, lineInput.logicalTop, lineInput.availableLogicalWidth);
    auto floatingContext = FloatingContext { m_floatingState };
    unsigned committedInlineItemCount = 0;

    UncommittedContent uncommittedContent;
    auto commitPendingContent = [&] {
        if (uncommittedContent.isEmpty())
            return;
        committedInlineItemCount += uncommittedContent.size();
        for (auto* uncommitted : uncommittedContent.inlineItems())
            commitInlineItemToLine(*line, *uncommitted);
        uncommittedContent.reset();
    };

    auto closeLine = [&] {
        // This might change at some point.
        ASSERT(committedInlineItemCount);
        return LineContent { lineInput.firstInlineItemIndex + (committedInlineItemCount - 1), line->close() };
    };
    LineBreaker lineBreaker(layoutState());
    // Iterate through the inline content and place the inline boxes on the current line.
    for (auto inlineItemIndex = lineInput.firstInlineItemIndex; inlineItemIndex < lineInput.inlineItems.size(); ++inlineItemIndex) {
        auto& inlineItem = lineInput.inlineItems[inlineItemIndex];
        if (inlineItem->isHardLineBreak()) {
            uncommittedContent.add(*inlineItem);
            commitPendingContent();
            return closeLine();
        }
        auto availableWidth = line->availableWidth() - uncommittedContent.width();
        auto currentLogicalRight = line->contentLogicalRight() + uncommittedContent.width();
        // FIXME: Ensure LineContext::trimmableWidth includes uncommitted content if needed.
        auto breakingContext = lineBreaker.breakingContext(*inlineItem, { availableWidth, currentLogicalRight, line->trailingTrimmableWidth(), !line->hasContent() });
        if (breakingContext.isAtBreakingOpportunity)
            commitPendingContent();

        // Content does not fit the current line.
        if (breakingContext.breakingBehavior == LineBreaker::BreakingBehavior::Wrap)
            return closeLine();

        // Partial content stays on the current line. 
        if (breakingContext.breakingBehavior == LineBreaker::BreakingBehavior::Break) {
            ASSERT(inlineItem->isText());

            ASSERT_NOT_IMPLEMENTED_YET();
            return closeLine();
        }

        if (inlineItem->isFloat()) {
            handleFloat(*line, floatingContext, *inlineItem);
            ++committedInlineItemCount;
            continue;
        }

        uncommittedContent.add(*inlineItem);
        if (breakingContext.isAtBreakingOpportunity)
            commitPendingContent();
    }
    commitPendingContent();
    return closeLine();
}

void InlineFormattingContext::LineLayout::layout(LayoutUnit widthConstraint) const
{
    ASSERT(!m_formattingState.inlineItems().isEmpty());

    auto lineLogicalTop = layoutState().displayBoxForLayoutBox(m_formattingRoot).contentBoxTop();
    auto& inlineItems = m_formattingState.inlineItems();
    unsigned currentInlineItemIndex = 0;
    while (currentInlineItemIndex < inlineItems.size()) {
        auto lineContent = placeInlineItems({ lineLogicalTop, widthConstraint, currentInlineItemIndex, inlineItems });
        createDisplayRuns(*lineContent.runs, widthConstraint);
        // We should always put at least one run on the line atm. This might change later on though.
        ASSERT(lineContent.lastInlineItemIndex);
        currentInlineItemIndex = *lineContent.lastInlineItemIndex + 1;
        lineLogicalTop = lineContent.runs->logicalBottom();
    }
}

LayoutUnit InlineFormattingContext::LineLayout::computedIntrinsicWidth(LayoutUnit widthConstraint) const
{
    // FIXME: Consider running it through layout().
    LayoutUnit maximumLineWidth;
    LayoutUnit lineLogicalRight;
    LayoutUnit trimmableTrailingWidth;

    LineBreaker lineBreaker(layoutState());
    auto& inlineContent = m_formattingState.inlineItems();
    for (auto& inlineItem : inlineContent) {
        auto breakingContext = lineBreaker.breakingContext(*inlineItem, { widthConstraint, lineLogicalRight, !lineLogicalRight });
        if (breakingContext.breakingBehavior == LineBreaker::BreakingBehavior::Wrap) {
            maximumLineWidth = std::max(maximumLineWidth, lineLogicalRight - trimmableTrailingWidth);
            trimmableTrailingWidth = { };
            lineLogicalRight = { };
        }
        if (TextUtil::isTrimmableContent(*inlineItem)) {
            // Skip leading whitespace.
            if (!lineLogicalRight)
                continue;
            trimmableTrailingWidth += inlineItem->width();
        } else
            trimmableTrailingWidth = { };
        lineLogicalRight += inlineItem->width();
    }
    return std::max(maximumLineWidth, lineLogicalRight - trimmableTrailingWidth);
}

void InlineFormattingContext::LineLayout::createDisplayRuns(const Line::Content& lineContent, LayoutUnit widthConstraint) const
{
    if (lineContent.isEmpty()) {
        // Spec tells us to create a zero height, empty line box.
        auto lineBox = Display::Rect { lineContent.logicalTop(), lineContent.logicalLeft(), 0 , 0 };
        m_formattingState.addLineBox({ lineBox });
        return;
    }

    auto& inlineDisplayRuns = m_formattingState.inlineRuns(); 
    Optional<unsigned> previousLineLastRunIndex = inlineDisplayRuns.isEmpty() ? Optional<unsigned>() : inlineDisplayRuns.size() - 1;
    // 9.4.2 Inline formatting contexts
    // A line box is always tall enough for all of the boxes it contains.

    // Ignore the initial strut.
    auto lineBox = Display::Rect { lineContent.logicalTop(), lineContent.logicalLeft(), 0 , !lineContent.isVisuallyEmpty() ? lineContent.logicalHeight() : LayoutUnit { } };
    // Create final display runs.
    auto& lineRuns = lineContent.runs();
    for (unsigned index = 0; index < lineRuns.size(); ++index) {
        auto& lineRun = lineRuns.at(index);

        auto& inlineItem = lineRun->inlineItem;
        auto& inlineRun = lineRun->inlineRun;
        auto& layoutBox = inlineItem.layoutBox();
        auto& displayBox = layoutState().displayBoxForLayoutBox(layoutBox);

        if (inlineItem.isHardLineBreak()) {
            displayBox.setTopLeft(inlineRun.logicalTopLeft());
            displayBox.setContentBoxWidth(inlineRun.logicalWidth());
            displayBox.setContentBoxHeight(inlineRun.logicalHeight());
            m_formattingState.addInlineRun(std::make_unique<Display::Run>(inlineRun));
            continue;
        }

        // Inline level box (replaced or inline-block)
        if (inlineItem.isBox()) {
            auto topLeft = inlineRun.logicalTopLeft();
            if (layoutBox.isInFlowPositioned())
                topLeft += Geometry::inFlowPositionedPositionOffset(layoutState(), layoutBox);
            displayBox.setTopLeft(topLeft);
            lineBox.expandHorizontally(inlineRun.logicalWidth());
            m_formattingState.addInlineRun(std::make_unique<Display::Run>(inlineRun));
            continue;
        }

        // Inline level container start (<span>)
        if (inlineItem.isContainerStart()) {
            displayBox.setTopLeft(inlineRun.logicalTopLeft());
            lineBox.expandHorizontally(inlineRun.logicalWidth());
            continue;
        }

        // Inline level container end (</span>)
        if (inlineItem.isContainerEnd()) {
            if (layoutBox.isInFlowPositioned()) {
                auto inflowOffset = Geometry::inFlowPositionedPositionOffset(layoutState(), layoutBox);
                displayBox.moveHorizontally(inflowOffset.width());
                displayBox.moveVertically(inflowOffset.height());
            }
            auto marginBoxWidth = inlineRun.logicalLeft() - displayBox.left();
            auto contentBoxWidth = marginBoxWidth - (displayBox.marginStart() + displayBox.borderLeft() + displayBox.paddingLeft().valueOr(0));
            // FIXME fix it for multiline.
            displayBox.setContentBoxWidth(contentBoxWidth);
            displayBox.setContentBoxHeight(inlineRun.logicalHeight());
            lineBox.expandHorizontally(inlineRun.logicalWidth());
            continue;
        }

        // Text content. Try to join multiple text runs when possible.
        ASSERT(inlineRun.textContext());        
        const Line::Content::Run* previousLineRun = !index ? nullptr : lineRuns[index - 1].get();
        if (!lineRun->isCollapsed) {
            auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
            auto previousRunCanBeExtended = previousLineRun ? previousLineRun->canBeExtended : false;
            auto requiresNewRun = !index || !previousRunCanBeExtended || &layoutBox != &previousLineRun->inlineItem.layoutBox();
            if (requiresNewRun)
                m_formattingState.addInlineRun(std::make_unique<Display::Run>(inlineRun));
            else {
                auto& lastDisplayRun = m_formattingState.inlineRuns().last();
                lastDisplayRun->expandHorizontally(inlineTextItem.width());
                lastDisplayRun->textContext()->expand(inlineRun.textContext()->length());
            }
            lineBox.expandHorizontally(inlineRun.logicalWidth());
        }
        // FIXME take content breaking into account when part of the layout box is on the previous line.
        auto firstInlineRunForLayoutBox = !previousLineRun || &previousLineRun->inlineItem.layoutBox() != &layoutBox;
        if (firstInlineRunForLayoutBox) {
            // Setup display box for the associated layout box.
            displayBox.setTopLeft(inlineRun.logicalTopLeft());
            displayBox.setContentBoxWidth(lineRun->isCollapsed ? LayoutUnit() : inlineRun.logicalWidth());
            displayBox.setContentBoxHeight(inlineRun.logicalHeight());
        } else if (!lineRun->isCollapsed) {
            // FIXME fix it for multirun/multiline.
            displayBox.setContentBoxWidth(displayBox.contentBoxWidth() + inlineRun.logicalWidth());
        }
    }
    // FIXME linebox needs to be ajusted after content alignment.
    m_formattingState.addLineBox({ lineBox });
    if (!lineContent.isVisuallyEmpty())
        alignRuns(m_formattingRoot.style().textAlign(), previousLineLastRunIndex.valueOr(-1) + 1, widthConstraint - lineContent.logicalWidth());
}

void InlineFormattingContext::LineLayout::handleFloat(Line& line, const FloatingContext& floatingContext, const InlineItem& floatItem) const
{
    auto& floatBox = floatItem.layoutBox();
    ASSERT(layoutState().hasDisplayBox(floatBox));
    auto& displayBox = layoutState().displayBoxForLayoutBox(floatBox);
    // Set static position first.
    displayBox.setTopLeft({ line.contentLogicalRight(), line.logicalTop() });
    // Float it.
    displayBox.setTopLeft(floatingContext.positionForFloat(floatBox));
    m_floatingState.append(floatBox);
    // Shrink availble space for current line and move existing inline runs.
    auto floatBoxWidth = floatItem.width();
    floatBox.isLeftFloatingPositioned() ? line.moveLogicalLeft(floatBoxWidth) : line.moveLogicalRight(floatBoxWidth);
}

void InlineFormattingContext::LineLayout::commitInlineItemToLine(Line& line, const InlineItem& inlineItem) const
{
    if (inlineItem.isContainerStart())
        return line.appendInlineContainerStart(inlineItem);

    if (inlineItem.isContainerEnd())
        return line.appendInlineContainerEnd(inlineItem);

    if (inlineItem.isHardLineBreak())
        return line.appendHardLineBreak(inlineItem);

    auto width = inlineItem.width();
    if (is<InlineTextItem>(inlineItem))
        return line.appendTextContent(downcast<InlineTextItem>(inlineItem), { width, inlineItem.style().fontMetrics().height() });

    auto& layoutBox = inlineItem.layoutBox();
    auto& displayBox = layoutState().displayBoxForLayoutBox(layoutBox);
    if (layoutBox.isReplaced())
        return line.appendReplacedInlineBox(inlineItem, { width, displayBox.height() });

    line.appendNonReplacedInlineBox(inlineItem, { width, displayBox.height() });
}

static Optional<LayoutUnit> horizontalAdjustmentForAlignment(TextAlignMode align, LayoutUnit remainingWidth)
{
    switch (align) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
    case TextAlignMode::Start:
        return { };
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
    case TextAlignMode::End:
        return std::max(remainingWidth, 0_lu);
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        return std::max(remainingWidth / 2, 0_lu);
    case TextAlignMode::Justify:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return { };
}

void InlineFormattingContext::LineLayout::alignRuns(TextAlignMode textAlign, unsigned firstRunIndex, LayoutUnit availableWidth) const
{
    auto adjustment = horizontalAdjustmentForAlignment(textAlign, availableWidth);
    if (!adjustment)
        return;

    auto& inlineDisplayRuns = m_formattingState.inlineRuns(); 
    for (unsigned index = firstRunIndex; index < inlineDisplayRuns.size(); ++index)
        inlineDisplayRuns[index]->moveHorizontally(*adjustment);
}

}
}

#endif
