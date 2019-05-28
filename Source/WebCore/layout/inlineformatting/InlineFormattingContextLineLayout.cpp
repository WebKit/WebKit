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
#include "InlineLineBreaker.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutState.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

struct UsedHeightAndDepth {
    LayoutUnit height;
    LayoutUnit depth;
};

static UsedHeightAndDepth halfLeadingMetrics(const FontMetrics& fontMetrics, LayoutUnit lineLogicalHeight)
{
    auto ascent = fontMetrics.ascent();
    auto descent = fontMetrics.descent();
    // 10.8.1 Leading and half-leading
    auto leading = lineLogicalHeight - (ascent + descent);
    // Inline tree is all integer based.
    auto adjustedAscent = std::max((ascent + leading / 2).floor(), 0);
    auto adjustedDescent = std::max((descent + leading / 2).ceil(), 0);
    return { adjustedAscent, adjustedDescent }; 
}

class Line {
public:
    Line(const LayoutState&);

    void reset(const LayoutPoint& topLeft, LayoutUnit availableWidth, LayoutUnit minimumLineHeight, LayoutUnit baselineOffset);

    struct LineItem {
        LineItem(Display::Run, const InlineItem&, bool isCollapsed, bool canBeExtended);

        // Relative to the baseline.
        Display::Run inlineRun;
        const InlineItem& inlineItem;
        bool isCollapsed { false };
        bool canBeExtended { false };
    };

    using LineItems = Vector<std::unique_ptr<LineItem>>;
    const LineItems& close();

    void appendTextContent(const InlineTextItem&, LayoutSize);
    void appendNonReplacedInlineBox(const InlineItem&, LayoutSize);
    void appendReplacedInlineBox(const InlineItem&, LayoutSize);
    void appendInlineContainerStart(const InlineItem&);
    void appendInlineContainerEnd(const InlineItem&);
    void appendHardLineBreak(const InlineItem&);

    bool hasContent() const;

    LayoutUnit trailingTrimmableWidth() const;

    void moveLogicalLeft(LayoutUnit);
    void moveLogicalRight(LayoutUnit);

    LayoutUnit availableWidth() const { return logicalWidth() - contentLogicalWidth(); }
    
    LayoutUnit contentLogicalRight() const { return logicalLeft() + contentLogicalWidth(); }
    LayoutUnit contentLogicalWidth() const { return m_contentLogicalWidth; }

    LayoutUnit logicalTop() const { return m_logicalTopLeft.y(); }
    LayoutUnit logicalLeft() const { return m_logicalTopLeft.x(); }
    LayoutUnit logicalRight() const { return logicalLeft() + logicalWidth(); }
    LayoutUnit logicalBottom() const { return logicalTop() + logicalHeight(); }
    LayoutUnit logicalWidth() const { return m_lineLogicalWidth; }
    LayoutUnit logicalHeight() const { return m_logicalHeight.height + m_logicalHeight.depth; }

private:
    void appendNonBreakableSpace(const InlineItem&, const Display::Rect& logicalRect);
    void removeTrailingTrimmableContent();

    const LayoutState& m_layoutState;
    LineItems m_lineItems;
    ListHashSet<LineItem*> m_trimmableContent;

    LayoutPoint m_logicalTopLeft;
    LayoutUnit m_contentLogicalWidth;

    UsedHeightAndDepth m_logicalHeight;
    LayoutUnit m_lineLogicalWidth;
};

Line::LineItem::LineItem(Display::Run inlineRun, const InlineItem& inlineItem, bool isCollapsed, bool canBeExtended)
    : inlineRun(inlineRun)
    , inlineItem(inlineItem)
    , isCollapsed(isCollapsed)
    , canBeExtended(canBeExtended)
{
}

Line::Line(const LayoutState& layoutState)
    : m_layoutState(layoutState)
{
}

void Line::reset(const LayoutPoint& topLeft, LayoutUnit availableWidth, LayoutUnit minimumHeight, LayoutUnit baselineOffset)
{
    m_logicalTopLeft = topLeft;
    m_lineLogicalWidth = availableWidth;
    m_logicalHeight = { baselineOffset, minimumHeight - baselineOffset };
    
    m_contentLogicalWidth = { };

    m_lineItems.clear();
    m_trimmableContent.clear();
}

const Line::LineItems& Line::close()
{
    removeTrailingTrimmableContent();
    // Convert inline run geometry from relative to the baseline to relative to logical top.
    for (auto& lineItem : m_lineItems) {
        auto adjustedLogicalTop = lineItem->inlineRun.logicalTop() + m_logicalHeight.height + m_logicalTopLeft.y();
        lineItem->inlineRun.setLogicalTop(adjustedLogicalTop);
    }
    return m_lineItems;
}

void Line::removeTrailingTrimmableContent()
{
    // Collapse trimmable trailing content
    LayoutUnit trimmableWidth;
    for (auto* trimmableRun : m_trimmableContent) {
        trimmableRun->isCollapsed = true;
        trimmableWidth += trimmableRun->inlineRun.logicalWidth();
    }
    m_contentLogicalWidth -= trimmableWidth;
}

void Line::moveLogicalLeft(LayoutUnit delta)
{
    if (!delta)
        return;
    ASSERT(delta > 0);
    // Shrink the line and move the items.
    m_logicalTopLeft.move(delta, 0);
    m_lineLogicalWidth -= delta;
    for (auto& lineItem : m_lineItems)
        lineItem->inlineRun.moveHorizontally(delta);
}

void Line::moveLogicalRight(LayoutUnit delta)
{
    ASSERT(delta > 0);
    m_lineLogicalWidth -= delta;
}

static bool isTrimmableContent(const InlineItem& inlineItem)
{
    if (!is<InlineTextItem>(inlineItem))
        return false;
    auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
    return inlineTextItem.isWhitespace() && inlineTextItem.style().collapseWhiteSpace();
}

LayoutUnit Line::trailingTrimmableWidth() const
{
    LayoutUnit trimmableWidth;
    for (auto* trimmableRun : m_trimmableContent)
        trimmableWidth += trimmableRun->inlineRun.logicalWidth();
    return trimmableWidth;
}

bool Line::hasContent() const
{
    // Return false for empty containers like <span></span>.
    if (m_lineItems.isEmpty())
        return false;
    for (auto& lineItem : m_lineItems) {
        if (lineItem->inlineItem.isContainerStart() || lineItem->inlineItem.isContainerEnd())
            continue;
        if (!lineItem->isCollapsed)
            return true;
    }
    return false;
}

void Line::appendNonBreakableSpace(const InlineItem& inlineItem, const Display::Rect& logicalRect)
{
    m_lineItems.append(std::make_unique<LineItem>(Display::Run { logicalRect }, inlineItem, false, false));
    m_contentLogicalWidth += inlineItem.width();
}

void Line::appendInlineContainerStart(const InlineItem& inlineItem)
{
    auto& layoutBox = inlineItem.layoutBox();
    auto& style = layoutBox.style();
    auto& fontMetrics = style.fontMetrics();
    
    auto alignAndAdjustLineHeight = [&] {
        LayoutUnit inlineBoxHeight = style.computedLineHeight();

        auto halfLeading = halfLeadingMetrics(fontMetrics, inlineBoxHeight);
        if (halfLeading.depth > 0)
            m_logicalHeight.depth = std::max(m_logicalHeight.depth, halfLeading.depth);
        if (halfLeading.height > 0)
            m_logicalHeight.height = std::max(m_logicalHeight.height, halfLeading.height);
    };

    alignAndAdjustLineHeight();
    auto& displayBox = m_layoutState.displayBoxForLayoutBox(layoutBox);
    auto containerHeight = fontMetrics.height() + displayBox.verticalBorder() + displayBox.verticalPadding().valueOr(0);
    auto logicalTop = -fontMetrics.ascent() - displayBox.borderTop() - displayBox.paddingTop().valueOr(0);
    auto logicalRect = Display::Rect { logicalTop, contentLogicalRight(), inlineItem.width(), containerHeight };
    appendNonBreakableSpace(inlineItem, logicalRect);
}

void Line::appendInlineContainerEnd(const InlineItem& inlineItem)
{
    // This is really just a placeholder to mark the end of the inline level container.
    auto logicalRect = Display::Rect { 0, contentLogicalRight(), inlineItem.width(), 0 };
    appendNonBreakableSpace(inlineItem, logicalRect);
}

void Line::appendTextContent(const InlineTextItem& inlineItem, LayoutSize runSize)
{
    auto isTrimmable = isTrimmableContent(inlineItem);
    if (!isTrimmable)
        m_trimmableContent.clear();

    auto shouldCollapseCompletely = [&] {
        if (!isTrimmable)
            return false;
        // Leading whitespace.
        if (m_lineItems.isEmpty())
            return true;
        // Check if the last item is trimmable as well.
        for (int index = m_lineItems.size() - 1; index >= 0; --index) {
            auto& inlineItem = m_lineItems[index]->inlineItem;
            if (inlineItem.isBox())
                return false;
            if (inlineItem.isText())
                return inlineItem.isText() && isTrimmableContent(downcast<InlineTextItem>(inlineItem));
            ASSERT(inlineItem.isContainerStart() || inlineItem.isContainerEnd());
        }
        return true;
    };

    // Collapsed line items don't contribute to the line width.
    auto isCompletelyCollapsed = shouldCollapseCompletely();
    auto canBeExtended = !isCompletelyCollapsed && !inlineItem.isCollapsed();
    auto logicalRect = Display::Rect { -inlineItem.style().fontMetrics().ascent(), contentLogicalRight(), runSize.width(), runSize.height() };
    auto textContext = Display::Run::TextContext { inlineItem.start(), inlineItem.isCollapsed() ? 1 : inlineItem.length() };
    auto displayRun = Display::Run(logicalRect, textContext);

    auto lineItem = std::make_unique<LineItem>(displayRun, inlineItem, isCompletelyCollapsed, canBeExtended);
    if (isTrimmable) 
        m_trimmableContent.add(lineItem.get());

    m_lineItems.append(WTFMove(lineItem));
    m_contentLogicalWidth += isCompletelyCollapsed ? LayoutUnit() : runSize.width();
}

void Line::appendNonReplacedInlineBox(const InlineItem& inlineItem, LayoutSize runSize)
{
    auto alignAndAdjustLineHeight = [&] {
        auto inlineBoxHeight = runSize.height();
        // FIXME: We need to look inside the inline-block's formatting context and check the lineboxes (if any) to be able to baseline align.
        if (inlineItem.layoutBox().establishesInlineFormattingContext()) {
            if (inlineBoxHeight == logicalHeight())
                return;
            // FIXME: This fails when the line height difference comes from font-size diff. 
            m_logicalHeight.depth = std::max<LayoutUnit>(0, m_logicalHeight.depth);
            m_logicalHeight.height = std::max(inlineBoxHeight, m_logicalHeight.height);
            return;
        }
        // 0 descent -> baseline aligment for now.
        m_logicalHeight.depth = std::max<LayoutUnit>(0, m_logicalHeight.depth);
        m_logicalHeight.height = std::max(inlineBoxHeight, m_logicalHeight.height);
    };

    alignAndAdjustLineHeight();
    auto& displayBox = m_layoutState.displayBoxForLayoutBox(inlineItem.layoutBox());
    auto logicalTop = -runSize.height();
    auto horizontalMargin = displayBox.horizontalMargin();
    auto logicalRect = Display::Rect { logicalTop, contentLogicalRight() + horizontalMargin.start, runSize.width(), runSize.height() };

    m_lineItems.append(std::make_unique<LineItem>(Display::Run { logicalRect }, inlineItem, false, false));
    m_contentLogicalWidth += (runSize.width() + horizontalMargin.start + horizontalMargin.end);
    m_trimmableContent.clear();
}

void Line::appendReplacedInlineBox(const InlineItem& inlineItem, LayoutSize runSize)
{
    // FIXME Surely replaced boxes behave differently.
    appendNonReplacedInlineBox(inlineItem, runSize);
}

void Line::appendHardLineBreak(const InlineItem& inlineItem)
{
    auto ascent = inlineItem.layoutBox().style().fontMetrics().ascent(); 
    auto logicalRect = Display::Rect { -ascent, contentLogicalRight(), { }, logicalHeight() };
    m_lineItems.append(std::make_unique<LineItem>(Display::Run { logicalRect }, inlineItem, false, false));
}

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

InlineFormattingContext::LineLayout::LineLayout(const InlineFormattingContext& inlineFormattingContext)
    : m_formattingContext(inlineFormattingContext)
    , m_formattingState(m_formattingContext.formattingState())
    , m_floatingState(m_formattingState.floatingState())
    , m_formattingRoot(downcast<Container>(m_formattingContext.root()))
{
}

void InlineFormattingContext::LineLayout::initializeLine(Line& line, LayoutUnit lineLogicalTop, LayoutUnit availableWidth) const
{
    auto& formattingRootDisplayBox = layoutState().displayBoxForLayoutBox(m_formattingRoot);
    auto lineLogicalLeft = formattingRootDisplayBox.contentBoxLeft();

    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    if (!m_floatingState.isEmpty()) {
        auto floatConstraints = m_floatingState.constraints({ lineLogicalTop }, m_formattingRoot);
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

    auto& formattingRootStyle = m_formattingRoot.style();
    auto mimimumLineHeight = formattingRootStyle.computedLineHeight();
    auto baselineOffset = halfLeadingMetrics(formattingRootStyle.fontMetrics(), mimimumLineHeight).height;
    line.reset({ lineLogicalLeft, lineLogicalTop }, availableWidth, mimimumLineHeight, baselineOffset);
}

unsigned InlineFormattingContext::LineLayout::createInlineRunsForLine(Line& line, unsigned startInlineItemIndex) const
{
    auto floatingContext = FloatingContext { m_floatingState };
    Optional<unsigned> lastCommittedIndex;

    UncommittedContent uncommittedContent;
    auto commitPendingContent = [&] {
        if (uncommittedContent.isEmpty())
            return;

        lastCommittedIndex = lastCommittedIndex.valueOr(startInlineItemIndex) + uncommittedContent.size();
        for (auto* uncommitted : uncommittedContent.inlineItems())
            commitInlineItemToLine(line, *uncommitted);
        uncommittedContent.reset();
    };

    LineBreaker lineBreaker(layoutState());
    // Iterate through the inline content and place the inline boxes on the current line.
    auto& inlineContent = m_formattingState.inlineItems();
    for (auto inlineItemIndex = startInlineItemIndex; inlineItemIndex < inlineContent.size(); ++inlineItemIndex) {
        auto& inlineItem = inlineContent[inlineItemIndex];
        if (inlineItem->isHardLineBreak()) {
            uncommittedContent.add(*inlineItem);
            commitPendingContent();
            return *lastCommittedIndex;
        }
        auto availableWidth = line.availableWidth() - uncommittedContent.width();
        auto currentLogicalRight = line.contentLogicalRight() + uncommittedContent.width();
        // FIXME: Ensure LineContext::trimmableWidth includes uncommitted content if needed.
        auto breakingContext = lineBreaker.breakingContext(*inlineItem, { availableWidth, currentLogicalRight, line.trailingTrimmableWidth(), !line.hasContent() });
        if (breakingContext.isAtBreakingOpportunity)
            commitPendingContent();

        // Content does not fit the current line.
        if (breakingContext.breakingBehavior == LineBreaker::BreakingBehavior::Wrap)
            return *lastCommittedIndex;

        // Partial content stays on the current line. 
        if (breakingContext.breakingBehavior == LineBreaker::BreakingBehavior::Break) {
            ASSERT(inlineItem->isText());

            ASSERT_NOT_IMPLEMENTED_YET();
            return *lastCommittedIndex;
        }

        if (inlineItem->isFloat()) {
            handleFloat(line, floatingContext, *inlineItem);
            continue;
        }

        uncommittedContent.add(*inlineItem);
        if (breakingContext.isAtBreakingOpportunity)
            commitPendingContent();
    }
    commitPendingContent();
    return *lastCommittedIndex;
}

void InlineFormattingContext::LineLayout::layout(LayoutUnit widthConstraint) const
{
    ASSERT(!m_formattingState.inlineItems().isEmpty());

    Line line(layoutState());
    initializeLine(line, layoutState().displayBoxForLayoutBox(m_formattingRoot).contentBoxTop(), widthConstraint);

    unsigned startInlineItemIndex = 0;
    while (true) {
        auto nextInlineItemIndex = createInlineRunsForLine(line, startInlineItemIndex);
        processInlineRuns(line);
        if (nextInlineItemIndex == m_formattingState.inlineItems().size())
            break;
        startInlineItemIndex = nextInlineItemIndex;
        initializeLine(line, line.logicalBottom(), widthConstraint);
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
        if (isTrimmableContent(*inlineItem)) {
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

void InlineFormattingContext::LineLayout::processInlineRuns(Line& line) const
{
    auto& lineItems = line.close();
    if (lineItems.isEmpty()) {
        // Spec tells us to create a zero height, empty line box.
        auto lineBox = Display::Rect { line.logicalTop(), line.logicalLeft(), 0 , 0 };
        m_formattingState.addLineBox({ lineBox });
        return;
    }

    auto& inlineDisplayRuns = m_formattingState.inlineRuns(); 
    Optional<unsigned> previousLineLastRunIndex = inlineDisplayRuns.isEmpty() ? Optional<unsigned>() : inlineDisplayRuns.size() - 1;
    // 9.4.2 Inline formatting contexts
    // A line box is always tall enough for all of the boxes it contains.

    // Ignore the initial strut.
    auto lineBox = Display::Rect { line.logicalTop(), line.logicalLeft(), 0 , line.hasContent() ? line.logicalHeight() : LayoutUnit { } };
    // Create final display runs.
    for (unsigned index = 0; index < lineItems.size(); ++index) {
        auto& lineItem = lineItems.at(index);

        auto& inlineItem = lineItem->inlineItem;
        auto& inlineRun = lineItem->inlineRun;
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
        const Line::LineItem* previousLineItem = !index ? nullptr : lineItems[index - 1].get();
        if (!lineItem->isCollapsed) {
            auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
            auto previousRunCanBeExtended = previousLineItem ? previousLineItem->canBeExtended : false;
            auto requiresNewRun = !index || !previousRunCanBeExtended || &layoutBox != &previousLineItem->inlineItem.layoutBox();
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
        auto firstInlineRunForLayoutBox = !previousLineItem || &previousLineItem->inlineItem.layoutBox() != &layoutBox;
        if (firstInlineRunForLayoutBox) {
            // Setup display box for the associated layout box.
            displayBox.setTopLeft(inlineRun.logicalTopLeft());
            displayBox.setContentBoxWidth(lineItem->isCollapsed ? LayoutUnit() : inlineRun.logicalWidth());
            displayBox.setContentBoxHeight(inlineRun.logicalHeight());
        } else if (!lineItem->isCollapsed) {
            // FIXME fix it for multirun/multiline.
            displayBox.setContentBoxWidth(displayBox.contentBoxWidth() + inlineRun.logicalWidth());
        }
    }
    // FIXME linebox needs to be ajusted after content alignment.
    m_formattingState.addLineBox({ lineBox });
    if (line.hasContent())
        alignRuns(m_formattingRoot.style().textAlign(), previousLineLastRunIndex.valueOr(-1) + 1, line.availableWidth());
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
