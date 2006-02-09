/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004 Apple Computer, Inc.
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
#include "dom2_traversalimpl.h"
#include "dom/dom_exception.h"
#include "DocumentImpl.h"

namespace DOM {

NodeFilterImpl::NodeFilterImpl(NodeFilterCondition *condition)
    : m_condition(condition)
{
}

short NodeFilterImpl::acceptNode(NodeImpl *node) const
{
    // cast to short silences "enumeral and non-enumeral types in return" warning
    return m_condition ? m_condition->acceptNode(node) : static_cast<short>(NodeFilter::FILTER_ACCEPT);
}

// --------------------------------------------------------------

TraversalImpl::TraversalImpl(NodeImpl *rootNode, int whatToShow, NodeFilterImpl *nodeFilter, bool expandEntityReferences)
    : m_root(rootNode)
    , m_whatToShow(whatToShow)
    , m_filter(nodeFilter)
    , m_expandEntityReferences(expandEntityReferences)
{
}

TraversalImpl::~TraversalImpl()
{
}

short TraversalImpl::acceptNode(NodeImpl *node) const
{
    // FIXME: If XML is implemented we have to check expandEntityRerefences in this function.
    // The bid twiddling here is done to map DOM node types, which are given as integers from
    // 1 through 12, to whatToShow bit masks.
    if (node && ((1 << (node->nodeType()-1)) & m_whatToShow) != 0)
        // cast to short silences "enumeral and non-enumeral types in return" warning
        return m_filter ? m_filter->acceptNode(node) : static_cast<short>(NodeFilter::FILTER_ACCEPT);
    return NodeFilter::FILTER_SKIP;
}

// --------------------------------------------------------------

NodeIteratorImpl::NodeIteratorImpl(NodeImpl *rootNode, int whatToShow, NodeFilterImpl *filter, bool expandEntityReferences)
    : TraversalImpl(rootNode, whatToShow, filter, expandEntityReferences)
    , m_beforeReferenceNode(true)
    , m_detached(false)
    , m_doc(rootNode ? rootNode->getDocument() : 0)
{
    if (document())
        document()->attachNodeIterator(this);
}

NodeIteratorImpl::~NodeIteratorImpl()
{
    if (document())
        document()->detachNodeIterator(this);
}

NodeImpl *NodeIteratorImpl::findNextNode(NodeImpl *node) const
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

NodeImpl *NodeIteratorImpl::nextNode(int &exceptioncode)
{
    if (detached()) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return 0;
    }

    NodeImpl *node = referenceNode() ? referenceNode() : root();
    if (!pointerBeforeReferenceNode() || acceptNode(node) != NodeFilter::FILTER_ACCEPT)
        node = findNextNode(node);
    if (node)
        setReferenceNode(node);
    setPointerBeforeReferenceNode(false);
    return node;
}

NodeImpl *NodeIteratorImpl::findPreviousNode(NodeImpl *node) const
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

NodeImpl *NodeIteratorImpl::previousNode(int &exceptioncode)
{
    NodeImpl *node = referenceNode() ? referenceNode() : root();
    if (pointerBeforeReferenceNode() || acceptNode(node) != NodeFilter::FILTER_ACCEPT)
        node = findPreviousNode(node);
    if (node)
        setReferenceNode(node);
    setPointerBeforeReferenceNode();
    return node;
}

void NodeIteratorImpl::detach(int &/*exceptioncode*/)
{
    if (!detached() && document())
        document()->detachNodeIterator(this);
    setDetached();
}

void NodeIteratorImpl::setReferenceNode(NodeImpl *node)
{
    m_referenceNode = node;
}

void NodeIteratorImpl::notifyBeforeNodeRemoval(NodeImpl *removedNode)
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
        NodeImpl *node = findNextNode(removedNode);
        if (node) {
            // Move out from under the node being removed if the reference node is
            // a descendant of the node being removed.
            if (willRemoveReferenceNodeAncestor) {
                while (node && node->isAncestor(removedNode))
                    node = findNextNode(node);
            }
            if (node)
                setReferenceNode(node);
        }
        else {
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
    }
    else {
        NodeImpl *node = findPreviousNode(removedNode);
        if (node) {
            // Move out from under the node being removed if the reference node is
            // a descendant of the node being removed.
            if (willRemoveReferenceNodeAncestor) {
                while (node && node->isAncestor(removedNode))
                    node = findPreviousNode(node);
            }
            if (node)
                setReferenceNode(node);
        }
        else {
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

// --------------------------------------------------------------

TreeWalkerImpl::TreeWalkerImpl(NodeImpl *rootNode, int whatToShow, NodeFilterImpl *filter, bool expandEntityReferences)
    : TraversalImpl(rootNode, whatToShow, filter, expandEntityReferences)
    , m_current(rootNode)
{
}

void TreeWalkerImpl::setCurrentNode(NodeImpl *node, int &exceptioncode)
{
    if (!node) {
        exceptioncode = DOMException::NOT_SUPPORTED_ERR;
        return;
    }

    m_current = node;
}

void TreeWalkerImpl::setCurrentNode(NodeImpl *node)
{
    assert(node);
    int dummy;
    setCurrentNode(node, dummy);
}

NodeImpl *TreeWalkerImpl::parentNode()
{
    NodeImpl *result = 0;
    for (NodeImpl *node = currentNode()->parentNode(); node && node != root(); node = node->parentNode()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

NodeImpl *TreeWalkerImpl::firstChild()
{
    NodeImpl *result = 0;
    for (NodeImpl *node = currentNode()->firstChild(); node; node = node->nextSibling()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

NodeImpl *TreeWalkerImpl::lastChild()
{
    NodeImpl *result = 0;
    for (NodeImpl *node = currentNode()->lastChild(); node; node = node->previousSibling()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

NodeImpl *TreeWalkerImpl::previousSibling()
{
    NodeImpl *result = 0;
    for (NodeImpl *node = currentNode()->previousSibling(); node; node = node->previousSibling()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

NodeImpl *TreeWalkerImpl::nextSibling()
{
    NodeImpl *result = 0;
    for (NodeImpl *node = currentNode()->nextSibling(); node; node = node->nextSibling()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

NodeImpl *TreeWalkerImpl::previousNode()
{
    NodeImpl *result = 0;
    for (NodeImpl *node = currentNode()->traversePreviousNode(); node; node = node->traversePreviousNode()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT && !ancestorRejected(node)) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

NodeImpl *TreeWalkerImpl::nextNode()
{
    NodeImpl *result = 0;
    for (NodeImpl *node = currentNode()->traverseNextNode(); node; node = node->traverseNextNode()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT && !ancestorRejected(node)) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

bool TreeWalkerImpl::ancestorRejected(const NodeImpl *node) const
{
    for (NodeImpl *a = node->parentNode(); a && a != root(); a = a->parentNode())
        if (acceptNode(a) == NodeFilter::FILTER_REJECT)
            return true;
    return false;
}

} // namespace DOM
