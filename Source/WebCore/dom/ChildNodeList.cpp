/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ChildNodeList.h"

#include "Element.h"
#include "ElementIterator.h"
#include "NodeRareData.h"

namespace WebCore {

EmptyNodeList::~EmptyNodeList()
{
    m_owner.get().nodeLists()->removeEmptyChildNodeList(this);
}

ChildNodeList::ChildNodeList(ContainerNode& parent)
    : m_parent(parent)
    , m_cachedLength(0)
    , m_cachedLengthValid(false)
    , m_cachedCurrentPosition(0)
    , m_cachedCurrentNode(nullptr)
{
}

ChildNodeList::~ChildNodeList()
{
    m_parent.get().nodeLists()->removeChildNodeList(this);
}

unsigned ChildNodeList::length() const
{
    if (!m_cachedLengthValid) {
        m_cachedLength = m_parent->childNodeCount();
        m_cachedLengthValid = true;
    }
    return m_cachedLength;
}

static Node* childFromFirst(const ContainerNode& parent, unsigned delta)
{
    Node* child = parent.firstChild();
    for (; delta && child; --delta)
        child = child->nextSibling();
    return child;
}

static Node* childFromLast(const ContainerNode& parent, unsigned delta)
{
    Node* child = parent.lastChild();
    for (; delta; --delta)
        child = child->previousSibling();
    ASSERT(child);
    return child;
}

Node* ChildNodeList::nodeBeforeCached(unsigned index) const
{
    ASSERT(m_cachedCurrentNode);
    ASSERT(index < m_cachedCurrentPosition);
    if (index < m_cachedCurrentPosition - index) {
        m_cachedCurrentNode = childFromFirst(m_parent.get(), index);
        m_cachedCurrentPosition = index;
        return m_cachedCurrentNode;
    }
    while (m_cachedCurrentNode && m_cachedCurrentPosition > index) {
        m_cachedCurrentNode = m_cachedCurrentNode->previousSibling();
        --m_cachedCurrentPosition;
    }
    return m_cachedCurrentNode;
}

Node* ChildNodeList::nodeAfterCached(unsigned index) const
{
    ASSERT(m_cachedCurrentNode);
    ASSERT(index > m_cachedCurrentPosition);
    ASSERT(!m_cachedLengthValid || index < m_cachedLength);
    if (m_cachedLengthValid && m_cachedLength - index < index - m_cachedCurrentPosition) {
        m_cachedCurrentNode = childFromLast(m_parent.get(), m_cachedLength - index - 1);
        m_cachedCurrentPosition = index;
        return m_cachedCurrentNode;
    }
    while (m_cachedCurrentNode && m_cachedCurrentPosition < index) {
        m_cachedCurrentNode = m_cachedCurrentNode->nextSibling();
        ++m_cachedCurrentPosition;
    }
    if (!m_cachedCurrentNode && !m_cachedLengthValid) {
        m_cachedLength = m_cachedCurrentPosition;
        m_cachedLengthValid = true;
    }
    return m_cachedCurrentNode;
}

Node* ChildNodeList::item(unsigned index) const
{
    if (m_cachedLengthValid && index >= m_cachedLength)
        return nullptr;
    if (m_cachedCurrentNode) {
        if (index > m_cachedCurrentPosition)
            return nodeAfterCached(index);
        if (index < m_cachedCurrentPosition)
            return nodeBeforeCached(index);
        return m_cachedCurrentNode;
    }
    if (m_cachedLengthValid && m_cachedLength - index < index)
        m_cachedCurrentNode = childFromLast(m_parent.get(), m_cachedLength - index - 1);
    else
        m_cachedCurrentNode = childFromFirst(m_parent.get(), index);
    m_cachedCurrentPosition = index;
    return m_cachedCurrentNode;
}

Node* ChildNodeList::namedItem(const AtomicString& name) const
{
    // FIXME: According to the spec child node list should not have namedItem().
    if (m_parent.get().inDocument()) {
        Element* element = m_parent.get().treeScope().getElementById(name);
        if (element && element->parentNode() == &m_parent.get())
            return element;
        if (!element || !m_parent.get().treeScope().containsMultipleElementsWithId(name))
            return nullptr;
    }
    auto children = elementChildren(m_parent.get());
    for (auto it = children.begin(), end = children.end(); it != end; ++it) {
        auto& element = *it;
        if (element.hasID() && element.idForStyleResolution() == name)
            return const_cast<Element*>(&element);
    }
    return nullptr;
}

void ChildNodeList::invalidateCache()
{
    m_cachedCurrentNode = nullptr;
    m_cachedLengthValid = false;
}

} // namespace WebCore
