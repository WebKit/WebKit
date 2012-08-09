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
#include "HTMLCollection.h"
#include "HTMLPropertiesCollection.h"
#include "PropertyNodeList.h"

namespace WebCore {

Node* DynamicNodeListCacheBase::rootNode() const
{
    if (isRootedAtDocument() && m_ownerNode->inDocument())
        return m_ownerNode->document();

    if (ownerNodeHasItemRefAttribute()) {
        if (m_ownerNode->inDocument())
            return m_ownerNode->document();

        Node* root = m_ownerNode.get();
        while (Node* parent = root->parentNode())
            root = parent;
        return root;
    }

    return m_ownerNode.get();
}

void DynamicNodeListCacheBase::invalidateCache() const
{
    m_cachedItem = 0;
    m_isLengthCacheValid = false;
    m_isItemCacheValid = false;
    m_isNameCacheValid = false;
    m_isItemRefElementsCacheValid = false;
    if (type() == NodeListCollectionType)
        return;

    const HTMLCollectionCacheBase* cacheBase = static_cast<const HTMLCollectionCacheBase*>(this);
    cacheBase->m_idCache.clear();
    cacheBase->m_nameCache.clear();
    cacheBase->m_cachedElementsArrayOffset = 0;

#if ENABLE(MICRODATA)
    // FIXME: There should be more generic mechanism to clear caches in subclasses.
    if (type() == ItemProperties)
        static_cast<const HTMLPropertiesCollection*>(this)->invalidateCache();
#endif
}

unsigned DynamicNodeList::length() const
{
    return lengthCommon();
}

Node* DynamicNodeList::item(unsigned offset) const
{
    return itemCommon(offset);
}

Node* DynamicNodeList::itemWithName(const AtomicString& elementId) const
{
    Node* rootNode = this->rootNode();

    if (rootNode->inDocument()) {
#if ENABLE(MICRODATA)
        if (rootType() == NodeListIsRootedAtDocumentIfOwnerHasItemrefAttr)
            static_cast<const PropertyNodeList*>(this)->updateRefElements();
#endif

        Element* element = rootNode->treeScope()->getElementById(elementId);
        if (element && nodeMatches(element) && element->isDescendantOf(rootNode))
            return element;
        if (!element)
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

} // namespace WebCore
