/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "RenderElement.h"

namespace WebCore {

class RenderTreePosition {
public:
    explicit RenderTreePosition(RenderElement& parent)
        : m_parent(parent)
    {
    }
    RenderTreePosition(RenderElement& parent, RenderObject* nextSibling)
        : m_parent(parent)
        , m_nextSibling(makeWeakPtr(nextSibling))
        , m_hasValidNextSibling(true)
    {
    }

    RenderElement& parent() const { return m_parent; }
    RenderObject* nextSibling() const { ASSERT(m_hasValidNextSibling); return m_nextSibling.get(); }

    void computeNextSibling(const Node&);
    void moveToLastChild();
    void invalidateNextSibling() { m_hasValidNextSibling = false; }
    void invalidateNextSibling(const RenderObject&);

    RenderObject* nextSiblingRenderer(const Node&) const;

private:
    RenderElement& m_parent;
    WeakPtr<RenderObject> m_nextSibling { nullptr };
    bool m_hasValidNextSibling { false };
#if ASSERT_ENABLED
    unsigned m_assertionLimitCounter { 0 };
#endif
};

inline void RenderTreePosition::moveToLastChild()
{
    m_nextSibling = nullptr;
    m_hasValidNextSibling = true;
}

} // namespace WebCore
