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

#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(Line);

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

Line::Content::Run::Run(const InlineItem& inlineItem, const Display::Rect& logicalRect, TextContext textContext, bool isCollapsed, bool canBeExtended)
    : inlineItem(inlineItem)
    , logicalRect(logicalRect)
    , textContext(textContext)
    , isCollapsed(isCollapsed)
    , canBeExtended(canBeExtended)
{
}

Line::Line(const LayoutState& layoutState, LayoutUnit logicalLeft, LayoutUnit availableWidth)
    : m_layoutState(layoutState)
    , m_content(std::make_unique<Line::Content>())
    , m_logicalTopLeft(logicalLeft, 0)
    , m_lineLogicalWidth(availableWidth)
    , m_skipVerticalAligment(true)
{
}

Line::Line(const LayoutState& layoutState, const LayoutPoint& topLeft, LayoutUnit availableWidth, LayoutUnit minimumHeight, LayoutUnit baselineOffset)
    : m_layoutState(layoutState)
    , m_content(std::make_unique<Line::Content>())
    , m_logicalTopLeft(topLeft)
    , m_logicalHeight({ baselineOffset, minimumHeight - baselineOffset })
    , m_lineLogicalWidth(availableWidth)
{
}

std::unique_ptr<Line::Content> Line::close()
{
    removeTrailingTrimmableContent();
    if (!m_skipVerticalAligment) {
        // Convert inline run geometry from relative to the baseline to relative to logical top.
        for (auto& run : m_content->runs()) {
            auto adjustedLogicalTop = run->logicalRect.top() + m_logicalHeight.height + m_logicalTopLeft.y();
            run->logicalRect.setTop(adjustedLogicalTop);
        }
    }
    m_content->setLogicalRect({ logicalTop(), logicalLeft(), contentLogicalWidth(), logicalHeight() });
    return WTFMove(m_content);
}

void Line::removeTrailingTrimmableContent()
{
    // Collapse trimmable trailing content
    LayoutUnit trimmableWidth;
    for (auto* trimmableRun : m_trimmableContent) {
        trimmableRun->isCollapsed = true;
        trimmableWidth += trimmableRun->logicalRect.width();
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
    for (auto& run : m_content->runs())
        run->logicalRect.moveHorizontally(delta);
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
        trimmableWidth += trimmableRun->logicalRect.width();
    return trimmableWidth;
}

void Line::appendNonBreakableSpace(const InlineItem& inlineItem, const Display::Rect& logicalRect)
{
    m_content->runs().append(std::make_unique<Content::Run>(inlineItem, logicalRect, Content::Run::TextContext { }, false, false));
    m_contentLogicalWidth += logicalRect.width();
}

void Line::appendInlineContainerStart(const InlineItem& inlineItem, LayoutUnit logicalWidth)
{
    auto logicalRect = Display::Rect { };
    logicalRect.setLeft(contentLogicalRight());
    logicalRect.setWidth(logicalWidth);

    if (!m_skipVerticalAligment) {
        auto logicalHeight = inlineItemHeight(inlineItem);
        adjustBaselineAndLineHeight(inlineItem, logicalHeight);

        auto& displayBox = m_layoutState.displayBoxForLayoutBox(inlineItem.layoutBox());
        auto logicalTop = -inlineItem.style().fontMetrics().ascent() - displayBox.borderTop() - displayBox.paddingTop().valueOr(0);
        logicalRect.setTop(logicalTop);
        logicalRect.setHeight(logicalHeight);
    }
    appendNonBreakableSpace(inlineItem, logicalRect);
}

void Line::appendInlineContainerEnd(const InlineItem& inlineItem, LayoutUnit logicalWidth)
{
    // This is really just a placeholder to mark the end of the inline level container.
    auto logicalRect = Display::Rect { 0, contentLogicalRight(), logicalWidth, 0 };
    appendNonBreakableSpace(inlineItem, logicalRect);
}

void Line::appendTextContent(const InlineTextItem& inlineItem, LayoutUnit logicalWidth)
{
    auto isTrimmable = TextUtil::isTrimmableContent(inlineItem);
    if (!isTrimmable)
        m_trimmableContent.clear();

    auto shouldCollapseCompletely = [&] {
        if (!isTrimmable)
            return false;
        // Leading whitespace.
        auto& runs = m_content->runs();
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
    
    auto logicalRect = Display::Rect { };
    logicalRect.setLeft(contentLogicalRight());
    logicalRect.setWidth(logicalWidth);
    if (!m_skipVerticalAligment) {
        logicalRect.setTop(-inlineItem.style().fontMetrics().ascent());
        logicalRect.setHeight(inlineItemHeight(inlineItem));
    }

    auto textContext = Content::Run::TextContext { inlineItem.start(), inlineItem.isCollapsed() ? 1 : inlineItem.length() };
    auto lineItem = std::make_unique<Content::Run>(inlineItem, logicalRect, textContext, isCompletelyCollapsed, canBeExtended);
    if (isTrimmable)
        m_trimmableContent.add(lineItem.get());

    m_content->runs().append(WTFMove(lineItem));
    m_contentLogicalWidth += isCompletelyCollapsed ? LayoutUnit() : logicalWidth;
}

void Line::appendNonReplacedInlineBox(const InlineItem& inlineItem, LayoutUnit logicalWidth)
{
    auto& displayBox = m_layoutState.displayBoxForLayoutBox(inlineItem.layoutBox());
    auto horizontalMargin = displayBox.horizontalMargin();    
    auto logicalRect = Display::Rect { };

    logicalRect.setLeft(contentLogicalRight() + horizontalMargin.start);
    logicalRect.setWidth(logicalWidth);
    if (!m_skipVerticalAligment) {
        auto logicalHeight = inlineItemHeight(inlineItem);
        adjustBaselineAndLineHeight(inlineItem, logicalHeight);

        logicalRect.setTop(-logicalHeight);
        logicalRect.setHeight(logicalHeight);
    }

    m_content->runs().append(std::make_unique<Content::Run>(inlineItem, logicalRect, Content::Run::TextContext { }, false, false));
    m_contentLogicalWidth += (logicalWidth + horizontalMargin.start + horizontalMargin.end);
    m_trimmableContent.clear();
}

void Line::appendReplacedInlineBox(const InlineItem& inlineItem, LayoutUnit logicalWidth)
{
    // FIXME Surely replaced boxes behave differently.
    appendNonReplacedInlineBox(inlineItem, logicalWidth);
}

void Line::appendHardLineBreak(const InlineItem& inlineItem)
{
    auto ascent = inlineItem.layoutBox().style().fontMetrics().ascent();
    auto logicalRect = Display::Rect { -ascent, contentLogicalRight(), { }, logicalHeight() };
    m_content->runs().append(std::make_unique<Content::Run>(inlineItem, logicalRect, Content::Run::TextContext { }, false, false));
}

void Line::adjustBaselineAndLineHeight(const InlineItem& inlineItem, LayoutUnit runHeight)
{
    ASSERT(!inlineItem.isContainerEnd() && !inlineItem.isText());
    auto& layoutBox = inlineItem.layoutBox();
    auto& style = layoutBox.style();

    if (inlineItem.isContainerStart()) {
        auto& fontMetrics = style.fontMetrics();
        auto halfLeading = halfLeadingMetrics(fontMetrics, style.computedLineHeight());
        if (halfLeading.depth > 0)
            m_logicalHeight.depth = std::max(m_logicalHeight.depth, halfLeading.depth);
        if (halfLeading.height > 0)
            m_logicalHeight.height = std::max(m_logicalHeight.height, halfLeading.height);
        return;
    }
    // Replaced and non-replaced inline level box.
    // FIXME: We need to look inside the inline-block's formatting context and check the lineboxes (if any) to be able to baseline align.
    if (layoutBox.establishesInlineFormattingContext()) {
        if (runHeight == logicalHeight())
            return;
        // FIXME: This fails when the line height difference comes from font-size diff.
        m_logicalHeight.depth = std::max<LayoutUnit>(0, m_logicalHeight.depth);
        m_logicalHeight.height = std::max(runHeight, m_logicalHeight.height);
        return;
    }
    // 0 descent -> baseline aligment for now.
    m_logicalHeight.depth = std::max<LayoutUnit>(0, m_logicalHeight.depth);
    m_logicalHeight.height = std::max(runHeight, m_logicalHeight.height);
}

LayoutUnit Line::inlineItemHeight(const InlineItem& inlineItem) const
{
    ASSERT(!m_skipVerticalAligment);
    auto& fontMetrics = inlineItem.style().fontMetrics();
    if (inlineItem.isLineBreak() || is<InlineTextItem>(inlineItem))
        return fontMetrics.height();

    auto& layoutBox = inlineItem.layoutBox();
    ASSERT(m_layoutState.hasDisplayBox(layoutBox));
    auto& displayBox = m_layoutState.displayBoxForLayoutBox(layoutBox);

    if (layoutBox.isFloatingPositioned())
        return displayBox.marginBox().height();

    if (layoutBox.isReplaced())
        return displayBox.height();

    if (inlineItem.isContainerStart() || inlineItem.isContainerEnd())
        return fontMetrics.height() + displayBox.verticalBorder() + displayBox.verticalPadding().valueOr(0);

    // Non-replaced inline box (e.g. inline-block)
    return displayBox.height();
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
