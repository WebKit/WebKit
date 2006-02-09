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
#include "NodeListImpl.h"

#include "DocumentImpl.h"
#include "NodeImpl.h"
#include "dom_node.h"

namespace DOM {

NodeListImpl::NodeListImpl(NodeImpl *_rootNode)
    : rootNode(_rootNode),
      isLengthCacheValid(false),
      isItemCacheValid(false)
{
    rootNode->ref();
    rootNode->registerNodeList(this);
}    

NodeListImpl::~NodeListImpl()
{
    rootNode->unregisterNodeList(this);
    rootNode->deref();
}

unsigned NodeListImpl::recursiveLength( NodeImpl *start ) const
{
    if (!start)
        start = rootNode;

    if (isLengthCacheValid && start == rootNode) {
        return cachedLength;
    }

    unsigned len = 0;

    for(NodeImpl *n = start->firstChild(); n != 0; n = n->nextSibling()) {
        if ( n->nodeType() == Node::ELEMENT_NODE ) {
            if (nodeMatches(n))
                len++;
            len+= recursiveLength(n);
        }
    }

    if (start == rootNode) {
        cachedLength = len;
        isLengthCacheValid = true;
    }

    return len;
}

NodeImpl *NodeListImpl::recursiveItem ( unsigned offset, NodeImpl *start) const
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

    NodeImpl *n = start;

    while (n) {
        if ( n->nodeType() == Node::ELEMENT_NODE ) {
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

        n = n->traverseNextNode(rootNode);
    }

    return 0; // no matching node in this subtree
}

NodeImpl* NodeListImpl::itemById(const AtomicString& elementId) const
{
    if (rootNode->isDocumentNode() || rootNode->inDocument()) {
        NodeImpl *node = rootNode->getDocument()->getElementById(elementId);

        if (!node || !nodeMatches(node))
            return 0;

        for (NodeImpl *p = node->parentNode(); p; p = p->parentNode()) {
            if (p == rootNode)
                return node;
        }

        return 0;
    }

    unsigned l = length();

    for ( unsigned i = 0; i < l; i++ ) {
        NodeImpl *node = item(i);
        
        if ( static_cast<ElementImpl *>(node)->getIDAttribute() == elementId ) {
            return node;
        }
    }

    return 0;
}

void NodeListImpl::rootNodeChildrenChanged()
{
    isLengthCacheValid = false;
    isItemCacheValid = false;     
    lastItem = 0;
}

}
