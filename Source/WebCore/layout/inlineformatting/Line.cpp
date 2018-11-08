/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "InlineFormattingState.h"
#include "InlineRunProvider.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

namespace WebCore {
namespace Layout {

InlineFormattingContext::Line::Line(InlineFormattingState& formattingState, const Box& formattingRoot)
    : m_formattingState(formattingState)
    , m_formattingRoot(formattingRoot)
    , m_alignmentIsJustify(m_formattingRoot.style().textAlign() == TextAlignMode::Justify)
{
}

void InlineFormattingContext::Line::init(const Display::Box::Rect& logicalRect)
{
    m_logicalRect = logicalRect;
    m_availableWidth = logicalRect.width();

    m_firstRunIndex = { };
    m_lastRunIsWhitespace = false;
    m_lastRunCanExpand = false;
    m_trailingTrimmableContent = { };
    m_closed = false;
}

void InlineFormattingContext::Line::adjustLogicalLeft(LayoutUnit delta)
{
    ASSERT(delta > 0);

    m_availableWidth -= delta;
    m_logicalRect.shiftLeftTo(m_logicalRect.left() + delta);

    if (!m_firstRunIndex)
        return;

    auto& inlineRuns = m_formattingState.inlineRuns();
    for (auto runIndex = *m_firstRunIndex; runIndex < inlineRuns.size(); ++runIndex)
        inlineRuns[runIndex].moveHorizontally(delta);
}

void InlineFormattingContext::Line::adjustLogicalRight(LayoutUnit delta)
{
    ASSERT(delta > 0);

    m_availableWidth -= delta;
    m_logicalRect.shiftRightTo(m_logicalRect.right() - delta);
}

static LayoutUnit adjustedLineLogicalLeft(TextAlignMode align, LayoutUnit lineLogicalLeft, LayoutUnit remainingWidth)
{
    switch (align) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
    case TextAlignMode::Start:
        return lineLogicalLeft;
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
    case TextAlignMode::End:
        return lineLogicalLeft + std::max(remainingWidth, LayoutUnit());
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        return lineLogicalLeft + std::max(remainingWidth / 2, LayoutUnit());
    case TextAlignMode::Justify:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return lineLogicalLeft;
}

static bool isTrimmableContent(const InlineRunProvider::Run& inlineRun)
{
    return inlineRun.isWhitespace() && inlineRun.style().collapseWhiteSpace();
}

LayoutUnit InlineFormattingContext::Line::contentLogicalRight()
{
    if (!m_firstRunIndex.has_value())
        return m_logicalRect.left();

    return m_formattingState.inlineRuns().last().logicalRight();
}

void InlineFormattingContext::Line::computeExpansionOpportunities(const InlineLineBreaker::Run& run)
{
    if (!m_alignmentIsJustify)
        return;

    auto isExpansionOpportunity = [](auto currentRunIsWhitespace, auto lastRunIsWhitespace) {
        return currentRunIsWhitespace || (!currentRunIsWhitespace && !lastRunIsWhitespace);
    };

    auto expansionBehavior = [](auto isAtExpansionOpportunity) {
        ExpansionBehavior expansionBehavior = AllowTrailingExpansion;
        expansionBehavior |= isAtExpansionOpportunity ? ForbidLeadingExpansion : AllowLeadingExpansion;
        return expansionBehavior;
    };

    auto isAtExpansionOpportunity = isExpansionOpportunity(run.content.isWhitespace(), m_lastRunIsWhitespace);

    auto& currentInlineRun = m_formattingState.inlineRuns().last();
    auto& expansionOpportunity = currentInlineRun.expansionOpportunity();
    if (isAtExpansionOpportunity)
        ++expansionOpportunity.count;

    expansionOpportunity.behavior = expansionBehavior(isAtExpansionOpportunity);
}

void InlineFormattingContext::Line::appendContent(const InlineLineBreaker::Run& run)
{
    ASSERT(!isClosed());

    auto& content = run.content;

    // Append this text run to the end of the last text run, if the last run is continuous.
    std::optional<InlineRun::TextContext> textRun;
    if (content.isText()) {
        auto textContext = content.textContext();
        auto runLength = textContext->isCollapsed() ? 1 : textContext->length();
        textRun = InlineRun::TextContext { textContext->start(), runLength };
    }

    auto requiresNewInlineRun = !hasContent() || !content.isText() || !m_lastRunCanExpand;
    if (requiresNewInlineRun) {
        // FIXME: This needs proper baseline handling
        auto inlineRun = InlineRun { { logicalTop(), contentLogicalRight(), run.width, logicalBottom() - logicalTop() }, content.inlineItem() };
        if (textRun)
            inlineRun.setTextContext({ textRun->start(), textRun->length() });
        m_formattingState.appendInlineRun(inlineRun);
    } else {
        // Non-text runs always require new inline run.
        ASSERT(textRun);
        auto& inlineRun = m_formattingState.inlineRuns().last();
        inlineRun.setWidth(inlineRun.width() + run.width);
        inlineRun.textContext()->setLength(inlineRun.textContext()->length() + textRun->length());
    }

    computeExpansionOpportunities(run);

    m_availableWidth -= run.width;
    m_lastRunIsWhitespace = content.isWhitespace();
    m_lastRunCanExpand = content.isText() && !content.textContext()->isCollapsed();
    m_firstRunIndex = m_firstRunIndex.value_or(m_formattingState.inlineRuns().size() - 1);
    m_trailingTrimmableContent = { };
    if (isTrimmableContent(content))
        m_trailingTrimmableContent = TrailingTrimmableContent { run.width, textRun->length() };
}

void InlineFormattingContext::Line::justifyRuns()
{
    if (!hasContent())
        return;

    auto& inlineRuns = m_formattingState.inlineRuns();
    auto& lastInlineRun = inlineRuns.last();

    // Adjust (forbid) trailing expansion for the last text run on line.
    auto expansionBehavior = lastInlineRun.expansionOpportunity().behavior;
    // Remove allow and add forbid.
    expansionBehavior ^= AllowTrailingExpansion;
    expansionBehavior |= ForbidTrailingExpansion;
    lastInlineRun.expansionOpportunity().behavior = expansionBehavior;

    // Collect expansion opportunities and justify the runs.
    auto widthToDistribute = availableWidth();
    if (widthToDistribute <= 0)
        return;

    auto expansionOpportunities = 0;
    for (auto runIndex = *m_firstRunIndex; runIndex < inlineRuns.size(); ++runIndex)
        expansionOpportunities += inlineRuns[runIndex].expansionOpportunity().count;

    if (!expansionOpportunities)
        return;

    float expansion = widthToDistribute.toFloat() / expansionOpportunities;
    LayoutUnit accumulatedExpansion = 0;
    for (auto runIndex = *m_firstRunIndex; runIndex < inlineRuns.size(); ++runIndex) {
        auto& inlineRun = inlineRuns[runIndex];
        auto expansionForRun = inlineRun.expansionOpportunity().count * expansion;

        inlineRun.expansionOpportunity().expansion = expansionForRun;
        inlineRun.setLogicalLeft(inlineRun.logicalLeft() + accumulatedExpansion);
        inlineRun.setWidth(inlineRun.width() + expansionForRun);
        accumulatedExpansion += expansionForRun;
    }
}

void InlineFormattingContext::Line::close(LastLine isLastLine)
{
    auto trimTrailingContent = [&]{

        if (!m_trailingTrimmableContent)
            return;

        auto& lastInlineRun = m_formattingState.inlineRuns().last();
        lastInlineRun.setWidth(lastInlineRun.width() - m_trailingTrimmableContent->width);
        lastInlineRun.textContext()->setLength(lastInlineRun.textContext()->length() - m_trailingTrimmableContent->length);

        if (!lastInlineRun.textContext()->length()) {
            if (*m_firstRunIndex == m_formattingState.inlineRuns().size() - 1)
                m_firstRunIndex = { };
            m_formattingState.inlineRuns().removeLast();
        }
        m_availableWidth += m_trailingTrimmableContent->width;
        m_trailingTrimmableContent = { };
    };

    auto alignRuns = [&]{

        if (!hasContent())
            return;

        auto textAlignment = !m_alignmentIsJustify ? m_formattingRoot.style().textAlign() : isLastLine == LastLine::No ? TextAlignMode::Justify : TextAlignMode::Left;
        if (textAlignment == TextAlignMode::Justify) {
            justifyRuns();
            return;
        }

        auto adjustedLogicalLeft = adjustedLineLogicalLeft(textAlignment, m_logicalRect.left(), m_availableWidth);
        if (m_logicalRect.left() == adjustedLogicalLeft)
            return;

        auto& inlineRuns = m_formattingState.inlineRuns();
        auto delta = adjustedLogicalLeft - m_logicalRect.left();
        for (auto runIndex = *m_firstRunIndex; runIndex < inlineRuns.size(); ++runIndex)
            inlineRuns[runIndex].setLogicalLeft(inlineRuns[runIndex].logicalLeft() + delta);
    };

    if (!hasContent())
        return;

    trimTrailingContent();
    alignRuns();

    m_isFirstLine = false;
    m_closed = true;
}

}
}

#endif
