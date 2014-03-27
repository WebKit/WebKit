/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2010, 2013 Apple Inc. All rights reserved.
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

LiveNodeList::LiveNodeList(ContainerNode& ownerNode, Type type, NodeListInvalidationType invalidationType, NodeListRootType rootType)
    : m_ownerNode(ownerNode)
    , m_rootType(rootType)
    , m_invalidationType(invalidationType)
    , m_type(static_cast<unsigned>(type))
{
    ASSERT(m_rootType == static_cast<unsigned>(rootType));
    ASSERT(m_invalidationType == static_cast<unsigned>(invalidationType));
    ASSERT(m_type == static_cast<unsigned>(type));
}

LiveNodeList::~LiveNodeList()
{
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
            return nullptr;
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

    return nullptr;
}

} // namespace WebCore
