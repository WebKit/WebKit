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
#include "LineLayoutContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "InlineLineBreaker.h"
#include "LayoutBox.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

static LayoutUnit inlineItemWidth(const FormattingContext& formattingContext, const InlineItem& inlineItem, LayoutUnit contentLogicalLeft)
{
    if (inlineItem.isLineBreak())
        return 0_lu;

    if (is<InlineTextItem>(inlineItem)) {
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        auto contentWidth = inlineTextItem.width();
        if (!contentWidth) {
            auto end = inlineTextItem.isCollapsible() ? inlineTextItem.start() + 1 : inlineTextItem.end();
            contentWidth = TextUtil::width(inlineTextItem.layoutBox(), inlineTextItem.start(), end, contentLogicalLeft);
        }
        auto wordSpacing = inlineTextItem.isWhitespace() ? LayoutUnit(inlineTextItem.style().fontCascade().wordSpacing()) : 0_lu;
        return *contentWidth + wordSpacing;
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

LineLayoutContext::LineLayoutContext(const InlineFormattingContext& inlineFormattingContext, const Container& formattingContextRoot, const InlineItems& inlineItems)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_formattingContextRoot(formattingContextRoot)
    , m_inlineItems(inlineItems)
{
}

LineLayoutContext::LineContent LineLayoutContext::layoutLine(LineBuilder& line, unsigned leadingInlineItemIndex, Optional<unsigned> partialLeadingContentLength)
{
    auto initialize = [&] {
        m_committedInlineItemCount = 0;
        m_uncommittedContent.reset();
        m_partialLeadingTextItem = { };
        m_partialTrailingTextItem = { };
        m_partialContent = { };
    };
    initialize();
    // Iterate through the inline content and place the inline boxes on the current line.
    // Start with the partial leading text from the previous line.
    auto firstNonPartialInlineItemIndex = leadingInlineItemIndex;
    if (partialLeadingContentLength) {
        // Handle partial inline item (split text from the previous line).
        auto& leadingTextItem = m_inlineItems[leadingInlineItemIndex];
        RELEASE_ASSERT(leadingTextItem->isText());
        // Construct a partial leading inline item.
        ASSERT(!m_partialLeadingTextItem);
        m_partialLeadingTextItem = downcast<InlineTextItem>(*leadingTextItem).right(*partialLeadingContentLength);
        if (placeInlineItem(line, *m_partialLeadingTextItem) == IsEndOfLine::Yes)
            return close(line, leadingInlineItemIndex);
        ++firstNonPartialInlineItemIndex;
    }

    for (auto inlineItemIndex = firstNonPartialInlineItemIndex; inlineItemIndex < m_inlineItems.size(); ++inlineItemIndex) {
        // FIXME: We should not need to re-measure the dropped, uncommitted content when re-using them on the next line.
        if (placeInlineItem(line, *m_inlineItems[inlineItemIndex]) == IsEndOfLine::Yes)
            return close(line, leadingInlineItemIndex);
    }
    // Check the uncommitted content whether they fit now that we know we are at a commit boundary.
    if (!m_uncommittedContent.isEmpty())
        processUncommittedContent(line);
    return close(line, leadingInlineItemIndex);
}

void LineLayoutContext::commitPendingContent(LineBuilder& line)
{
    if (m_uncommittedContent.isEmpty())
        return;
    m_committedInlineItemCount += m_uncommittedContent.size();
    for (auto& uncommittedRun : m_uncommittedContent.runs())
        line.append(uncommittedRun.inlineItem, uncommittedRun.logicalWidth);
    m_uncommittedContent.reset();
}

LineLayoutContext::LineContent LineLayoutContext::close(LineBuilder& line, unsigned leadingInlineItemIndex)
{
    ASSERT(m_committedInlineItemCount || line.hasIntrusiveFloat());
    m_uncommittedContent.reset();
    if (!m_committedInlineItemCount)
        return LineContent { { }, { }, WTFMove(m_floats), line.close(), line.lineBox() };

    auto trailingInlineItemIndex = leadingInlineItemIndex + m_committedInlineItemCount - 1;
    auto isLastLineWithInlineContent = [&] {
        if (m_partialContent)
            return LineBuilder::IsLastLineWithInlineContent::No;
        // Skip floats backwards to see if this is going to be the last line with inline content.
        for (auto i = m_inlineItems.size(); i--;) {
            if (!m_inlineItems[i]->isFloat())
                return i == trailingInlineItemIndex ? LineBuilder::IsLastLineWithInlineContent::Yes : LineBuilder::IsLastLineWithInlineContent::No;
        }
        // There has to be at least one non-float item.
        ASSERT_NOT_REACHED();
        return LineBuilder::IsLastLineWithInlineContent::No;
    };

    return LineContent { trailingInlineItemIndex, m_partialContent, WTFMove(m_floats), line.close(isLastLineWithInlineContent()), line.lineBox() };
}

LineLayoutContext::IsEndOfLine LineLayoutContext::placeInlineItem(LineBuilder& line, const InlineItem& inlineItem)
{
    auto currentLogicalRight = line.lineBox().logicalRight();
    auto itemLogicalWidth = inlineItemWidth(formattingContext(), inlineItem, currentLogicalRight);

    // Floats are special, they are intrusive but they don't really participate in the line layout context.
    if (inlineItem.isFloat()) {
        // FIXME: It gets a bit more complicated when there's some uncommitted content whether they should be added to the current line
        // e.g. text_content<div style="float: left"></div>continuous_text_content
        // Not sure what to do when the float takes up the available space and we've got continuous content. Browser engines don't agree.
        // Let's just commit the pending content and try placing the float for now.
        if (!m_uncommittedContent.isEmpty()) {
            if (processUncommittedContent(line) == IsEndOfLine::Yes)
                return IsEndOfLine::Yes;
        }
        auto lineIsConsideredEmpty = line.isVisuallyEmpty() && !line.hasIntrusiveFloat();
        if (LineBreaker().shouldWrapFloatBox(itemLogicalWidth, line.availableWidth() + line.trailingTrimmableWidth(), lineIsConsideredEmpty))
            return IsEndOfLine::Yes;

        // This float can sit on the current line.
        auto& floatBox = inlineItem.layoutBox();
        // Shrink available space for current line and move existing inline runs.
        floatBox.isLeftFloatingPositioned() ? line.moveLogicalLeft(itemLogicalWidth) : line.moveLogicalRight(itemLogicalWidth);
        m_floats.append(makeWeakPtr(inlineItem));
        ++m_committedInlineItemCount;
        line.setHasIntrusiveFloat();
        return IsEndOfLine::No;
    }
    // Line breaks are also special.
    if (inlineItem.isLineBreak()) {
        auto isEndOfLine = !m_uncommittedContent.isEmpty() ? processUncommittedContent(line) : IsEndOfLine::No;
        // When the uncommitted content fits(or the line is empty), add the line break to this line as well.
        if (isEndOfLine == IsEndOfLine::No) {
            m_uncommittedContent.append(inlineItem, itemLogicalWidth);
            commitPendingContent(line);
        }
        return IsEndOfLine::Yes;
    }
    //
    auto isEndOfLine = IsEndOfLine::No;
    // Can we commit the pending content already?
    if (LineBreaker::Content::isAtContentBoundary(inlineItem, m_uncommittedContent))
        isEndOfLine = processUncommittedContent(line);
    // The current item might fit as well.
    if (isEndOfLine == IsEndOfLine::No)
        m_uncommittedContent.append(inlineItem, itemLogicalWidth);
    return isEndOfLine;
}

LineLayoutContext::IsEndOfLine LineLayoutContext::processUncommittedContent(LineBuilder& line)
{
    auto shouldDisableHyphenation = [&] {
        auto& style = m_formattingContextRoot.style();
        unsigned limitLines = style.hyphenationLimitLines() == RenderStyle::initialHyphenationLimitLines() ? std::numeric_limits<unsigned>::max() : style.hyphenationLimitLines();
        return m_successiveHyphenatedLineCount >= limitLines;
    };

    // Check if the pending content fits.
    auto lineIsConsideredEmpty = line.isVisuallyEmpty() && !line.hasIntrusiveFloat();
    auto lineBreaker = LineBreaker { };
    if (shouldDisableHyphenation())
        lineBreaker.setHyphenationDisabled();
    auto lineStatus = LineBreaker::LineStatus { line.availableWidth(), line.trailingTrimmableWidth(), line.isTrailingRunFullyTrimmable(), lineIsConsideredEmpty };
    auto breakingContext = lineBreaker.breakingContextForInlineContent(m_uncommittedContent, lineStatus);
    // The uncommitted content can fully, partially fit the current line (commit/partial commit) or not at all (reset).
    if (breakingContext.contentBreak == LineBreaker::BreakingContext::ContentBreak::Keep)
        commitPendingContent(line);
    else if (breakingContext.contentBreak == LineBreaker::BreakingContext::ContentBreak::Split) {
        ASSERT(breakingContext.partialTrailingContent);
        ASSERT(m_uncommittedContent.runs()[breakingContext.partialTrailingContent->runIndex].inlineItem.isText());
        // Turn the uncommitted trailing run into a partial trailing run.
        auto overflowInlineTextItemIndex = breakingContext.partialTrailingContent->runIndex;
        auto& overflowInlineTextItem = downcast<InlineTextItem>(m_uncommittedContent.runs()[overflowInlineTextItemIndex].inlineItem);

        // Construct a partial trailing inline run.
        ASSERT(!m_partialTrailingTextItem);
        auto trailingContentLength = breakingContext.partialTrailingContent->length;
        m_partialTrailingTextItem = overflowInlineTextItem.left(trailingContentLength);
        m_partialContent = LineContent::PartialContent { breakingContext.partialTrailingContent->needsHyphen, overflowInlineTextItem.length() - trailingContentLength };
        // Keep the non-overflow part of the uncommitted runs and add the trailing partial content.
        m_uncommittedContent.trim(overflowInlineTextItemIndex);
        m_uncommittedContent.append(*m_partialTrailingTextItem, breakingContext.partialTrailingContent->logicalWidth);
        commitPendingContent(line);
    } else if (breakingContext.contentBreak == LineBreaker::BreakingContext::ContentBreak::Wrap)
        m_uncommittedContent.reset();
    else
        ASSERT_NOT_REACHED();
    // Adjust hyphenated line count
    m_successiveHyphenatedLineCount = breakingContext.partialTrailingContent && breakingContext.partialTrailingContent->needsHyphen ? m_successiveHyphenatedLineCount + 1 : 0;
    return breakingContext.contentBreak == LineBreaker::BreakingContext::ContentBreak::Keep ? IsEndOfLine::No :IsEndOfLine::Yes;
}

}
}

#endif
