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
#include "RootInlineBox.h"
#include <wtf/RefCountedArray.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace LayoutIntegration {

class RunIteratorLegacyPath {
public:
    RunIteratorLegacyPath(const InlineBox* inlineBox, Vector<const InlineBox*>&& sortedInlineBoxes = { }, size_t sortedInlineBoxIndex = 0)
        : m_inlineBox(inlineBox)
        , m_logicalOrderCache(WTFMove(sortedInlineBoxes))
        , m_logicalOrderCacheIndex(sortedInlineBoxIndex)
    { }

    enum class LogicalOrder { Start, End };
    RunIteratorLegacyPath(const RootInlineBox& root, LogicalOrder order)
        : m_logicalOrderCache(inlineBoxesInLogicalOrder(root))
    {
        if (!m_logicalOrderCache.isEmpty()) {
            m_logicalOrderCacheIndex = order == LogicalOrder::Start ? 0 : m_logicalOrderCache.size() - 1;
            m_inlineBox = m_logicalOrderCache[m_logicalOrderCacheIndex];
        }
    }

    bool isText() const { return m_inlineBox->isInlineTextBox(); }

    FloatRect rect() const { return m_inlineBox->frameRect(); }

    bool isHorizontal() const { return m_inlineBox->isHorizontal(); }
    bool dirOverride() const { return m_inlineBox->dirOverride(); }
    bool isLineBreak() const { return m_inlineBox->isLineBreak(); }

    unsigned minimumCaretOffset() const { return m_inlineBox->caretMinOffset(); }
    unsigned maximumCaretOffset() const { return m_inlineBox->caretMaxOffset(); }

    unsigned char bidiLevel() const { return m_inlineBox->bidiLevel(); }

    bool hasHyphen() const { return inlineTextBox()->hasHyphen(); }
    StringView text() const { return StringView(inlineTextBox()->renderer().text()).substring(inlineTextBox()->start(), inlineTextBox()->len()); }
    unsigned start() const { return inlineTextBox()->start(); }
    unsigned end() const { return inlineTextBox()->end(); }
    unsigned length() const { return inlineTextBox()->len(); }

    unsigned offsetForPosition(float x) const { return inlineTextBox()->offsetForPosition(x); }
    float positionForOffset(unsigned offset) const { return inlineTextBox()->positionForOffset(offset); }

    bool isSelectable(unsigned start, unsigned end) const { return inlineTextBox()->isSelected(start, end); }
    LayoutRect selectionRect(unsigned start, unsigned end) const { return inlineTextBox()->localSelectionRect(start, end); }

    const RenderObject& renderer() const
    {
        return m_inlineBox->renderer();
    }

    void traverseNextTextRun() { m_inlineBox = inlineTextBox()->nextTextBox(); }
    void traverseNextTextRunInTextOrder()
    {
        if (!m_logicalOrderCache.isEmpty()) {
            traverseNextInlineBoxInCacheOrder();
            ASSERT(!m_inlineBox || is<InlineTextBox>(m_inlineBox));
            return;
        }
        traverseNextTextRun();
    }

    void traverseNextOnLine()
    {
        m_inlineBox = m_inlineBox->nextLeafOnLine();
    }

    void traversePreviousOnLine()
    {
        m_inlineBox = m_inlineBox->previousLeafOnLine();
    }

    void traverseNextOnLineInLogicalOrder()
    {
        initializeLogicalOrderCacheForLine();
        traverseNextInlineBoxInCacheOrder();
    }

    void traversePreviousOnLineInLogicalOrder()
    {
        initializeLogicalOrderCacheForLine();
        traversePreviousInlineBoxInCacheOrder();
    }

    bool operator==(const RunIteratorLegacyPath& other) const { return m_inlineBox == other.m_inlineBox; }

    bool atEnd() const { return !m_inlineBox; }
    void setAtEnd() { m_inlineBox = nullptr; }

    InlineBox* legacyInlineBox() const { return const_cast<InlineBox*>(m_inlineBox); }
    const RootInlineBox& rootInlineBox() const { return m_inlineBox->root(); }

private:
    const InlineTextBox* inlineTextBox() const { return downcast<InlineTextBox>(m_inlineBox); }

    static Vector<const InlineBox*> inlineBoxesInLogicalOrder(const RootInlineBox& root)
    {
        Vector<InlineBox*> inlineBoxes;
        root.collectLeafBoxesInLogicalOrder(inlineBoxes);
        return reinterpret_cast<Vector<const InlineBox*>&>(inlineBoxes);
    }
    void initializeLogicalOrderCacheForLine()
    {
        if (!m_inlineBox || !m_logicalOrderCache.isEmpty())
            return;
        m_logicalOrderCache = inlineBoxesInLogicalOrder(m_inlineBox->root());
        for (m_logicalOrderCacheIndex = 0; m_logicalOrderCacheIndex < m_logicalOrderCache.size(); ++m_logicalOrderCacheIndex) {
            if (m_logicalOrderCache[m_logicalOrderCacheIndex] == m_inlineBox)
                return;
        }
        ASSERT_NOT_REACHED();
    }

    void traverseNextInlineBoxInCacheOrder();
    void traversePreviousInlineBoxInCacheOrder();

    const InlineBox* m_inlineBox { nullptr };
    RefCountedArray<const InlineBox*> m_logicalOrderCache;
    size_t m_logicalOrderCacheIndex { 0 };
};


inline void RunIteratorLegacyPath::traverseNextInlineBoxInCacheOrder()
{
    ASSERT(!m_logicalOrderCache.isEmpty());
    ++m_logicalOrderCacheIndex;
    m_inlineBox = m_logicalOrderCacheIndex < m_logicalOrderCache.size() ? m_logicalOrderCache[m_logicalOrderCacheIndex] : nullptr;
}

inline void RunIteratorLegacyPath::traversePreviousInlineBoxInCacheOrder()
{
    ASSERT(!m_logicalOrderCache.isEmpty());
    if (!m_logicalOrderCacheIndex) {
        m_inlineBox = nullptr;
        return;
    }
    m_inlineBox = m_logicalOrderCache[--m_logicalOrderCacheIndex];
}

}
}
