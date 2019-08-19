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

#include "InlineFormattingContext.h"
#include "TextUtil.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(Line);

Line::Content::Run::Run(const InlineItem& inlineItem, const Display::Rect& logicalRect)
    : m_layoutBox(inlineItem.layoutBox())
    , m_type(inlineItem.type())
    , m_logicalRect(logicalRect)
{
}

Line::Content::Run::Run(const InlineItem& inlineItem, const TextContext& textContext, const Display::Rect& logicalRect)
    : m_layoutBox(inlineItem.layoutBox())
    , m_type(inlineItem.type())
    , m_logicalRect(logicalRect)
    , m_textContext(textContext)
{
}

Line::Line(const LayoutState& layoutState, const InitialConstraints& initialConstraints, SkipVerticalAligment skipVerticalAligment)
    : m_layoutState(layoutState)
    , m_content(makeUnique<Line::Content>())
    , m_logicalTopLeft(initialConstraints.logicalTopLeft)
    , m_baseline({ initialConstraints.heightAndBaseline.baselineOffset, initialConstraints.heightAndBaseline.height - initialConstraints.heightAndBaseline.baselineOffset })
    , m_initialStrut(initialConstraints.heightAndBaseline.strut)
    , m_lineLogicalHeight(initialConstraints.heightAndBaseline.height)
    , m_lineLogicalWidth(initialConstraints.availableLogicalWidth)
    , m_skipVerticalAligment(skipVerticalAligment == SkipVerticalAligment::Yes)
{
}

static bool isInlineContainerConsideredEmpty(const LayoutState& layoutState, const Box& layoutBox)
{
    // Note that this does not check whether the inline container has content. It simply checks if the container itself is considered empty.
    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    return !(displayBox.horizontalBorder() || (displayBox.horizontalPadding() && displayBox.horizontalPadding().value()));
}

bool Line::isVisuallyEmpty() const
{
    // FIXME: This should be cached instead -as the inline items are being added.
    // Return true for empty inline containers like <span></span>.
    for (auto& run : m_content->runs()) {
        if (run->isContainerStart()) {
            if (!isInlineContainerConsideredEmpty(m_layoutState, run->layoutBox()))
                return false;
            continue;
        }
        if (run->isContainerEnd())
            continue;
        if (run->layoutBox().establishesFormattingContext()) {
            ASSERT(run->layoutBox().isInlineBlockBox());
            auto& displayBox = m_layoutState.displayBoxForLayoutBox(run->layoutBox());
            if (!displayBox.width())
                continue;
            if (m_skipVerticalAligment || displayBox.height())
                return false;
            continue;
        }
        if (!run->textContext() || !run->textContext()->isCollapsed)
            return false;
    }
    return true;
}

std::unique_ptr<Line::Content> Line::close()
{
    removeTrailingTrimmableContent();
    if (!m_skipVerticalAligment) {
        if (isVisuallyEmpty()) {
            m_baseline = { };
            m_baselineTop = { };
            m_lineLogicalHeight = { };
        }

        // Remove descent when all content is baseline aligned but none of them have descent.
        if (InlineFormattingContext::Quirks::lineDescentNeedsCollapsing(m_layoutState, *m_content)) {
            m_lineLogicalHeight -= m_baseline.descent;
            m_baseline.descent = { };
        }

        for (auto& run : m_content->runs()) {
            LayoutUnit logicalTop;
            auto& layoutBox = run->layoutBox();
            auto verticalAlign = layoutBox.style().verticalAlign();
            auto ascent = layoutBox.style().fontMetrics().ascent();

            switch (verticalAlign) {
            case VerticalAlign::Baseline:
                if (run->isLineBreak() || run->isText())
                    logicalTop = baselineOffset() - ascent;
                else if (run->isContainerStart()) {
                    auto& displayBox = m_layoutState.displayBoxForLayoutBox(layoutBox);
                    logicalTop = baselineOffset() - ascent - displayBox.borderTop() - displayBox.paddingTop().valueOr(0);
                } else if (layoutBox.isInlineBlockBox() && layoutBox.establishesInlineFormattingContext()) {
                    auto& formattingState = downcast<InlineFormattingState>(m_layoutState.establishedFormattingState(layoutBox));
                    // Spec makes us generate at least one line -even if it is empty.
                    ASSERT(!formattingState.lineBoxes().isEmpty());
                    auto inlineBlockBaseline = formattingState.lineBoxes().last().baseline();
                    logicalTop = baselineOffset() - inlineBlockBaseline.ascent;
                } else
                    logicalTop = baselineOffset() - run->logicalRect().height();
                break;
            case VerticalAlign::Top:
                logicalTop = { };
                break;
            case VerticalAlign::Bottom:
                logicalTop = logicalBottom() - run->logicalRect().height();
                break;
            default:
                ASSERT_NOT_IMPLEMENTED_YET();
                break;
            }
            run->adjustLogicalTop(logicalTop);
            // Convert runs from relative to the line top/left to the formatting root's border box top/left.
            run->moveVertically(this->logicalTop());
            run->moveHorizontally(this->logicalLeft());
        }
    }
    m_content->setLogicalRect({ logicalTop(), logicalLeft(), contentLogicalWidth(), logicalHeight() });
    m_content->setBaseline(m_baseline);
    m_content->setBaselineOffset(baselineOffset());
    return WTFMove(m_content);
}

void Line::removeTrailingTrimmableContent()
{
    // Collapse trimmable trailing content
    LayoutUnit trimmableWidth;
    for (auto* trimmableRun : m_trimmableContent) {
        ASSERT(trimmableRun->isText());
        trimmableRun->setTextIsCollapsed();
        trimmableWidth += trimmableRun->logicalRect().width();
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
}

void Line::moveLogicalRight(LayoutUnit delta)
{
    ASSERT(delta > 0);
    m_lineLogicalWidth -= delta;
}

LayoutUnit Line::trailingTrimmableWidth() const
{
    LayoutUnit trimmableWidth;
    for (auto* trimmableRun : m_trimmableContent) {
        ASSERT(!trimmableRun->textContext()->isCollapsed);
        trimmableWidth += trimmableRun->logicalRect().width();
    }
    return trimmableWidth;
}

void Line::append(const InlineItem& inlineItem, LayoutUnit logicalWidth)
{
    if (inlineItem.isHardLineBreak())
        return appendHardLineBreak(inlineItem);
    if (is<InlineTextItem>(inlineItem))
        return appendTextContent(downcast<InlineTextItem>(inlineItem), logicalWidth);
    if (inlineItem.isContainerStart())
        return appendInlineContainerStart(inlineItem, logicalWidth);
    if (inlineItem.isContainerEnd())
        return appendInlineContainerEnd(inlineItem, logicalWidth);
    if (inlineItem.layoutBox().replaced())
        return appendReplacedInlineBox(inlineItem, logicalWidth);
    appendNonReplacedInlineBox(inlineItem, logicalWidth);
}

void Line::appendNonBreakableSpace(const InlineItem& inlineItem, const Display::Rect& logicalRect)
{
    m_content->runs().append(makeUnique<Content::Run>(inlineItem, logicalRect));
    m_contentLogicalWidth += logicalRect.width();
}

void Line::appendInlineContainerStart(const InlineItem& inlineItem, LayoutUnit logicalWidth)
{
    auto logicalRect = Display::Rect { };
    logicalRect.setLeft(contentLogicalWidth());
    logicalRect.setWidth(logicalWidth);

    if (!m_skipVerticalAligment) {
        auto logicalHeight = inlineItemContentHeight(inlineItem);
        adjustBaselineAndLineHeight(inlineItem, logicalHeight);
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
        // Empty run.
        if (!inlineItem.length()) {
            ASSERT(!logicalWidth);
            return true;
        }
        if (!isTrimmable)
            return false;
        // Leading whitespace.
        auto& runs = m_content->runs();
        if (runs.isEmpty())
            return true;
        // Check if the last item is trimmable as well.
        for (int index = runs.size() - 1; index >= 0; --index) {
            auto& run = runs[index];
            if (run->isBox())
                return false;
            if (run->isText())
                return run->textContext()->isWhitespace && run->layoutBox().style().collapseWhiteSpace();
            ASSERT(run->isContainerStart() || run->isContainerEnd());
        }
        return true;
    };

    // Collapsed line items don't contribute to the line width.
    auto isCompletelyCollapsed = shouldCollapseCompletely();
    auto canBeExtended = !isCompletelyCollapsed && !inlineItem.isCollapsed();
    
    auto logicalRect = Display::Rect { };
    logicalRect.setLeft(contentLogicalWidth());
    logicalRect.setWidth(logicalWidth);
    if (!m_skipVerticalAligment) {
        auto runHeight = inlineItemContentHeight(inlineItem);
        logicalRect.setHeight(runHeight);
        adjustBaselineAndLineHeight(inlineItem, runHeight);
    }

    auto textContext = Content::Run::TextContext { inlineItem.start(), inlineItem.isCollapsed() ? 1 : inlineItem.length(), isCompletelyCollapsed, inlineItem.isWhitespace(), canBeExtended };
    auto lineItem = makeUnique<Content::Run>(inlineItem, textContext, logicalRect);
    if (isTrimmable && !isCompletelyCollapsed)
        m_trimmableContent.add(lineItem.get());

    m_content->runs().append(WTFMove(lineItem));
    m_contentLogicalWidth += isCompletelyCollapsed ? LayoutUnit() : logicalWidth;
}

void Line::appendNonReplacedInlineBox(const InlineItem& inlineItem, LayoutUnit logicalWidth)
{
    auto& displayBox = m_layoutState.displayBoxForLayoutBox(inlineItem.layoutBox());
    auto horizontalMargin = displayBox.horizontalMargin();    
    auto logicalRect = Display::Rect { };

    logicalRect.setLeft(contentLogicalWidth() + horizontalMargin.start);
    logicalRect.setWidth(logicalWidth);
    if (!m_skipVerticalAligment) {
        adjustBaselineAndLineHeight(inlineItem, displayBox.marginBoxHeight());
        logicalRect.setHeight(inlineItemContentHeight(inlineItem));
    }

    m_content->runs().append(makeUnique<Content::Run>(inlineItem, logicalRect));
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
    auto logicalRect = Display::Rect { };
    logicalRect.setLeft(contentLogicalWidth());
    logicalRect.setWidth({ });
    if (!m_skipVerticalAligment) {
        adjustBaselineAndLineHeight(inlineItem, { });
        logicalRect.setHeight(logicalHeight());
    }
    m_content->runs().append(makeUnique<Content::Run>(inlineItem, logicalRect));
}

void Line::adjustBaselineAndLineHeight(const InlineItem& inlineItem, LayoutUnit runHeight)
{
    ASSERT(!inlineItem.isContainerEnd());
    auto& layoutBox = inlineItem.layoutBox();
    auto& style = layoutBox.style();

    if (inlineItem.isContainerStart()) {
        // Inline containers stretch the line by their font size.
        // Vertical margins, paddings and borders don't contribute to the line height.
        auto& fontMetrics = style.fontMetrics();
        LayoutUnit contentHeight;
        if (style.verticalAlign() == VerticalAlign::Baseline) {
            auto halfLeading = halfLeadingMetrics(fontMetrics, style.computedLineHeight());
            // Both halfleading ascent and descent could be negative (tall font vs. small line-height value)
            if (halfLeading.descent > 0)
                m_baseline.descent = std::max(m_baseline.descent, halfLeading.descent);
            if (halfLeading.ascent > 0)
                m_baseline.ascent = std::max(m_baseline.ascent, halfLeading.ascent);
            contentHeight = m_baseline.height();
        } else
            contentHeight = fontMetrics.height();
        m_lineLogicalHeight = std::max(m_lineLogicalHeight, contentHeight);
        m_baselineTop = std::max(m_baselineTop, m_lineLogicalHeight - m_baseline.height());
        return;
    }

    if (inlineItem.isText() || inlineItem.isHardLineBreak()) {
        // For text content we set the baseline either through the initial strut (set by the formatting context root) or
        // through the inline container (start) -see above. Normally the text content itself does not stretch the line.
        if (!m_initialStrut)
            return;
        m_baseline.ascent = std::max(m_initialStrut->ascent, m_baseline.ascent);
        m_baseline.descent = std::max(m_initialStrut->descent, m_baseline.descent);
        m_lineLogicalHeight = std::max(m_lineLogicalHeight, m_baseline.height());
        m_baselineTop = std::max(m_baselineTop, m_lineLogicalHeight - m_baseline.height());
        m_initialStrut = { };
        return;
    }

    if (inlineItem.isBox()) {
        switch (style.verticalAlign()) {
        case VerticalAlign::Baseline: {
            auto newBaselineCandidate = LineBox::Baseline { runHeight, 0 };
            if (layoutBox.isInlineBlockBox() && layoutBox.establishesInlineFormattingContext()) {
                // Inline-blocks with inline content always have baselines.
                auto& formattingState = downcast<InlineFormattingState>(m_layoutState.establishedFormattingState(layoutBox));
                // Spec makes us generate at least one line -even if it is empty.
                ASSERT(!formattingState.lineBoxes().isEmpty());
                newBaselineCandidate = formattingState.lineBoxes().last().baseline();
            }
            m_baseline.ascent = std::max(newBaselineCandidate.ascent, m_baseline.ascent);
            m_baseline.descent = std::max(newBaselineCandidate.descent, m_baseline.descent);
            m_lineLogicalHeight = std::max(std::max(m_lineLogicalHeight, runHeight), m_baseline.height());
            // Baseline ascent/descent never shrink -> max.
            m_baselineTop = std::max(m_baselineTop, m_lineLogicalHeight - m_baseline.height());
            break;
        }
        case VerticalAlign::Top:
            // Top align content never changes the baseline offset, it only pushes the bottom of the line further down.
            m_lineLogicalHeight = std::max(runHeight, m_lineLogicalHeight);
            break;
        case VerticalAlign::Bottom:
            // Bottom aligned, tall content pushes the baseline further down from the line top.
            if (runHeight > m_lineLogicalHeight) {
                m_baselineTop += (runHeight - m_lineLogicalHeight);
                m_lineLogicalHeight = runHeight;
            }
            break;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        return;
    }
    ASSERT_NOT_REACHED();
}

LayoutUnit Line::inlineItemContentHeight(const InlineItem& inlineItem) const
{
    ASSERT(!m_skipVerticalAligment);
    auto& fontMetrics = inlineItem.style().fontMetrics();
    if (inlineItem.isLineBreak() || is<InlineTextItem>(inlineItem))
        return fontMetrics.height();

    auto& layoutBox = inlineItem.layoutBox();
    ASSERT(m_layoutState.hasDisplayBox(layoutBox));
    auto& displayBox = m_layoutState.displayBoxForLayoutBox(layoutBox);

    if (layoutBox.isFloatingPositioned())
        return displayBox.borderBoxHeight();

    if (layoutBox.replaced())
        return displayBox.borderBoxHeight();

    if (inlineItem.isContainerStart() || inlineItem.isContainerEnd())
        return fontMetrics.height() + displayBox.verticalBorder() + displayBox.verticalPadding().valueOr(0);

    // Non-replaced inline box (e.g. inline-block)
    return displayBox.borderBoxHeight();
}

LineBox::Baseline Line::halfLeadingMetrics(const FontMetrics& fontMetrics, LayoutUnit lineLogicalHeight)
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
