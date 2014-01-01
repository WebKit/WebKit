/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2007, 2008, 2013 Apple Inc. All rights reserved.
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

#include "ElementIterator.h"
#include "NodeRareData.h"

namespace WebCore {

EmptyNodeList::~EmptyNodeList()
{
    m_owner.get().nodeLists()->removeEmptyChildNodeList(this);
}

ChildNodeList::ChildNodeList(ContainerNode& parent)
    : m_parent(parent)
{
}

ChildNodeList::~ChildNodeList()
{
    m_parent.get().nodeLists()->removeChildNodeList(this);
}

unsigned ChildNodeList::length() const
{
    return m_indexCache.nodeCount(*this);
}

Node* ChildNodeList::item(unsigned index) const
{
    return m_indexCache.nodeAt(*this, index);
}

Node* ChildNodeList::collectionFirst() const
{
    return m_parent->firstChild();
}

Node* ChildNodeList::collectionLast() const
{
    return m_parent->lastChild();
}

Node* ChildNodeList::collectionTraverseForward(Node& current, unsigned count, unsigned& traversedCount) const
{
    ASSERT(count);
    Node* child = &current;
    for (traversedCount = 0; traversedCount < count; ++traversedCount) {
        child = child->nextSibling();
        if (!child)
            return nullptr;
    }
    return child;
}

Node* ChildNodeList::collectionTraverseBackward(Node& current, unsigned count) const
{
    ASSERT(count);
    Node* child = &current;
    for (; count && child ; --count)
        child = child->previousSibling();
    return child;
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
    for (auto& element : childrenOfType<Element>(m_parent.get())) {
        if (element.hasID() && element.idForStyleResolution() == name)
            return const_cast<Element*>(&element);
    }
    return nullptr;
}

void ChildNodeList::invalidateCache()
{
    m_indexCache.invalidate();
}

} // namespace WebCore
