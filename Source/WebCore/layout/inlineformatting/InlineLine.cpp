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
#include "InlineLine.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

namespace WebCore {
namespace Layout {

bool Line::Content::isVisuallyEmpty() const
{
    // Return true for empty inline containers like <span></span>.
    for (auto& run : m_runs) {
        if (run->inlineItem.isContainerStart() || run->inlineItem.isContainerEnd())
            continue;
        if (!run->isCollapsed)
            return false;
    }
    return true;
}

Line::Content::Run::Run(Display::Run inlineRun, const InlineItem& inlineItem, bool isCollapsed, bool canBeExtended)
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

    m_content = { };

    m_trimmableContent.clear();
}

const Line::Content& Line::close()
{
    removeTrailingTrimmableContent();
    // Convert inline run geometry from relative to the baseline to relative to logical top.
    for (auto& run : m_content.runs()) {
        auto adjustedLogicalTop = run->inlineRun.logicalTop() + m_logicalHeight.height + m_logicalTopLeft.y();
        run->inlineRun.setLogicalTop(adjustedLogicalTop);
    }
    m_content.setLogicalRect({ logicalTop(), logicalLeft(), contentLogicalWidth(), logicalHeight() });
    return m_content;
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
    for (auto& run : m_content.runs())
        run->inlineRun.moveHorizontally(delta);
}

void Line::moveLogicalRight(LayoutUnit delta)
{
    ASSERT(delta > 0);
    m_lineLogicalWidth -= delta;
}

LayoutUnit Line::trailingTrimmableWidth() const
{
    LayoutUnit trimmableWidth;
    for (auto* trimmableRun : m_trimmableContent)
        trimmableWidth += trimmableRun->inlineRun.logicalWidth();
    return trimmableWidth;
}

void Line::appendNonBreakableSpace(const InlineItem& inlineItem, const Display::Rect& logicalRect)
{
    m_content.runs().append(std::make_unique<Content::Run>(Display::Run { logicalRect }, inlineItem, false, false));
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
    auto isTrimmable = TextUtil::isTrimmableContent(inlineItem);
    if (!isTrimmable)
        m_trimmableContent.clear();

    auto shouldCollapseCompletely = [&] {
        if (!isTrimmable)
            return false;
        // Leading whitespace.
        auto& runs = m_content.runs();
        if (runs.isEmpty())
            return true;
        // Check if the last item is trimmable as well.
        for (int index = runs.size() - 1; index >= 0; --index) {
            auto& inlineItem = runs[index]->inlineItem;
            if (inlineItem.isBox())
                return false;
            if (inlineItem.isText())
                return TextUtil::isTrimmableContent(inlineItem);
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

    auto lineItem = std::make_unique<Content::Run>(displayRun, inlineItem, isCompletelyCollapsed, canBeExtended);
    if (isTrimmable)
        m_trimmableContent.add(lineItem.get());

    m_content.runs().append(WTFMove(lineItem));
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

    m_content.runs().append(std::make_unique<Content::Run>(Display::Run { logicalRect }, inlineItem, false, false));
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
    m_content.runs().append(std::make_unique<Content::Run>(Display::Run { logicalRect }, inlineItem, false, false));
}

Line::UsedHeightAndDepth Line::halfLeadingMetrics(const FontMetrics& fontMetrics, LayoutUnit lineLogicalHeight)
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

}
}

#endif
