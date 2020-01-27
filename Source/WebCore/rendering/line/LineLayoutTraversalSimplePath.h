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

#include "LineLayoutTraversal.h"
#include "SimpleLineLayoutResolver.h"

namespace WebCore {
namespace LineLayoutTraversal {

class SimplePath {
public:
    SimplePath(SimpleLineLayout::RunResolver::Iterator iterator, SimpleLineLayout::RunResolver::Iterator end)
        : m_iterator(iterator)
        , m_end(end)
    { }

    FloatRect rect() const { return (*m_iterator).rect(); }

    float baselineOffset() const { return (*m_iterator).baselineOffset(); }

    bool isLeftToRightDirection() const { return true; }
    bool isHorizontal() const { return true; }
    bool dirOverride() const { return false; }
    bool isLineBreak() const { return (*m_iterator).isLineBreak(); }

    bool useLineBreakBoxRenderTreeDumpQuirk() const
    {
        if (m_iterator.atBegin())
            return false;
        auto previous = m_iterator;
        --previous;
        return !(*previous).isEndOfLine();
    }

    bool hasHyphen() const { return (*m_iterator).hasHyphen(); }
    StringView text() const { return (*m_iterator).text(); }
    unsigned localStartOffset() const { return (*m_iterator).localStart(); }
    unsigned localEndOffset() const { return (*m_iterator).localEnd(); }
    unsigned length() const { return (*m_iterator).end() - (*m_iterator).start(); }

    bool isLastOnLine() const
    {
        auto next = m_iterator;
        ++next;
        return next == m_end || (*m_iterator).lineIndex() != (*next).lineIndex();
    }
    bool isLast() const
    {
        auto next = m_iterator;
        ++next;
        return next == m_end;
    };

    void traverseNextTextBoxInVisualOrder()
    {
        // This function is currently only used for consecutive text runs.
        ++m_iterator;
    }
    void traverseNextTextBoxInTextOrder() {  traverseNextTextBoxInVisualOrder(); }

    bool operator==(const SimplePath& other) const { return m_iterator == other.m_iterator; }
    bool atEnd() const { return m_iterator == m_end; }

private:
    SimpleLineLayout::RunResolver::Iterator m_iterator;
    SimpleLineLayout::RunResolver::Iterator m_end;
};

}
}
