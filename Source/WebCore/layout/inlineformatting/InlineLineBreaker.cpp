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
#include "InlineLineBreaker.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FontCascade.h"
#include "Hyphenation.h"
#include "InlineRunProvider.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineLineBreaker);

InlineLineBreaker::InlineLineBreaker(const LayoutState& layoutState, const InlineContent& inlineContent, const Vector<InlineRunProvider::Run>& inlineRuns)
    : m_layoutState(layoutState)
    , m_textUtil(inlineContent)
    , m_inlineRuns(inlineRuns)
{
}

std::optional<InlineLineBreaker::Run> InlineLineBreaker::nextRun(LayoutUnit contentLogicalLeft, LayoutUnit availableWidth, bool lineIsEmpty)
{
    if (isAtContentEnd())
        return std::nullopt;

    InlineRunProvider::Run currentInlineRun = m_inlineRuns[m_currentRunIndex];
    // Adjust the current run if it is split midword.
    if (m_splitPosition) {
        ASSERT(currentInlineRun.isText());
        m_splitPosition = std::nullopt;
    }

    if (currentInlineRun.isLineBreak()) {
        ++m_currentRunIndex;
        return Run { Run::Position::LineEnd, 0, currentInlineRun };
    }

    auto contentWidth = runWidth(currentInlineRun, contentLogicalLeft);
    // 1. Plenty of space left.
    if (contentWidth <= availableWidth) {
        ++m_currentRunIndex;
        return Run { lineIsEmpty ? Run::Position::LineBegin : Run::Position::Undetermined, contentWidth, currentInlineRun };
    }

    // 2. No space left whatsoever.
    if (availableWidth <= 0) {
        ++m_currentRunIndex;
        return Run { Run::Position::LineBegin, contentWidth, currentInlineRun };
    }

    // 3. Some space left. Let's find out what we need to do with this run.
    auto breakingBehavior = lineBreakingBehavior(currentInlineRun, lineIsEmpty);
    if (breakingBehavior == LineBreakingBehavior::Keep) {
        ++m_currentRunIndex;
        return Run { lineIsEmpty ? Run::Position::LineBegin : Run::Position::Undetermined, contentWidth, currentInlineRun };
    }

    if (breakingBehavior == LineBreakingBehavior::WrapToNextLine) {
        ++m_currentRunIndex;
        return Run { Run::Position::LineBegin, contentWidth, currentInlineRun };
    }

    ASSERT(breakingBehavior == LineBreakingBehavior::Break);
    // Split content.
    return splitRun(currentInlineRun, contentLogicalLeft, availableWidth, lineIsEmpty);
}

bool InlineLineBreaker::isAtContentEnd() const
{
    return m_currentRunIndex == m_inlineRuns.size();
}

InlineLineBreaker::LineBreakingBehavior InlineLineBreaker::lineBreakingBehavior(const InlineRunProvider::Run& inlineRun, bool lineIsEmpty)
{
    // Line breaking behaviour:
    // 1. Whitesapce collapse on -> push whitespace to next line.
    // 2. Whitespace collapse off -> whitespace is split where possible.
    // 3. Non-whitespace -> first run on the line -> either split or kept on the line. (depends on overflow-wrap)
    // 4. Non-whitespace -> already content on the line -> either gets split (word-break: break-all) or gets pushed to the next line.
    // (Hyphenate when possible)
    // 5. Non-text type -> next line
    auto& style = inlineRun.style();

    if (inlineRun.isWhitespace())
        return style.collapseWhiteSpace() ? LineBreakingBehavior::WrapToNextLine : LineBreakingBehavior::Break;

    if (inlineRun.isNonWhitespace()) {
        auto shouldHypenate = !m_hyphenationIsDisabled && style.hyphens() == Hyphens::Auto && canHyphenate(style.locale());
        if (shouldHypenate)
            return LineBreakingBehavior::Break;

        if (style.autoWrap()) {
            // Break any word
            if (style.wordBreak() == WordBreak::BreakAll)
                return LineBreakingBehavior::Break;

            // Break first run on line.
            if (lineIsEmpty && style.breakWords() && style.preserveNewline())
                return LineBreakingBehavior::Break;
        }

        // Non-breakable non-whitespace run.
        return lineIsEmpty ? LineBreakingBehavior::Keep : LineBreakingBehavior::WrapToNextLine;
    }

    ASSERT(inlineRun.isBox() || inlineRun.isFloat());
    // Non-text inline runs.
    return LineBreakingBehavior::WrapToNextLine;
}

LayoutUnit InlineLineBreaker::runWidth(const InlineRunProvider::Run& inlineRun, LayoutUnit contentLogicalLeft) const
{
    ASSERT(!inlineRun.isLineBreak());

    if (inlineRun.isText()) {
        auto textContext = inlineRun.textContext();
        return m_textUtil.width(inlineRun.inlineItem(), textContext->start(), textContext->isCollapsed() ? 1 : textContext->length(), contentLogicalLeft);
    }

    ASSERT(inlineRun.isBox() || inlineRun.isFloat());
    auto& layoutBox = inlineRun.inlineItem().layoutBox();
    ASSERT(m_layoutState.hasDisplayBox(layoutBox));
    auto& displayBox = m_layoutState.displayBoxForLayoutBox(layoutBox);
    return displayBox.width();
}

InlineLineBreaker::Run InlineLineBreaker::splitRun(const InlineRunProvider::Run& inlineRun, LayoutUnit, LayoutUnit, bool)
{
    return { Run::Position::Undetermined, { }, inlineRun };
}

std::optional<ItemPosition> InlineLineBreaker::adjustSplitPositionWithHyphenation(const InlineRunProvider::Run&, ItemPosition, LayoutUnit, LayoutUnit, bool) const
{
    return { };
}

}
}
#endif
