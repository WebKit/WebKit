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
#include "InlineLineLayout.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FloatingContext.h"
#include "FloatingState.h"
#include "InlineFormattingState.h"
#include "LayoutBox.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

static LayoutUnit inlineItemWidth(const FormattingContext& formattingContext, const InlineItem& inlineItem, LayoutUnit contentLogicalLeft)
{
    if (inlineItem.isLineBreak())
        return 0;

    if (is<InlineTextItem>(inlineItem)) {
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        auto end = inlineTextItem.isCollapsed() ? inlineTextItem.start() + 1 : inlineTextItem.end();
        return TextUtil::width(inlineTextItem.layoutBox(), inlineTextItem.start(), end, contentLogicalLeft);
    }

    auto& layoutBox = inlineItem.layoutBox();
    auto& boxGeometry = formattingContext.geometryForBox(layoutBox);

    if (layoutBox.isFloatingPositioned())
        return boxGeometry.marginBoxWidth();

    if (layoutBox.replaced())
        return boxGeometry.width();

    if (inlineItem.isContainerStart())
        return boxGeometry.marginStart() + boxGeometry.borderLeft() + boxGeometry.paddingLeft().valueOr(0);

    if (inlineItem.isContainerEnd())
        return boxGeometry.marginEnd() + boxGeometry.borderRight() + boxGeometry.paddingRight().valueOr(0);

    // Non-replaced inline box (e.g. inline-block)
    return boxGeometry.width();
}

LineLayout::LineInput::LineInput(const Line::InitialConstraints& initialLineConstraints, TextAlignMode horizontalAlignment, IndexAndRange firstToProcess, const InlineItems& inlineItems)
    : initialConstraints(initialLineConstraints)
    , horizontalAlignment(horizontalAlignment)
    , firstInlineItem(firstToProcess)
    , inlineItems(inlineItems)
{
}

LineLayout::LineInput::LineInput(const Line::InitialConstraints& initialLineConstraints, IndexAndRange firstToProcess, const InlineItems& inlineItems)
    : initialConstraints(initialLineConstraints)
    , skipAlignment(Line::SkipAlignment::Yes)
    , firstInlineItem(firstToProcess)
    , inlineItems(inlineItems)
{
}

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

LineLayout::LineLayout(const InlineFormattingContext& inlineFormattingContext, const LineInput& lineInput)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_lineInput(lineInput)
    , m_line(inlineFormattingContext, lineInput.initialConstraints, lineInput.horizontalAlignment, lineInput.skipAlignment)
    , m_lineHasIntrusiveFloat(lineInput.initialConstraints.lineIsConstrainedByFloat)
{
}

LineLayout::LineContent LineLayout::layout()
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

void LineLayout::commitPendingContent()
{
    if (m_uncommittedContent.isEmpty())
        return;
    m_committedInlineItemCount += m_uncommittedContent.size();
    for (auto& uncommittedRun : m_uncommittedContent.runs())
        m_line.append(uncommittedRun.inlineItem, uncommittedRun.logicalWidth);
    m_uncommittedContent.reset();
}

LineLayout::LineContent LineLayout::close()
{
    ASSERT(m_committedInlineItemCount || m_lineHasIntrusiveFloat);
    if (!m_committedInlineItemCount)
        return LineContent { WTF::nullopt, WTFMove(m_floats), m_line.close(), m_line.lineBox() };

    auto lastInlineItemIndex = m_lineInput.firstInlineItem.index + m_committedInlineItemCount - 1;
    Optional<IndexAndRange::Range> partialContext;
    if (m_trailingPartialInlineTextItem)
        partialContext = IndexAndRange::Range { m_trailingPartialInlineTextItem->start(), m_trailingPartialInlineTextItem->length() };

    auto lastCommitedItem = IndexAndRange { lastInlineItemIndex, partialContext };
    return LineContent { lastCommitedItem, WTFMove(m_floats), m_line.close(), m_line.lineBox() };
}

LineLayout::IsEndOfLine LineLayout::placeInlineItem(const InlineItem& inlineItem)
{
    auto availableWidth = m_line.availableWidth() - m_uncommittedContent.width();
    auto currentLogicalRight = m_line.lineBox().logicalRight() + m_uncommittedContent.width();
    auto itemLogicalWidth = inlineItemWidth(formattingContext(), inlineItem, currentLogicalRight);

    // FIXME: Ensure LineContext::trimmableWidth includes uncommitted content if needed.
    auto lineIsConsideredEmpty = !m_line.hasContent() && !m_lineHasIntrusiveFloat;
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
        // Shrink available space for current line and move existing inline runs.
        auto floatBoxWidth = formattingContext().geometryForBox(floatBox).marginBoxWidth();
        floatBox.isLeftFloatingPositioned() ? m_line.moveLogicalLeft(floatBoxWidth) : m_line.moveLogicalRight(floatBoxWidth);
        m_floats.append(makeWeakPtr(inlineItem));
        ++m_committedInlineItemCount;
        m_lineHasIntrusiveFloat = true;
        return IsEndOfLine::No;
    }

    m_uncommittedContent.add(inlineItem, itemLogicalWidth);
    if (breakingContext.isAtBreakingOpportunity)
        commitPendingContent();

    return inlineItem.isHardLineBreak() ? IsEndOfLine::Yes : IsEndOfLine::No;
}

}
}

#endif
