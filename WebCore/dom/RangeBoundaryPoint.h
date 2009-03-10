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

#ifndef RangeBoundaryPoint_h
#define RangeBoundaryPoint_h

#include "Node.h"
#include "Position.h"

namespace WebCore {

class RangeBoundaryPoint {
public:
    RangeBoundaryPoint();
    explicit RangeBoundaryPoint(PassRefPtr<Node> container);

    const Position& position() const;
    Node* container() const;
    int offset() const;
    Node* childBefore() const;

    void clear();

    void set(PassRefPtr<Node> container, int offset, Node* childBefore);
    void setOffset(int offset);
    void setToChild(Node* child);
    void setToStart(PassRefPtr<Node> container);
    void setToEnd(PassRefPtr<Node> container);

    void childBeforeWillBeRemoved();
    void invalidateOffset() const;

private:
    static const int invalidOffset = -1;

    mutable Position m_position;
    Node* m_childBefore;
};

inline RangeBoundaryPoint::RangeBoundaryPoint()
    : m_childBefore(0)
{
}

inline RangeBoundaryPoint::RangeBoundaryPoint(PassRefPtr<Node> container)
    : m_position(container, 0)
    , m_childBefore(0)
{
}

inline Node* RangeBoundaryPoint::container() const
{
    return m_position.container.get();
}

inline Node* RangeBoundaryPoint::childBefore() const
{
    return m_childBefore;
}

inline const Position& RangeBoundaryPoint::position() const
{
    if (m_position.m_offset >= 0)
        return m_position;
    ASSERT(m_childBefore);
    m_position.m_offset = m_childBefore->nodeIndex() + 1;
    return m_position;
}

inline int RangeBoundaryPoint::offset() const
{
    return position().m_offset;
}

inline void RangeBoundaryPoint::clear()
{
    m_position.clear();
    m_childBefore = 0;
}

inline void RangeBoundaryPoint::set(PassRefPtr<Node> container, int offset, Node* childBefore)
{
    ASSERT(offset >= 0);
    ASSERT(childBefore == (offset ? container->childNode(offset - 1) : 0));
    m_position.container = container;
    m_position.m_offset = offset;
    m_childBefore = childBefore;
}

inline void RangeBoundaryPoint::setOffset(int offset)
{
    ASSERT(m_position.container);
    ASSERT(m_position.container->offsetInCharacters());
    ASSERT(m_position.m_offset >= 0);
    ASSERT(!m_childBefore);
    m_position.m_offset = offset;
}

inline void RangeBoundaryPoint::setToChild(Node* child)
{
    ASSERT(child);
    ASSERT(child->parentNode());
    m_position.container = child->parentNode();
    m_childBefore = child->previousSibling();
    m_position.m_offset = m_childBefore ? invalidOffset : 0;
}

inline void RangeBoundaryPoint::setToStart(PassRefPtr<Node> container)
{
    ASSERT(container);
    m_position.container = container;
    m_position.m_offset = 0;
    m_childBefore = 0;
}

inline void RangeBoundaryPoint::setToEnd(PassRefPtr<Node> container)
{
    ASSERT(container);
    m_position.container = container;
    if (m_position.container->offsetInCharacters()) {
        m_position.m_offset = m_position.container->maxCharacterOffset();
        m_childBefore = 0;
    } else {
        m_childBefore = m_position.container->lastChild();
        m_position.m_offset = m_childBefore ? invalidOffset : 0;
    }
}

inline void RangeBoundaryPoint::childBeforeWillBeRemoved()
{
    ASSERT(m_position.m_offset);
    m_childBefore = m_childBefore->previousSibling();
    if (!m_childBefore)
        m_position.m_offset = 0;
    else if (m_position.m_offset > 0)
        --m_position.m_offset;
}

inline void RangeBoundaryPoint::invalidateOffset() const
{
    m_position.m_offset = invalidOffset;
}

inline bool operator==(const RangeBoundaryPoint& a, const RangeBoundaryPoint& b)
{
    if (a.container() != b.container())
        return false;
    if (a.childBefore() || b.childBefore()) {
        if (a.childBefore() != b.childBefore())
            return false;
    } else {
        if (a.offset() != b.offset())
            return false;
    }
    return true;
}

}

#endif
