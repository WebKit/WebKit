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

InlineFormattingContext::Line::Line(InlineFormattingState& formattingState)
    : m_formattingState(formattingState)
{
}

void InlineFormattingContext::Line::init(const Display::Box::Rect& logicalRect)
{
    m_logicalRect = logicalRect;
    m_availableWidth = logicalRect.width();

    m_firstRunIndex = { };
    m_lastRunType = { };
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

static bool isTrimmableContent(const InlineRunProvider::Run& inlineRun)
{
    return inlineRun.isWhitespace() && inlineRun.style().collapseWhiteSpace();
}

LayoutUnit InlineFormattingContext::Line::contentLogicalRight() const
{
    if (!m_firstRunIndex.has_value())
        return m_logicalRect.left();

    return m_formattingState.inlineRuns().last().logicalRight();
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

    m_availableWidth -= run.width;
    m_lastRunType = content.type();
    m_lastRunCanExpand = content.isText() && !content.textContext()->isCollapsed();
    m_firstRunIndex = m_firstRunIndex.value_or(m_formattingState.inlineRuns().size() - 1);
    m_trailingTrimmableContent = { };
    if (isTrimmableContent(content))
        m_trailingTrimmableContent = TrailingTrimmableContent { run.width, textRun->length() };
}

InlineFormattingContext::Line::RunRange InlineFormattingContext::Line::close()
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

    if (!hasContent())
        return { };

    trimTrailingContent();
    m_isFirstLine = false;
    m_closed = true;

    if (!m_firstRunIndex)
        return { };
    return { m_firstRunIndex, m_formattingState.inlineRuns().size() - 1 };
}

}
}

#endif
