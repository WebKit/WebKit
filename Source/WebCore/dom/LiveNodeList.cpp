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
#include "LiveNodeList.h"

#include "ClassNodeList.h"
#include "Element.h"
#include "ElementTraversal.h"
#include "HTMLCollection.h"
#include "TagNodeList.h"

namespace WebCore {

ContainerNode& LiveNodeList::rootNode() const
{
    if (isRootedAtDocument() && ownerNode().inDocument())
        return ownerNode().document();

    return ownerNode();
}

template <class NodeListType>
inline bool isMatchingElement(const NodeListType*, Element*);

template <> inline bool isMatchingElement(const LiveNodeList* nodeList, Element* element)
{
    return nodeList->nodeMatches(element);
}

template <> inline bool isMatchingElement(const HTMLTagNodeList* nodeList, Element* element)
{
    return nodeList->nodeMatchesInlined(element);
}

template <> inline bool isMatchingElement(const ClassNodeList* nodeList, Element* element)
{
    return nodeList->nodeMatchesInlined(element);
}

ALWAYS_INLINE Element* LiveNodeList::iterateForPreviousElement(Element* current) const
{
    ContainerNode& rootNode = this->rootNode();
    for (; current; current = ElementTraversal::previous(current, &rootNode)) {
        if (isMatchingElement(static_cast<const LiveNodeList*>(this), current))
            return current;
    }
    return 0;
}

ALWAYS_INLINE Element* LiveNodeList::itemBefore(Element* previous) const
{
    Element* current;
    if (LIKELY(!!previous)) // Without this LIKELY, length() and item() can be 10% slower.
        current = ElementTraversal::previous(previous, &rootNode());
    else
        current = ElementTraversal::lastWithin(&rootNode());

    return iterateForPreviousElement(current);
}

template <class NodeListType>
inline Element* firstMatchingElement(const NodeListType* nodeList, ContainerNode* root)
{
    Element* element = ElementTraversal::firstWithin(root);
    while (element && !isMatchingElement(nodeList, element))
        element = ElementTraversal::next(element, root);
    return element;
}

template <class NodeListType>
inline Element* nextMatchingElement(const NodeListType* nodeList, Element* current, ContainerNode* root)
{
    do {
        current = ElementTraversal::next(current, root);
    } while (current && !isMatchingElement(nodeList, current));
    return current;
}

template <class NodeListType>
inline Element* traverseMatchingElementsForwardToOffset(const NodeListType* nodeList, unsigned offset, Element* currentElement, unsigned& currentOffset, ContainerNode* root)
{
    ASSERT_WITH_SECURITY_IMPLICATION(currentOffset < offset);
    while ((currentElement = nextMatchingElement(nodeList, currentElement, root))) {
        if (++currentOffset == offset)
            return currentElement;
    }
    return 0;
}

inline Element* LiveNodeList::traverseLiveNodeListFirstElement(ContainerNode* root) const
{
    if (type() == HTMLTagNodeListType)
        return firstMatchingElement(static_cast<const HTMLTagNodeList*>(this), root);
    if (type() == ClassNodeListType)
        return firstMatchingElement(static_cast<const ClassNodeList*>(this), root);
    return firstMatchingElement(static_cast<const LiveNodeList*>(this), root);
}

inline Element* LiveNodeList::traverseLiveNodeListForwardToOffset(unsigned offset, Element* currentElement, unsigned& currentOffset, ContainerNode* root) const
{
    if (type() == HTMLTagNodeListType)
        return traverseMatchingElementsForwardToOffset(static_cast<const HTMLTagNodeList*>(this), offset, currentElement, currentOffset, root);
    if (type() == ClassNodeListType)
        return traverseMatchingElementsForwardToOffset(static_cast<const ClassNodeList*>(this), offset, currentElement, currentOffset, root);
    return traverseMatchingElementsForwardToOffset(static_cast<const LiveNodeList*>(this), offset, currentElement, currentOffset, root);
}

bool ALWAYS_INLINE LiveNodeList::isLastItemCloserThanLastOrCachedItem(unsigned offset) const
{
    ASSERT(isLengthCacheValid());
    unsigned distanceFromLastItem = cachedLength() - offset;
    if (!isElementCacheValid())
        return distanceFromLastItem < offset;

    return cachedElementOffset() < offset && distanceFromLastItem < offset - cachedElementOffset();
}
    
bool ALWAYS_INLINE LiveNodeList::isFirstItemCloserThanCachedItem(unsigned offset) const
{
    ASSERT(isElementCacheValid());
    if (cachedElementOffset() < offset)
        return false;

    unsigned distanceFromCachedItem = cachedElementOffset() - offset;
    return offset < distanceFromCachedItem;
}

unsigned LiveNodeList::length() const
{
    if (isLengthCacheValid())
        return cachedLength();

    item(UINT_MAX);
    ASSERT(isLengthCacheValid());
    
    return cachedLength();
}

Node* LiveNodeList::item(unsigned offset) const
{
    if (isElementCacheValid() && cachedElementOffset() == offset)
        return cachedElement();

    if (isLengthCacheValid() && cachedLength() <= offset)
        return 0;

    ContainerNode& root = rootNode();

    if (isLengthCacheValid() && isLastItemCloserThanLastOrCachedItem(offset)) {
        Element* lastItem = itemBefore(0);
        ASSERT(lastItem);
        setCachedElement(*lastItem, cachedLength() - 1);
    } else if (!isElementCacheValid() || isFirstItemCloserThanCachedItem(offset)) {
        Element* firstItem = traverseLiveNodeListFirstElement(&root);
        if (!firstItem) {
            setLengthCache(0);
            return 0;
        }
        setCachedElement(*firstItem, 0);
        ASSERT(!cachedElementOffset());
    }

    if (cachedElementOffset() == offset)
        return cachedElement();

    return elementBeforeOrAfterCachedElement(offset, &root);
}

inline Element* LiveNodeList::elementBeforeOrAfterCachedElement(unsigned offset, ContainerNode* root) const
{
    unsigned currentOffset = cachedElementOffset();
    Element* currentItem = cachedElement();
    ASSERT(currentItem);
    ASSERT(currentOffset != offset);

    if (offset < cachedElementOffset()) {
        while ((currentItem = itemBefore(currentItem))) {
            ASSERT(currentOffset);
            currentOffset--;
            if (currentOffset == offset) {
                setCachedElement(*currentItem, currentOffset);
                return currentItem;
            }
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    currentItem = traverseLiveNodeListForwardToOffset(offset, currentItem, currentOffset, root);

    if (!currentItem) {
        // Did not find the item. On plus side, we now know the length.
        setLengthCache(currentOffset + 1);
        return 0;
    }
    setCachedElement(*currentItem, currentOffset);
    return currentItem;
}

void LiveNodeList::invalidateCache() const
{
    m_cachedElement = nullptr;
    m_isLengthCacheValid = false;
    m_isElementCacheValid = false;
}

Node* LiveNodeList::namedItem(const AtomicString& elementId) const
{
    // FIXME: Why doesn't this look into the name attribute like HTMLCollection::namedItem does?
    Node& rootNode = this->rootNode();

    if (rootNode.inDocument()) {
        Element* element = rootNode.treeScope().getElementById(elementId);
        if (element && nodeMatches(element) && element->isDescendantOf(&rootNode))
            return element;
        if (!element)
            return 0;
        // In the case of multiple nodes with the same name, just fall through.
    }

    unsigned length = this->length();
    for (unsigned i = 0; i < length; i++) {
        Node* node = item(i);
        if (!node->isElementNode())
            continue;
        Element* element = toElement(node);
        // FIXME: This should probably be using getIdAttribute instead of idForStyleResolution.
        if (element->hasID() && element->idForStyleResolution() == elementId)
            return node;
    }

    return 0;
}

} // namespace WebCore
