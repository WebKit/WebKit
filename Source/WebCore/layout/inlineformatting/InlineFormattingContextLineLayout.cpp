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

static LayoutUnit inlineItemWidth(const LayoutState& layoutState, const InlineItem& inlineItem, LayoutUnit contentLogicalLeft)
{
    if (inlineItem.isLineBreak())
        return 0;

    if (is<InlineTextItem>(inlineItem)) {
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        auto end = inlineTextItem.isCollapsed() ? inlineTextItem.start() + 1 : inlineTextItem.end();
        return TextUtil::width(inlineTextItem.layoutBox(), inlineTextItem.start(), end, contentLogicalLeft);
    }

    auto& layoutBox = inlineItem.layoutBox();
    ASSERT(layoutState.hasDisplayBox(layoutBox));
    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);

    if (layoutBox.isFloatingPositioned())
        return displayBox.marginBoxWidth();

    if (layoutBox.replaced())
        return displayBox.width();

    if (inlineItem.isContainerStart())
        return displayBox.marginStart() + displayBox.borderLeft() + displayBox.paddingLeft().valueOr(0);

    if (inlineItem.isContainerEnd())
        return displayBox.marginEnd() + displayBox.borderRight() + displayBox.paddingRight().valueOr(0);

    // Non-replaced inline box (e.g. inline-block)
    return displayBox.width();
}

struct IndexAndRange {
    unsigned index { 0 };
    struct Range {
        unsigned start { 0 };
        unsigned length { 0 };
    };
    Optional<Range> partialContext;
};

struct LineInput {
    LineInput(const Line::InitialConstraints& initialLineConstraints, Line::SkipVerticalAligment, IndexAndRange firstToProcess, const InlineItems&);

    Line::InitialConstraints initialConstraints;
    // FIXME Alternatively we could just have a second pass with vertical positioning (preferred width computation opts out) 
    Line::SkipVerticalAligment skipVerticalAligment;
    IndexAndRange firstInlineItem;
    const InlineItems& inlineItems;
    Optional<LayoutUnit> floatMinimumLogicalBottom;
};

struct LineContent {
    Optional<IndexAndRange> lastCommitted;
    Vector<WeakPtr<InlineItem>> floats;
    std::unique_ptr<Line::Content> runs;
};

class LineLayout {
public:
    LineLayout(const LayoutState&, const LineInput&);

    LineContent layout();

private:
    const LayoutState& layoutState() const { return m_layoutState; }
    enum class IsEndOfLine { No, Yes };
    IsEndOfLine placeInlineItem(const InlineItem&);
    void commitPendingContent();
    LineContent close();
    
    struct UncommittedContent {
        struct Run {
            const InlineItem& inlineItem;
            LayoutUnit logicalWidth;
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

    const LayoutState& m_layoutState;
    const LineInput& m_lineInput;
    Line m_line;
    LineBreaker m_lineBreaker;
    bool m_lineHasFloatBox { false };
    UncommittedContent m_uncommittedContent;
    unsigned m_committedInlineItemCount { 0 };
    Vector<WeakPtr<InlineItem>> m_floats;
    std::unique_ptr<InlineTextItem> m_leadingPartialInlineTextItem;
    std::unique_ptr<InlineTextItem> m_trailingPartialInlineTextItem;
};

void LineLayout::UncommittedContent::add(const InlineItem& inlineItem, LayoutUnit logicalWidth)
{
    m_uncommittedRuns.append({ inlineItem, logicalWidth });
    m_width += logicalWidth;
}

void LineLayout::UncommittedContent::reset()
{
    m_uncommittedRuns.clear();
    m_width = 0;
}

LineLayout::LineLayout(const LayoutState& layoutState, const LineInput& lineInput)
    : m_layoutState(layoutState)
    , m_lineInput(lineInput)
    , m_line(layoutState, lineInput.initialConstraints, lineInput.skipVerticalAligment)
    , m_lineHasFloatBox(lineInput.floatMinimumLogicalBottom.hasValue())
{
}

void LineLayout::commitPendingContent()
{
    if (m_uncommittedContent.isEmpty())
        return;
    m_committedInlineItemCount += m_uncommittedContent.size();
    for (auto& uncommittedRun : m_uncommittedContent.runs())
        m_line.append(uncommittedRun.inlineItem, uncommittedRun.logicalWidth);
    m_uncommittedContent.reset();
}

LineContent LineLayout::close()
{
    ASSERT(m_committedInlineItemCount || m_lineHasFloatBox);
    if (!m_committedInlineItemCount)
        return LineContent { WTF::nullopt, WTFMove(m_floats), m_line.close() };

    auto lastInlineItemIndex = m_lineInput.firstInlineItem.index + m_committedInlineItemCount - 1;
    Optional<IndexAndRange::Range> partialContext;
    if (m_trailingPartialInlineTextItem)
        partialContext = IndexAndRange::Range { m_trailingPartialInlineTextItem->start(), m_trailingPartialInlineTextItem->length() };

    auto lastCommitedItem = IndexAndRange { lastInlineItemIndex, partialContext };
    return LineContent { lastCommitedItem, WTFMove(m_floats), m_line.close() };
}

LineLayout::IsEndOfLine LineLayout::placeInlineItem(const InlineItem& inlineItem)
{
    auto availableWidth = m_line.availableWidth() - m_uncommittedContent.width();
    auto currentLogicalRight = m_line.contentLogicalRight() + m_uncommittedContent.width();
    auto itemLogicalWidth = inlineItemWidth(layoutState(), inlineItem, currentLogicalRight);

    // FIXME: Ensure LineContext::trimmableWidth includes uncommitted content if needed.
    auto lineIsConsideredEmpty = !m_line.hasContent() && !m_lineHasFloatBox;
    auto breakingContext = m_lineBreaker.breakingContext(inlineItem, itemLogicalWidth, { availableWidth, currentLogicalRight, m_line.trailingTrimmableWidth(), lineIsConsideredEmpty });
    if (breakingContext.isAtBreakingOpportunity)
        commitPendingContent();

    // Content does not fit the current line.
    if (breakingContext.breakingBehavior == LineBreaker::BreakingBehavior::Wrap)
        return IsEndOfLine::Yes;

    // Partial content stays on the current line.
    if (breakingContext.breakingBehavior == LineBreaker::BreakingBehavior::Split) {
        ASSERT(inlineItem.isText());
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        auto splitData = TextUtil::split(inlineTextItem.layoutBox(), inlineTextItem.start(), inlineTextItem.length(), itemLogicalWidth, availableWidth, currentLogicalRight);
        // Construct a partial trailing inline item.
        ASSERT(!m_trailingPartialInlineTextItem);
        m_trailingPartialInlineTextItem = inlineTextItem.split(splitData.start, splitData.length);
        m_uncommittedContent.add(*m_trailingPartialInlineTextItem, splitData.logicalWidth);
        commitPendingContent();
        return IsEndOfLine::Yes;
    }

    ASSERT(breakingContext.breakingBehavior == LineBreaker::BreakingBehavior::Keep);
    if (inlineItem.isFloat()) {
        auto& floatBox = inlineItem.layoutBox();
        ASSERT(layoutState().hasDisplayBox(floatBox));
        // Shrink available space for current line and move existing inline runs.
        auto floatBoxWidth = layoutState().displayBoxForLayoutBox(floatBox).marginBoxWidth();
        floatBox.isLeftFloatingPositioned() ? m_line.moveLogicalLeft(floatBoxWidth) : m_line.moveLogicalRight(floatBoxWidth);
        m_floats.append(makeWeakPtr(inlineItem));
        ++m_committedInlineItemCount;
        m_lineHasFloatBox = true;
        return IsEndOfLine::No;
    }

    m_uncommittedContent.add(inlineItem, itemLogicalWidth);
    if (breakingContext.isAtBreakingOpportunity)
        commitPendingContent();

    return inlineItem.isHardLineBreak() ? IsEndOfLine::Yes : IsEndOfLine::No;
}

LineContent LineLayout::layout()
{
    // Iterate through the inline content and place the inline boxes on the current line.
    // Start with the partial text from the previous line.
    auto firstInlineItem = m_lineInput.firstInlineItem;
    unsigned firstNonPartialIndex = firstInlineItem.index;
    if (firstInlineItem.partialContext) {
        // Handle partial inline item (split text from the previous line).
        auto& originalTextItem = m_lineInput.inlineItems[firstInlineItem.index];
        RELEASE_ASSERT(originalTextItem->isText());

        auto textRange = *firstInlineItem.partialContext;
        // Construct a partial leading inline item.
        ASSERT(!m_leadingPartialInlineTextItem);
        m_leadingPartialInlineTextItem = downcast<InlineTextItem>(*originalTextItem).split(textRange.start, textRange.length);
        if (placeInlineItem(*m_leadingPartialInlineTextItem) == IsEndOfLine::Yes)
            return close();
        ++firstNonPartialIndex;
    }

    for (auto inlineItemIndex = firstNonPartialIndex; inlineItemIndex < m_lineInput.inlineItems.size(); ++inlineItemIndex) {
        if (placeInlineItem(*m_lineInput.inlineItems[inlineItemIndex]) == IsEndOfLine::Yes)
            return close();
    }
    commitPendingContent();
    return close();
}

LineInput::LineInput(const Line::InitialConstraints& initialLineConstraints, Line::SkipVerticalAligment skipVerticalAligment, IndexAndRange firstToProcess, const InlineItems& inlineItems)
    : initialConstraints(initialLineConstraints)
    , skipVerticalAligment(skipVerticalAligment)
    , firstInlineItem(firstToProcess)
    , inlineItems(inlineItems)
{
}

InlineFormattingContext::InlineLayout::InlineLayout(const InlineFormattingContext& inlineFormattingContext)
    : m_layoutState(inlineFormattingContext.layoutState())
    , m_formattingRoot(downcast<Container>(inlineFormattingContext.root()))
{
}

void InlineFormattingContext::InlineLayout::layout(const InlineItems& inlineItems, LayoutUnit widthConstraint) const
{
    auto& formattingRootDisplayBox = layoutState().displayBoxForLayoutBox(m_formattingRoot);
    auto& floatingState = layoutState().establishedFormattingState(m_formattingRoot).floatingState();

    auto lineLogicalTop = formattingRootDisplayBox.contentBoxTop();
    auto lineLogicalLeft = formattingRootDisplayBox.contentBoxLeft();

    auto applyFloatConstraint = [&](auto& lineInput) {
        // Check for intruding floats and adjust logical left/available width for this line accordingly.
        if (floatingState.isEmpty())
            return;
        auto availableWidth = lineInput.initialConstraints.availableLogicalWidth;
        auto lineLogicalLeft = lineInput.initialConstraints.logicalTopLeft.x();
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
        lineInput.initialConstraints.availableLogicalWidth = availableWidth;
        lineInput.initialConstraints.logicalTopLeft.setX(lineLogicalLeft);
    };

    IndexAndRange currentInlineItem;
    while (currentInlineItem.index < inlineItems.size()) {
        auto lineInput = LineInput { { { lineLogicalLeft, lineLogicalTop }, widthConstraint, Quirks::lineHeightConstraints(layoutState(), m_formattingRoot) }, Line::SkipVerticalAligment::No, currentInlineItem, inlineItems };
        applyFloatConstraint(lineInput);
        auto lineContent = LineLayout(layoutState(), lineInput).layout();
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

LayoutUnit InlineFormattingContext::InlineLayout::computedIntrinsicWidth(const InlineItems& inlineItems, LayoutUnit widthConstraint) const
{
    LayoutUnit maximumLineWidth;
    IndexAndRange currentInlineItem;
    while (currentInlineItem.index < inlineItems.size()) {
        auto lineContent = LineLayout(layoutState(), { { { }, widthConstraint, Quirks::lineHeightConstraints(layoutState(), m_formattingRoot) }, Line::SkipVerticalAligment::Yes, currentInlineItem, inlineItems }).layout();
        currentInlineItem = { lineContent.lastCommitted->index + 1, WTF::nullopt };
        LayoutUnit floatsWidth;
        for (auto& floatItem : lineContent.floats)
            floatsWidth += layoutState().displayBoxForLayoutBox(floatItem->layoutBox()).marginBoxWidth();
        maximumLineWidth = std::max(maximumLineWidth, floatsWidth + lineContent.runs->logicalWidth());
    }
    return maximumLineWidth;
}

void InlineFormattingContext::InlineLayout::createDisplayRuns(const Line::Content& lineContent, const Vector<WeakPtr<InlineItem>>& floats, LayoutUnit widthConstraint) const
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
            formattingState.addInlineRun(makeUnique<Display::Run>(logicalRect));
            continue;
        }

        // Inline level box (replaced or inline-block)
        if (lineRun->isBox()) {
            auto topLeft = logicalRect.topLeft();
            if (layoutBox.isInFlowPositioned())
                topLeft += Geometry::inFlowPositionedPositionOffset(layoutState(), layoutBox);
            displayBox.setTopLeft(topLeft);
            lineBoxRect.expandHorizontally(logicalRect.width());
            formattingState.addInlineRun(makeUnique<Display::Run>(logicalRect));
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
                formattingState.addInlineRun(makeUnique<Display::Run>(logicalRect, Display::Run::TextContext { textContext->start, textContext->length }));
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

void InlineFormattingContext::InlineLayout::alignRuns(TextAlignMode textAlign, InlineRuns& inlineDisplayRuns, unsigned firstRunIndex, LayoutUnit availableWidth) const
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
