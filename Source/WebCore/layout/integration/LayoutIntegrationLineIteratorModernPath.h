/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutIntegrationInlineContent.h"
#include "LayoutIntegrationRunIteratorModernPath.h"
#include "RenderBlockFlow.h"

namespace WebCore {

namespace LayoutIntegration {

class RunIteratorModernPath;

class LineIteratorModernPath {
public:
    LineIteratorModernPath(const InlineContent& inlineContent, size_t lineIndex)
        : m_inlineContent(&inlineContent)
        , m_lineIndex(lineIndex)
    {
        ASSERT(lineIndex <= lines().size());
    }
    LineIteratorModernPath(LineIteratorModernPath&&) = default;
    LineIteratorModernPath(const LineIteratorModernPath&) = default;
    LineIteratorModernPath& operator=(const LineIteratorModernPath&) = default;
    LineIteratorModernPath& operator=(LineIteratorModernPath&&) = default;

    LayoutUnit top() const { return LayoutUnit::fromFloatRound(line().enclosingContentTop()); }
    LayoutUnit bottom() const { return LayoutUnit::fromFloatRound(line().enclosingContentBottom()); }
    LayoutUnit lineBoxTop() const { return LayoutUnit::fromFloatRound(line().lineBoxTop()); }
    LayoutUnit lineBoxBottom() const { return LayoutUnit::fromFloatRound(line().lineBoxBottom()); }

    LayoutUnit selectionTop() const { return !m_lineIndex ? top() : LineIteratorModernPath(*m_inlineContent, m_lineIndex - 1).selectionBottom(); }
    // FIXME: Remove the containingBlock().borderAndPaddingBefore() offset after retiring legacy line layout. It also requires changes in RenderText::positionForPoint to find the first line with offset.
    // - the "before" value is already factored in to the line offset
    // - this logic negates the first line's natural offset (e.g. block has no border/padding but the first line has a computed offset).
    LayoutUnit selectionTopForHitTesting() const { return !m_lineIndex ? containingBlock().borderAndPaddingBefore() : selectionTop(); };
    LayoutUnit selectionBottom() const { return bottom(); }

    float contentLogicalLeft() const { return line().lineBoxLeft() + line().contentLeft(); }
    float contentLogicalRight() const { return contentLogicalLeft() + line().contentWidth(); }
    float y() const { return lineBoxTop(); }
    float logicalHeight() const { return lineBoxBottom() - lineBoxTop(); }
    bool isHorizontal() const { return true; }
    FontBaseline baselineType() const { return AlphabeticBaseline; }

    const RenderBlockFlow& containingBlock() const { return m_inlineContent->containingBlock(); }
    const LegacyRootInlineBox* legacyRootInlineBox() const { return nullptr; }

    void traverseNext()
    {
        ASSERT(!atEnd());

        ++m_lineIndex;
    }

    void traversePrevious()
    {
        ASSERT(!atEnd());

        if (!m_lineIndex) {
            setAtEnd();
            return;
        }

        --m_lineIndex;
    }

    bool operator==(const LineIteratorModernPath& other) const { return m_inlineContent == other.m_inlineContent && m_lineIndex == other.m_lineIndex; }

    bool atEnd() const { return m_lineIndex == lines().size(); }

    RunIteratorModernPath firstRun() const
    {
        if (!line().runCount())
            return { *m_inlineContent };
        return { *m_inlineContent, line().firstRunIndex() };
    }

    RunIteratorModernPath lastRun() const
    {
        auto runCount = line().runCount();
        if (!runCount)
            return { *m_inlineContent };
        return { *m_inlineContent, line().firstRunIndex() + runCount - 1 };
    }

    RunIteratorModernPath logicalStartRun() const
    {
        return firstRun();
    }

    RunIteratorModernPath logicalEndRun() const
    {
        return lastRun();
    }

private:
    void setAtEnd() { m_lineIndex = lines().size(); }

    const InlineContent::Lines& lines() const { return m_inlineContent->lines; }
    const Line& line() const { return lines()[m_lineIndex]; }

    RefPtr<const InlineContent> m_inlineContent;
    size_t m_lineIndex { 0 };
};

}
}

#endif

