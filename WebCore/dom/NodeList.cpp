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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "NodeList.h"

#include "Document.h"
#include "Element.h"
#include "Node.h"

namespace WebCore {

NodeList::NodeList(PassRefPtr<Node> _rootNode)
    : rootNode(_rootNode),
      isLengthCacheValid(false),
      isItemCacheValid(false)
{
    rootNode->registerNodeList(this);
}    

NodeList::~NodeList()
{
    rootNode->unregisterNodeList(this);
}

unsigned NodeList::recursiveLength(Node* start) const
{
    if (!start)
        start = rootNode.get();

    if (isLengthCacheValid && start == rootNode)
        return cachedLength;

    unsigned len = 0;

    for (Node* n = start->firstChild(); n; n = n->nextSibling())
        if (n->isElementNode()) {
            if (nodeMatches(n))
                len++;
            len += recursiveLength(n);
        }

    if (start == rootNode) {
        cachedLength = len;
        isLengthCacheValid = true;
    }

    return len;
}

Node* NodeList::recursiveItem(unsigned offset, Node* start) const
{
    int remainingOffset = offset;
    if (!start) {
        start = rootNode->firstChild();
        if (isItemCacheValid) {
            if (offset == lastItemOffset) {
                return lastItem;
            } else if (offset > lastItemOffset) {
                start = lastItem;
                remainingOffset -= lastItemOffset;
            }
        }
    }

    for (Node *n = start; n; n = n->traverseNextNode(rootNode.get())) {
        if (n->isElementNode()) {
            if (nodeMatches(n)) {
                if (!remainingOffset) {
                    lastItem = n;
                    lastItemOffset = offset;
                    isItemCacheValid = true;
                    return n;
                }
                remainingOffset--;
            }
        }
    }

    return 0; // no matching node in this subtree
}

Node* NodeList::itemById(const AtomicString& elementId) const
{
    if (rootNode->isDocumentNode() || rootNode->inDocument()) {
        Node* node = rootNode->document()->getElementById(elementId);

        if (!node || !nodeMatches(node))
            return 0;

        for (Node* p = node->parentNode(); p; p = p->parentNode())
            if (p == rootNode)
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
    isLengthCacheValid = false;
    isItemCacheValid = false;     
    lastItem = 0;
}

}
