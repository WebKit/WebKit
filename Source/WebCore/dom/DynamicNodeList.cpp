/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
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
#include "DynamicNodeList.h"

#include "Document.h"
#include "Element.h"

namespace WebCore {

DynamicSubtreeNodeList::SubtreeCaches::SubtreeCaches()
    : m_cachedItem(0)
    , m_isLengthCacheValid(false)
    , m_isItemCacheValid(false)
    , m_domTreeVersionAtTimeOfCaching(0)
{
}

void DynamicSubtreeNodeList::SubtreeCaches::setLengthCache(Node* node, unsigned length)
{
    if (m_isItemCacheValid && !domVersionIsConsistent()) {
        m_cachedItem = node;
        m_isItemCacheValid = false;
    } else if (!m_isItemCacheValid)
        m_cachedItem = node; // Used in domVersionIsConsistent.
    m_cachedLength = length;
    m_isLengthCacheValid = true;
    m_domTreeVersionAtTimeOfCaching = node->document()->domTreeVersion();
}

void DynamicSubtreeNodeList::SubtreeCaches::setItemCache(Node* item, unsigned offset)
{
    if (m_isLengthCacheValid && !domVersionIsConsistent())
        m_isLengthCacheValid = false;
    m_cachedItem = item;
    m_cachedItemOffset = offset;
    m_isItemCacheValid = true;
    m_domTreeVersionAtTimeOfCaching = item->document()->domTreeVersion();
}

void DynamicSubtreeNodeList::SubtreeCaches::reset()
{
    m_cachedItem = 0;
    m_isLengthCacheValid = false;
    m_isItemCacheValid = false;
}

DynamicSubtreeNodeList::DynamicSubtreeNodeList(PassRefPtr<Node> node)
    : DynamicNodeList(node)
{
    rootNode()->registerDynamicSubtreeNodeList(this);
}

DynamicSubtreeNodeList::~DynamicSubtreeNodeList()
{
    rootNode()->unregisterDynamicSubtreeNodeList(this);
}

unsigned DynamicSubtreeNodeList::length() const
{
    if (m_caches.isLengthCacheValid())
        return m_caches.cachedLength();

    unsigned length = 0;

    for (Node* n = node()->firstChild(); n; n = n->traverseNextNode(rootNode()))
        length += n->isElementNode() && nodeMatches(static_cast<Element*>(n));

    m_caches.setLengthCache(node(), length);

    return length;
}

Node* DynamicSubtreeNodeList::itemForwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const
{
    ASSERT(remainingOffset >= 0);
    for (Node* n = start; n; n = n->traverseNextNode(rootNode())) {
        if (n->isElementNode() && nodeMatches(static_cast<Element*>(n))) {
            if (!remainingOffset) {
                m_caches.setItemCache(n, offset);
                return n;
            }
            --remainingOffset;
        }
    }

    return 0; // no matching node in this subtree
}

Node* DynamicSubtreeNodeList::itemBackwardsFromCurrent(Node* start, unsigned offset, int remainingOffset) const
{
    ASSERT(remainingOffset < 0);
    for (Node* n = start; n; n = n->traversePreviousNode(rootNode())) {
        if (n->isElementNode() && nodeMatches(static_cast<Element*>(n))) {
            if (!remainingOffset) {
                m_caches.setItemCache(n, offset);
                return n;
            }
            ++remainingOffset;
        }
    }

    return 0; // no matching node in this subtree
}

Node* DynamicSubtreeNodeList::item(unsigned offset) const
{
    int remainingOffset = offset;
    Node* start = node()->firstChild();
    if (m_caches.isItemCacheValid()) {
        if (offset == m_caches.cachedItemOffset())
            return m_caches.cachedItem();
        if (offset > m_caches.cachedItemOffset() || m_caches.cachedItemOffset() - offset < offset) {
            start = m_caches.cachedItem();
            remainingOffset -= m_caches.cachedItemOffset();
        }
    }

    if (remainingOffset < 0)
        return itemBackwardsFromCurrent(start, offset, remainingOffset);
    return itemForwardsFromCurrent(start, offset, remainingOffset);
}

Node* DynamicNodeList::itemWithName(const AtomicString& elementId) const
{
    if (node()->isDocumentNode() || node()->inDocument()) {
        Element* node = this->node()->treeScope()->getElementById(elementId);
        if (node && nodeMatches(node) && node->isDescendantOf(this->node()))
            return node;
        if (!node)
            return 0;
        // In the case of multiple nodes with the same name, just fall through.
    }

    unsigned length = this->length();
    for (unsigned i = 0; i < length; i++) {
        Node* node = item(i);
        // FIXME: This should probably be using getIdAttribute instead of idForStyleResolution.
        if (node->hasID() && static_cast<Element*>(node)->idForStyleResolution() == elementId)
            return node;
    }

    return 0;
}

bool DynamicSubtreeNodeList::isDynamicNodeList() const
{
    return true;
}

void DynamicSubtreeNodeList::invalidateCache()
{
    m_caches.reset();
}

DynamicSubtreeNodeList::Caches::Caches()
    : lastItem(0)
    , isLengthCacheValid(false)
    , isItemCacheValid(false)
{
}

PassRefPtr<DynamicNodeList::Caches> DynamicNodeList::Caches::create()
{
    return adoptRef(new Caches());
}

void DynamicNodeList::Caches::reset()
{
    lastItem = 0;
    isLengthCacheValid = false;
    isItemCacheValid = false;     
}

} // namespace WebCore
