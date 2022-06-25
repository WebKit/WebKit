/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "NodeIterator.h"

#include "Document.h"
#include "NodeTraversal.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(NodeIterator);

inline NodeIterator::NodePointer::NodePointer(Node& node, bool isPointerBeforeNode)
    : node(&node)
    , isPointerBeforeNode(isPointerBeforeNode)
{
}

inline void NodeIterator::NodePointer::clear()
{
    node = nullptr;
}

inline bool NodeIterator::NodePointer::moveToNext(Node& root)
{
    if (!node)
        return false;
    if (isPointerBeforeNode) {
        isPointerBeforeNode = false;
        return true;
    }
    node = NodeTraversal::next(*node, &root);
    return node;
}

inline bool NodeIterator::NodePointer::moveToPrevious(Node& root)
{
    if (!node)
        return false;
    if (!isPointerBeforeNode) {
        isPointerBeforeNode = true;
        return true;
    }
    if (node == &root) {
        node = nullptr;
        return false;
    }
    node = NodeTraversal::previous(*node);
    return node;
}

inline NodeIterator::NodeIterator(Node& rootNode, unsigned whatToShow, RefPtr<NodeFilter>&& filter)
    : NodeIteratorBase(rootNode, whatToShow, WTFMove(filter))
    , m_referenceNode(rootNode, true)
{
    root().document().attachNodeIterator(*this);
}

Ref<NodeIterator> NodeIterator::create(Node& rootNode, unsigned whatToShow, RefPtr<NodeFilter>&& filter)
{
    return adoptRef(*new NodeIterator(rootNode, whatToShow, WTFMove(filter)));
}

NodeIterator::~NodeIterator()
{
    root().document().detachNodeIterator(*this);
}

ExceptionOr<RefPtr<Node>> NodeIterator::nextNode()
{
    RefPtr<Node> result;

    m_candidateNode = m_referenceNode;
    while (m_candidateNode.moveToNext(root())) {
        // NodeIterators treat the DOM tree as a flat list of nodes.
        // In other words, FILTER_REJECT does not pass over descendants
        // of the rejected node. Hence, FILTER_REJECT is the same as FILTER_SKIP.
        RefPtr<Node> provisionalResult = m_candidateNode.node;

        auto filterResult = acceptNode(*provisionalResult);
        if (filterResult.hasException()) {
            m_candidateNode.clear();
            return filterResult.releaseException();
        }

        bool nodeWasAccepted = filterResult.returnValue() == NodeFilter::FILTER_ACCEPT;
        if (nodeWasAccepted) {
            m_referenceNode = m_candidateNode;
            result = WTFMove(provisionalResult);
            break;
        }
    }

    m_candidateNode.clear();
    return result;
}

ExceptionOr<RefPtr<Node>> NodeIterator::previousNode()
{
    RefPtr<Node> result;

    m_candidateNode = m_referenceNode;
    while (m_candidateNode.moveToPrevious(root())) {
        // NodeIterators treat the DOM tree as a flat list of nodes.
        // In other words, FILTER_REJECT does not pass over descendants
        // of the rejected node. Hence, FILTER_REJECT is the same as FILTER_SKIP.
        RefPtr<Node> provisionalResult = m_candidateNode.node;

        auto filterResult = acceptNode(*provisionalResult);
        if (filterResult.hasException()) {
            m_candidateNode.clear();
            return filterResult.releaseException();
        }

        bool nodeWasAccepted = filterResult.returnValue() == NodeFilter::FILTER_ACCEPT;
        if (nodeWasAccepted) {
            m_referenceNode = m_candidateNode;
            result = WTFMove(provisionalResult);
            break;
        }
    }

    m_candidateNode.clear();
    return result;
}

void NodeIterator::nodeWillBeRemoved(Node& removedNode)
{
    updateForNodeRemoval(removedNode, m_candidateNode);
    updateForNodeRemoval(removedNode, m_referenceNode);
}

void NodeIterator::updateForNodeRemoval(Node& removedNode, NodePointer& referenceNode) const
{
    ASSERT(&root().document() == &removedNode.document());

    // Iterator is not affected if the removed node is the reference node and is the root.
    // or if removed node is not the reference node, or the ancestor of the reference node.
    if (!removedNode.isDescendantOf(root()))
        return;
    bool willRemoveReferenceNode = &removedNode == referenceNode.node;
    bool willRemoveReferenceNodeAncestor = referenceNode.node && referenceNode.node->isDescendantOf(removedNode);
    if (!willRemoveReferenceNode && !willRemoveReferenceNodeAncestor)
        return;

    if (referenceNode.isPointerBeforeNode) {
        Node* node = NodeTraversal::next(removedNode, &root());
        if (node) {
            // Move out from under the node being removed if the new reference
            // node is a descendant of the node being removed.
            while (node && node->isDescendantOf(removedNode))
                node = NodeTraversal::next(*node, &root());
            if (node)
                referenceNode.node = node;
        } else {
            node = NodeTraversal::previous(removedNode);
            if (node) {
                // Move out from under the node being removed if the reference node is
                // a descendant of the node being removed.
                if (willRemoveReferenceNodeAncestor) {
                    while (node && node->isDescendantOf(&removedNode))
                        node = NodeTraversal::previous(*node);
                }
                if (node) {
                    // Removing last node.
                    // Need to move the pointer after the node preceding the 
                    // new reference node.
                    referenceNode.node = node;
                    referenceNode.isPointerBeforeNode = false;
                }
            }
        }
    } else {
        Node* node = NodeTraversal::previous(removedNode);
        if (node) {
            // Move out from under the node being removed if the reference node is
            // a descendant of the node being removed.
            if (willRemoveReferenceNodeAncestor) {
                while (node && node->isDescendantOf(removedNode))
                    node = NodeTraversal::previous(*node);
            }
            if (node)
                referenceNode.node = node;
        } else {
            // FIXME: This branch doesn't appear to have any LayoutTests.
            node = NodeTraversal::next(removedNode, &root());
            // Move out from under the node being removed if the reference node is
            // a descendant of the node being removed.
            if (willRemoveReferenceNodeAncestor) {
                while (node && node->isDescendantOf(removedNode))
                    node = NodeTraversal::previous(*node);
            }
            if (node)
                referenceNode.node = node;
        }
    }
}

} // namespace WebCore
