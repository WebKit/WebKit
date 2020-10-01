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

#include "InlineTextBox.h"
#include "RenderText.h"
#include <wtf/Vector.h>

namespace WebCore {
namespace LayoutIntegration {

class LegacyPath {
public:
    LegacyPath(const InlineBox* inlineBox, Vector<const InlineTextBox*>&& sortedInlineTextBoxes = { }, size_t sortedInlineTextBoxIndex = 0)
        : m_inlineBox(inlineBox)
        , m_sortedInlineTextBoxes(WTFMove(sortedInlineTextBoxes))
        , m_sortedInlineTextBoxIndex(sortedInlineTextBoxIndex)
    { }

    bool isText() const { return m_inlineBox->isInlineTextBox(); }

    FloatRect rect() const { return m_inlineBox->frameRect(); }

    bool isLeftToRightDirection() const { return m_inlineBox->isLeftToRightDirection(); }
    bool isHorizontal() const { return m_inlineBox->isHorizontal(); }
    bool dirOverride() const { return m_inlineBox->dirOverride(); }
    bool isLineBreak() const { return m_inlineBox->isLineBreak(); }
    float baseline() const { return m_inlineBox->baselinePosition(AlphabeticBaseline); }

    unsigned minimumCaretOffset() const { return m_inlineBox->caretMinOffset(); }
    unsigned maximumCaretOffset() const { return m_inlineBox->caretMaxOffset(); }

    bool useLineBreakBoxRenderTreeDumpQuirk() const
    {
        return !m_inlineBox->behavesLikeText();
    }

    bool hasHyphen() const { return inlineTextBox()->hasHyphen(); }
    StringView text() const { return StringView(inlineTextBox()->renderer().text()).substring(inlineTextBox()->start(), inlineTextBox()->len()); }
    unsigned localStartOffset() const { return inlineTextBox()->start(); }
    unsigned localEndOffset() const { return inlineTextBox()->end(); }
    unsigned length() const { return inlineTextBox()->len(); }

    inline bool isLastTextRunOnLine() const
    {
        auto* next = nextInlineTextBoxInTextOrder();
        return !next || &inlineTextBox()->root() != &next->root();
    }
    inline bool isLastTextRun() const { return !nextInlineTextBoxInTextOrder(); };

    void traverseNextTextRunInVisualOrder() { m_inlineBox = inlineTextBox()->nextTextBox(); }
    void traverseNextTextRunInTextOrder()
    {
        m_inlineBox = nextInlineTextBoxInTextOrder();
        if (!m_sortedInlineTextBoxes.isEmpty())
            ++m_sortedInlineTextBoxIndex;
    }

    void traverseNextOnLine()
    {
        m_inlineBox = m_inlineBox->nextLeafOnLine();
    }

    bool operator==(const LegacyPath& other) const { return m_inlineBox == other.m_inlineBox; }
    bool atEnd() const { return !m_inlineBox; }

    InlineBox* legacyInlineBox() const { return const_cast<InlineBox*>(m_inlineBox); }

private:
    const InlineTextBox* inlineTextBox() const { return downcast<InlineTextBox>(m_inlineBox); }
    const InlineTextBox* nextInlineTextBoxInTextOrder() const;

    const InlineBox* m_inlineBox;
    Vector<const InlineTextBox*> m_sortedInlineTextBoxes;
    size_t m_sortedInlineTextBoxIndex { 0 };
};

inline const InlineTextBox* LegacyPath::nextInlineTextBoxInTextOrder() const
{
    if (!m_sortedInlineTextBoxes.isEmpty()) {
        if (m_sortedInlineTextBoxIndex + 1 < m_sortedInlineTextBoxes.size())
            return m_sortedInlineTextBoxes[m_sortedInlineTextBoxIndex + 1];
        return nullptr;
    }
    return inlineTextBox()->nextTextBox();
}

}
}
