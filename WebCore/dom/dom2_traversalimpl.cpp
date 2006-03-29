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

#include "Document.h"
#include "ExceptionCode.h"

namespace WebCore {

short NodeFilterCondition::acceptNode(Node*) const
{
    return NodeFilter::FILTER_ACCEPT;
}

// --------------------------------------------------------------

NodeFilter::NodeFilter(NodeFilterCondition *condition)
    : m_condition(condition)
{
}

short NodeFilter::acceptNode(Node *node) const
{
    // cast to short silences "enumeral and non-enumeral types in return" warning
    return m_condition ? m_condition->acceptNode(node) : static_cast<short>(FILTER_ACCEPT);
}

// --------------------------------------------------------------

Traversal::Traversal(Node* rootNode, unsigned whatToShow, PassRefPtr<NodeFilter> nodeFilter, bool expandEntityReferences)
    : m_root(rootNode)
    , m_whatToShow(whatToShow)
    , m_filter(nodeFilter)
    , m_expandEntityReferences(expandEntityReferences)
{
}

Traversal::~Traversal()
{
}

short Traversal::acceptNode(Node *node) const
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

NodeIterator::NodeIterator(Node *rootNode, unsigned whatToShow, PassRefPtr<NodeFilter> filter, bool expandEntityReferences)
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

Node *NodeIterator::findNextNode(Node *node) const
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

Node *NodeIterator::nextNode(ExceptionCode& ec)
{
    if (detached()) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    Node *node = referenceNode() ? referenceNode() : root();
    if (!pointerBeforeReferenceNode() || acceptNode(node) != NodeFilter::FILTER_ACCEPT)
        node = findNextNode(node);
    if (node)
        setReferenceNode(node);
    setPointerBeforeReferenceNode(false);
    return node;
}

Node *NodeIterator::findPreviousNode(Node *node) const
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

Node *NodeIterator::previousNode(ExceptionCode&)
{
    Node *node = referenceNode() ? referenceNode() : root();
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

void NodeIterator::setReferenceNode(Node *node)
{
    m_referenceNode = node;
}

void NodeIterator::notifyBeforeNodeRemoval(Node *removedNode)
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
        Node *node = findNextNode(removedNode);
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
        Node *node = findPreviousNode(removedNode);
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

TreeWalker::TreeWalker(Node *rootNode, unsigned whatToShow, PassRefPtr<NodeFilter> filter, bool expandEntityReferences)
    : Traversal(rootNode, whatToShow, filter, expandEntityReferences)
    , m_current(rootNode)
{
}

void TreeWalker::setCurrentNode(Node *node, ExceptionCode& ec)
{
    if (!node) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    m_current = node;
}

void TreeWalker::setCurrentNode(Node *node)
{
    assert(node);
    int dummy;
    setCurrentNode(node, dummy);
}

Node *TreeWalker::parentNode()
{
    Node *result = 0;
    for (Node *node = currentNode()->parentNode(); node && node != root(); node = node->parentNode()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

Node *TreeWalker::firstChild()
{
    Node *result = 0;
    for (Node *node = currentNode()->firstChild(); node; node = node->nextSibling()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

Node *TreeWalker::lastChild()
{
    Node *result = 0;
    for (Node *node = currentNode()->lastChild(); node; node = node->previousSibling()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

Node *TreeWalker::previousSibling()
{
    Node *result = 0;
    for (Node *node = currentNode()->previousSibling(); node; node = node->previousSibling()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

Node *TreeWalker::nextSibling()
{
    Node *result = 0;
    for (Node *node = currentNode()->nextSibling(); node; node = node->nextSibling()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

Node *TreeWalker::previousNode()
{
    Node *result = 0;
    for (Node *node = currentNode()->traversePreviousNode(); node; node = node->traversePreviousNode()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT && !ancestorRejected(node)) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

Node *TreeWalker::nextNode()
{
    Node *result = 0;
    for (Node *node = currentNode()->traverseNextNode(); node; node = node->traverseNextNode()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT && !ancestorRejected(node)) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

bool TreeWalker::ancestorRejected(const Node *node) const
{
    for (Node *a = node->parentNode(); a && a != root(); a = a->parentNode())
        if (acceptNode(a) == NodeFilter::FILTER_REJECT)
            return true;
    return false;
}

} // namespace WebCore
