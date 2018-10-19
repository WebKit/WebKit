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

InlineFormattingContext::Line::Line(InlineFormattingState& inlineFormattingState)
    : m_inlineFormattingState(inlineFormattingState)
{
}

void InlineFormattingContext::Line::setConstraints(LayoutUnit lineLogicalLeft, LayoutUnit availableWidth)
{
    m_contentLogicalLeft = lineLogicalLeft;
    m_availableWidth = availableWidth;
}

static bool isNonCollapsedText(const InlineRunProvider::Run& inlineRun)
{
    return inlineRun.isText() && !inlineRun.textContext()->isCollapsed();
}

static bool isTrimmableContent(const InlineRunProvider::Run& inlineRun)
{
    return inlineRun.isWhitespace() && inlineRun.style().collapseWhiteSpace();
}

void InlineFormattingContext::Line::appendContent(const InlineLineBreaker::Run& run)
{
    auto lastRunWasNotCollapsedText = m_lastRunIsNotCollapsedText;
    auto hadContent = hasContent();
    auto contentLogicalLeft = m_contentLogicalLeft;
    m_trailingTrimmableContent = { };

    auto& inlineRun = run.inlineRun;
    m_availableWidth -= run.width;
    m_lastRunIsNotCollapsedText = isNonCollapsedText(inlineRun);
    m_isEmpty = false;
    m_contentLogicalLeft += run.width;

    // Append this text run to the end of the last text run, if the last run is continuous.
    if (inlineRun.isText()) {
        auto textContext = inlineRun.textContext();
        auto runLength = textContext->isCollapsed() ? 1 : textContext->length();

        if (isTrimmableContent(inlineRun))
            m_trailingTrimmableContent = TrailingTrimmableContent { run.width, runLength };

        if (hadContent && lastRunWasNotCollapsedText) {
            auto& inlineRun = m_inlineFormattingState.inlineRuns().last();
            inlineRun.setWidth(inlineRun.width() + run.width);
            inlineRun.textContext()->setLength(inlineRun.textContext()->length() + runLength);
            return;
        }

        return m_inlineFormattingState.appendInlineRun({ contentLogicalLeft, run.width, textContext->start(), runLength, inlineRun.inlineItem()});
    }

    m_inlineFormattingState.appendInlineRun({ contentLogicalLeft, run.width, inlineRun.inlineItem() });
}

void InlineFormattingContext::Line::close()
{
    auto trimTrailingContent = [&]() {

        if (!m_trailingTrimmableContent)
            return;

        auto& lastInlineRun = m_inlineFormattingState.inlineRuns().last();
        lastInlineRun.setWidth(lastInlineRun.width() - m_trailingTrimmableContent->width);
        lastInlineRun.textContext()->setLength(lastInlineRun.textContext()->length() - m_trailingTrimmableContent->length);

        if (!lastInlineRun.textContext()->length())
            m_inlineFormattingState.inlineRuns().removeLast();
        m_availableWidth += m_trailingTrimmableContent->width;
        m_trailingTrimmableContent = { };
    };

    trimTrailingContent();
    m_isEmpty = true;
}

}
}

#endif
