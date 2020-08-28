/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "BoundaryPoint.h"
#include "CharacterData.h"

namespace WebCore {

class RangeBoundaryPoint {
public:
    explicit RangeBoundaryPoint(Node& container);

    Node& container() const;
    unsigned offset() const;
    Node* childBefore() const;

    void set(Ref<Node>&& container, unsigned offset, Node* childBefore);
    void setOffset(unsigned);

    void setToBeforeNode(Node&);
    void setToAfterNode(Node&);
    void setToBeforeContents(Ref<Node>&&);
    void setToAfterContents(Ref<Node>&&);

    void childBeforeWillBeRemoved();
    void invalidateOffset() const;

private:
    Ref<Node> m_container;
    mutable Optional<unsigned> m_offsetInContainer { 0 };
    RefPtr<Node> m_childBeforeBoundary;
};

BoundaryPoint makeBoundaryPoint(const RangeBoundaryPoint&);

inline RangeBoundaryPoint::RangeBoundaryPoint(Node& container)
    : m_container(container)
{
}

inline Node& RangeBoundaryPoint::container() const
{
    return m_container;
}

inline Node* RangeBoundaryPoint::childBefore() const
{
    return m_childBeforeBoundary.get();
}

inline unsigned RangeBoundaryPoint::offset() const
{
    if (!m_offsetInContainer) {
        ASSERT(m_childBeforeBoundary);
        m_offsetInContainer = m_childBeforeBoundary->computeNodeIndex() + 1;
    }
    return *m_offsetInContainer;
}

inline void RangeBoundaryPoint::set(Ref<Node>&& container, unsigned offset, Node* childBefore)
{
    ASSERT(childBefore == (offset ? container->traverseToChildAt(offset - 1) : 0));
    m_container = WTFMove(container);
    m_offsetInContainer = offset;
    m_childBeforeBoundary = childBefore;
}

inline void RangeBoundaryPoint::setOffset(unsigned offset)
{
    ASSERT(m_container->isCharacterDataNode());
    ASSERT(m_offsetInContainer);
    ASSERT(!m_childBeforeBoundary);
    m_offsetInContainer = offset;
}

inline void RangeBoundaryPoint::setToBeforeNode(Node& child)
{
    ASSERT(child.parentNode());
    m_childBeforeBoundary = child.previousSibling();
    m_container = *child.parentNode();
    m_offsetInContainer = m_childBeforeBoundary ? WTF::nullopt : makeOptional(0U);
}

inline void RangeBoundaryPoint::setToAfterNode(Node& child)
{
    ASSERT(child.parentNode());
    m_childBeforeBoundary = &child;
    m_container = *child.parentNode();
    m_offsetInContainer = m_childBeforeBoundary ? WTF::nullopt : makeOptional(0U);
}

inline void RangeBoundaryPoint::setToBeforeContents(Ref<Node>&& container)
{
    m_container = WTFMove(container);
    m_offsetInContainer = 0;
    m_childBeforeBoundary = nullptr;
}

inline void RangeBoundaryPoint::setToAfterContents(Ref<Node>&& container)
{
    m_container = WTFMove(container);
    if (is<CharacterData>(m_container)) {
        m_offsetInContainer = downcast<CharacterData>(m_container.get()).length();
        m_childBeforeBoundary = nullptr;
    } else {
        m_childBeforeBoundary = m_container->lastChild();
        m_offsetInContainer = m_childBeforeBoundary ? WTF::nullopt : makeOptional(0U);
    }
}

inline void RangeBoundaryPoint::childBeforeWillBeRemoved()
{
    ASSERT(!m_offsetInContainer || m_offsetInContainer.value());
    m_childBeforeBoundary = m_childBeforeBoundary->previousSibling();
    if (!m_childBeforeBoundary)
        m_offsetInContainer = 0;
    else if (m_offsetInContainer && m_offsetInContainer.value() > 0)
        --(m_offsetInContainer.value());
}

inline void RangeBoundaryPoint::invalidateOffset() const
{
    m_offsetInContainer = WTF::nullopt;
}

inline bool operator==(const RangeBoundaryPoint& a, const RangeBoundaryPoint& b)
{
    if (&a.container() != &b.container())
        return false;
    if (a.childBefore() || b.childBefore())
        return a.childBefore() == b.childBefore();
    return a.offset() == b.offset();
}

inline BoundaryPoint makeBoundaryPoint(const RangeBoundaryPoint& point)
{
    return { point.container(), point.offset() };
}

} // namespace WebCore
