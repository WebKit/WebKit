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

    FloatRect rect() const { return run().rect(); }

    bool isHorizontal() const { return true; }
    bool dirOverride() const { return false; }
    bool isLineBreak() const { return run().isLineBreak(); }

    unsigned minimumCaretOffset() const { return isText() ? start() : 0; }
    unsigned maximumCaretOffset() const { return isText() ? end() : 1; }

    unsigned char bidiLevel() const { return 0; }

    bool hasHyphen() const { return run().textContent()->hasHyphen(); }
    StringView text() const { return run().textContent()->originalContent(); }
    unsigned start() const { return run().textContent()->start(); }
    unsigned end() const { return run().textContent()->end(); }
    unsigned length() const { return run().textContent()->length(); }

    // FIXME: Make a shared generic version of this.
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

        bool includePartialGlyphs = true;
        return run().style().fontCascade().offsetForPosition(createTextRun(HyphenMode::Ignore), localX, includePartialGlyphs);
    }

    // FIXME: Make a shared generic version of this.
    float positionForOffset(unsigned offset) const
    {
        ASSERT(offset >= start());
        ASSERT(offset <= end());

        if (isLineBreak())
            return rect().x();

        auto endOffset = clampedOffset(offset);

        LayoutRect selectionRect = LayoutRect(rect().x(), 0, 0, 0);
        TextRun textRun = createTextRun(HyphenMode::Ignore);
        run().style().fontCascade().adjustSelectionRectForText(textRun, selectionRect, 0, endOffset);
        return snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), textRun.ltr()).maxX();
    }

    bool isSelectable(unsigned start, unsigned end) const
    {
        return clampedOffset(start) < clampedOffset(end);
    }

    LayoutRect selectionRect(unsigned rangeStart, unsigned rangeEnd) const
    {
        unsigned clampedStart = clampedOffset(rangeStart);
        unsigned clampedEnd = clampedOffset(rangeEnd);

        if (clampedStart >= clampedEnd && !(rangeStart == rangeEnd && rangeStart >= start() && rangeStart <= end()))
            return { };

        auto logicalLeft = LayoutUnit(isHorizontal() ? rect().x() : rect().y());
        auto logicalRight = LayoutUnit(isHorizontal() ? rect().maxX() : rect().maxY());
        auto logicalWidth = logicalRight - logicalLeft;

        // FIXME: These should share implementation with the line iterator.
        auto selectionTop = LayoutUnit::fromFloatRound(line().enclosingContentTop());
        auto selectionHeight = LayoutUnit::fromFloatRound(line().enclosingContentBottom() - line().enclosingContentTop());

        LayoutRect selectionRect { logicalLeft, selectionTop, logicalWidth, selectionHeight };

        TextRun textRun = createTextRun(HyphenMode::Include);
        if (clampedStart || clampedEnd != textRun.length())
            run().style().fontCascade().adjustSelectionRectForText(textRun, selectionRect, clampedStart, clampedEnd);

        return snappedSelectionRect(selectionRect, logicalRight, selectionTop, selectionHeight, isHorizontal());
    }

    const RenderObject& renderer() const
    {
        return m_inlineContent->rendererForLayoutBox(run().layoutBox());
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

    void traverseNextOnLineInLogicalOrder()
    {
        traverseNextOnLine();
    }

    void traversePreviousOnLineInLogicalOrder()
    {
        traversePreviousOnLine();
    }

    bool operator==(const RunIteratorModernPath& other) const { return m_inlineContent == other.m_inlineContent && m_runIndex == other.m_runIndex; }

    bool atEnd() const { return m_runIndex == runs().size() || !run().hasUnderlyingLayout(); }
    void setAtEnd() { m_runIndex = runs().size(); }

    InlineBox* legacyInlineBox() const
    {
        return nullptr;
    }

private:
    friend class RunIterator;

    unsigned clampedOffset(unsigned offset) const
    {
        auto clampedOffset = std::max(start(), std::min(offset, end())) - start();
        // We treat the last codepoint in this run and the hyphen as a single unit.
        if (hasHyphen() && clampedOffset == length())
            clampedOffset += run().style().hyphenString().length();

        return clampedOffset;
    }

    enum class HyphenMode { Include, Ignore };
    TextRun createTextRun(HyphenMode hyphenMode) const
    {
        auto& style = run().style();
        auto expansion = run().expansion();
        auto rect = this->rect();
        auto xPos = rect.x() - (line().lineBoxLeft() + line().contentLeftOffset());

        auto textForRun = [&] {
            if (hyphenMode == HyphenMode::Ignore || !hasHyphen())
                return text().toStringWithoutCopying();

            return makeString(text(), style.hyphenString());
        }();

        TextRun textRun { textForRun, xPos, expansion.horizontalExpansion, expansion.behavior };
        textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
        return textRun;
    };

    const InlineContent::Runs& runs() const { return m_inlineContent->runs; }
    const Run& run() const { return runs()[m_runIndex]; }
    const Line& line() const { return m_inlineContent->lineForRun(run()); }

    RefPtr<const InlineContent> m_inlineContent;
    size_t m_runIndex { 0 };
};

}
}

#endif
