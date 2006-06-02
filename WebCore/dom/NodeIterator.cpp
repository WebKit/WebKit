/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004 Apple Computer, Inc.
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
 *
 */

#include "config.h"
#include "NodeIterator.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "NodeFilter.h"

namespace WebCore {

NodeIterator::NodeIterator(Node* rootNode, unsigned whatToShow, PassRefPtr<NodeFilter> filter, bool expandEntityReferences)
    : Traversal(rootNode, whatToShow, filter, expandEntityReferences)
    , m_beforeReferenceNode(true)
    , m_detached(false)
    , m_doc(rootNode ? rootNode->document() : 0)
{
    if (document())
        document()->attachNodeIterator(this);
}

NodeIterator::~NodeIterator()
{
    if (document())
        document()->detachNodeIterator(this);
}

Node* NodeIterator::findNextNode(Node* node) const
{
    while ((node = node->traverseNextNode())) {
        // NodeIterators treat the DOM tree as a flat list of nodes.
        // In other words, FILTER_REJECT does not pass over descendants
        // of the rejected node. Hence, FILTER_REJECT is the same as FILTER_SKIP.
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT)
            break;
    }
    return node;
}

Node* NodeIterator::nextNode(ExceptionCode& ec)
{
    if (detached()) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    Node* node = referenceNode() ? referenceNode() : root();
    if (!pointerBeforeReferenceNode() || acceptNode(node) != NodeFilter::FILTER_ACCEPT)
        node = findNextNode(node);
    if (node)
        setReferenceNode(node);
    setPointerBeforeReferenceNode(false);
    return node;
}

Node* NodeIterator::findPreviousNode(Node* node) const
{
    while ((node = node->traversePreviousNode())) {
        // NodeIterators treat the DOM tree as a flat list of nodes.
        // In other words, FILTER_REJECT does not pass over descendants
        // of the rejected node. Hence, FILTER_REJECT is the same as FILTER_SKIP.
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT)
            break;
    }
    return node;
}

Node* NodeIterator::previousNode(ExceptionCode&)
{
    Node* node = referenceNode() ? referenceNode() : root();
    if (pointerBeforeReferenceNode() || acceptNode(node) != NodeFilter::FILTER_ACCEPT)
        node = findPreviousNode(node);
    if (node)
        setReferenceNode(node);
    setPointerBeforeReferenceNode();
    return node;
}

void NodeIterator::detach(ExceptionCode&)
{
    if (!detached() && document())
        document()->detachNodeIterator(this);
    setDetached();
}

void NodeIterator::setReferenceNode(Node* node)
{
    m_referenceNode = node;
}

void NodeIterator::notifyBeforeNodeRemoval(Node* removedNode)
{
    // Iterator is not affected if the removed node is the reference node and is the root.
    // or if removed node is not the reference node, or the ancestor of the reference node.
    if (!removedNode || removedNode == root())
        return;
    bool willRemoveReferenceNode = removedNode == referenceNode();
    bool willRemoveReferenceNodeAncestor = referenceNode() && referenceNode()->isAncestor(removedNode);
    if (!willRemoveReferenceNode && !willRemoveReferenceNodeAncestor)
        return;

    if (pointerBeforeReferenceNode()) {
        Node* node = findNextNode(removedNode);
        if (node) {
            // Move out from under the node being removed if the reference node is
            // a descendant of the node being removed.
            if (willRemoveReferenceNodeAncestor) {
                while (node && node->isAncestor(removedNode))
                    node = findNextNode(node);
            }
            if (node)
                setReferenceNode(node);
        } else {
            node = findPreviousNode(removedNode);
            if (node) {
                // Move out from under the node being removed if the reference node is
                // a descendant of the node being removed.
                if (willRemoveReferenceNodeAncestor) {
                    while (node && node->isAncestor(removedNode))
                        node = findPreviousNode(node);
                }
                if (node) {
                    // Removing last node.
                    // Need to move the pointer after the node preceding the 
                    // new reference node.
                    setReferenceNode(node);
                    setPointerBeforeReferenceNode(false);
                }
            }
        }
    } else {
        Node* node = findPreviousNode(removedNode);
        if (node) {
            // Move out from under the node being removed if the reference node is
            // a descendant of the node being removed.
            if (willRemoveReferenceNodeAncestor) {
                while (node && node->isAncestor(removedNode))
                    node = findPreviousNode(node);
            }
            if (node)
                setReferenceNode(node);
        } else {
            node = findNextNode(removedNode);
            // Move out from under the node being removed if the reference node is
            // a descendant of the node being removed.
            if (willRemoveReferenceNodeAncestor) {
                while (node && node->isAncestor(removedNode))
                    node = findPreviousNode(node);
            }
            if (node)
                setReferenceNode(node);
        }
    }
}

} // namespace WebCore
