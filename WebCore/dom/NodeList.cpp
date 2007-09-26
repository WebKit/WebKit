/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
#include "NodeList.h"

#include "Document.h"
#include "Element.h"

namespace WebCore {

NodeList::NodeList(PassRefPtr<Node> rootNode)
    : m_rootNode(rootNode)
    , m_caches(new Caches)
    , m_ownsCaches(true)
{
    m_rootNode->registerNodeList(this);
}    

NodeList::NodeList(PassRefPtr<Node> rootNode, NodeList::Caches* info)
    : m_rootNode(rootNode)
    , m_caches(info)
    , m_ownsCaches(false)
{
    m_rootNode->registerNodeList(this);
}    

NodeList::~NodeList()
{
    m_rootNode->unregisterNodeList(this);
    if (m_ownsCaches)
        delete m_caches;
}

unsigned NodeList::recursiveLength(Node* start) const
{
    if (!start)
        start = m_rootNode.get();

    if (m_caches->isLengthCacheValid && start == m_rootNode)
        return m_caches->cachedLength;

    unsigned len = 0;

    for (Node* n = start->firstChild(); n; n = n->nextSibling())
        if (n->isElementNode()) {
            if (nodeMatches(n))
                len++;
            len += recursiveLength(n);
        }

    if (start == m_rootNode) {
        m_caches->cachedLength = len;
        m_caches->isLengthCacheValid = true;
    }

    return len;
}

Node* NodeList::itemForwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const
{
    ASSERT(remainingOffset >= 0);

    for (Node *n = start; n; n = n->traverseNextNode(m_rootNode.get())) {
        if (n->isElementNode()) {
            if (nodeMatches(n)) {
                if (!remainingOffset) {
                    m_caches->lastItem = n;
                    m_caches->lastItemOffset = offset;
                    m_caches->isItemCacheValid = true;
                    return n;
                }
                remainingOffset--;
            }
        }
    }

    return 0; // no matching node in this subtree
}

Node* NodeList::itemBackwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const
{
    ASSERT(remainingOffset < 0);
    for (Node *n = start; n; n = n->traversePreviousNode(m_rootNode.get())) {
        if (n->isElementNode()) {
            if (nodeMatches(n)) {
                if (!remainingOffset) {
                    m_caches->lastItem = n;
                    m_caches->lastItemOffset = offset;
                    m_caches->isItemCacheValid = true;
                    return n;
                }
                remainingOffset++;
            }
        }
    }

    return 0; // no matching node in this subtree
}

Node* NodeList::recursiveItem(unsigned offset, Node* start) const
{
    int remainingOffset = offset;
    if (!start) {
        start = m_rootNode->firstChild();
        if (m_caches->isItemCacheValid) {
            if (offset == m_caches->lastItemOffset) {
                return m_caches->lastItem;
            } else if (offset > m_caches->lastItemOffset || m_caches->lastItemOffset - offset < offset) {
                start = m_caches->lastItem;
                remainingOffset -= m_caches->lastItemOffset;
            }
        }
    }

    if (remainingOffset < 0)
        return itemBackwardsFromCurrent(start, offset, remainingOffset);
    else
        return itemForwardsFromCurrent(start, offset, remainingOffset);
}

Node* NodeList::itemWithName(const AtomicString& elementId) const
{
    if (m_rootNode->isDocumentNode() || m_rootNode->inDocument()) {
        Node* node = m_rootNode->document()->getElementById(elementId);

        if (!node || !nodeMatches(node))
            return 0;

        for (Node* p = node->parentNode(); p; p = p->parentNode())
            if (p == m_rootNode)
                return node;

        return 0;
    }

    unsigned l = length();
    for (unsigned i = 0; i < l; i++) {
        Node* node = item(i);
        if (node->isElementNode() && static_cast<Element*>(node)->getIDAttribute() == elementId)
            return node;
    }

    return 0;
}

void NodeList::rootNodeChildrenChanged()
{
    m_caches->reset();
}


NodeList::Caches::Caches()
    : lastItem(0)
    , isLengthCacheValid(false)
    , isItemCacheValid(false)
{
}

void NodeList::Caches::reset()
{
    lastItem = 0;
    isLengthCacheValid = false;
    isItemCacheValid = false;     
}

}
