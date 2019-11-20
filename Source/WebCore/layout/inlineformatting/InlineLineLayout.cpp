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

#include "InlineLineBreaker.h"
#include "LayoutBox.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

static LayoutUnit inlineItemWidth(const FormattingContext& formattingContext, const InlineItem& inlineItem, LayoutUnit contentLogicalLeft)
{
    if (inlineItem.isForcedLineBreak())
        return 0;

    if (is<InlineTextItem>(inlineItem)) {
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        if (auto contentWidth = inlineTextItem.width())
            return *contentWidth;
        auto end = inlineTextItem.isCollapsible() ? inlineTextItem.start() + 1 : inlineTextItem.end();
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

LineLayout::LineInput::LineInput(const Line::InitialConstraints& initialLineConstraints, TextAlignMode horizontalAlignment, const InlineItems& inlineItems, unsigned leadingInlineItemIndex, Optional<PartialContent> leadingPartialContent)
    : initialConstraints(initialLineConstraints)
    , horizontalAlignment(horizontalAlignment)
    , inlineItems(inlineItems)
    , leadingInlineItemIndex(leadingInlineItemIndex)
    , leadingPartialContent(leadingPartialContent)
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

LineLayout::LineLayout(const InlineFormattingContext& inlineFormattingContext, Line::SkipAlignment skipAlignment, const LineInput& lineInput)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_lineInput(lineInput)
    , m_line(inlineFormattingContext, lineInput.initialConstraints, lineInput.horizontalAlignment, skipAlignment)
    , m_lineHasIntrusiveFloat(lineInput.initialConstraints.lineIsConstrainedByFloat)
{
}

LineLayout::LineContent LineLayout::layout()
{
    // Iterate through the inline content and place the inline boxes on the current line.
    // Start with the partial leading text from the previous line.
    auto firstNonPartialInlineItemIndex = m_lineInput.leadingInlineItemIndex;
    if (m_lineInput.leadingPartialContent) {
        // Handle partial inline item (split text from the previous line).
        auto& leadingTextItem = m_lineInput.inlineItems[m_lineInput.leadingInlineItemIndex];
        RELEASE_ASSERT(leadingTextItem->isText());
        // Construct a partial leading inline item.
        ASSERT(!m_leadingPartialTextItem);
        m_leadingPartialTextItem = downcast<InlineTextItem>(*leadingTextItem).right(m_lineInput.leadingPartialContent->length);
        if (placeInlineItem(*m_leadingPartialTextItem) == IsEndOfLine::Yes)
            return close();
        ++firstNonPartialInlineItemIndex;
    }

    for (auto inlineItemIndex = firstNonPartialInlineItemIndex; inlineItemIndex < m_lineInput.inlineItems.size(); ++inlineItemIndex) {
        // FIXME: We should not need to re-measure the dropped, uncommitted content when re-using them on the next line.
        if (placeInlineItem(*m_lineInput.inlineItems[inlineItemIndex]) == IsEndOfLine::Yes)
            return close();
    }
    // Check the uncommitted content whether they fit now that we know we are at a commit boundary.
    if (!m_uncommittedContent.isEmpty())
        processUncommittedContent();
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
    m_uncommittedContent.reset();
    if (!m_committedInlineItemCount)
        return LineContent { { }, { }, WTFMove(m_floats), m_line.close(), m_line.lineBox() };

    Optional<PartialContent> overflowContent;
    if (m_overflowTextLength)
        overflowContent = PartialContent { *m_overflowTextLength };
    auto trailingInlineItemIndex = m_lineInput.leadingInlineItemIndex + m_committedInlineItemCount - 1;

    auto isLastLineWithInlineContent = [&] {
        if (overflowContent)
            return Line::IsLastLineWithInlineContent::No;
        // Skip floats backwards to see if this is going to be the last line with inline content.
        for (auto i = m_lineInput.inlineItems.size(); i--;) {
            if (!m_lineInput.inlineItems[i]->isFloat())
                return i == trailingInlineItemIndex ? Line::IsLastLineWithInlineContent::Yes : Line::IsLastLineWithInlineContent::No;
        }
        // There has to be at least one non-float item.
        ASSERT_NOT_REACHED();
        return Line::IsLastLineWithInlineContent::No;
    };

    return LineContent { trailingInlineItemIndex, overflowContent, WTFMove(m_floats), m_line.close(isLastLineWithInlineContent()), m_line.lineBox() };
}

LineLayout::IsEndOfLine LineLayout::placeInlineItem(const InlineItem& inlineItem)
{
    auto currentLogicalRight = m_line.lineBox().logicalRight();
    auto itemLogicalWidth = inlineItemWidth(formattingContext(), inlineItem, currentLogicalRight);
    auto lineIsConsideredEmpty = !m_line.hasContent() && !m_lineHasIntrusiveFloat;

    // Floats are special, they are intrusive but they don't really participate in the line layout context.
    if (inlineItem.isFloat()) {
        // FIXME: It gets a bit more complicated when there's some uncommitted content whether they should be added to the current line
        // e.g. text_content<div style="float: left"></div>continuous_text_content
        // Not sure what to do when the float takes up the available space and we've got continuous content. Browser engines don't agree.
        // Let's just commit the pending content and try placing the float for now.
        if (!m_uncommittedContent.isEmpty()) {
            if (processUncommittedContent() == IsEndOfLine::Yes)
                return IsEndOfLine::Yes;
        }
        if (LineBreaker().shouldWrapFloatBox(itemLogicalWidth, m_line.availableWidth() + m_line.trailingTrimmableWidth(), lineIsConsideredEmpty))
            return IsEndOfLine::Yes;

        // This float can sit on the current line.
        auto& floatBox = inlineItem.layoutBox();
        // Shrink available space for current line and move existing inline runs.
        floatBox.isLeftFloatingPositioned() ? m_line.moveLogicalLeft(itemLogicalWidth) : m_line.moveLogicalRight(itemLogicalWidth);
        m_floats.append(makeWeakPtr(inlineItem));
        ++m_committedInlineItemCount;
        m_lineHasIntrusiveFloat = true;
        return IsEndOfLine::No;
    }
    // Forced line breaks are also special.
    if (inlineItem.isForcedLineBreak()) {
        auto isEndOfLine = !m_uncommittedContent.isEmpty() ? processUncommittedContent() : IsEndOfLine::No;
        // When the uncommitted content fits(or the line is empty), add the line break to this line as well.
        if (isEndOfLine == IsEndOfLine::No) {
            m_uncommittedContent.add(inlineItem, itemLogicalWidth);
            commitPendingContent();
        }
        return IsEndOfLine::Yes;
    }
    //
    auto isEndOfLine = IsEndOfLine::No;
    if (!m_uncommittedContent.isEmpty() && shouldProcessUncommittedContent(inlineItem))
        isEndOfLine = processUncommittedContent();
    // The current item might fit as well.
    if (isEndOfLine == IsEndOfLine::No)
        m_uncommittedContent.add(inlineItem, itemLogicalWidth);
    return isEndOfLine;
}

LineLayout::IsEndOfLine LineLayout::processUncommittedContent()
{
    // Check if the pending content fits.
    auto lineIsConsideredEmpty = !m_line.hasContent() && !m_lineHasIntrusiveFloat;
    auto breakingContext = LineBreaker().breakingContextForInlineContent(m_uncommittedContent.runs(), m_uncommittedContent.width(), m_line.availableWidth(), lineIsConsideredEmpty);
    // The uncommitted content can fully, partially fit the current line (commit/partial commit) or not at all (reset).
    if (breakingContext.contentBreak == LineBreaker::BreakingContext::ContentBreak::Keep)
        commitPendingContent();
    else if (breakingContext.contentBreak == LineBreaker::BreakingContext::ContentBreak::Split) {
        ASSERT(breakingContext.trailingPartialContent);
        ASSERT(m_uncommittedContent.runs()[breakingContext.trailingPartialContent->runIndex].inlineItem.isText());
        // Turn the uncommitted trailing run into a partial trailing run.
        auto overflowInlineTextItemIndex = breakingContext.trailingPartialContent->runIndex;
        auto& overflowInlineTextItem = downcast<InlineTextItem>(m_uncommittedContent.runs()[overflowInlineTextItemIndex].inlineItem);

        // Construct a partial trailing inline run.
        ASSERT(!m_trailingPartialTextItem);
        auto trailingContentLength = breakingContext.trailingPartialContent->length;
        m_trailingPartialTextItem = overflowInlineTextItem.left(trailingContentLength);
        m_overflowTextLength = overflowInlineTextItem.length() - trailingContentLength;
        // Keep the non-overflow part of the uncommitted runs and add the trailing partial content.
        m_uncommittedContent.trim(overflowInlineTextItemIndex);
        m_uncommittedContent.add(*m_trailingPartialTextItem, breakingContext.trailingPartialContent->logicalWidth);
        commitPendingContent();
    } else if (breakingContext.contentBreak == LineBreaker::BreakingContext::ContentBreak::Wrap)
        m_uncommittedContent.reset();
    else
        ASSERT_NOT_REACHED();
    return breakingContext.contentBreak == LineBreaker::BreakingContext::ContentBreak::Keep ? IsEndOfLine::No :IsEndOfLine::Yes;
}

bool LineLayout::shouldProcessUncommittedContent(const InlineItem& inlineItem) const
{
    // https://drafts.csswg.org/css-text-3/#line-break-details
    // Figure out if the new incoming content puts the uncommitted content on commit boundary.
    // e.g. <span>continuous</span> <- uncomitted content ->
    // [inline container start][text content][inline container end]
    // An incoming <img> box would enable us to commit the "<span>continuous</span>" content
    // while additional text content would not.
    ASSERT(!inlineItem.isFloat() && !inlineItem.isForcedLineBreak());
    ASSERT(!m_uncommittedContent.isEmpty());

    auto* lastUncomittedContent = &m_uncommittedContent.runs().last().inlineItem;
    if (inlineItem.isText()) {
        // any content' ' -> whitespace is always a commit boundary.
        if (downcast<InlineTextItem>(inlineItem).isWhitespace())
            return true;
        // <span>text -> the inline container start and the text content form an unbreakable continuous content.
        if (lastUncomittedContent->isContainerStart())
            return false;
        // </span>text -> need to check what's before the </span>.
        // text</span>text -> continuous content
        // <img></span>text -> commit bounday
        if (lastUncomittedContent->isContainerEnd()) {
            auto& runs = m_uncommittedContent.runs();
            // text</span><span></span></span>text -> check all the way back until we hit either a box or some text
            for (auto i = m_uncommittedContent.size(); i--;) {
                auto& previousInlineItem = runs[i].inlineItem;
                if (previousInlineItem.isContainerStart() || previousInlineItem.isContainerEnd())
                    continue;
                ASSERT(previousInlineItem.isText() || previousInlineItem.isBox());
                lastUncomittedContent = &previousInlineItem;
                break;
            }
            // Did not find any content (e.g. <span></span>text)
            if (lastUncomittedContent->isContainerEnd())
                return false;
        }
        // texttext -> continuous content.
        // ' 'text -> commit boundary.
        if (lastUncomittedContent->isText())
            return downcast<InlineTextItem>(*lastUncomittedContent).isWhitespace();
        // <img>text -> the inline box is on a commit boundary.
        if (lastUncomittedContent->isBox())
            return true;
        ASSERT_NOT_REACHED();
    }

    if (inlineItem.isBox()) {
        // <span><img> -> the inline container start and the content form an unbreakable continuous content.
        if (lastUncomittedContent->isContainerStart())
            return false;
        // </span><img> -> ok to commit the </span>.
        if (lastUncomittedContent->isContainerEnd())
            return true;
        // <img>text and <img><img> -> these combinations are ok to commit.
        if (lastUncomittedContent->isText() || lastUncomittedContent->isBox())
            return true;
        ASSERT_NOT_REACHED();
    }

    if (inlineItem.isContainerStart() || inlineItem.isContainerEnd()) {
        // <span><span> or </span><span> -> can't commit the previous content yet.
        if (lastUncomittedContent->isContainerStart() || lastUncomittedContent->isContainerEnd())
            return false;
        // ' '<span> -> let's commit the whitespace
        // text<span> -> but not yet the non-whitespace; we need to know what comes next (e.g. text<span>text or text<span><img>).
        if (lastUncomittedContent->isText())
            return downcast<InlineTextItem>(*lastUncomittedContent).isWhitespace();
        // <img><span> -> it's ok to commit the inline box content.
        // <img></span> -> the inline box and the closing inline container form an unbreakable continuous content.
        if (lastUncomittedContent->isBox())
            return inlineItem.isContainerStart();
        ASSERT_NOT_REACHED();
    }

    ASSERT_NOT_REACHED();
    return true;
}

void LineLayout::UncommittedContent::trim(unsigned newSize)
{
    for (auto i = m_uncommittedRuns.size(); i--;)
        m_width -= m_uncommittedRuns[i].logicalWidth;
    m_uncommittedRuns.shrink(newSize);
}


}
}

#endif
