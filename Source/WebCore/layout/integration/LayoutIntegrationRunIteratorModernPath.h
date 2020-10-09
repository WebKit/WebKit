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

#pragma once

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FontCascade.h"
#include "LayoutIntegrationInlineContent.h"

namespace WebCore {

namespace LayoutIntegration {

inline FloatRect verticallyRoundedRect(const FloatRect& rect)
{
    return { FloatPoint(rect.x(), roundf(rect.y())), rect.size() };
}

class RunIteratorModernPath {
public:
    RunIteratorModernPath(const InlineContent& inlineContent)
        : m_inlineContent(&inlineContent)
    {
        setAtEnd();
    }
    RunIteratorModernPath(const InlineContent& inlineContent, size_t startIndex)
        : m_inlineContent(&inlineContent)
        , m_runIndex(startIndex)
    {
    }

    bool isText() const { return !!run().textContent(); }

    FloatRect rect() const { return verticallyRoundedRect(run().rect()); }

    float baseline() const { return line().baseline(); }

    bool isHorizontal() const { return true; }
    bool dirOverride() const { return false; }
    bool isLineBreak() const { return run().isLineBreak(); }

    unsigned minimumCaretOffset() const { return isText() ? localStartOffset() : 0; }
    unsigned maximumCaretOffset() const { return isText() ? localEndOffset() : 1; }

    unsigned char bidiLevel() const { return 0; }

    bool useLineBreakBoxRenderTreeDumpQuirk() const
    {
        if (!m_runIndex)
            return false;
        auto& previous = runs()[m_runIndex - 1];
        return previous.lineIndex() == run().lineIndex();
    }

    bool hasHyphen() const { return run().textContent()->needsHyphen(); }
    StringView text() const { return run().textContent()->content(); }
    unsigned localStartOffset() const { return run().textContent()->start(); }
    unsigned localEndOffset() const { return run().textContent()->end(); }
    unsigned length() const { return run().textContent()->length(); }

    inline unsigned offsetForPosition(float x) const
    {
        if (isLineBreak())
            return 0;
        auto rect = this->rect();
        auto localX = x - rect.x();
        if (localX > rect.width())
            return length();
        if (localX < 0)
            return 0;

        auto& style = run().style();

        auto createTextRun = [&] {
            auto expansion = run().expansion();
            auto xPos = rect.x() - (line().rect().x() + line().horizontalAlignmentOffset());
            TextRun textRun { text(), xPos, expansion.horizontalExpansion, expansion.behavior };
            textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
            return textRun;
        };

        bool includePartialGlyphs = true;
        return run().style().fontCascade().offsetForPosition(createTextRun(), localX, includePartialGlyphs);
    }

    bool isLastTextRunOnLine() const
    {
        if (isLastTextRun())
            return true;

        auto& next = runs()[m_runIndex + 1];
        return run().lineIndex() != next.lineIndex();
    }

    bool isLastTextRun() const
    {
        ASSERT(!atEnd());
        ASSERT(run().textContent());

        if (m_runIndex + 1 == runs().size())
            return true;
        return &run().layoutBox() != &runs()[m_runIndex + 1].layoutBox();
    };

    const RenderObject& renderer() const
    {
        return *m_inlineContent->rendererForLayoutBox(run().layoutBox());
    }

    void traverseNextTextRun()
    {
        ASSERT(!atEnd());
        ASSERT(run().textContent());

        auto& layoutBox = run().layoutBox();

        ++m_runIndex;

        if (!atEnd() && &layoutBox != &run().layoutBox())
            setAtEnd();
    }

    void traverseNextTextRunInTextOrder()
    {
        // FIXME: No RTL in LFC.
        traverseNextTextRun();
    }

    void traverseNextOnLine()
    {
        ASSERT(!atEnd());

        auto oldLineIndex = run().lineIndex();

        ++m_runIndex;

        if (!atEnd() && oldLineIndex != run().lineIndex())
            setAtEnd();
    }

    void traversePreviousOnLine()
    {
        ASSERT(!atEnd());

        if (!m_runIndex) {
            setAtEnd();
            return;
        }

        auto oldLineIndex = run().lineIndex();

        --m_runIndex;

        if (oldLineIndex != run().lineIndex())
            setAtEnd();
    }

    bool operator==(const RunIteratorModernPath& other) const { return m_inlineContent == other.m_inlineContent && m_runIndex == other.m_runIndex; }

    bool atEnd() const { return m_runIndex == runs().size() || !run().hasUnderlyingLayout(); }
    void setAtEnd() { m_runIndex = runs().size(); }

    InlineBox* legacyInlineBox() const
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

private:
    friend class RunIterator;

    const InlineContent::Runs& runs() const { return m_inlineContent->runs; }
    const Run& run() const { return runs()[m_runIndex]; }
    const Line& line() const { return m_inlineContent->lineForRun(run()); }

    RefPtr<const InlineContent> m_inlineContent;
    size_t m_runIndex { 0 };
};

}
}

#endif
