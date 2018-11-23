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
#include "InlineRunProvider.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

namespace WebCore {
namespace Layout {

void InlineFormattingContext::Line::init(const LayoutPoint& topLeft, LayoutUnit availableWidth, LayoutUnit minimalHeight)
{
    m_logicalRect.setTopLeft(topLeft);
    m_logicalRect.setWidth(availableWidth);
    m_logicalRect.setHeight(minimalHeight);
    m_availableWidth = availableWidth;

    m_inlineRuns.clear();
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

    for (auto& inlineRun : m_inlineRuns)
        inlineRun.moveHorizontally(delta);
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
    if (m_inlineRuns.isEmpty())
        return m_logicalRect.left();

    return m_inlineRuns.last().logicalRight();
}

void InlineFormattingContext::Line::appendContent(const InlineRunProvider::Run& run, const LayoutSize& runSize)
{
    ASSERT(!isClosed());

    // Append this text run to the end of the last text run, if the last run is continuous.
    std::optional<InlineRun::TextContext> textRun;
    if (run.isText()) {
        auto textContext = run.textContext();
        auto runLength = textContext->isCollapsed() ? 1 : textContext->length();
        textRun = InlineRun::TextContext { textContext->start(), runLength };
    }

    auto requiresNewInlineRun = !hasContent() || !run.isText() || !m_lastRunCanExpand;
    if (requiresNewInlineRun) {
        // FIXME: This needs proper baseline handling
        auto inlineRun = InlineRun { { logicalTop(), contentLogicalRight(), runSize.width(), runSize.height() }, run.inlineItem() };
        if (textRun)
            inlineRun.setTextContext({ textRun->start(), textRun->length() });
        m_inlineRuns.append(inlineRun);
        m_logicalRect.setHeight(std::max(runSize.height(), m_logicalRect.height()));
    } else {
        // Non-text runs always require new inline run.
        ASSERT(textRun);
        auto& inlineRun = m_inlineRuns.last();
        ASSERT(runSize.height() == inlineRun.logicalHeight());
        inlineRun.setLogicalWidth(inlineRun.logicalWidth() + runSize.width());
        inlineRun.textContext()->setLength(inlineRun.textContext()->length() + textRun->length());
    }

    m_availableWidth -= runSize.width();
    m_lastRunType = run.type();
    m_lastRunCanExpand = run.isText() && !run.textContext()->isCollapsed();
    m_trailingTrimmableContent = { };
    if (isTrimmableContent(run))
        m_trailingTrimmableContent = TrailingTrimmableContent { runSize.width(), textRun->length() };
}

void InlineFormattingContext::Line::close()
{
    auto trimTrailingContent = [&]{

        if (!m_trailingTrimmableContent)
            return;

        auto& lastInlineRun = m_inlineRuns.last();
        lastInlineRun.setLogicalWidth(lastInlineRun.logicalWidth() - m_trailingTrimmableContent->width);
        lastInlineRun.textContext()->setLength(lastInlineRun.textContext()->length() - m_trailingTrimmableContent->length);

        if (!lastInlineRun.textContext()->length())
            m_inlineRuns.removeLast();
        m_availableWidth += m_trailingTrimmableContent->width;
        m_trailingTrimmableContent = { };
    };

    if (!hasContent())
        return;

    trimTrailingContent();
    m_isFirstLine = false;
    m_closed = true;
}

}
}

#endif
