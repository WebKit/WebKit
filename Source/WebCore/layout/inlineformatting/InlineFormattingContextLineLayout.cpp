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

struct InlineIndexAndSplitPosition {
    unsigned index { 0 };
    Optional<unsigned> splitPosition;
};

struct LineInput {
    LineInput(LayoutPoint logicalTopLeft, LayoutUnit availableLogicalWidth, Line::SkipVerticalAligment, InlineIndexAndSplitPosition firstToProcess, const InlineItems&);
    struct HorizontalConstraint {
        HorizontalConstraint(LayoutPoint logicalTopLeft, LayoutUnit availableLogicalWidth);

        LayoutPoint logicalTopLeft;
        LayoutUnit availableLogicalWidth;
    };
    HorizontalConstraint horizontalConstraint;
    // FIXME Alternatively we could just have a second pass with vertical positioning (preferred width computation opts out) 
    Line::SkipVerticalAligment skipVerticalAligment;
    InlineIndexAndSplitPosition firstInlineItem;
    const InlineItems& inlineItems;
    Optional<LayoutUnit> floatMinimumLogicalBottom;
};

struct LineContent {
    Optional<InlineIndexAndSplitPosition> lastCommitted;
    Vector<WeakPtr<InlineItem>> floats;
    std::unique_ptr<Line::Content> runs;
};

struct UncommittedContent {
    struct Run {
        const InlineItem& inlineItem;
        LayoutUnit logicalWidth;
        // FIXME: add optional breaking context (start and end position) for split text content.
    };
    void add(const InlineItem&, LayoutUnit logicalWidth);
    void reset();

    Vector<Run> runs() { return m_uncommittedRuns; }
    bool isEmpty() const { return m_uncommittedRuns.isEmpty(); }
    unsigned size() const { return m_uncommittedRuns.size(); }
    LayoutUnit width() const { return m_width; }

private:
    Vector<Run> m_uncommittedRuns;
    LayoutUnit m_width;
};

void UncommittedContent::add(const InlineItem& inlineItem, LayoutUnit logicalWidth)
{
    m_uncommittedRuns.append({ inlineItem, logicalWidth });
    m_width += logicalWidth;
}

void UncommittedContent::reset()
{
    m_uncommittedRuns.clear();
    m_width = 0;
}

LineInput::HorizontalConstraint::HorizontalConstraint(LayoutPoint logicalTopLeft, LayoutUnit availableLogicalWidth)
    : logicalTopLeft(logicalTopLeft)
    , availableLogicalWidth(availableLogicalWidth)
{
}

LineInput::LineInput(LayoutPoint logicalTopLeft, LayoutUnit availableLogicalWidth, Line::SkipVerticalAligment skipVerticalAligment, InlineIndexAndSplitPosition firstToProcess, const InlineItems& inlineItems)
    : horizontalConstraint(logicalTopLeft, availableLogicalWidth)
    , skipVerticalAligment(skipVerticalAligment)
    , firstInlineItem(firstToProcess)
    , inlineItems(inlineItems)
{
}

InlineFormattingContext::LineLayout::LineLayout(const InlineFormattingContext& inlineFormattingContext)
    : m_layoutState(inlineFormattingContext.layoutState())
    , m_formattingRoot(downcast<Container>(inlineFormattingContext.root()))
{
}

static LayoutUnit inlineItemWidth(const LayoutState& layoutState, const InlineItem& inlineItem, LayoutUnit contentLogicalLeft)
{
    if (inlineItem.isLineBreak())
        return 0;

    if (is<InlineTextItem>(inlineItem)) {
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        auto end = inlineTextItem.isCollapsed() ? inlineTextItem.start() + 1 : inlineTextItem.end();
        return TextUtil::width(downcast<InlineBox>(inlineTextItem.layoutBox()), inlineTextItem.start(), end, contentLogicalLeft);
    }

    auto& layoutBox = inlineItem.layoutBox();
    ASSERT(layoutState.hasDisplayBox(layoutBox));
    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);

    if (layoutBox.isFloatingPositioned())
        return displayBox.marginBoxWidth();

    if (layoutBox.isReplaced())
        return displayBox.width();

    if (inlineItem.isContainerStart())
        return displayBox.marginStart() + displayBox.borderLeft() + displayBox.paddingLeft().valueOr(0);

    if (inlineItem.isContainerEnd())
        return displayBox.marginEnd() + displayBox.borderRight() + displayBox.paddingRight().valueOr(0);

    // Non-replaced inline box (e.g. inline-block)
    return displayBox.width();
}

LineContent InlineFormattingContext::LineLayout::placeInlineItems(const LineInput& lineInput) const
{
    auto initialLineConstraints = Line::InitialConstraints {
        lineInput.horizontalConstraint.logicalTopLeft,
        lineInput.horizontalConstraint.availableLogicalWidth,
        Quirks::lineHeightConstraints(layoutState(), m_formattingRoot)
    };
    auto line = Line { layoutState(), initialLineConstraints, lineInput.skipVerticalAligment };

    Vector<WeakPtr<InlineItem>> floats;
    unsigned committedInlineItemCount = 0;
    Optional<unsigned> splitPosition;

    UncommittedContent uncommittedContent;
    auto commitPendingContent = [&] {
        if (uncommittedContent.isEmpty())
            return;
        committedInlineItemCount += uncommittedContent.size();
        for (auto& uncommittedRun : uncommittedContent.runs())
            line.append(uncommittedRun.inlineItem, uncommittedRun.logicalWidth);
        uncommittedContent.reset();
    };

    auto lineHasFloatBox = lineInput.floatMinimumLogicalBottom.hasValue();
    auto closeLine = [&] {
        ASSERT(committedInlineItemCount || lineHasFloatBox);
        if (!committedInlineItemCount)
            return LineContent { WTF::nullopt, WTFMove(floats), line.close() };
        auto lastCommitedItem = InlineIndexAndSplitPosition { lineInput.firstInlineItem.index + (committedInlineItemCount - 1), splitPosition };
        return LineContent { lastCommitedItem, WTFMove(floats), line.close() };
    };
    LineBreaker lineBreaker;
    // Iterate through the inline content and place the inline boxes on the current line.
    for (auto inlineItemIndex = lineInput.firstInlineItem.index; inlineItemIndex < lineInput.inlineItems.size(); ++inlineItemIndex) {
        auto availableWidth = line.availableWidth() - uncommittedContent.width();
        auto currentLogicalRight = line.contentLogicalRight() + uncommittedContent.width();
        auto& inlineItem = lineInput.inlineItems[inlineItemIndex];
        auto itemLogicalWidth = inlineItemWidth(layoutState(), *inlineItem, currentLogicalRight);

        // FIXME: Ensure LineContext::trimmableWidth includes uncommitted content if needed.
        auto lineIsConsideredEmpty = !line.hasContent() && !lineHasFloatBox;
        auto breakingContext = lineBreaker.breakingContext(*inlineItem, itemLogicalWidth, { availableWidth, currentLogicalRight, line.trailingTrimmableWidth(), lineIsConsideredEmpty });
        if (breakingContext.isAtBreakingOpportunity)
            commitPendingContent();

        // Content does not fit the current line.
        if (breakingContext.breakingBehavior == LineBreaker::BreakingBehavior::Wrap)
            return closeLine();

        // Partial content stays on the current line. 
        if (breakingContext.breakingBehavior == LineBreaker::BreakingBehavior::Split) {
            ASSERT(inlineItem->isText());

            ASSERT_NOT_IMPLEMENTED_YET();
            return closeLine();
        }

        ASSERT(breakingContext.breakingBehavior == LineBreaker::BreakingBehavior::Keep);
        if (inlineItem->isFloat()) {
            auto& floatBox = inlineItem->layoutBox();
            ASSERT(layoutState().hasDisplayBox(floatBox));
            // Shrink availble space for current line and move existing inline runs.
            auto floatBoxWidth = layoutState().displayBoxForLayoutBox(floatBox).marginBoxWidth();
            floatBox.isLeftFloatingPositioned() ? line.moveLogicalLeft(floatBoxWidth) : line.moveLogicalRight(floatBoxWidth);
            floats.append(makeWeakPtr(*inlineItem));
            ++committedInlineItemCount;
            lineHasFloatBox = true;
            continue;
        }

        uncommittedContent.add(*inlineItem, itemLogicalWidth);
        if (breakingContext.isAtBreakingOpportunity)
            commitPendingContent();

        if (inlineItem->isHardLineBreak())
            return closeLine();
    }
    commitPendingContent();
    return closeLine();
}

void InlineFormattingContext::LineLayout::layout(const InlineItems& inlineItems, LayoutUnit widthConstraint) const
{
    ASSERT(!inlineItems.isEmpty());

    auto& formattingRootDisplayBox = layoutState().displayBoxForLayoutBox(m_formattingRoot);
    auto& floatingState = layoutState().establishedFormattingState(m_formattingRoot).floatingState();

    auto lineLogicalTop = formattingRootDisplayBox.contentBoxTop();
    auto lineLogicalLeft = formattingRootDisplayBox.contentBoxLeft();

    auto applyFloatConstraint = [&](auto& lineInput) {
        // Check for intruding floats and adjust logical left/available width for this line accordingly.
        if (floatingState.isEmpty())
            return;
        auto availableWidth = lineInput.horizontalConstraint.availableLogicalWidth;
        auto lineLogicalLeft = lineInput.horizontalConstraint.logicalTopLeft.x();
        auto floatConstraints = floatingState.constraints({ lineLogicalTop }, m_formattingRoot);
        // Check if these constraints actually put limitation on the line.
        if (floatConstraints.left && floatConstraints.left->x <= formattingRootDisplayBox.contentBoxLeft())
            floatConstraints.left = { };

        if (floatConstraints.right && floatConstraints.right->x >= formattingRootDisplayBox.contentBoxRight())
            floatConstraints.right = { };

        // Set the minimum float bottom value as a hint for the next line if needed.
        static auto inifitePoint = PointInContextRoot::max();
        auto floatMinimumLogicalBottom = std::min(floatConstraints.left.valueOr(inifitePoint).y, floatConstraints.right.valueOr(inifitePoint).y);
        if (floatMinimumLogicalBottom != inifitePoint.y)
            lineInput.floatMinimumLogicalBottom = floatMinimumLogicalBottom;

        if (floatConstraints.left && floatConstraints.right) {
            ASSERT(floatConstraints.left->x <= floatConstraints.right->x);
            availableWidth = floatConstraints.right->x - floatConstraints.left->x;
            lineLogicalLeft = floatConstraints.left->x;
        } else if (floatConstraints.left) {
            ASSERT(floatConstraints.left->x >= lineLogicalLeft);
            availableWidth -= (floatConstraints.left->x - lineLogicalLeft);
            lineLogicalLeft = floatConstraints.left->x;
        } else if (floatConstraints.right) {
            ASSERT(floatConstraints.right->x >= lineLogicalLeft);
            availableWidth = floatConstraints.right->x - lineLogicalLeft;
        }
        lineInput.horizontalConstraint.availableLogicalWidth = availableWidth;
        lineInput.horizontalConstraint.logicalTopLeft.setX(lineLogicalLeft);
    };

    InlineIndexAndSplitPosition currentInlineItem;
    while (currentInlineItem.index < inlineItems.size()) {
        auto lineInput = LineInput { { lineLogicalLeft, lineLogicalTop }, widthConstraint, Line::SkipVerticalAligment::No, currentInlineItem, inlineItems };
        applyFloatConstraint(lineInput);
        auto lineContent = placeInlineItems(lineInput);
        createDisplayRuns(*lineContent.runs, lineContent.floats, widthConstraint);
        if (!lineContent.lastCommitted) {
            // Floats prevented us putting any content on the line.
            ASSERT(lineInput.floatMinimumLogicalBottom);
            ASSERT(lineContent.runs->isEmpty());
            lineLogicalTop = *lineInput.floatMinimumLogicalBottom;
        } else {
            currentInlineItem = { lineContent.lastCommitted->index + 1, WTF::nullopt };
            lineLogicalTop = lineContent.runs->logicalBottom();
        }
    }
}

LayoutUnit InlineFormattingContext::LineLayout::computedIntrinsicWidth(const InlineItems& inlineItems, LayoutUnit widthConstraint) const
{
    LayoutUnit maximumLineWidth;
    InlineIndexAndSplitPosition currentInlineItem;
    while (currentInlineItem.index < inlineItems.size()) {
        auto lineContent = placeInlineItems({ { }, widthConstraint, Line::SkipVerticalAligment::Yes, currentInlineItem, inlineItems });
        currentInlineItem = { lineContent.lastCommitted->index + 1, WTF::nullopt };
        LayoutUnit floatsWidth;
        for (auto& floatItem : lineContent.floats)
            floatsWidth += layoutState().displayBoxForLayoutBox(floatItem->layoutBox()).marginBoxWidth();
        maximumLineWidth = std::max(maximumLineWidth, floatsWidth + lineContent.runs->logicalWidth());
    }
    return maximumLineWidth;
}

void InlineFormattingContext::LineLayout::createDisplayRuns(const Line::Content& lineContent, const Vector<WeakPtr<InlineItem>>& floats, LayoutUnit widthConstraint) const
{
    auto& formattingState = downcast<InlineFormattingState>(layoutState().establishedFormattingState(m_formattingRoot));
    auto& floatingState = formattingState.floatingState();
    auto floatingContext = FloatingContext { floatingState };

    // Move floats to their final position.
    for (auto floatItem : floats) {
        auto& floatBox = floatItem->layoutBox();
        ASSERT(layoutState().hasDisplayBox(floatBox));
        auto& displayBox = layoutState().displayBoxForLayoutBox(floatBox);
        // Set static position first.
        displayBox.setTopLeft({ lineContent.logicalLeft(), lineContent.logicalTop() });
        // Float it.
        displayBox.setTopLeft(floatingContext.positionForFloat(floatBox));
        floatingState.append(floatBox);
    }

    if (lineContent.isEmpty()) {
        // Spec tells us to create a zero height, empty line box.
        auto lineBoxRect = Display::Rect { lineContent.logicalTop(), lineContent.logicalLeft(), 0 , 0 };
        formattingState.addLineBox({ lineBoxRect, lineContent.baseline(), lineContent.baselineOffset() });
        return;
    }

    auto& inlineDisplayRuns = formattingState.inlineRuns();
    Optional<unsigned> previousLineLastRunIndex = inlineDisplayRuns.isEmpty() ? Optional<unsigned>() : inlineDisplayRuns.size() - 1;
    // 9.4.2 Inline formatting contexts
    // A line box is always tall enough for all of the boxes it contains.
    auto lineBoxRect = Display::Rect { lineContent.logicalTop(), lineContent.logicalLeft(), 0, lineContent.logicalHeight()};
    // Create final display runs.
    auto& lineRuns = lineContent.runs();
    for (unsigned index = 0; index < lineRuns.size(); ++index) {
        auto& lineRun = lineRuns.at(index);
        auto& logicalRect = lineRun->logicalRect();
        auto& layoutBox = lineRun->layoutBox();
        auto& displayBox = layoutState().displayBoxForLayoutBox(layoutBox);

        if (lineRun->isLineBreak()) {
            displayBox.setTopLeft(logicalRect.topLeft());
            displayBox.setContentBoxWidth(logicalRect.width());
            displayBox.setContentBoxHeight(logicalRect.height());
            formattingState.addInlineRun(std::make_unique<Display::Run>(logicalRect));
            continue;
        }

        // Inline level box (replaced or inline-block)
        if (lineRun->isBox()) {
            auto topLeft = logicalRect.topLeft();
            if (layoutBox.isInFlowPositioned())
                topLeft += Geometry::inFlowPositionedPositionOffset(layoutState(), layoutBox);
            displayBox.setTopLeft(topLeft);
            lineBoxRect.expandHorizontally(logicalRect.width());
            formattingState.addInlineRun(std::make_unique<Display::Run>(logicalRect));
            continue;
        }

        // Inline level container start (<span>)
        if (lineRun->isContainerStart()) {
            displayBox.setTopLeft(logicalRect.topLeft());
            lineBoxRect.expandHorizontally(logicalRect.width());
            continue;
        }

        // Inline level container end (</span>)
        if (lineRun->isContainerEnd()) {
            if (layoutBox.isInFlowPositioned()) {
                auto inflowOffset = Geometry::inFlowPositionedPositionOffset(layoutState(), layoutBox);
                displayBox.moveHorizontally(inflowOffset.width());
                displayBox.moveVertically(inflowOffset.height());
            }
            auto marginBoxWidth = logicalRect.left() - displayBox.left();
            auto contentBoxWidth = marginBoxWidth - (displayBox.marginStart() + displayBox.borderLeft() + displayBox.paddingLeft().valueOr(0));
            // FIXME fix it for multiline.
            displayBox.setContentBoxWidth(contentBoxWidth);
            displayBox.setContentBoxHeight(logicalRect.height());
            lineBoxRect.expandHorizontally(logicalRect.width());
            continue;
        }

        // Text content. Try to join multiple text runs when possible.
        ASSERT(lineRun->isText());
        auto textContext = lineRun->textContext();   
        const Line::Content::Run* previousLineRun = !index ? nullptr : lineRuns[index - 1].get();
        if (!textContext->isCollapsed) {
            auto previousRunCanBeExtended = previousLineRun && previousLineRun->textContext() ? previousLineRun->textContext()->canBeExtended : false;
            auto requiresNewRun = !index || !previousRunCanBeExtended || &layoutBox != &previousLineRun->layoutBox();
            if (requiresNewRun)
                formattingState.addInlineRun(std::make_unique<Display::Run>(logicalRect, Display::Run::TextContext { textContext->start, textContext->length }));
            else {
                auto& lastDisplayRun = formattingState.inlineRuns().last();
                lastDisplayRun->expandHorizontally(logicalRect.width());
                lastDisplayRun->textContext()->expand(textContext->length);
            }
            lineBoxRect.expandHorizontally(logicalRect.width());
        }
        // FIXME take content breaking into account when part of the layout box is on the previous line.
        auto firstInlineRunForLayoutBox = !previousLineRun || &previousLineRun->layoutBox() != &layoutBox;
        if (firstInlineRunForLayoutBox) {
            // Setup display box for the associated layout box.
            displayBox.setTopLeft(logicalRect.topLeft());
            displayBox.setContentBoxWidth(textContext->isCollapsed ? LayoutUnit() : logicalRect.width());
            displayBox.setContentBoxHeight(logicalRect.height());
        } else if (!textContext->isCollapsed) {
            // FIXME fix it for multirun/multiline.
            displayBox.setContentBoxWidth(displayBox.contentBoxWidth() + logicalRect.width());
        }
    }
    // FIXME linebox needs to be ajusted after content alignment.
    formattingState.addLineBox({ lineBoxRect, lineContent.baseline(), lineContent.baselineOffset() });
    alignRuns(m_formattingRoot.style().textAlign(), inlineDisplayRuns, previousLineLastRunIndex.valueOr(-1) + 1, widthConstraint - lineContent.logicalWidth());
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

void InlineFormattingContext::LineLayout::alignRuns(TextAlignMode textAlign, InlineRuns& inlineDisplayRuns, unsigned firstRunIndex, LayoutUnit availableWidth) const
{
    auto adjustment = horizontalAdjustmentForAlignment(textAlign, availableWidth);
    if (!adjustment)
        return;

    for (unsigned index = firstRunIndex; index < inlineDisplayRuns.size(); ++index)
        inlineDisplayRuns[index]->moveHorizontally(*adjustment);
}

}
}

#endif
