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

#include "Node.h"
#include "Position.h"

namespace WebCore {

class RangeBoundaryPoint {
public:
    explicit RangeBoundaryPoint(Node* container);

    explicit RangeBoundaryPoint(const RangeBoundaryPoint&);

    const Position toPosition() const;

    Node* container() const;
    unsigned offset() const;
    Node* childBefore() const;

    void clear();

    void set(Ref<Node>&& container, unsigned offset, Node* childBefore);
    void setOffset(unsigned);

    void setToBeforeChild(Node&);
    void setToAfterChild(Node&);
    void setToStartOfNode(Ref<Node>&&);
    void setToEndOfNode(Ref<Node>&&);

    void childBeforeWillBeRemoved();
    void invalidateOffset() const;
    void ensureOffsetIsValid() const;

private:
    RefPtr<Node> m_containerNode;
    mutable std::optional<unsigned> m_offsetInContainer { 0 };
    RefPtr<Node> m_childBeforeBoundary;
};

inline RangeBoundaryPoint::RangeBoundaryPoint(Node* container)
    : m_containerNode(container)
{
    ASSERT(m_containerNode);
}

inline RangeBoundaryPoint::RangeBoundaryPoint(const RangeBoundaryPoint& other)
    : m_containerNode(other.container())
    , m_offsetInContainer(other.offset())
    , m_childBeforeBoundary(other.childBefore())
{
}

inline Node* RangeBoundaryPoint::container() const
{
    return m_containerNode.get();
}

inline Node* RangeBoundaryPoint::childBefore() const
{
    return m_childBeforeBoundary.get();
}

inline void RangeBoundaryPoint::ensureOffsetIsValid() const
{
    if (m_offsetInContainer)
        return;

    ASSERT(m_childBeforeBoundary);
    m_offsetInContainer = m_childBeforeBoundary->computeNodeIndex() + 1;
}

inline const Position RangeBoundaryPoint::toPosition() const
{
    ensureOffsetIsValid();
    return createLegacyEditingPosition(m_containerNode.get(), m_offsetInContainer.value());
}

inline unsigned RangeBoundaryPoint::offset() const
{
    ensureOffsetIsValid();
    return m_offsetInContainer.value();
}

inline void RangeBoundaryPoint::clear()
{
    m_containerNode = nullptr;
    m_offsetInContainer = 0;
    m_childBeforeBoundary = nullptr;
}

inline void RangeBoundaryPoint::set(Ref<Node>&& container, unsigned offset, Node* childBefore)
{
    ASSERT(childBefore == (offset ? container->traverseToChildAt(offset - 1) : 0));
    m_containerNode = WTFMove(container);
    m_offsetInContainer = offset;
    m_childBeforeBoundary = childBefore;
}

inline void RangeBoundaryPoint::setOffset(unsigned offset)
{
    ASSERT(m_containerNode);
    ASSERT(m_containerNode->isCharacterDataNode());
    ASSERT(m_offsetInContainer);
    ASSERT(!m_childBeforeBoundary);
    m_offsetInContainer = offset;
}

inline void RangeBoundaryPoint::setToBeforeChild(Node& child)
{
    ASSERT(child.parentNode());
    m_childBeforeBoundary = child.previousSibling();
    m_containerNode = child.parentNode();
    m_offsetInContainer = m_childBeforeBoundary ? std::nullopt : std::optional<unsigned>(0);
}

inline void RangeBoundaryPoint::setToAfterChild(Node& child)
{
    ASSERT(child.parentNode());
    m_childBeforeBoundary = &child;
    m_containerNode = child.parentNode();
    m_offsetInContainer = m_childBeforeBoundary ? std::nullopt : std::optional<unsigned>(0);
}

inline void RangeBoundaryPoint::setToStartOfNode(Ref<Node>&& container)
{
    m_containerNode = WTFMove(container);
    m_offsetInContainer = 0;
    m_childBeforeBoundary = nullptr;
}

inline void RangeBoundaryPoint::setToEndOfNode(Ref<Node>&& container)
{
    m_containerNode = WTFMove(container);
    if (m_containerNode->isCharacterDataNode()) {
        m_offsetInContainer = m_containerNode->maxCharacterOffset();
        m_childBeforeBoundary = nullptr;
    } else {
        m_childBeforeBoundary = m_containerNode->lastChild();
        m_offsetInContainer = m_childBeforeBoundary ? std::nullopt : std::optional<unsigned>(0);
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
    m_offsetInContainer = std::nullopt;
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

} // namespace WebCore
