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
{
}

void InlineFormattingContext::Line::init(LayoutUnit lineLogicalLeft, LayoutUnit availableWidth)
{
    m_lineLogicalLeft = lineLogicalLeft;
    m_lineWidth = availableWidth;
    m_availableWidth = availableWidth;
    m_firstRunIndex = { };
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

static bool isNonCollapsedText(const InlineRunProvider::Run& inlineRun)
{
    return inlineRun.isText() && !inlineRun.textContext()->isCollapsed();
}

static bool isTrimmableContent(const InlineRunProvider::Run& inlineRun)
{
    return inlineRun.isWhitespace() && inlineRun.style().collapseWhiteSpace();
}

LayoutUnit InlineFormattingContext::Line::contentLogicalRight()
{
    if (!m_firstRunIndex.has_value())
        return m_lineLogicalLeft;

    return m_formattingState.inlineRuns().last().logicalRight();
}

void InlineFormattingContext::Line::appendContent(const InlineLineBreaker::Run& run)
{
    auto lastRunWasNotCollapsedText = m_lastRunIsNotCollapsedText;
    m_trailingTrimmableContent = { };
    m_availableWidth -= run.width;

    auto& content = run.content;
    m_lastRunIsNotCollapsedText = isNonCollapsedText(content);

    // Append this text run to the end of the last text run, if the last run is continuous.
    std::optional<InlineRun::TextContext> textRun;
    if (content.isText()) {
        auto textContext = content.textContext();
        auto runLength = textContext->isCollapsed() ? 1 : textContext->length();

        if (isTrimmableContent(content))
            m_trailingTrimmableContent = TrailingTrimmableContent { run.width, runLength };

        if (hasContent() && lastRunWasNotCollapsedText) {
            auto& inlineRun = m_formattingState.inlineRuns().last();
            inlineRun.setWidth(inlineRun.width() + run.width);
            inlineRun.textContext()->setLength(inlineRun.textContext()->length() + runLength);
            return;
        }
        textRun = InlineRun::TextContext { textContext->start(), runLength };
    }

    if (textRun)
        m_formattingState.appendInlineRun(InlineRun::createTextRun(contentLogicalRight(), run.width, textRun->start(), textRun->length(), content.inlineItem()));
    else
        m_formattingState.appendInlineRun(InlineRun::createRun(contentLogicalRight(), run.width, content.inlineItem()));
    m_firstRunIndex = m_firstRunIndex.value_or(m_formattingState.inlineRuns().size() - 1);
}

void InlineFormattingContext::Line::close()
{
    auto trimTrailingContent = [&]() {

        if (!m_trailingTrimmableContent)
            return;

        auto& lastInlineRun = m_formattingState.inlineRuns().last();
        lastInlineRun.setWidth(lastInlineRun.width() - m_trailingTrimmableContent->width);
        lastInlineRun.textContext()->setLength(lastInlineRun.textContext()->length() - m_trailingTrimmableContent->length);

        if (!lastInlineRun.textContext()->length()) {
            m_formattingState.inlineRuns().removeLast();
            if (m_firstRunIndex.value())
                --*m_firstRunIndex;
            else
                m_firstRunIndex = { };
        }
        m_availableWidth += m_trailingTrimmableContent->width;
        m_trailingTrimmableContent = { };
    };

    auto alignRuns = [&]() {

        if (!hasContent())
            return;

        auto adjustedLogicalLeft = adjustedLineLogicalLeft(m_formattingRoot.style().textAlign(), m_lineLogicalLeft, m_availableWidth);
        if (m_lineLogicalLeft == adjustedLogicalLeft)
            return;

        auto& inlineRuns = m_formattingState.inlineRuns();
        auto delta = adjustedLogicalLeft - m_lineLogicalLeft;
        for (auto runIndex = *m_firstRunIndex; runIndex < inlineRuns.size(); ++runIndex)
            inlineRuns[runIndex].setLogicalLeft(inlineRuns[runIndex].logicalLeft() + delta);
    };

    if (!hasContent())
        return;

    trimTrailingContent();
    alignRuns();
}

}
}

#endif
